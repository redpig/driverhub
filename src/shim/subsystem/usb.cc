// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/subsystem/usb.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

// USB subsystem shim.
//
// Maintains a registry of USB drivers. On Fuchsia, usb_register_driver would
// connect to the fuchsia.hardware.usb/Usb FIDL protocol and register for
// device matching. URB submission would translate to USB request FIDL calls.
//
// In userspace, all USB operations are simulated.

namespace {

// URB structure (simplified).
struct urb_impl {
  struct usb_device* dev;
  unsigned int pipe;
  void* transfer_buffer;
  int transfer_buffer_length;
  int actual_length;
  int status;
  void (*complete)(struct urb*);
  void* context;
};

std::mutex g_usb_mu;
std::vector<struct usb_driver*> g_usb_drivers;

}  // namespace

extern "C" {

int usb_register_driver(struct usb_driver* driver) {
  std::lock_guard<std::mutex> lock(g_usb_mu);
  g_usb_drivers.push_back(driver);
  fprintf(stderr, "driverhub: usb: registered driver '%s'\n", driver->name);
  return 0;
}

void usb_deregister(struct usb_driver* driver) {
  std::lock_guard<std::mutex> lock(g_usb_mu);
  g_usb_drivers.erase(
      std::remove(g_usb_drivers.begin(), g_usb_drivers.end(), driver),
      g_usb_drivers.end());
  fprintf(stderr, "driverhub: usb: unregistered driver '%s'\n", driver->name);
}

struct urb* usb_alloc_urb(int iso_packets, unsigned int mem_flags) {
  (void)iso_packets;
  (void)mem_flags;
  auto* impl = static_cast<urb_impl*>(calloc(1, sizeof(urb_impl)));
  return reinterpret_cast<struct urb*>(impl);
}

void usb_free_urb(struct urb* urb) {
  free(urb);
}

int usb_submit_urb(struct urb* urb, unsigned int mem_flags) {
  (void)mem_flags;
  if (!urb) return -22;  // -EINVAL

  // Simulated: immediately complete with success.
  auto* impl = reinterpret_cast<urb_impl*>(urb);
  impl->status = 0;
  impl->actual_length = impl->transfer_buffer_length;

  fprintf(stderr, "driverhub: usb: submit_urb (simulated, %d bytes)\n",
          impl->transfer_buffer_length);
  return 0;
}

void usb_kill_urb(struct urb* urb) {
  if (!urb) return;
  auto* impl = reinterpret_cast<urb_impl*>(urb);
  impl->status = -104;  // -ECONNRESET
  fprintf(stderr, "driverhub: usb: kill_urb\n");
}

int usb_control_msg(struct usb_device* dev, unsigned int pipe,
                    unsigned char request, unsigned char requesttype,
                    unsigned short value, unsigned short index,
                    void* data, unsigned short size, int timeout) {
  (void)dev;
  (void)pipe;
  (void)request;
  (void)requesttype;
  (void)value;
  (void)index;
  (void)timeout;
  // Simulated: zero-fill response data and return success.
  if (data && size > 0 && (requesttype & 0x80)) {
    memset(data, 0, size);
  }
  fprintf(stderr, "driverhub: usb: control_msg(req=0x%02x, val=0x%04x) "
                  "(simulated)\n",
          request, value);
  return size;
}

int usb_bulk_msg(struct usb_device* dev, unsigned int pipe,
                 void* data, int len, int* actual_length, int timeout) {
  (void)dev;
  (void)pipe;
  (void)timeout;
  if (actual_length) *actual_length = len;
  fprintf(stderr, "driverhub: usb: bulk_msg(%d bytes) (simulated)\n", len);
  return 0;
}

// Pipe construction helpers.
unsigned int usb_sndctrlpipe(struct usb_device* dev, unsigned int endpoint) {
  (void)dev;
  return (endpoint << 15) | (0 << 30);  // Control, OUT.
}

unsigned int usb_rcvctrlpipe(struct usb_device* dev, unsigned int endpoint) {
  (void)dev;
  return (endpoint << 15) | (0 << 30) | 0x80;  // Control, IN.
}

unsigned int usb_sndbulkpipe(struct usb_device* dev, unsigned int endpoint) {
  (void)dev;
  return (endpoint << 15) | (3 << 30);  // Bulk, OUT.
}

unsigned int usb_rcvbulkpipe(struct usb_device* dev, unsigned int endpoint) {
  (void)dev;
  return (endpoint << 15) | (3 << 30) | 0x80;  // Bulk, IN.
}

}  // extern "C"
