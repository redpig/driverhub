// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_DRM_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_DRM_H_

// KMI shims for the Linux DRM/KMS (Direct Rendering Manager / Kernel Mode
// Setting) subsystem APIs.
//
// Provides: drm_dev_alloc, drm_dev_register, drm_dev_unregister, drm_dev_put,
//           drm_mode_config_init, drm_mode_config_cleanup,
//           drm_connector_init, drm_encoder_init, drm_crtc_init_with_planes,
//           drm_universal_plane_init, drm_simple_display_pipe_init,
//           drm_atomic_helper_connector_destroy_state, etc.
//
// On Fuchsia: maps to fuchsia.hardware.display FIDL protocol.

#include <stddef.h>
#include <stdint.h>

#include "src/shim/kernel/device.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Forward declarations ---

struct drm_device;
struct drm_driver;
struct drm_connector;
struct drm_encoder;
struct drm_crtc;
struct drm_plane;
struct drm_framebuffer;
struct drm_mode_config;
struct drm_display_mode;

// --- DRM driver structure ---

struct drm_driver {
  const char *name;
  const char *desc;
  const char *date;
  int major;
  int minor;
  int patchlevel;
  unsigned int driver_features;

  // File operations (simplified).
  void *fops;
};

// Driver feature flags.
#define DRIVER_GEM          0x1000
#define DRIVER_MODESET      0x2000
#define DRIVER_ATOMIC       0x10000

// --- DRM device ---

struct drm_mode_config {
  int min_width, min_height;
  int max_width, max_height;
  void *funcs;  // const struct drm_mode_config_funcs *
  void *helper_private;
};

struct drm_device {
  struct device *dev;
  struct drm_driver *driver;
  void *dev_private;
  struct drm_mode_config mode_config;
  int registered;
};

// --- DRM connector ---

#define DRM_MODE_CONNECTOR_Unknown     0
#define DRM_MODE_CONNECTOR_DSI         16
#define DRM_MODE_CONNECTOR_eDP         14
#define DRM_MODE_CONNECTOR_HDMIA       11
#define DRM_MODE_CONNECTOR_DisplayPort 10

// Connector status.
#define connector_status_connected     1
#define connector_status_disconnected  2
#define connector_status_unknown       3

struct drm_connector_funcs {
  void (*destroy)(struct drm_connector *connector);
  void (*reset)(struct drm_connector *connector);
  int (*fill_modes)(struct drm_connector *connector,
                    unsigned int max_width, unsigned int max_height);
  void *atomic_duplicate_state;
  void *atomic_destroy_state;
};

struct drm_connector_helper_funcs {
  int (*get_modes)(struct drm_connector *connector);
  void *mode_valid;
  void *best_encoder;
  void *atomic_best_encoder;
};

struct drm_connector {
  struct drm_device *dev;
  int connector_type;
  int connector_type_id;
  int status;
  void *helper_private;
  struct device kdev;
};

// --- DRM encoder ---

#define DRM_MODE_ENCODER_NONE  0
#define DRM_MODE_ENCODER_DSI   6
#define DRM_MODE_ENCODER_TMDS  2

struct drm_encoder_funcs {
  void (*destroy)(struct drm_encoder *encoder);
};

struct drm_encoder {
  struct drm_device *dev;
  int encoder_type;
  unsigned int possible_crtcs;
  unsigned int possible_clones;
};

// --- DRM CRTC ---

struct drm_crtc_funcs {
  void (*destroy)(struct drm_crtc *crtc);
  void *set_config;
  void *page_flip;
  void *reset;
  void *atomic_duplicate_state;
  void *atomic_destroy_state;
};

struct drm_crtc_helper_funcs {
  void *mode_fixup;
  void *mode_set;
  void *mode_set_nofb;
  void *atomic_enable;
  void *atomic_disable;
  void *atomic_check;
  void *atomic_flush;
};

struct drm_crtc {
  struct drm_device *dev;
  int index;
  void *helper_private;
};

// --- DRM plane ---

#define DRM_PLANE_TYPE_PRIMARY   0
#define DRM_PLANE_TYPE_OVERLAY   1
#define DRM_PLANE_TYPE_CURSOR    2

struct drm_plane_funcs {
  void (*destroy)(struct drm_plane *plane);
  void *update_plane;
  void *disable_plane;
  void *reset;
  void *atomic_duplicate_state;
  void *atomic_destroy_state;
};

struct drm_plane_helper_funcs {
  void *prepare_fb;
  void *cleanup_fb;
  void *atomic_check;
  void *atomic_update;
  void *atomic_disable;
};

struct drm_plane {
  struct drm_device *dev;
  int type;
  void *helper_private;
};

// --- DRM framebuffer ---

struct drm_framebuffer {
  struct drm_device *dev;
  unsigned int width;
  unsigned int height;
  unsigned int pitches[4];
  unsigned int offsets[4];
  uint32_t format;
  int refcount;
};

// --- Display mode ---

struct drm_display_mode {
  int clock;  // in kHz
  int hdisplay, hsync_start, hsync_end, htotal;
  int vdisplay, vsync_start, vsync_end, vtotal;
  int vrefresh;
  unsigned int flags;
  unsigned int type;
  char name[32];
};

#define DRM_MODE_TYPE_DRIVER     (1 << 6)
#define DRM_MODE_TYPE_PREFERRED  (1 << 3)

// --- Simple display pipe (for simple panel drivers) ---

struct drm_simple_display_pipe_funcs {
  void *enable;
  void *disable;
  void *check;
  void *update;
  void *prepare_fb;
  void *cleanup_fb;
};

struct drm_simple_display_pipe {
  struct drm_crtc crtc;
  struct drm_plane plane;
  struct drm_encoder encoder;
  struct drm_connector connector;
};

// --- DRM device lifecycle ---

struct drm_device *drm_dev_alloc(struct drm_driver *driver,
                                 struct device *parent);
int drm_dev_register(struct drm_device *dev, unsigned long flags);
void drm_dev_unregister(struct drm_device *dev);
void drm_dev_put(struct drm_device *dev);

// --- Mode config ---

void drm_mode_config_init(struct drm_device *dev);
void drm_mode_config_cleanup(struct drm_device *dev);

// --- Connector ---

int drm_connector_init(struct drm_device *dev,
                       struct drm_connector *connector,
                       const struct drm_connector_funcs *funcs,
                       int connector_type);
void drm_connector_cleanup(struct drm_connector *connector);
int drm_connector_register(struct drm_connector *connector);
void drm_connector_unregister(struct drm_connector *connector);

void drm_connector_helper_add(struct drm_connector *connector,
                              const struct drm_connector_helper_funcs *funcs);

// --- Encoder ---

int drm_encoder_init(struct drm_device *dev,
                     struct drm_encoder *encoder,
                     const struct drm_encoder_funcs *funcs,
                     int encoder_type);
void drm_encoder_cleanup(struct drm_encoder *encoder);

// --- CRTC ---

int drm_crtc_init_with_planes(struct drm_device *dev,
                               struct drm_crtc *crtc,
                               struct drm_plane *primary,
                               struct drm_plane *cursor,
                               const struct drm_crtc_funcs *funcs);
void drm_crtc_cleanup(struct drm_crtc *crtc);

void drm_crtc_helper_add(struct drm_crtc *crtc,
                          const struct drm_crtc_helper_funcs *funcs);

// --- Plane ---

int drm_universal_plane_init(struct drm_device *dev,
                              struct drm_plane *plane,
                              unsigned int possible_crtcs,
                              const struct drm_plane_funcs *funcs,
                              const uint32_t *formats,
                              unsigned int format_count,
                              void *format_modifiers,
                              int type, const char *name, ...);
void drm_plane_cleanup(struct drm_plane *plane);

void drm_plane_helper_add(struct drm_plane *plane,
                           const struct drm_plane_helper_funcs *funcs);

// --- Simple display pipe ---

int drm_simple_display_pipe_init(
    struct drm_device *dev,
    struct drm_simple_display_pipe *pipe,
    const struct drm_simple_display_pipe_funcs *funcs,
    const uint32_t *formats, unsigned int format_count,
    void *format_modifiers,
    struct drm_connector *connector);

// --- Framebuffer helpers ---

void drm_framebuffer_init(struct drm_device *dev,
                          struct drm_framebuffer *fb,
                          void *funcs);
void drm_framebuffer_cleanup(struct drm_framebuffer *fb);

// --- Mode helpers ---

struct drm_display_mode *drm_mode_duplicate(struct drm_device *dev,
                                             const struct drm_display_mode *mode);
void drm_mode_probed_add(struct drm_connector *connector,
                          struct drm_display_mode *mode);
void drm_mode_set_name(struct drm_display_mode *mode);

// --- Atomic helpers (commonly used by GKI display drivers) ---

void drm_atomic_helper_connector_reset(struct drm_connector *connector);
void drm_atomic_helper_connector_destroy_state(
    struct drm_connector *connector, void *state);
void *drm_atomic_helper_connector_duplicate_state(
    struct drm_connector *connector);

// --- GEM (Graphics Execution Manager) helpers ---

struct drm_gem_object {
  struct drm_device *dev;
  size_t size;
  int handle_count;
  void *filp;
};

struct drm_gem_object *drm_gem_object_lookup(void *filp, unsigned int handle);
void drm_gem_object_put(struct drm_gem_object *obj);

// --- Panel helpers (for embedded display panels) ---

struct drm_panel {
  struct drm_device *drm;
  struct drm_connector *connector;
  struct device *dev;
  void *funcs;
};

struct drm_panel_funcs {
  int (*prepare)(struct drm_panel *panel);
  int (*unprepare)(struct drm_panel *panel);
  int (*enable)(struct drm_panel *panel);
  int (*disable)(struct drm_panel *panel);
  int (*get_modes)(struct drm_panel *panel,
                   struct drm_connector *connector);
};

void drm_panel_init(struct drm_panel *panel, struct device *dev,
                    const struct drm_panel_funcs *funcs, int connector_type);
int drm_panel_add(struct drm_panel *panel);
void drm_panel_remove(struct drm_panel *panel);
int drm_panel_prepare(struct drm_panel *panel);
int drm_panel_unprepare(struct drm_panel *panel);
int drm_panel_enable(struct drm_panel *panel);
int drm_panel_disable(struct drm_panel *panel);

// --- Backlight (used by panel drivers) ---

struct backlight_device;
struct backlight_properties {
  int brightness;
  int max_brightness;
  int type;
};

#define BACKLIGHT_RAW 1

struct backlight_device *devm_backlight_device_register(
    struct device *dev, const char *name, struct device *parent,
    void *devdata, void *ops, struct backlight_properties *props);

int backlight_enable(struct backlight_device *bd);
int backlight_disable(struct backlight_device *bd);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_DRM_H_
