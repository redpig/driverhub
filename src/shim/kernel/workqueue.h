// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_KERNEL_WORKQUEUE_H_
#define DRIVERHUB_SRC_SHIM_KERNEL_WORKQUEUE_H_

// KMI shims for Linux kernel workqueue APIs.
//
// Provides: INIT_WORK, schedule_work, cancel_work_sync, flush_work,
//           create_singlethread_workqueue, destroy_workqueue,
//           queue_work, queue_delayed_work, cancel_delayed_work_sync
//
// Backed by a thread pool in userspace.
// On Fuchsia: async::Loop / async::PostTask.

#ifdef __cplusplus
extern "C" {
#endif

struct work_struct {
  void (*func)(struct work_struct *);
  void* _opaque;  // Internal state.
  int _pending;
};

struct delayed_work {
  struct work_struct work;
  unsigned long delay;  // In jiffies.
  void* _timer_opaque;
};

struct workqueue_struct;

#define INIT_WORK(w, f) do { (w)->func = (f); (w)->_opaque = 0; (w)->_pending = 0; } while(0)
#define INIT_DELAYED_WORK(dw, f) do { INIT_WORK(&(dw)->work, (f)); (dw)->delay = 0; (dw)->_timer_opaque = 0; } while(0)

int schedule_work(struct work_struct *work);
int cancel_work_sync(struct work_struct *work);
void flush_work(struct work_struct *work);

struct workqueue_struct *create_singlethread_workqueue(const char *name);
void destroy_workqueue(struct workqueue_struct *wq);

int queue_work(struct workqueue_struct *wq, struct work_struct *work);
int queue_delayed_work(struct workqueue_struct *wq,
                       struct delayed_work *dwork,
                       unsigned long delay);
int cancel_delayed_work_sync(struct delayed_work *dwork);

// Flush all pending work on a workqueue.
void flush_workqueue(struct workqueue_struct *wq);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_KERNEL_WORKQUEUE_H_
