// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_BLOCK_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_BLOCK_H_

// KMI shims for the Linux block layer.
//
// Provides: gendisk, request_queue, bio, blk_mq_* APIs
//
// On Fuchsia, block device registration maps to
// fuchsia.storage.block/Block FIDL protocol.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct device;
struct request_queue;
struct gendisk;
struct bio;
struct request;
struct blk_mq_tag_set;

// --- Block device flags ---
#define GENHD_FL_REMOVABLE  1
#define GENHD_FL_EXT_DEVT   64

// --- Request flags ---
#define REQ_OP_READ   0
#define REQ_OP_WRITE  1
#define REQ_OP_FLUSH  2
#define REQ_OP_DISCARD 3

// --- Block size ---
#define SECTOR_SHIFT 9
#define SECTOR_SIZE  (1 << SECTOR_SHIFT)

// --- blk_mq types ---

enum blk_eh_timer_return {
  BLK_EH_DONE = 0,
  BLK_EH_RESET_TIMER = 1,
};

struct blk_mq_hw_ctx {
  unsigned int queue_num;
  void* driver_data;
};

struct blk_mq_queue_data {
  struct request* rq;
  int last;
};

struct io_comp_batch;

typedef int (blk_mq_queue_rq_fn)(struct blk_mq_hw_ctx*, const struct blk_mq_queue_data*);
typedef void (blk_mq_complete_rq_fn)(struct request*);
typedef int (blk_mq_init_hctx_fn)(struct blk_mq_hw_ctx*, void*, unsigned int);
typedef void (blk_mq_exit_hctx_fn)(struct blk_mq_hw_ctx*, unsigned int);
typedef int (blk_mq_init_request_fn)(struct blk_mq_tag_set*, struct request*, unsigned int, unsigned int);
typedef void (blk_mq_exit_request_fn)(struct blk_mq_tag_set*, struct request*, unsigned int);
typedef enum blk_eh_timer_return (blk_mq_timeout_fn)(struct request*);

// Multi-queue block ops — filled by the driver.
struct blk_mq_ops {
  blk_mq_queue_rq_fn* queue_rq;
  blk_mq_complete_rq_fn* complete;
  blk_mq_init_hctx_fn* init_hctx;
  blk_mq_exit_hctx_fn* exit_hctx;
  blk_mq_init_request_fn* init_request;
  blk_mq_exit_request_fn* exit_request;
  blk_mq_timeout_fn* timeout;
  void (*map_queues)(struct blk_mq_tag_set*);
  int (*poll)(struct blk_mq_hw_ctx*, struct io_comp_batch*);  // NOLINT
};

// Tag set — describes hardware queue configuration.
struct blk_mq_tag_set {
  const struct blk_mq_ops* ops;
  unsigned int nr_hw_queues;
  unsigned int queue_depth;
  unsigned int cmd_size;
  unsigned int numa_node;
  unsigned int timeout;
  unsigned int flags;
  void* driver_data;
  // Internal.
  struct request_queue* queue;
};

// Block I/O vector (single segment).
struct bio_vec {
  void* bv_page;        // struct page* in kernel
  unsigned int bv_len;
  unsigned int bv_offset;
};

// Block I/O request.
struct bio {
  struct bio* bi_next;
  struct gendisk* bi_bdev;
  unsigned int bi_opf;
  int bi_status;
  unsigned short bi_vcnt;
  unsigned short bi_max_vecs;
  struct bio_vec* bi_io_vec;
  uint64_t bi_iter_sector;
  unsigned int bi_iter_size;
  void (*bi_end_io)(struct bio*);
  void* bi_private;
};

// Block layer request (multi-queue).
struct request {
  struct request_queue* q;
  struct blk_mq_hw_ctx* mq_hctx;
  unsigned int cmd_flags;
  unsigned int __data_len;
  uint64_t __sector;
  struct bio* bio;
  void* end_io_data;
  int tag;
  int internal_tag;
  unsigned long deadline;
};

// Block device operations — implemented by the block driver.
struct block_device_operations {
  int (*open)(struct gendisk*, unsigned int /*mode*/);
  void (*release)(struct gendisk*);
  int (*ioctl)(struct gendisk*, unsigned int /*mode*/, unsigned int /*cmd*/, unsigned long /*arg*/);
  int (*getgeo)(struct gendisk*, struct hd_geometry*);
  void* owner;
};

// Disk geometry.
struct hd_geometry {
  unsigned char heads;
  unsigned char sectors;
  unsigned short cylinders;
  unsigned long start;
};

// Generic disk structure.
struct gendisk {
  int major;
  int first_minor;
  int minors;
  char disk_name[32];
  struct block_device_operations* fops;
  struct request_queue* queue;
  void* private_data;
  int flags;
  uint64_t capacity;  // in sectors
  struct device* driverfs_dev;  // parent device
};

// Request queue (simplified).
struct request_queue {
  struct blk_mq_tag_set* tag_set;
  void* queuedata;
  unsigned short max_hw_sectors;
  unsigned short max_segments;
  unsigned int max_segment_size;
  unsigned int dma_alignment;
};

// --- blk_mq API ---

int blk_mq_alloc_tag_set(struct blk_mq_tag_set* set);
void blk_mq_free_tag_set(struct blk_mq_tag_set* set);

struct request_queue* blk_mq_init_queue(struct blk_mq_tag_set* set);
struct gendisk* blk_mq_alloc_disk(struct blk_mq_tag_set* set, void* queuedata);
void blk_mq_start_hw_queues(struct request_queue* q);
void blk_mq_stop_hw_queues(struct request_queue* q);
void blk_mq_start_stopped_hw_queues(struct request_queue* q, int async);
void blk_mq_complete_request(struct request* rq);
void blk_mq_end_request(struct request* rq, int error);
void blk_mq_start_request(struct request* rq);
void blk_mq_free_request(struct request* rq);
void blk_mq_kick_requeue_list(struct request_queue* q);
void blk_mq_requeue_request(struct request* rq, int kick_requeue_list);
void blk_mq_tagset_busy_iter(struct blk_mq_tag_set* set,
                             void (*fn)(struct request*, void*, int),
                             void* data);
int blk_mq_map_queues(struct blk_mq_tag_set* set);

// --- Request helpers ---

static inline unsigned int blk_rq_bytes(const struct request* rq) {
  return rq->__data_len;
}

static inline uint64_t blk_rq_pos(const struct request* rq) {
  return rq->__sector;
}

static inline unsigned int req_op(const struct request* rq) {
  return rq->cmd_flags & 0xFF;
}

// --- Gendisk API ---

struct gendisk* alloc_disk(int minors);
void del_gendisk(struct gendisk* disk);
void put_disk(struct gendisk* disk);
int add_disk(struct gendisk* disk);
void set_capacity(struct gendisk* disk, uint64_t sectors);
uint64_t get_capacity(struct gendisk* disk);

// --- Request queue configuration ---

void blk_queue_logical_block_size(struct request_queue* q, unsigned int size);
void blk_queue_physical_block_size(struct request_queue* q, unsigned int size);
void blk_queue_max_hw_sectors(struct request_queue* q, unsigned int sectors);
void blk_queue_max_segments(struct request_queue* q, unsigned short segs);
void blk_queue_max_segment_size(struct request_queue* q, unsigned int size);
void blk_queue_dma_alignment(struct request_queue* q, int mask);
void blk_queue_flag_set(unsigned int flag, struct request_queue* q);
void blk_queue_flag_clear(unsigned int flag, struct request_queue* q);
void blk_queue_rq_timeout(struct request_queue* q, unsigned long timeout);

// Queue flags.
#define QUEUE_FLAG_NONROT   6
#define QUEUE_FLAG_DISCARD  13

// --- Bio helpers ---

struct bio* bio_alloc(struct gendisk* disk, unsigned short nr_vecs,
                      unsigned int opf, unsigned int gfp_flags);
void bio_put(struct bio* bio);
void bio_endio(struct bio* bio);
void submit_bio(struct bio* bio);

// --- Block device registration helpers ---

int register_blkdev(unsigned int major, const char* name);
void unregister_blkdev(unsigned int major, const char* name);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_BLOCK_H_
