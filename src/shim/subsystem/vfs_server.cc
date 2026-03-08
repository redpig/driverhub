// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unified VFS IPC server for DevFs, SysFs, and ProcFs.
//
// Serves all three virtual filesystem trees over a single Unix-domain
// socket at /tmp/driverhub-vfs.sock.  The devctl client tool connects
// here to browse and interact with devices, sysfs attributes, and
// proc entries created by loaded .ko modules.
//
// Protocol (text-based, one command per connection):
//
//   LS /dev                     List /dev entries
//   LS /sys                     List /sys entries
//   LS /proc                    List /proc entries
//   LS /dev/rfkill              List specific path
//   CAT /proc/<entry>           Read a proc entry
//   CAT /sys/<path>             Read a sysfs attribute
//   ECHO <data> > /proc/<path>  Write to a proc entry
//   ECHO <data> > /sys/<path>   Write to a sysfs attribute
//   OPEN /dev/<name>            Open a device, returns fd
//   READ <fd> <count>           Read from open device fd
//   WRITE <fd> <data>           Write to open device fd
//   IOCTL <fd> <cmd> <arg>      Ioctl on open device fd
//   CLOSE <fd>                  Close a device fd
//   TREE                        Show full VFS tree
//   STAT <path>                 Show node info

#include "src/shim/subsystem/vfs_service.h"

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace driverhub {

namespace {

constexpr const char* kDefaultSocket = "/tmp/driverhub-vfs.sock";

std::atomic<bool> g_running{false};
std::thread g_server_thread;
int g_listen_fd = -1;
std::mutex g_ready_mu;
std::condition_variable g_ready_cv;
bool g_ready = false;

const char* NodeTypeName(VfsNodeType type) {
  switch (type) {
    case VfsNodeType::kDirectory:    return "dir";
    case VfsNodeType::kCharDevice:   return "chrdev";
    case VfsNodeType::kMiscDevice:   return "misc";
    case VfsNodeType::kProcEntry:    return "proc";
    case VfsNodeType::kSysfsAttr:    return "sysfs";
    case VfsNodeType::kDebugfsEntry: return "debugfs";
    default:                         return "unknown";
  }
}

std::string FormatNode(const VfsNode* node) {
  std::ostringstream os;
  os << NodeTypeName(node->type) << " ";
  os << std::oct << node->mode << std::dec << " ";
  os << node->path;
  if (node->type == VfsNodeType::kMiscDevice ||
      node->type == VfsNodeType::kCharDevice) {
    os << " (" << node->major << ":" << node->minor << ")";
  }
  if (node->fops) os << " [fops]";
  if (node->dev_attr) os << " [attr]";
  if (node->single_show) os << " [show]";
  return os.str();
}

void FormatTree(const VfsNode* node, int depth, std::ostringstream& os) {
  for (int i = 0; i < depth; i++) os << "  ";
  if (node->type == VfsNodeType::kDirectory) {
    os << node->name << "/\n";
  } else {
    os << node->name;
    if (node->type == VfsNodeType::kMiscDevice ||
        node->type == VfsNodeType::kCharDevice) {
      os << " (" << node->major << ":" << node->minor << ")";
    }
    os << "\n";
  }
  for (const auto& [name, child] : node->children) {
    FormatTree(child.get(), depth + 1, os);
  }
}

// Determine which service owns a path.
enum class VfsRoot { kDev, kSys, kProc, kUnknown };

VfsRoot ParseRoot(const std::string& path, std::string* subpath) {
  if (path.substr(0, 4) == "/dev" || path.substr(0, 3) == "dev") {
    size_t start = (path[0] == '/') ? 5 : 4;
    *subpath = (start < path.size()) ? path.substr(start) : "";
    return VfsRoot::kDev;
  }
  if (path.substr(0, 4) == "/sys" || path.substr(0, 3) == "sys") {
    size_t start = (path[0] == '/') ? 5 : 4;
    *subpath = (start < path.size()) ? path.substr(start) : "";
    return VfsRoot::kSys;
  }
  if (path.substr(0, 5) == "/proc" || path.substr(0, 4) == "proc") {
    size_t start = (path[0] == '/') ? 6 : 5;
    *subpath = (start < path.size()) ? path.substr(start) : "";
    return VfsRoot::kProc;
  }
  *subpath = path;
  return VfsRoot::kUnknown;
}

void HandleClient(int client_fd) {
  char buf[4096];
  ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
  if (n <= 0) {
    close(client_fd);
    return;
  }
  buf[n] = '\0';
  while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';

  std::string cmd(buf);
  std::string response;

  std::istringstream iss(cmd);
  std::string verb;
  iss >> verb;
  for (auto& c : verb) c = static_cast<char>(toupper(c));

  if (verb == "LS") {
    std::string path;
    iss >> path;
    if (path.empty()) path = "/";

    std::ostringstream os;
    std::string subpath;
    VfsRoot root = ParseRoot(path, &subpath);

    std::vector<const VfsNode*> nodes;
    if (root == VfsRoot::kDev) {
      if (subpath.empty()) {
        nodes = DevFs::Instance().tree().ListDir("");
      } else {
        auto* node = DevFs::Instance().tree().Lookup(subpath);
        if (node && node->type == VfsNodeType::kDirectory) {
          for (const auto& [n, c] : node->children)
            nodes.push_back(c.get());
        } else if (node) {
          nodes.push_back(node);
        }
      }
    } else if (root == VfsRoot::kSys) {
      if (subpath.empty()) {
        nodes = SysFs::Instance().tree().ListDir("");
      } else {
        auto* node = SysFs::Instance().tree().Lookup(subpath);
        if (node && node->type == VfsNodeType::kDirectory) {
          for (const auto& [n, c] : node->children)
            nodes.push_back(c.get());
        } else if (node) {
          nodes.push_back(node);
        }
      }
    } else if (root == VfsRoot::kProc) {
      if (subpath.empty()) {
        nodes = ProcFs::Instance().tree().ListDir("");
      } else {
        auto* node = ProcFs::Instance().tree().Lookup(subpath);
        if (node && node->type == VfsNodeType::kDirectory) {
          for (const auto& [n, c] : node->children)
            nodes.push_back(c.get());
        } else if (node) {
          nodes.push_back(node);
        }
      }
    } else {
      // List all three roots.
      os << "/dev  (devfs)\n/sys  (sysfs)\n/proc (procfs)\n";
      response = "OK\n" + os.str();
      write(client_fd, response.data(), response.size());
      close(client_fd);
      return;
    }

    os << "OK " << nodes.size() << " entries\n";
    for (const auto* node : nodes) {
      os << FormatNode(node) << "\n";
    }
    response = os.str();

  } else if (verb == "CAT") {
    std::string path;
    iss >> path;
    std::string subpath;
    VfsRoot root = ParseRoot(path, &subpath);

    char read_buf[4096];
    ssize_t ret = -22;

    if (root == VfsRoot::kSys) {
      ret = SysFs::Instance().ReadAttr(subpath, read_buf, sizeof(read_buf) - 1);
    } else if (root == VfsRoot::kProc) {
      ret = ProcFs::Instance().Read(subpath, read_buf, sizeof(read_buf) - 1);
    } else {
      response = "ERR: CAT not supported for " + path + "\n";
      write(client_fd, response.data(), response.size());
      close(client_fd);
      return;
    }

    if (ret >= 0) {
      read_buf[ret] = '\0';
      response = "OK " + std::to_string(ret) + " bytes\n";
      response += std::string(read_buf, ret);
      if (ret > 0 && read_buf[ret-1] != '\n') response += "\n";
    } else {
      response = "ERR: read failed (" + std::to_string(ret) + ")\n";
    }

  } else if (verb == "ECHO") {
    // ECHO <data> > <path>
    std::string rest;
    std::getline(iss, rest);
    // Trim leading space.
    if (!rest.empty() && rest[0] == ' ') rest = rest.substr(1);

    auto arrow_pos = rest.find('>');
    if (arrow_pos == std::string::npos) {
      response = "ERR: usage: ECHO <data> > <path>\n";
    } else {
      std::string data = rest.substr(0, arrow_pos);
      // Trim trailing spaces from data.
      while (!data.empty() && data.back() == ' ') data.pop_back();
      std::string path = rest.substr(arrow_pos + 1);
      // Trim leading spaces from path.
      while (!path.empty() && path[0] == ' ') path = path.substr(1);

      std::string subpath;
      VfsRoot root = ParseRoot(path, &subpath);

      ssize_t ret = -22;
      if (root == VfsRoot::kSys) {
        ret = SysFs::Instance().WriteAttr(subpath, data.c_str(), data.size());
      } else if (root == VfsRoot::kProc) {
        ret = ProcFs::Instance().Write(subpath, data.c_str(), data.size());
      }

      if (ret >= 0) {
        response = "OK wrote " + std::to_string(ret) + " bytes\n";
      } else {
        response = "ERR: write failed (" + std::to_string(ret) + ")\n";
      }
    }

  } else if (verb == "OPEN") {
    std::string path;
    iss >> path;
    std::string subpath;
    VfsRoot root = ParseRoot(path, &subpath);

    if (root != VfsRoot::kDev) {
      response = "ERR: OPEN only works on /dev paths\n";
    } else {
      int fd = DevFs::Instance().Open(subpath);
      if (fd >= 0) {
        response = "OK fd=" + std::to_string(fd) + "\n";
      } else {
        response = "ERR: open failed (" + std::to_string(fd) + ")\n";
      }
    }

  } else if (verb == "READ") {
    int fd = 0;
    size_t count = 4096;
    iss >> fd >> count;
    if (count > 4096) count = 4096;
    char read_buf[4096];
    ssize_t ret = DevFs::Instance().Read(fd, read_buf, count);
    if (ret >= 0) {
      response = "OK " + std::to_string(ret) + " bytes\n";
      response += std::string(read_buf, ret);
    } else {
      response = "ERR: read failed (" + std::to_string(ret) + ")\n";
    }

  } else if (verb == "WRITE") {
    int fd = 0;
    iss >> fd;
    std::string data;
    std::getline(iss, data);
    if (!data.empty() && data[0] == ' ') data = data.substr(1);
    ssize_t ret = DevFs::Instance().Write(fd, data.c_str(), data.size());
    if (ret >= 0) {
      response = "OK wrote " + std::to_string(ret) + " bytes\n";
    } else {
      response = "ERR: write failed (" + std::to_string(ret) + ")\n";
    }

  } else if (verb == "IOCTL") {
    int fd = 0;
    unsigned int cmd = 0;
    unsigned long arg = 0;
    iss >> fd >> cmd >> arg;
    long ret = DevFs::Instance().Ioctl(fd, cmd, arg);
    if (ret >= 0) {
      response = "OK result=" + std::to_string(ret) + "\n";
    } else {
      response = "ERR: ioctl failed (" + std::to_string(ret) + ")\n";
    }

  } else if (verb == "CLOSE") {
    int fd = 0;
    iss >> fd;
    int ret = DevFs::Instance().Close(fd);
    if (ret == 0) {
      response = "OK closed fd=" + std::to_string(fd) + "\n";
    } else {
      response = "ERR: close failed (" + std::to_string(ret) + ")\n";
    }

  } else if (verb == "TREE") {
    std::ostringstream os;
    os << "OK\n";
    FormatTree(DevFs::Instance().tree().root(), 0, os);
    FormatTree(SysFs::Instance().tree().root(), 0, os);
    FormatTree(ProcFs::Instance().tree().root(), 0, os);
    response = os.str();

  } else if (verb == "STAT") {
    std::string path;
    iss >> path;
    std::string subpath;
    VfsRoot root = ParseRoot(path, &subpath);

    const VfsNode* node = nullptr;
    if (root == VfsRoot::kDev) {
      node = DevFs::Instance().tree().Lookup(subpath);
    } else if (root == VfsRoot::kSys) {
      node = SysFs::Instance().tree().Lookup(subpath);
    } else if (root == VfsRoot::kProc) {
      node = ProcFs::Instance().tree().Lookup(subpath);
    }

    if (node) {
      response = "OK\n" + FormatNode(node) + "\n";
    } else {
      response = "ERR: not found: " + path + "\n";
    }

  } else {
    response = "ERR: unknown command: " + verb + "\n"
               "Commands: LS, CAT, ECHO, OPEN, READ, WRITE, IOCTL, "
               "CLOSE, TREE, STAT\n";
  }

  write(client_fd, response.data(), response.size());
  close(client_fd);
}

void ServerLoop(const std::string& socket_path) {
  unlink(socket_path.c_str());

  g_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (g_listen_fd < 0) {
    perror("driverhub: vfs_server: socket");
    return;
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

  if (bind(g_listen_fd, reinterpret_cast<struct sockaddr*>(&addr),
           sizeof(addr)) < 0) {
    perror("driverhub: vfs_server: bind");
    close(g_listen_fd);
    g_listen_fd = -1;
    return;
  }

  if (listen(g_listen_fd, 8) < 0) {
    perror("driverhub: vfs_server: listen");
    close(g_listen_fd);
    g_listen_fd = -1;
    return;
  }

  fprintf(stderr, "driverhub: vfs_server: listening on %s\n",
          socket_path.c_str());

  {
    std::lock_guard<std::mutex> lk(g_ready_mu);
    g_ready = true;
  }
  g_ready_cv.notify_all();

  while (g_running.load()) {
    struct pollfd pfd;
    pfd.fd = g_listen_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int ret = poll(&pfd, 1, 1000);
    if (ret > 0 && (pfd.revents & POLLIN)) {
      int client_fd = accept(g_listen_fd, nullptr, nullptr);
      if (client_fd >= 0) {
        HandleClient(client_fd);
      }
    }
  }

  close(g_listen_fd);
  g_listen_fd = -1;
  unlink(socket_path.c_str());
  fprintf(stderr, "driverhub: vfs_server: stopped\n");
}

}  // namespace

void StartVfsServer(const std::string& socket_path) {
  if (g_running.exchange(true)) return;

  g_ready = false;
  std::string path = socket_path.empty() ? kDefaultSocket : socket_path;
  g_server_thread = std::thread(ServerLoop, path);
  g_server_thread.detach();

  std::unique_lock<std::mutex> lk(g_ready_mu);
  g_ready_cv.wait_for(lk, std::chrono::seconds(5), [] { return g_ready; });
}

void StopVfsServer() {
  g_running.store(false);
  if (g_listen_fd >= 0) {
    shutdown(g_listen_fd, SHUT_RDWR);
  }
}

}  // namespace driverhub
