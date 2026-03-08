// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// rfkillctl — command-line tool for querying and controlling RF kill switches
// via the DriverHub rfkill service.
//
// Usage:
//   rfkillctl list                    List all radios
//   rfkillctl block <index>           Soft-block a radio
//   rfkillctl unblock <index>         Soft-unblock a radio
//   rfkillctl block all [type]        Soft-block all radios (airplane mode)
//   rfkillctl unblock all [type]      Soft-unblock all radios
//
// Connects to the rfkill service via Unix-domain socket at /tmp/rfkill.sock.
// On Fuchsia, would connect via fuchsia.driverhub.Rfkill FIDL instead.

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr const char* kDefaultSocket = "/tmp/rfkill.sock";

// Send a command to the rfkill server and print the response.
int SendCommand(const char* socket_path, const std::string& command) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("rfkillctl: socket");
    return 1;
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

  if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr),
              sizeof(addr)) < 0) {
    fprintf(stderr, "rfkillctl: cannot connect to %s\n", socket_path);
    fprintf(stderr, "Is the DriverHub rfkill service running?\n");
    close(fd);
    return 1;
  }

  // Send command.
  std::string msg = command + "\n";
  write(fd, msg.data(), msg.size());

  // Shutdown write side to signal end of command.
  shutdown(fd, SHUT_WR);

  // Read response.
  char buf[4096];
  ssize_t total = 0;
  while (true) {
    ssize_t n = read(fd, buf + total, sizeof(buf) - total - 1);
    if (n <= 0) break;
    total += n;
  }
  buf[total] = '\0';
  close(fd);

  // Print response.
  printf("%s", buf);

  // Return 0 if OK, 1 if ERR.
  return (total > 0 && strncmp(buf, "OK", 2) == 0) ? 0 : 1;
}

void PrintUsage() {
  printf(
      "rfkillctl — DriverHub RF kill switch control\n"
      "\n"
      "Usage:\n"
      "  rfkillctl list                    List all registered radios\n"
      "  rfkillctl block <index>           Soft-block a radio by index\n"
      "  rfkillctl unblock <index>         Soft-unblock a radio by index\n"
      "  rfkillctl block all [type]        Soft-block all radios (airplane mode)\n"
      "  rfkillctl unblock all [type]      Soft-unblock all radios\n"
      "\n"
      "Radio types: wlan, bluetooth, uwb, wimax, wwan, gps, fm, nfc\n"
      "\n"
      "Options:\n"
      "  -s <path>    Socket path (default: /tmp/rfkill.sock)\n"
      "  -h           Show this help\n"
      "\n"
      "Examples:\n"
      "  rfkillctl list                    Show all RF switches\n"
      "  rfkillctl block 0                 Block radio 0\n"
      "  rfkillctl block all               Airplane mode (block all)\n"
      "  rfkillctl unblock all wlan        Unblock all WiFi radios\n");
}

}  // namespace

int main(int argc, char* argv[]) {
  const char* socket_path = kDefaultSocket;

  // Parse options.
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
      fprintf(stderr, "rfkillctl: unknown option: %s\n", argv[argi]);
      return 1;
    }
  }

  if (argi >= argc) {
    PrintUsage();
    return 1;
  }

  std::string verb = argv[argi++];

  if (verb == "list") {
    return SendCommand(socket_path, "LIST");
  } else if (verb == "block" || verb == "unblock") {
    std::string cmd_verb = (verb == "block") ? "BLOCK" : "UNBLOCK";

    if (argi >= argc) {
      fprintf(stderr, "rfkillctl: %s requires an index or 'all'\n",
              verb.c_str());
      return 1;
    }

    std::string target = argv[argi++];
    if (target == "all") {
      std::string type_arg;
      if (argi < argc) {
        type_arg = " ";
        type_arg += argv[argi];
      }
      return SendCommand(socket_path, cmd_verb + "ALL" + type_arg);
    } else {
      return SendCommand(socket_path, cmd_verb + " " + target);
    }
  } else if (verb == "help" || verb == "-h" || verb == "--help") {
    PrintUsage();
    return 0;
  } else {
    fprintf(stderr, "rfkillctl: unknown command: %s\n", verb.c_str());
    PrintUsage();
    return 1;
  }
}
