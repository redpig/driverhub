// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// devctl — unified CLI for DriverHub virtual filesystems.
//
// Provides a single tool for interacting with /dev, /sys, and /proc
// as served by the DriverHub VFS services.  This is the DriverHub
// equivalent of Starnix's filesystem access layer.
//
// Usage:
//   devctl ls /dev               List /dev entries
//   devctl ls /sys               List /sys entries
//   devctl ls /proc              List /proc entries
//   devctl cat /proc/meminfo     Read a proc entry
//   devctl cat /sys/class/rfkill/rfkill0/state  Read a sysfs attr
//   devctl write /sys/...  "1"   Write to a sysfs attribute
//   devctl open /dev/rfkill      Open a device
//   devctl read <fd> [count]     Read from device fd
//   devctl write <fd> <data>     Write to device fd
//   devctl ioctl <fd> <cmd> <arg>  Ioctl on device fd
//   devctl close <fd>            Close device fd
//   devctl tree                  Show full VFS tree
//   devctl stat /dev/rfkill      Show node info

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr const char* kDefaultSocket = "/tmp/driverhub-vfs.sock";

int SendCommand(const char* socket_path, const std::string& command) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("devctl: socket");
    return 1;
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

  if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr),
              sizeof(addr)) < 0) {
    fprintf(stderr, "devctl: cannot connect to %s\n", socket_path);
    fprintf(stderr, "Is the DriverHub VFS service running?\n");
    close(fd);
    return 1;
  }

  std::string msg = command + "\n";
  write(fd, msg.data(), msg.size());
  shutdown(fd, SHUT_WR);

  char buf[8192];
  ssize_t total = 0;
  while (true) {
    ssize_t n = read(fd, buf + total, sizeof(buf) - total - 1);
    if (n <= 0) break;
    total += n;
  }
  buf[total] = '\0';
  close(fd);

  printf("%s", buf);
  return (total > 0 && strncmp(buf, "OK", 2) == 0) ? 0 : 1;
}

void PrintUsage() {
  printf(
      "devctl — DriverHub virtual filesystem control\n"
      "\n"
      "Usage:\n"
      "  devctl ls [path]                 List directory entries\n"
      "  devctl cat <path>                Read a proc/sysfs entry\n"
      "  devctl write <path> <data>       Write to a proc/sysfs entry\n"
      "  devctl open <path>               Open a /dev device node\n"
      "  devctl read <fd> [count]         Read from an open device\n"
      "  devctl ioctl <fd> <cmd> [arg]    Send ioctl to device\n"
      "  devctl close <fd>                Close a device fd\n"
      "  devctl tree                      Show full VFS tree\n"
      "  devctl stat <path>               Show node info\n"
      "\n"
      "Paths:\n"
      "  /dev/*    Device nodes (misc_register, cdev_add)\n"
      "  /sys/*    Sysfs attributes (device_create_file, sysfs_create_group)\n"
      "  /proc/*   Proc entries (proc_create, proc_create_single_data)\n"
      "\n"
      "Options:\n"
      "  -s <path>    Socket path (default: /tmp/driverhub-vfs.sock)\n"
      "  -h           Show this help\n"
      "\n"
      "Examples:\n"
      "  devctl ls /dev                   Show all device nodes\n"
      "  devctl tree                      Show complete VFS tree\n"
      "  devctl stat /dev/rfkill          Show rfkill device info\n"
      "  devctl cat /proc/version         Read /proc/version\n"
      "  devctl open /dev/rfkill          Open rfkill device\n"
      "  devctl read 100 256              Read 256 bytes from fd 100\n"
      "  devctl close 100                 Close fd 100\n");
}

}  // namespace

int main(int argc, char* argv[]) {
  const char* socket_path = kDefaultSocket;

  int argi = 1;
  while (argi < argc && argv[argi][0] == '-') {
    if (strcmp(argv[argi], "-s") == 0 && argi + 1 < argc) {
      socket_path = argv[++argi];
      argi++;
    } else if (strcmp(argv[argi], "-h") == 0 ||
               strcmp(argv[argi], "--help") == 0) {
      PrintUsage();
      return 0;
    } else {
      fprintf(stderr, "devctl: unknown option: %s\n", argv[argi]);
      return 1;
    }
  }

  if (argi >= argc) {
    PrintUsage();
    return 1;
  }

  std::string verb = argv[argi++];

  if (verb == "ls") {
    std::string path = (argi < argc) ? argv[argi] : "/";
    return SendCommand(socket_path, "LS " + path);

  } else if (verb == "cat") {
    if (argi >= argc) {
      fprintf(stderr, "devctl: cat requires a path\n");
      return 1;
    }
    return SendCommand(socket_path, "CAT " + std::string(argv[argi]));

  } else if (verb == "write") {
    if (argi + 1 >= argc) {
      fprintf(stderr, "devctl: write requires <path> <data>\n");
      return 1;
    }
    std::string path = argv[argi++];
    std::string data = argv[argi];
    // Check if path looks like a fd (all digits) for device write.
    bool is_fd = true;
    for (char c : path) {
      if (!isdigit(c)) { is_fd = false; break; }
    }
    if (is_fd) {
      return SendCommand(socket_path, "WRITE " + path + " " + data);
    }
    return SendCommand(socket_path, "ECHO " + data + " > " + path);

  } else if (verb == "open") {
    if (argi >= argc) {
      fprintf(stderr, "devctl: open requires a path\n");
      return 1;
    }
    return SendCommand(socket_path, "OPEN " + std::string(argv[argi]));

  } else if (verb == "read") {
    if (argi >= argc) {
      fprintf(stderr, "devctl: read requires <fd> [count]\n");
      return 1;
    }
    std::string fd_str = argv[argi++];
    std::string count = (argi < argc) ? argv[argi] : "4096";
    return SendCommand(socket_path, "READ " + fd_str + " " + count);

  } else if (verb == "ioctl") {
    if (argi + 1 >= argc) {
      fprintf(stderr, "devctl: ioctl requires <fd> <cmd> [arg]\n");
      return 1;
    }
    std::string fd_str = argv[argi++];
    std::string cmd_str = argv[argi++];
    std::string arg_str = (argi < argc) ? argv[argi] : "0";
    return SendCommand(socket_path,
                        "IOCTL " + fd_str + " " + cmd_str + " " + arg_str);

  } else if (verb == "close") {
    if (argi >= argc) {
      fprintf(stderr, "devctl: close requires <fd>\n");
      return 1;
    }
    return SendCommand(socket_path, "CLOSE " + std::string(argv[argi]));

  } else if (verb == "tree") {
    return SendCommand(socket_path, "TREE");

  } else if (verb == "stat") {
    if (argi >= argc) {
      fprintf(stderr, "devctl: stat requires a path\n");
      return 1;
    }
    return SendCommand(socket_path, "STAT " + std::string(argv[argi]));

  } else if (verb == "help" || verb == "-h" || verb == "--help") {
    PrintUsage();
    return 0;

  } else {
    fprintf(stderr, "devctl: unknown command: %s\n", verb.c_str());
    PrintUsage();
    return 1;
  }
}
