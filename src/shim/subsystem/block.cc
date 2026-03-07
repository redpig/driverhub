// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/subsystem/block.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

// Block layer shim.
//
// Maintains a registry of block devices and request queues. On Fuchsia,
// gendisk registration would expose the device via the
// fuchsia.storage.block/Block FIDL protocol. In userspace, all I/O
// operations are simulated.

namespace {

std::mutex g_block_mu;

struct RegisteredBlock {
  struct gendisk* disk;
  bool active;
};

std::vector<RegisteredBlock>& block_registry() {
  static std::vector<RegisteredBlock> registry;
  return registry;
}

int g_blkdev_major_counter = 240;  // Start above standard Linux majors.

}  // namespace

extern "C" {

// --- blk_mq tag set ---

int blk_mq_alloc_tag_set(struct blk_mq_tag_set* set) {
  if (!set || !set->ops || !set->ops->queue_rq)
    return -22;  // -EINVAL

  fprintf(stderr, "driverhub: block: alloc_tag_set(queues=%u, depth=%u, "
                  "cmd_size=%u)\n",
          set->nr_hw_queues, set->queue_depth, set->cmd_size);
  return 0;
}

void blk_mq_free_tag_set(struct blk_mq_tag_set* set) {
  fprintf(stderr, "driverhub: block: free_tag_set\n");
  (void)set;
}

struct request_queue* blk_mq_init_queue(struct blk_mq_tag_set* set) {
  auto* q = static_cast<struct request_queue*>(
      calloc(1, sizeof(struct request_queue)));
  if (!q)
    return reinterpret_cast<struct request_queue*>((long)-12);

  q->tag_set = set;
  q->max_hw_sectors = 256;
  q->max_segments = 128;
  q->max_segment_size = 65536;
  set->queue = q;

  fprintf(stderr, "driverhub: block: init_queue = %p\n", (void*)q);
  return q;
}

struct gendisk* blk_mq_alloc_disk(struct blk_mq_tag_set* set,
                                   void* queuedata) {
  auto* q = blk_mq_init_queue(set);
  if ((long)q < 0)
    return reinterpret_cast<struct gendisk*>(q);

  q->queuedata = queuedata;

  auto* disk = static_cast<struct gendisk*>(
      calloc(1, sizeof(struct gendisk)));
  if (!disk) {
    free(q);
    return reinterpret_cast<struct gendisk*>((long)-12);
  }

  disk->queue = q;
  disk->minors = 1;
  disk->private_data = queuedata;

  fprintf(stderr, "driverhub: block: alloc_disk = %p (queue=%p)\n",
          (void*)disk, (void*)q);
  return disk;
}

void blk_mq_start_hw_queues(struct request_queue* q) {
  fprintf(stderr, "driverhub: block: start_hw_queues(%p)\n", (void*)q);
}

void blk_mq_stop_hw_queues(struct request_queue* q) {
  fprintf(stderr, "driverhub: block: stop_hw_queues(%p)\n", (void*)q);
}

void blk_mq_start_stopped_hw_queues(struct request_queue* q, int async) {
  (void)async;
  fprintf(stderr, "driverhub: block: start_stopped_hw_queues(%p)\n",
          (void*)q);
}

void blk_mq_complete_request(struct request* rq) {
  if (rq && rq->q && rq->q->tag_set &&
      rq->q->tag_set->ops && rq->q->tag_set->ops->complete) {
    rq->q->tag_set->ops->complete(rq);
  }
}

void blk_mq_end_request(struct request* rq, int error) {
  (void)rq;
  (void)error;
  // In a real implementation, this would complete the I/O and free resources.
}

void blk_mq_start_request(struct request* rq) {
  (void)rq;
  // Marks the request as started for timeout tracking.
}

void blk_mq_free_request(struct request* rq) {
  free(rq);
}

void blk_mq_kick_requeue_list(struct request_queue* q) {
  (void)q;
}

void blk_mq_requeue_request(struct request* rq, int kick_requeue_list) {
  (void)rq;
  (void)kick_requeue_list;
}

void blk_mq_tagset_busy_iter(struct blk_mq_tag_set* set,
                              void (*fn)(struct request*, void*, int),
                              void* data) {
  (void)set;
  (void)fn;
  (void)data;
  // Iterates over in-flight requests — no-op in simulation.
}

int blk_mq_map_queues(struct blk_mq_tag_set* set) {
  (void)set;
  return 0;
}

// --- Gendisk API ---

struct gendisk* alloc_disk(int minors) {
  auto* disk = static_cast<struct gendisk*>(
      calloc(1, sizeof(struct gendisk)));
  if (!disk)
    return nullptr;
  disk->minors = minors;
  fprintf(stderr, "driverhub: block: alloc_disk(%d)\n", minors);
  return disk;
}

int add_disk(struct gendisk* disk) {
  if (!disk) return -22;

  std::lock_guard<std::mutex> lock(g_block_mu);
  block_registry().push_back({disk, true});

  fprintf(stderr, "driverhub: block: add_disk '%s' (major=%d, capacity=%lu "
                  "sectors, %lu MB)\n",
          disk->disk_name, disk->major,
          (unsigned long)disk->capacity,
          (unsigned long)(disk->capacity * 512 / (1024 * 1024)));

  // On Fuchsia, this would create a fuchsia.storage.block/Block FIDL
  // server and register it as a DFv2 child node.
  return 0;
}

void del_gendisk(struct gendisk* disk) {
  if (!disk) return;

  std::lock_guard<std::mutex> lock(g_block_mu);
  for (auto& entry : block_registry()) {
    if (entry.disk == disk) {
      entry.active = false;
      break;
    }
  }

  fprintf(stderr, "driverhub: block: del_gendisk '%s'\n", disk->disk_name);
}

void put_disk(struct gendisk* disk) {
  if (!disk) return;

  std::lock_guard<std::mutex> lock(g_block_mu);
  auto& reg = block_registry();
  for (auto it = reg.begin(); it != reg.end(); ++it) {
    if (it->disk == disk) {
      reg.erase(it);
      break;
    }
  }

  if (disk->queue) {
    free(disk->queue);
  }
  free(disk);
}

void set_capacity(struct gendisk* disk, uint64_t sectors) {
  if (disk) {
    disk->capacity = sectors;
    fprintf(stderr, "driverhub: block: set_capacity '%s' = %lu sectors "
                    "(%lu MB)\n",
            disk->disk_name, (unsigned long)sectors,
            (unsigned long)(sectors * 512 / (1024 * 1024)));
  }
}

uint64_t get_capacity(struct gendisk* disk) {
  return disk ? disk->capacity : 0;
}

// --- Request queue configuration ---

void blk_queue_logical_block_size(struct request_queue* q, unsigned int size) {
  (void)q;
  fprintf(stderr, "driverhub: block: logical_block_size = %u\n", size);
}

void blk_queue_physical_block_size(struct request_queue* q, unsigned int size) {
  (void)q;
  fprintf(stderr, "driverhub: block: physical_block_size = %u\n", size);
}

void blk_queue_max_hw_sectors(struct request_queue* q, unsigned int sectors) {
  if (q) q->max_hw_sectors = static_cast<unsigned short>(sectors);
}

void blk_queue_max_segments(struct request_queue* q, unsigned short segs) {
  if (q) q->max_segments = segs;
}

void blk_queue_max_segment_size(struct request_queue* q, unsigned int size) {
  if (q) q->max_segment_size = size;
}

void blk_queue_dma_alignment(struct request_queue* q, int mask) {
  if (q) q->dma_alignment = static_cast<unsigned int>(mask);
}

void blk_queue_flag_set(unsigned int flag, struct request_queue* q) {
  (void)flag;
  (void)q;
}

void blk_queue_flag_clear(unsigned int flag, struct request_queue* q) {
  (void)flag;
  (void)q;
}

void blk_queue_rq_timeout(struct request_queue* q, unsigned long timeout) {
  (void)q;
  (void)timeout;
}

// --- Bio ---

struct bio* bio_alloc(struct gendisk* disk, unsigned short nr_vecs,
                       unsigned int opf, unsigned int gfp_flags) {
  (void)gfp_flags;
  auto* b = static_cast<struct bio*>(calloc(1, sizeof(struct bio)));
  if (!b) return nullptr;

  b->bi_bdev = disk;
  b->bi_opf = opf;
  b->bi_max_vecs = nr_vecs;
  if (nr_vecs > 0) {
    b->bi_io_vec = static_cast<struct bio_vec*>(
        calloc(nr_vecs, sizeof(struct bio_vec)));
  }
  return b;
}

void bio_put(struct bio* bio) {
  if (!bio) return;
  free(bio->bi_io_vec);
  free(bio);
}

void bio_endio(struct bio* bio) {
  if (bio && bio->bi_end_io) {
    bio->bi_end_io(bio);
  }
}

void submit_bio(struct bio* bio) {
  fprintf(stderr, "driverhub: block: submit_bio(op=%u, sector=%lu, %u bytes)\n",
          bio ? bio->bi_opf : 0,
          bio ? (unsigned long)bio->bi_iter_sector : 0,
          bio ? bio->bi_iter_size : 0);
  // Simulated: immediately complete.
  if (bio) {
    bio->bi_status = 0;
    bio_endio(bio);
  }
}

// --- Block device registration ---

int register_blkdev(unsigned int major, const char* name) {
  int assigned = major;
  if (major == 0) {
    assigned = g_blkdev_major_counter++;
  }
  fprintf(stderr, "driverhub: block: register_blkdev(%u, '%s') = %d\n",
          major, name ? name : "?", assigned);
  return assigned;
}

void unregister_blkdev(unsigned int major, const char* name) {
  fprintf(stderr, "driverhub: block: unregister_blkdev(%u, '%s')\n",
          major, name ? name : "?");
}

// --- Query API for test harnesses ---

int driverhub_block_device_count(void) {
  std::lock_guard<std::mutex> lock(g_block_mu);
  int count = 0;
  for (const auto& entry : block_registry()) {
    if (entry.active) count++;
  }
  return count;
}

struct gendisk* driverhub_block_get_device(int index) {
  std::lock_guard<std::mutex> lock(g_block_mu);
  int i = 0;
  for (const auto& entry : block_registry()) {
    if (entry.active) {
      if (i == index) return entry.disk;
      i++;
    }
  }
  return nullptr;
}

}  // extern "C"
