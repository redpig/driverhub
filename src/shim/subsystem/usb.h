// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_USB_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_USB_H_

// KMI shims for Linux USB subsystem APIs.
//
// Provides: usb_register_driver, usb_deregister, usb_submit_urb,
//           usb_kill_urb, usb_control_msg, usb_bulk_msg, etc.
//
// All backed by fuchsia.hardware.usb FIDL protocol.

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct usb_device;
struct usb_interface;
struct urb;

// Device ID table entry for USB matching.
struct usb_device_id {
  unsigned short match_flags;
  unsigned short idVendor;
  unsigned short idProduct;
  unsigned short bcdDevice_lo;
  unsigned short bcdDevice_hi;
  unsigned char bDeviceClass;
  unsigned char bDeviceSubClass;
  unsigned char bDeviceProtocol;
  unsigned char bInterfaceClass;
  unsigned char bInterfaceSubClass;
  unsigned char bInterfaceProtocol;
  unsigned long driver_info;
};

// USB driver structure. Modules populate this and pass to usb_register.
struct usb_driver {
  const char* name;
  int (*probe)(struct usb_interface* intf, const struct usb_device_id* id);
  void (*disconnect)(struct usb_interface* intf);
  int (*suspend)(struct usb_interface* intf, /* pm_message_t */ int message);
  int (*resume)(struct usb_interface* intf);
  const struct usb_device_id* id_table;
};

// Register/unregister a USB driver with the Fuchsia USB stack via FIDL.
int usb_register_driver(struct usb_driver* driver);
void usb_deregister(struct usb_driver* driver);

// Convenience macro matching Linux's module_usb_driver pattern.
#define usb_register(driver) usb_register_driver(driver)

// URB submission and management.
struct urb* usb_alloc_urb(int iso_packets, unsigned int mem_flags);
void usb_free_urb(struct urb* urb);
int usb_submit_urb(struct urb* urb, unsigned int mem_flags);
void usb_kill_urb(struct urb* urb);

// Synchronous transfer helpers.
int usb_control_msg(struct usb_device* dev, unsigned int pipe,
                    unsigned char request, unsigned char requesttype,
                    unsigned short value, unsigned short index,
                    void* data, unsigned short size, int timeout);

int usb_bulk_msg(struct usb_device* dev, unsigned int pipe,
                 void* data, int len, int* actual_length, int timeout);

// Pipe construction helpers.
unsigned int usb_sndctrlpipe(struct usb_device* dev, unsigned int endpoint);
unsigned int usb_rcvctrlpipe(struct usb_device* dev, unsigned int endpoint);
unsigned int usb_sndbulkpipe(struct usb_device* dev, unsigned int endpoint);
unsigned int usb_rcvbulkpipe(struct usb_device* dev, unsigned int endpoint);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_USB_H_
