// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/subsystem/drm.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

// DRM/KMS subsystem shim.
//
// Maintains simulated DRM device state. In userspace, all operations succeed
// and maintain minimal state tracking for device/connector/encoder/crtc/plane
// lifecycle management.
//
// On Fuchsia, display operations would be backed by
// fuchsia.hardware.display/Controller FIDL connections.

namespace {

std::mutex g_drm_mu;
std::vector<struct drm_device*> g_drm_devices;
std::vector<struct drm_panel*> g_panels;

int g_next_connector_id = 1;
int g_next_crtc_index = 0;

}  // namespace

extern "C" {

// --- DRM device lifecycle ---

struct drm_device* drm_dev_alloc(struct drm_driver* driver,
                                 struct device* parent) {
  auto* dev = static_cast<struct drm_device*>(calloc(1, sizeof(struct drm_device)));
  if (!dev) return nullptr;
  dev->driver = driver;
  dev->dev = parent;
  dev->registered = 0;
  fprintf(stderr, "driverhub: drm: allocated device (%s)\n",
          driver ? driver->name : "unknown");
  return dev;
}

int drm_dev_register(struct drm_device* dev, unsigned long flags) {
  (void)flags;
  if (!dev) return -22;  // -EINVAL
  std::lock_guard<std::mutex> lock(g_drm_mu);
  dev->registered = 1;
  g_drm_devices.push_back(dev);
  fprintf(stderr, "driverhub: drm: registered device (%s)\n",
          dev->driver ? dev->driver->name : "unknown");
  return 0;
}

void drm_dev_unregister(struct drm_device* dev) {
  if (!dev) return;
  std::lock_guard<std::mutex> lock(g_drm_mu);
  dev->registered = 0;
  for (auto it = g_drm_devices.begin(); it != g_drm_devices.end(); ++it) {
    if (*it == dev) {
      g_drm_devices.erase(it);
      break;
    }
  }
  fprintf(stderr, "driverhub: drm: unregistered device\n");
}

void drm_dev_put(struct drm_device* dev) {
  if (!dev) return;
  fprintf(stderr, "driverhub: drm: freed device\n");
  free(dev);
}

// --- Mode config ---

void drm_mode_config_init(struct drm_device* dev) {
  if (!dev) return;
  dev->mode_config.min_width = 0;
  dev->mode_config.min_height = 0;
  dev->mode_config.max_width = 8192;
  dev->mode_config.max_height = 8192;
  fprintf(stderr, "driverhub: drm: mode config initialized\n");
}

void drm_mode_config_cleanup(struct drm_device* dev) {
  if (!dev) return;
  fprintf(stderr, "driverhub: drm: mode config cleaned up\n");
}

// --- Connector ---

int drm_connector_init(struct drm_device* dev,
                       struct drm_connector* connector,
                       const struct drm_connector_funcs* funcs,
                       int connector_type) {
  if (!dev || !connector) return -22;
  connector->dev = dev;
  connector->connector_type = connector_type;
  connector->connector_type_id = g_next_connector_id++;
  connector->status = connector_status_connected;
  (void)funcs;
  fprintf(stderr, "driverhub: drm: connector init (type=%d)\n", connector_type);
  return 0;
}

void drm_connector_cleanup(struct drm_connector* connector) {
  if (!connector) return;
  connector->dev = nullptr;
  fprintf(stderr, "driverhub: drm: connector cleanup\n");
}

int drm_connector_register(struct drm_connector* connector) {
  (void)connector;
  return 0;
}

void drm_connector_unregister(struct drm_connector* connector) {
  (void)connector;
}

void drm_connector_helper_add(struct drm_connector* connector,
                              const struct drm_connector_helper_funcs* funcs) {
  if (connector) {
    connector->helper_private = const_cast<void*>(
        static_cast<const void*>(funcs));
  }
}

// --- Encoder ---

int drm_encoder_init(struct drm_device* dev,
                     struct drm_encoder* encoder,
                     const struct drm_encoder_funcs* funcs,
                     int encoder_type) {
  if (!dev || !encoder) return -22;
  (void)funcs;
  encoder->dev = dev;
  encoder->encoder_type = encoder_type;
  encoder->possible_crtcs = 1;
  encoder->possible_clones = 0;
  fprintf(stderr, "driverhub: drm: encoder init (type=%d)\n", encoder_type);
  return 0;
}

void drm_encoder_cleanup(struct drm_encoder* encoder) {
  if (!encoder) return;
  encoder->dev = nullptr;
}

// --- CRTC ---

int drm_crtc_init_with_planes(struct drm_device* dev,
                               struct drm_crtc* crtc,
                               struct drm_plane* primary,
                               struct drm_plane* cursor,
                               const struct drm_crtc_funcs* funcs) {
  if (!dev || !crtc) return -22;
  (void)primary;
  (void)cursor;
  (void)funcs;
  crtc->dev = dev;
  crtc->index = g_next_crtc_index++;
  fprintf(stderr, "driverhub: drm: crtc init (index=%d)\n", crtc->index);
  return 0;
}

void drm_crtc_cleanup(struct drm_crtc* crtc) {
  if (!crtc) return;
  crtc->dev = nullptr;
}

void drm_crtc_helper_add(struct drm_crtc* crtc,
                          const struct drm_crtc_helper_funcs* funcs) {
  if (crtc) {
    crtc->helper_private = const_cast<void*>(
        static_cast<const void*>(funcs));
  }
}

// --- Plane ---

int drm_universal_plane_init(struct drm_device* dev,
                              struct drm_plane* plane,
                              unsigned int possible_crtcs,
                              const struct drm_plane_funcs* funcs,
                              const uint32_t* formats,
                              unsigned int format_count,
                              void* format_modifiers,
                              int type, const char* name, ...) {
  if (!dev || !plane) return -22;
  (void)possible_crtcs;
  (void)funcs;
  (void)formats;
  (void)format_count;
  (void)format_modifiers;
  (void)name;
  plane->dev = dev;
  plane->type = type;
  fprintf(stderr, "driverhub: drm: plane init (type=%d)\n", type);
  return 0;
}

void drm_plane_cleanup(struct drm_plane* plane) {
  if (!plane) return;
  plane->dev = nullptr;
}

void drm_plane_helper_add(struct drm_plane* plane,
                           const struct drm_plane_helper_funcs* funcs) {
  if (plane) {
    plane->helper_private = const_cast<void*>(
        static_cast<const void*>(funcs));
  }
}

// --- Simple display pipe ---

int drm_simple_display_pipe_init(
    struct drm_device* dev,
    struct drm_simple_display_pipe* pipe,
    const struct drm_simple_display_pipe_funcs* funcs,
    const uint32_t* formats, unsigned int format_count,
    void* format_modifiers,
    struct drm_connector* connector) {
  if (!dev || !pipe) return -22;
  (void)funcs;
  (void)formats;
  (void)format_count;
  (void)format_modifiers;
  (void)connector;
  pipe->crtc.dev = dev;
  pipe->plane.dev = dev;
  pipe->encoder.dev = dev;
  fprintf(stderr, "driverhub: drm: simple display pipe initialized\n");
  return 0;
}

// --- Framebuffer helpers ---

void drm_framebuffer_init(struct drm_device* dev,
                          struct drm_framebuffer* fb,
                          void* funcs) {
  if (!fb) return;
  (void)funcs;
  fb->dev = dev;
  fb->refcount = 1;
}

void drm_framebuffer_cleanup(struct drm_framebuffer* fb) {
  if (!fb) return;
  fb->dev = nullptr;
}

// --- Mode helpers ---

struct drm_display_mode* drm_mode_duplicate(struct drm_device* dev,
                                             const struct drm_display_mode* mode) {
  (void)dev;
  if (!mode) return nullptr;
  auto* dup = static_cast<struct drm_display_mode*>(
      malloc(sizeof(struct drm_display_mode)));
  if (dup) memcpy(dup, mode, sizeof(*dup));
  return dup;
}

void drm_mode_probed_add(struct drm_connector* connector,
                          struct drm_display_mode* mode) {
  (void)connector;
  (void)mode;
  // In a full implementation, this would add the mode to the connector's
  // probed mode list.
}

void drm_mode_set_name(struct drm_display_mode* mode) {
  if (!mode) return;
  snprintf(mode->name, sizeof(mode->name), "%dx%d",
           mode->hdisplay, mode->vdisplay);
}

// --- Atomic helpers ---

void drm_atomic_helper_connector_reset(struct drm_connector* connector) {
  (void)connector;
}

void drm_atomic_helper_connector_destroy_state(
    struct drm_connector* connector, void* state) {
  (void)connector;
  free(state);
}

void* drm_atomic_helper_connector_duplicate_state(
    struct drm_connector* connector) {
  (void)connector;
  // Return a minimal state allocation.
  return calloc(1, 64);
}

// --- GEM helpers ---

struct drm_gem_object* drm_gem_object_lookup(void* filp,
                                              unsigned int handle) {
  (void)filp;
  (void)handle;
  return nullptr;
}

void drm_gem_object_put(struct drm_gem_object* obj) {
  (void)obj;
}

// --- Panel helpers ---

void drm_panel_init(struct drm_panel* panel, struct device* dev,
                    const struct drm_panel_funcs* funcs, int connector_type) {
  if (!panel) return;
  (void)connector_type;
  panel->dev = dev;
  panel->funcs = const_cast<void*>(static_cast<const void*>(funcs));
  panel->drm = nullptr;
  panel->connector = nullptr;
  fprintf(stderr, "driverhub: drm: panel init\n");
}

int drm_panel_add(struct drm_panel* panel) {
  if (!panel) return -22;
  std::lock_guard<std::mutex> lock(g_drm_mu);
  g_panels.push_back(panel);
  fprintf(stderr, "driverhub: drm: panel added\n");
  return 0;
}

void drm_panel_remove(struct drm_panel* panel) {
  if (!panel) return;
  std::lock_guard<std::mutex> lock(g_drm_mu);
  for (auto it = g_panels.begin(); it != g_panels.end(); ++it) {
    if (*it == panel) {
      g_panels.erase(it);
      break;
    }
  }
  fprintf(stderr, "driverhub: drm: panel removed\n");
}

int drm_panel_prepare(struct drm_panel* panel) {
  if (!panel || !panel->funcs) return -22;
  auto* funcs = static_cast<struct drm_panel_funcs*>(panel->funcs);
  if (funcs->prepare) return funcs->prepare(panel);
  return 0;
}

int drm_panel_unprepare(struct drm_panel* panel) {
  if (!panel || !panel->funcs) return -22;
  auto* funcs = static_cast<struct drm_panel_funcs*>(panel->funcs);
  if (funcs->unprepare) return funcs->unprepare(panel);
  return 0;
}

int drm_panel_enable(struct drm_panel* panel) {
  if (!panel || !panel->funcs) return -22;
  auto* funcs = static_cast<struct drm_panel_funcs*>(panel->funcs);
  if (funcs->enable) return funcs->enable(panel);
  return 0;
}

int drm_panel_disable(struct drm_panel* panel) {
  if (!panel || !panel->funcs) return -22;
  auto* funcs = static_cast<struct drm_panel_funcs*>(panel->funcs);
  if (funcs->disable) return funcs->disable(panel);
  return 0;
}

// --- Backlight ---

struct backlight_device {
  int brightness;
  int max_brightness;
  int enabled;
};

struct backlight_device* devm_backlight_device_register(
    struct device* dev, const char* name, struct device* parent,
    void* devdata, void* ops, struct backlight_properties* props) {
  (void)dev;
  (void)parent;
  (void)devdata;
  (void)ops;
  auto* bd = static_cast<struct backlight_device*>(
      calloc(1, sizeof(struct backlight_device)));
  if (!bd) return nullptr;
  if (props) {
    bd->brightness = props->brightness;
    bd->max_brightness = props->max_brightness;
  }
  bd->enabled = 0;
  fprintf(stderr, "driverhub: drm: backlight '%s' registered\n",
          name ? name : "");
  return bd;
}

int backlight_enable(struct backlight_device* bd) {
  if (!bd) return -22;
  bd->enabled = 1;
  return 0;
}

int backlight_disable(struct backlight_device* bd) {
  if (!bd) return -22;
  bd->enabled = 0;
  return 0;
}

}  // extern "C"
