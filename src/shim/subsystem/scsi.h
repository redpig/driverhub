// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_SCSI_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_SCSI_H_

// KMI shims for the Linux SCSI mid-layer.
//
// Provides: Scsi_Host, scsi_device, scsi_cmnd, scsi_host_alloc,
//           scsi_add_host, scsi_scan_host, scsi_remove_host, etc.
//
// UFS (ufshcd) is a SCSI low-level driver that uses these APIs.
// On Fuchsia, SCSI command dispatch would map to block FIDL operations.

#include <stddef.h>
#include <stdint.h>

#include "src/shim/kernel/device.h"

#ifdef __cplusplus
extern "C" {
#endif
struct request_queue;
struct gendisk;
struct Scsi_Host;
struct scsi_device;
struct scsi_cmnd;
struct scsi_target;

// --- SCSI result codes ---
#define DID_OK          0x00
#define DID_NO_CONNECT  0x01
#define DID_BUS_BUSY    0x02
#define DID_TIME_OUT    0x03
#define DID_BAD_TARGET  0x04
#define DID_ABORT       0x05
#define DID_ERROR       0x07
#define DID_RESET       0x08
#define DID_REQUEUE     0x0C

#define COMMAND_COMPLETE    0x00
#define SAM_STAT_GOOD       0x00
#define SAM_STAT_CHECK_CONDITION  0x02
#define SAM_STAT_BUSY       0x08
#define SAM_STAT_TASK_SET_FULL  0x28

// SCSI opcodes used by UFS.
#define TEST_UNIT_READY     0x00
#define REQUEST_SENSE       0x03
#define INQUIRY             0x12
#define READ_CAPACITY_10    0x25
#define READ_10             0x28
#define WRITE_10            0x2A
#define SYNCHRONIZE_CACHE   0x35
#define READ_CAPACITY_16    0x9E
#define READ_16             0x88
#define WRITE_16            0x8A
#define REPORT_LUNS         0xA0
#define SECURITY_PROTOCOL_IN  0xA2
#define SECURITY_PROTOCOL_OUT 0xB5
#define UNMAP               0x42
#define WRITE_BUFFER        0x3B
#define READ_BUFFER         0x3C

// Max CDB length.
#define MAX_COMMAND_SIZE 16

// --- SCSI host template ---

struct scsi_host_template {
  const char* name;
  const char* proc_name;
  int (*queuecommand)(struct Scsi_Host*, struct scsi_cmnd*);
  int (*eh_abort_handler)(struct scsi_cmnd*);
  int (*eh_device_reset_handler)(struct scsi_cmnd*);
  int (*eh_host_reset_handler)(struct scsi_cmnd*);
  int (*slave_alloc)(struct scsi_device*);
  int (*slave_configure)(struct scsi_device*);
  void (*slave_destroy)(struct scsi_device*);
  int (*change_queue_depth)(struct scsi_device*, int);
  int (*map_queues)(struct Scsi_Host*);
  int can_queue;
  int this_id;
  unsigned short sg_tablesize;
  unsigned short max_sectors;
  unsigned short cmd_per_lun;
  int tag_alloc_policy;
  unsigned int cmd_size;
  unsigned char eh_noresume:1;
  unsigned char skip_settle_delay:1;
  void* module;  // struct module*
};

// --- Scsi_Host ---

struct Scsi_Host {
  struct scsi_host_template* hostt;
  unsigned int max_id;
  unsigned int max_lun;
  unsigned int max_channel;
  unsigned int max_cmd_len;
  unsigned int can_queue;
  unsigned int sg_tablesize;
  unsigned int max_sectors;
  unsigned int cmd_per_lun;
  unsigned int unique_id;
  unsigned int host_no;
  unsigned int nr_hw_queues;
  unsigned int tag_set_flags;
  int this_id;
  unsigned long irq;
  void* hostdata;          // hostdata[0] — driver private
  struct device shost_gendev;
  struct device shost_dev;
};

// --- scsi_device ---

struct scsi_device {
  struct Scsi_Host* host;
  struct request_queue* request_queue;
  unsigned int id;
  unsigned int lun;
  unsigned int channel;
  unsigned int type;
  unsigned int scsi_level;
  unsigned int sector_size;
  unsigned int max_device_blocked;
  unsigned int queue_depth;
  unsigned char removable:1;
  unsigned char lockable:1;
  void* hostdata;
  struct device sdev_gendev;
  struct device sdev_dev;
};

// --- scsi_cmnd ---

struct scsi_cmnd {
  struct Scsi_Host* host;
  struct scsi_device* device;
  unsigned char cmnd[MAX_COMMAND_SIZE];
  unsigned int cmd_len;
  int result;
  unsigned int transfersize;
  unsigned int underflow;
  unsigned char* sense_buffer;
  void* request;  // struct request*
  void (*scsi_done)(struct scsi_cmnd*);
  // Scatter-gather.
  struct scatterlist* sgl;
  unsigned int sg_cnt;
  unsigned int sdb_length;  // total transfer length
};

// --- Scatter-gather list ---

struct scatterlist {
  void* page_link;  // struct page*
  unsigned int offset;
  unsigned int length;
  uint64_t dma_address;
  unsigned int dma_length;
};

struct scsi_target {
  struct scsi_device* sdev;
  unsigned int id;
  unsigned int channel;
  unsigned int max_lun;
};

// --- SCSI Host API ---

struct Scsi_Host* scsi_host_alloc(struct scsi_host_template* tmpl,
                                   int privsize);
int scsi_add_host_with_dma(struct Scsi_Host* host, struct device* dev,
                           struct device* dma_dev);
void scsi_scan_host(struct Scsi_Host* host);
void scsi_remove_host(struct Scsi_Host* host);
void scsi_host_put(struct Scsi_Host* host);

// Convenience wrapper matching most driver usage.
static inline int scsi_add_host(struct Scsi_Host* host, struct device* dev) {
  return scsi_add_host_with_dma(host, dev, dev);
}

// --- SCSI Command helpers ---

void scsi_done(struct scsi_cmnd* cmd);

static inline unsigned int scsi_bufflen(struct scsi_cmnd* cmd) {
  return cmd ? cmd->sdb_length : 0;
}

static inline struct scatterlist* scsi_sglist(struct scsi_cmnd* cmd) {
  return cmd ? cmd->sgl : nullptr;
}

static inline unsigned int scsi_sg_count(struct scsi_cmnd* cmd) {
  return cmd ? cmd->sg_cnt : 0;
}

// --- SCSI Device API ---

int scsi_change_queue_depth(struct scsi_device* sdev, int depth);
void scsi_report_bus_reset(struct Scsi_Host* host, int channel);

// DMA mapping for SCSI sg lists (simplified).
int scsi_dma_map(struct scsi_cmnd* cmd);
void scsi_dma_unmap(struct scsi_cmnd* cmd);

// --- Host private data ---

static inline void* shost_priv(struct Scsi_Host* host) {
  return host ? host->hostdata : nullptr;
}

static inline unsigned int scsi_host_busy(struct Scsi_Host* host) {
  (void)host;
  return 0;
}

// --- Device queue depth ---

static inline void scsi_activate_tcq(struct scsi_device* sdev, int depth) {
  (void)sdev;
  (void)depth;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_SCSI_H_
