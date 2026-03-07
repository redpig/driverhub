// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/subsystem/scsi.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

// SCSI mid-layer shim.
//
// Provides host allocation, device scanning, and command dispatch for
// SCSI low-level drivers (such as UFS/ufshcd). On Fuchsia, SCSI command
// completion would drive the fuchsia.storage.block/Block FIDL responses.
//
// In userspace, SCSI operations are simulated — scan discovers a single
// LUN, and commands complete immediately.

namespace {

std::mutex g_scsi_mu;

struct RegisteredHost {
  struct Scsi_Host* host;
  bool active;
};

std::vector<RegisteredHost>& scsi_host_registry() {
  static std::vector<RegisteredHost> registry;
  return registry;
}

int g_host_no_counter = 0;

}  // namespace

extern "C" {

struct Scsi_Host* scsi_host_alloc(struct scsi_host_template* tmpl,
                                   int privsize) {
  if (!tmpl) return nullptr;

  // Allocate Scsi_Host + private data in one block.
  size_t total = sizeof(struct Scsi_Host) + static_cast<size_t>(privsize);
  auto* host = static_cast<struct Scsi_Host*>(calloc(1, total));
  if (!host)
    return nullptr;

  host->hostt = tmpl;
  host->can_queue = tmpl->can_queue > 0 ? static_cast<unsigned int>(tmpl->can_queue) : 1;
  host->sg_tablesize = tmpl->sg_tablesize > 0 ? tmpl->sg_tablesize : 64;
  host->max_sectors = tmpl->max_sectors > 0 ? tmpl->max_sectors : 256;
  host->cmd_per_lun = tmpl->cmd_per_lun > 0 ? tmpl->cmd_per_lun : 1;
  host->this_id = tmpl->this_id;
  host->max_id = 8;
  host->max_lun = 256;  // UFS can have many LUNs.
  host->max_channel = 0;
  host->max_cmd_len = MAX_COMMAND_SIZE;
  host->host_no = static_cast<unsigned int>(g_host_no_counter++);

  // hostdata points to space immediately after the Scsi_Host struct.
  host->hostdata = reinterpret_cast<void*>(
      reinterpret_cast<char*>(host) + sizeof(struct Scsi_Host));

  host->shost_gendev.init_name = tmpl->proc_name ? tmpl->proc_name : tmpl->name;

  fprintf(stderr, "driverhub: scsi: host_alloc '%s' (host%u, can_queue=%u, "
                  "sg_tablesize=%u, privsize=%d)\n",
          tmpl->name ? tmpl->name : "?", host->host_no,
          host->can_queue, host->sg_tablesize, privsize);
  return host;
}

int scsi_add_host_with_dma(struct Scsi_Host* host, struct device* dev,
                            struct device* dma_dev) {
  (void)dma_dev;
  if (!host) return -22;  // -EINVAL

  if (dev) {
    host->shost_gendev.parent = dev;
  }

  std::lock_guard<std::mutex> lock(g_scsi_mu);
  scsi_host_registry().push_back({host, true});

  fprintf(stderr, "driverhub: scsi: add_host host%u (parent=%s)\n",
          host->host_no,
          dev && dev->init_name ? dev->init_name : "(none)");
  return 0;
}

void scsi_scan_host(struct Scsi_Host* host) {
  if (!host) return;

  fprintf(stderr, "driverhub: scsi: scan_host host%u (max_id=%u, "
                  "max_lun=%u, max_channel=%u)\n",
          host->host_no, host->max_id, host->max_lun, host->max_channel);

  // Simulated scan: log discovery. In a real SCSI scan, we'd send
  // INQUIRY/REPORT LUNS to each target and create scsi_device instances.
  // For UFS, the host controller reports LUNs via the UFS protocol.
  fprintf(stderr, "driverhub: scsi: scan complete for host%u "
                  "(simulated, no real devices)\n",
          host->host_no);
}

void scsi_remove_host(struct Scsi_Host* host) {
  if (!host) return;

  std::lock_guard<std::mutex> lock(g_scsi_mu);
  for (auto& entry : scsi_host_registry()) {
    if (entry.host == host) {
      entry.active = false;
      break;
    }
  }

  fprintf(stderr, "driverhub: scsi: remove_host host%u\n", host->host_no);
}

void scsi_host_put(struct Scsi_Host* host) {
  if (!host) return;

  std::lock_guard<std::mutex> lock(g_scsi_mu);
  auto& reg = scsi_host_registry();
  for (auto it = reg.begin(); it != reg.end(); ++it) {
    if (it->host == host) {
      reg.erase(it);
      break;
    }
  }

  fprintf(stderr, "driverhub: scsi: host_put host%u (freed)\n", host->host_no);
  free(host);
}

// --- SCSI Command ---

void scsi_done(struct scsi_cmnd* cmd) {
  if (cmd && cmd->scsi_done) {
    cmd->scsi_done(cmd);
  }
}

int scsi_change_queue_depth(struct scsi_device* sdev, int depth) {
  if (!sdev) return 0;

  int old = static_cast<int>(sdev->queue_depth);
  sdev->queue_depth = static_cast<unsigned int>(depth > 0 ? depth : 1);
  fprintf(stderr, "driverhub: scsi: change_queue_depth %u:%u:%u "
                  "%d -> %u\n",
          sdev->channel, sdev->id, sdev->lun, old, sdev->queue_depth);
  return static_cast<int>(sdev->queue_depth);
}

void scsi_report_bus_reset(struct Scsi_Host* host, int channel) {
  fprintf(stderr, "driverhub: scsi: bus_reset host%u channel=%d\n",
          host ? host->host_no : 0, channel);
}

// --- SCSI DMA mapping (simplified) ---

int scsi_dma_map(struct scsi_cmnd* cmd) {
  if (!cmd) return -22;

  // In userspace, DMA addresses are just virtual addresses.
  // Return the scatter-gather count.
  if (cmd->sgl && cmd->sg_cnt > 0) {
    for (unsigned int i = 0; i < cmd->sg_cnt; i++) {
      cmd->sgl[i].dma_address =
          reinterpret_cast<uint64_t>(cmd->sgl[i].page_link) +
          cmd->sgl[i].offset;
      cmd->sgl[i].dma_length = cmd->sgl[i].length;
    }
    return static_cast<int>(cmd->sg_cnt);
  }
  return 0;
}

void scsi_dma_unmap(struct scsi_cmnd* cmd) {
  (void)cmd;
  // No-op in userspace.
}

// --- Query API for test harnesses ---

int driverhub_scsi_host_count(void) {
  std::lock_guard<std::mutex> lock(g_scsi_mu);
  int count = 0;
  for (const auto& entry : scsi_host_registry()) {
    if (entry.active) count++;
  }
  return count;
}

struct Scsi_Host* driverhub_scsi_get_host(int index) {
  std::lock_guard<std::mutex> lock(g_scsi_mu);
  int i = 0;
  for (const auto& entry : scsi_host_registry()) {
    if (entry.active) {
      if (i == index) return entry.host;
      i++;
    }
  }
  return nullptr;
}

}  // extern "C"
