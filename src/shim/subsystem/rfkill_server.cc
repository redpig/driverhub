// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/subsystem/rfkill_server.h"

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>

#include "src/shim/subsystem/rfkill.h"

namespace driverhub {

namespace {

constexpr const char* kDefaultSocket = "/tmp/rfkill.sock";

std::atomic<bool> g_running{false};
std::thread g_server_thread;
int g_listen_fd = -1;
std::mutex g_ready_mu;
std::condition_variable g_ready_cv;
bool g_ready = false;

// Format a RadioInfo into a text line.
std::string FormatRadio(const RadioInfo& r) {
  std::ostringstream os;
  os << r.index << " " << RfkillTypeName(r.type) << " " << r.name
     << " soft=" << (r.state.soft_blocked ? "blocked" : "unblocked")
     << " hard=" << (r.state.hard_blocked ? "blocked" : "unblocked");
  return os.str();
}

RfkillType ParseType(const std::string& s) {
  if (s == "wlan" || s == "1") return RfkillType::kWlan;
  if (s == "bluetooth" || s == "bt" || s == "2") return RfkillType::kBluetooth;
  if (s == "uwb" || s == "3") return RfkillType::kUwb;
  if (s == "wimax" || s == "4") return RfkillType::kWimax;
  if (s == "wwan" || s == "5") return RfkillType::kWwan;
  if (s == "gps" || s == "6") return RfkillType::kGps;
  if (s == "fm" || s == "7") return RfkillType::kFm;
  if (s == "nfc" || s == "8") return RfkillType::kNfc;
  return RfkillType::kAll;
}

// Handle a single client connection.
void HandleClient(int client_fd) {
  char buf[1024];
  ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
  if (n <= 0) {
    close(client_fd);
    return;
  }
  buf[n] = '\0';

  // Trim trailing newline.
  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
    buf[--n] = '\0';
  }

  std::string cmd(buf);
  std::string response;

  // Parse command.
  std::istringstream iss(cmd);
  std::string verb;
  iss >> verb;

  // Convert to uppercase for comparison.
  for (auto& c : verb) c = static_cast<char>(toupper(c));

  auto& svc = RfkillService::Instance();

  if (verb == "LIST") {
    auto radios = svc.GetRadios();
    if (radios.empty()) {
      response = "OK 0 radios\n";
    } else {
      std::ostringstream os;
      os << "OK " << radios.size() << " radios\n";
      for (const auto& r : radios) {
        os << FormatRadio(r) << "\n";
      }
      response = os.str();
    }
  } else if (verb == "BLOCK") {
    uint32_t idx = 0;
    if (iss >> idx) {
      if (svc.SetSoftBlock(idx, true)) {
        response = "OK blocked " + std::to_string(idx) + "\n";
      } else {
        response = "ERR radio not found\n";
      }
    } else {
      response = "ERR usage: BLOCK <index>\n";
    }
  } else if (verb == "UNBLOCK") {
    uint32_t idx = 0;
    if (iss >> idx) {
      if (svc.SetSoftBlock(idx, false)) {
        response = "OK unblocked " + std::to_string(idx) + "\n";
      } else {
        response = "ERR radio not found\n";
      }
    } else {
      response = "ERR usage: UNBLOCK <index>\n";
    }
  } else if (verb == "BLOCKALL") {
    std::string type_str;
    RfkillType type = RfkillType::kAll;
    if (iss >> type_str) {
      type = ParseType(type_str);
    }
    svc.SetSoftBlockAll(type, true);
    response = "OK blocked all " + std::string(RfkillTypeName(type)) + "\n";
  } else if (verb == "UNBLOCKALL") {
    std::string type_str;
    RfkillType type = RfkillType::kAll;
    if (iss >> type_str) {
      type = ParseType(type_str);
    }
    svc.SetSoftBlockAll(type, false);
    response = "OK unblocked all " + std::string(RfkillTypeName(type)) + "\n";
  } else {
    response = "ERR unknown command: " + verb + "\n"
               "Commands: LIST, BLOCK <idx>, UNBLOCK <idx>, "
               "BLOCKALL [type], UNBLOCKALL [type]\n";
  }

  // Send response.
  write(client_fd, response.data(), response.size());
  close(client_fd);
}

void ServerLoop(const std::string& socket_path) {
  // Clean up any stale socket.
  unlink(socket_path.c_str());

  g_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (g_listen_fd < 0) {
    perror("driverhub: rfkill_server: socket");
    return;
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

  if (bind(g_listen_fd, reinterpret_cast<struct sockaddr*>(&addr),
           sizeof(addr)) < 0) {
    perror("driverhub: rfkill_server: bind");
    close(g_listen_fd);
    g_listen_fd = -1;
    return;
  }

  if (listen(g_listen_fd, 4) < 0) {
    perror("driverhub: rfkill_server: listen");
    close(g_listen_fd);
    g_listen_fd = -1;
    return;
  }

  fprintf(stderr, "driverhub: rfkill_server: listening on %s\n",
          socket_path.c_str());

  // Signal that the server is ready.
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

    int ret = poll(&pfd, 1, 1000 /* 1s timeout */);
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
  fprintf(stderr, "driverhub: rfkill_server: stopped\n");
}

}  // namespace

void StartRfkillServer(const std::string& socket_path) {
  if (g_running.exchange(true)) return;  // Already running.

  g_ready = false;
  std::string path = socket_path.empty() ? kDefaultSocket : socket_path;
  g_server_thread = std::thread(ServerLoop, path);
  g_server_thread.detach();

  // Wait for the server thread to start listening.
  std::unique_lock<std::mutex> lk(g_ready_mu);
  g_ready_cv.wait_for(lk, std::chrono::seconds(5), [] { return g_ready; });
}

void StopRfkillServer() {
  g_running.store(false);
  if (g_listen_fd >= 0) {
    // Interrupt the select() by closing the fd.
    shutdown(g_listen_fd, SHUT_RDWR);
  }
}

}  // namespace driverhub
