// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_IIO_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_IIO_H_

// Industrial I/O (IIO) subsystem shim: sensors (accelerometer, gyroscope,
// proximity, light, barometer, magnetometer, ADC).
//
// Provides:
//   iio_device_alloc, devm_iio_device_alloc, iio_device_free
//   iio_device_register, devm_iio_device_register, iio_device_unregister
//   iio_push_to_buffers_with_timestamp, iio_trigger_notify_done
//   iio_priv
//
// On Fuchsia: sensor data can map to fuchsia.input.report or
// fuchsia.sensors framework.

#include <cstddef>
#include <cstdint>

struct device;

#ifdef __cplusplus
extern "C" {
#endif

// IIO channel types (subset matching common Android sensors).
enum iio_chan_type {
  IIO_VOLTAGE = 0,
  IIO_CURRENT,
  IIO_TEMP,
  IIO_ACCEL,
  IIO_ANGL_VEL,    // Gyroscope
  IIO_MAGN,        // Magnetometer
  IIO_LIGHT,
  IIO_PROXIMITY,
  IIO_PRESSURE,
  IIO_HUMIDITYRELATIVE,
  IIO_STEPS,
  IIO_ACTIVITY,
  IIO_TIMESTAMP,
};

enum iio_modifier {
  IIO_NO_MOD = 0,
  IIO_MOD_X,
  IIO_MOD_Y,
  IIO_MOD_Z,
};

enum iio_event_type {
  IIO_EV_TYPE_THRESH = 0,
  IIO_EV_TYPE_MAG,
  IIO_EV_TYPE_ROC,
  IIO_EV_TYPE_THRESH_ADAPTIVE,
  IIO_EV_TYPE_MAG_ADAPTIVE,
  IIO_EV_TYPE_CHANGE,
};

enum iio_event_direction {
  IIO_EV_DIR_EITHER = 0,
  IIO_EV_DIR_RISING,
  IIO_EV_DIR_FALLING,
};

// Channel info attributes.
enum iio_chan_info_enum {
  IIO_CHAN_INFO_RAW = 0,
  IIO_CHAN_INFO_PROCESSED,
  IIO_CHAN_INFO_SCALE,
  IIO_CHAN_INFO_OFFSET,
  IIO_CHAN_INFO_CALIBSCALE,
  IIO_CHAN_INFO_CALIBBIAS,
  IIO_CHAN_INFO_SAMP_FREQ,
  IIO_CHAN_INFO_OVERSAMPLING_RATIO,
};

#define BIT(n) (1UL << (n))

struct iio_chan_spec {
  enum iio_chan_type type;
  int channel;
  int channel2;
  unsigned long address;
  int scan_index;
  struct {
    char sign;
    uint8_t realbits;
    uint8_t storagebits;
    uint8_t shift;
    uint8_t repeat;
    int endianness;
  } scan_type;
  long info_mask_separate;
  long info_mask_shared_by_type;
  long info_mask_shared_by_all;
  int modified;
  int indexed;
  enum iio_modifier extend_name;
};

struct iio_info {
  int (*read_raw)(struct iio_dev*, struct iio_chan_spec const*,
                  int*, int*, long);
  int (*write_raw)(struct iio_dev*, struct iio_chan_spec const*,
                   int, int, long);
  int (*read_event_config)(struct iio_dev*,
                           const struct iio_chan_spec*,
                           enum iio_event_type,
                           enum iio_event_direction);
  int (*write_event_config)(struct iio_dev*,
                            const struct iio_chan_spec*,
                            enum iio_event_type,
                            enum iio_event_direction, int);
};

struct iio_trigger;

struct iio_dev {
  const char* name;
  const struct iio_info* info;
  const struct iio_chan_spec* channels;
  int num_channels;
  unsigned long available_scan_masks;
  struct iio_trigger* trig;
  int modes;
  struct device* dev_parent;
};

// IIO device modes.
#define INDIO_DIRECT_MODE       0x01
#define INDIO_BUFFER_TRIGGERED  0x02
#define INDIO_BUFFER_SOFTWARE   0x04
#define INDIO_BUFFER_HARDWARE   0x08

// IIO return values for read_raw.
#define IIO_VAL_INT          1
#define IIO_VAL_INT_PLUS_MICRO 2
#define IIO_VAL_INT_PLUS_NANO  3
#define IIO_VAL_FRACTIONAL   10

struct iio_dev* iio_device_alloc(struct device* parent, int sizeof_priv);
struct iio_dev* devm_iio_device_alloc(struct device* parent, int sizeof_priv);
void iio_device_free(struct iio_dev* indio_dev);

int iio_device_register(struct iio_dev* indio_dev);
int devm_iio_device_register(struct device* dev,
                              struct iio_dev* indio_dev);
void iio_device_unregister(struct iio_dev* indio_dev);

void* iio_priv(const struct iio_dev* indio_dev);

int iio_push_to_buffers_with_timestamp(struct iio_dev* indio_dev,
                                        void* data, int64_t timestamp);

// Trigger support.
struct iio_trigger* devm_iio_trigger_alloc(struct device* dev,
                                            const char* fmt, ...);
int devm_iio_trigger_register(struct device* dev,
                               struct iio_trigger* trig);
void iio_trigger_notify_done(struct iio_trigger* trig);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_IIO_H_
