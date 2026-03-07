// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_FUCHSIA_SERVICE_BRIDGE_H_
#define DRIVERHUB_SRC_FUCHSIA_SERVICE_BRIDGE_H_

// FIDL service bridge: connects Linux subsystem registrations to DFv2
// service offers.
//
// When a .ko module calls a Linux registration function (e.g.
// gpiochip_add_data, i2c_add_driver, spi_register_driver,
// usb_register_driver), the subsystem shim notifies the service bridge.
// The bridge then:
//   1. Creates DFv2 child nodes with appropriate service offers.
//   2. Serves the corresponding FIDL protocol, routing calls back through
//      the Linux driver's callbacks.
//
// This makes GKI modules fully functional Fuchsia drivers: other DFv2
// drivers can discover and bind to them via composite node matching on
// the offered FIDL services.
//
// Example flow for GPIO:
//   .ko calls gpiochip_add_data(gc, data)
//     → gpio shim stores chip and calls dh_bridge_gpio_chip_added(gc)
//       → bridge creates per-pin child nodes in DFv2 topology
//         → each child offers fuchsia.hardware.gpio.Service
//           → downstream driver binds to "driverhub-gpio/gpio-<pin>"
//             → FIDL calls route to gc->direction_input(), gc->get(), etc.

#ifdef __cplusplus
extern "C" {
#endif

struct gpio_chip;
struct i2c_driver;
struct spi_driver;
struct usb_driver;

// Subsystem type identifiers for service offers.
#define DH_SERVICE_GPIO    1
#define DH_SERVICE_I2C     2
#define DH_SERVICE_SPI     3
#define DH_SERVICE_USB     4

// ---------------------------------------------------------------------------
// Notification callbacks — called by subsystem shims when providers register.
// These are C-linkage functions so they can be called from the shim layer.
// ---------------------------------------------------------------------------

// Called by gpio.cc when gpiochip_add_data() succeeds.
// The bridge creates per-pin DFv2 child nodes offering GPIO service.
void dh_bridge_gpio_chip_added(struct gpio_chip* gc);

// Called by gpio.cc when gpiochip_remove() is called.
void dh_bridge_gpio_chip_removed(struct gpio_chip* gc);

// Called by i2c.cc when i2c_add_driver() succeeds.
// The bridge creates a DFv2 child node offering I2C service.
void dh_bridge_i2c_driver_added(struct i2c_driver* drv);

// Called by i2c.cc when i2c_del_driver() is called.
void dh_bridge_i2c_driver_removed(struct i2c_driver* drv);

// Called by spi.cc when spi_register_driver() succeeds.
// The bridge creates a DFv2 child node offering SPI service.
void dh_bridge_spi_driver_added(struct spi_driver* drv);

// Called by spi.cc when spi_unregister_driver() is called.
void dh_bridge_spi_driver_removed(struct spi_driver* drv);

// Called by usb.cc when usb_register_driver() succeeds.
// The bridge creates a DFv2 child node offering USB service.
void dh_bridge_usb_driver_added(struct usb_driver* drv);

// Called by usb.cc when usb_deregister() is called.
void dh_bridge_usb_driver_removed(struct usb_driver* drv);

// ---------------------------------------------------------------------------
// Bridge lifecycle — called by the DFv2 driver or runner.
// ---------------------------------------------------------------------------

// Opaque handle to the service bridge context (C-compatible).
typedef void* dh_bridge_ctx_t;

// Called by DriverHubDriver or KoRunner to install bridge callbacks.
// The ctx is passed back to the add_child_fn callback.
//
// add_child_fn signature:
//   int add_child_fn(ctx, service_type, name, instance_id, provider_ptr)
//     - service_type: DH_SERVICE_GPIO, DH_SERVICE_I2C, etc.
//     - name: child node name (e.g. "gpio-5")
//     - instance_id: pin/bus/device index
//     - provider_ptr: pointer to the provider (gpio_chip*, i2c_driver*, etc.)
//     Returns 0 on success.
//
// remove_child_fn signature:
//   void remove_child_fn(ctx, service_type, name, provider_ptr)
typedef int (*dh_bridge_add_child_fn)(dh_bridge_ctx_t ctx,
                                       int service_type,
                                       const char* name,
                                       int instance_id,
                                       void* provider_ptr);
typedef void (*dh_bridge_remove_child_fn)(dh_bridge_ctx_t ctx,
                                           int service_type,
                                           const char* name,
                                           void* provider_ptr);

void dh_bridge_init(dh_bridge_ctx_t ctx,
                    dh_bridge_add_child_fn add_fn,
                    dh_bridge_remove_child_fn remove_fn);

// Tear down the bridge. Called during shutdown.
void dh_bridge_teardown(void);

// ---------------------------------------------------------------------------
// Query API — used by FIDL service servers to route calls back to drivers.
// ---------------------------------------------------------------------------

// Look up a GPIO chip by its base pin number.
struct gpio_chip* dh_bridge_find_gpio_chip(int base);

// Look up a registered I2C driver by name.
struct i2c_driver* dh_bridge_find_i2c_driver(const char* name);

// Look up a registered SPI driver by name.
struct spi_driver* dh_bridge_find_spi_driver(const char* name);

// Look up a registered USB driver by name.
struct usb_driver* dh_bridge_find_usb_driver(const char* name);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_FUCHSIA_SERVICE_BRIDGE_H_
