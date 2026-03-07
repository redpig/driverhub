// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/subsystem/fs.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// VFS subsystem shim.
//
// Provides implementations of Linux VFS APIs: character devices, misc devices,
// procfs, debugfs, and sysfs. Entries are tracked in internal registries.
//
// On Fuchsia:
// - procfs/debugfs → Fuchsia Inspect or devfs pseudo-entries
// - sysfs → Fuchsia devfs properties or Inspect
// - char/misc devices → fuchsia.io or custom FIDL protocols
// - kobjects → Fuchsia component topology nodes
//
// In the runner model, these registries are bridged to Starnix via the
// fuchsia.driverhub.DeviceFs FIDL protocol so that Linux binaries running
// under Starnix can see /sys, /proc, and /dev entries created by .ko modules.
// See docs/starnix-integration.md for the full design.

// Define opaque types (forward-declared in fs.h).
struct dh_class {
  int id;
};

struct proc_dir_entry {
  int id;
};

struct dentry {
  int id;
};

namespace {

std::mutex g_fs_mu;

// --- Character device registry ---

struct ChrdevEntry {
  std::string name;
  unsigned int major;
  unsigned int count;
  const struct file_operations *fops;
  struct cdev *cdev;
};

std::vector<ChrdevEntry> g_chrdevs;
unsigned int g_next_dynamic_major = 240;

// --- Misc device registry ---

std::vector<struct miscdevice *> g_misc_devices;
int g_next_misc_minor = 64;

// --- Class registry ---

struct ClassEntry {
  std::string name;
};

std::unordered_map<struct dh_class *, ClassEntry> g_classes;

// --- Proc entries ---

struct ProcEntry {
  std::string name;
  std::string path;  // Full path for logging.
  const struct file_operations *fops;
  void *data;
  int is_dir;
};

std::unordered_map<struct proc_dir_entry *, ProcEntry> g_proc_entries;

// --- Debugfs entries ---

struct DebugfsEntry {
  std::string name;
  const struct file_operations *fops;
  void *data;
  int is_dir;
  struct dentry *parent;
};

std::unordered_map<struct dentry *, DebugfsEntry> g_debugfs_entries;

// --- Sysfs ---

std::vector<std::pair<struct kobject *, const struct attribute_group *>>
    g_sysfs_groups;

// --- Kobjects ---

std::vector<struct kobject *> g_kobjects;

}  // namespace

extern "C" {

// ============================================================
// Character device (cdev)
// ============================================================

void cdev_init(struct cdev *cdev, const struct file_operations *fops) {
  if (!cdev) return;
  memset(cdev, 0, sizeof(*cdev));
  cdev->ops = fops;
}

struct cdev *cdev_alloc(void) {
  auto *c = static_cast<struct cdev *>(calloc(1, sizeof(struct cdev)));
  return c;
}

int cdev_add(struct cdev *cdev, dev_t dev, unsigned int count) {
  if (!cdev) return -22;
  cdev->dev = dev;
  cdev->count = count;
  std::lock_guard<std::mutex> lock(g_fs_mu);
  g_chrdevs.push_back({"cdev", MAJOR(dev), count, cdev->ops, cdev});
  fprintf(stderr, "driverhub: cdev: added %u:%u (count=%u)\n",
          MAJOR(dev), MINOR(dev), count);
  return 0;
}

void cdev_del(struct cdev *cdev) {
  if (!cdev) return;
  std::lock_guard<std::mutex> lock(g_fs_mu);
  for (auto it = g_chrdevs.begin(); it != g_chrdevs.end(); ++it) {
    if (it->cdev == cdev) {
      g_chrdevs.erase(it);
      break;
    }
  }
  fprintf(stderr, "driverhub: cdev: deleted\n");
}

int register_chrdev_region(dev_t from, unsigned int count, const char *name) {
  std::lock_guard<std::mutex> lock(g_fs_mu);
  g_chrdevs.push_back({name ? name : "", MAJOR(from), count, nullptr, nullptr});
  fprintf(stderr, "driverhub: chrdev: registered region %u:%u count=%u '%s'\n",
          MAJOR(from), MINOR(from), count, name ? name : "");
  return 0;
}

int alloc_chrdev_region(dev_t *dev, unsigned int baseminor,
                        unsigned int count, const char *name) {
  std::lock_guard<std::mutex> lock(g_fs_mu);
  unsigned int major = g_next_dynamic_major++;
  *dev = MKDEV(major, baseminor);
  g_chrdevs.push_back({name ? name : "", major, count, nullptr, nullptr});
  fprintf(stderr, "driverhub: chrdev: alloc region %u:%u count=%u '%s'\n",
          major, baseminor, count, name ? name : "");
  return 0;
}

void unregister_chrdev_region(dev_t from, unsigned int count) {
  (void)count;
  std::lock_guard<std::mutex> lock(g_fs_mu);
  unsigned int major = MAJOR(from);
  for (auto it = g_chrdevs.begin(); it != g_chrdevs.end(); ++it) {
    if (it->major == major) {
      g_chrdevs.erase(it);
      break;
    }
  }
  fprintf(stderr, "driverhub: chrdev: unregistered region %u\n", major);
}

int register_chrdev(unsigned int major, const char *name,
                    const struct file_operations *fops) {
  std::lock_guard<std::mutex> lock(g_fs_mu);
  if (major == 0) {
    major = g_next_dynamic_major++;
  }
  g_chrdevs.push_back({name ? name : "", major, 256, fops, nullptr});
  fprintf(stderr, "driverhub: chrdev: registered '%s' major=%u\n",
          name ? name : "", major);
  return static_cast<int>(major);
}

void unregister_chrdev(unsigned int major, const char *name) {
  (void)name;
  std::lock_guard<std::mutex> lock(g_fs_mu);
  for (auto it = g_chrdevs.begin(); it != g_chrdevs.end(); ++it) {
    if (it->major == major) {
      g_chrdevs.erase(it);
      break;
    }
  }
  fprintf(stderr, "driverhub: chrdev: unregistered major=%u\n", major);
}

// ============================================================
// class / device_create
// ============================================================

struct dh_class *class_create(void *owner, const char *name) {
  (void)owner;
  auto *cls = static_cast<struct dh_class *>(calloc(1, sizeof(struct dh_class)));
  if (!cls) return nullptr;
  std::lock_guard<std::mutex> lock(g_fs_mu);
  g_classes[cls] = {name ? name : ""};
  fprintf(stderr, "driverhub: class: created '%s'\n", name ? name : "");
  return cls;
}

void class_destroy(struct dh_class *cls) {
  if (!cls) return;
  std::lock_guard<std::mutex> lock(g_fs_mu);
  g_classes.erase(cls);
  free(cls);
  fprintf(stderr, "driverhub: class: destroyed\n");
}

struct device *device_create(struct dh_class *cls, struct device *parent,
                             dev_t devt, void *drvdata, const char *fmt, ...) {
  (void)cls;
  (void)parent;
  auto *dev = static_cast<struct device *>(calloc(1, sizeof(struct device)));
  if (!dev) return nullptr;
  dev->driver_data = drvdata;

  char name[64];
  va_list args;
  va_start(args, fmt);
  vsnprintf(name, sizeof(name), fmt, args);
  va_end(args);
  dev->init_name = strdup(name);

  fprintf(stderr, "driverhub: device_create '%s' (%u:%u)\n",
          name, MAJOR(devt), MINOR(devt));
  return dev;
}

void device_destroy(struct dh_class *cls, dev_t devt) {
  (void)cls;
  fprintf(stderr, "driverhub: device_destroy %u:%u\n",
          MAJOR(devt), MINOR(devt));
}

// ============================================================
// Misc device
// ============================================================

int misc_register(struct miscdevice *misc) {
  if (!misc) return -22;
  std::lock_guard<std::mutex> lock(g_fs_mu);
  if (misc->minor == MISC_DYNAMIC_MINOR) {
    misc->minor = g_next_misc_minor++;
  }
  g_misc_devices.push_back(misc);
  fprintf(stderr, "driverhub: misc: registered '%s' minor=%d\n",
          misc->name ? misc->name : "", misc->minor);
  return 0;
}

void misc_deregister(struct miscdevice *misc) {
  if (!misc) return;
  std::lock_guard<std::mutex> lock(g_fs_mu);
  for (auto it = g_misc_devices.begin(); it != g_misc_devices.end(); ++it) {
    if (*it == misc) {
      g_misc_devices.erase(it);
      break;
    }
  }
  fprintf(stderr, "driverhub: misc: deregistered '%s'\n",
          misc->name ? misc->name : "");
}

// ============================================================
// seq_file
// ============================================================

void seq_printf(struct seq_file *m, const char *fmt, ...) {
  if (!m || !m->buf) return;
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(m->buf + m->count, m->size - m->count, fmt, args);
  va_end(args);
  if (n > 0 && m->count + static_cast<size_t>(n) < m->size) {
    m->count += n;
  }
}

void seq_puts(struct seq_file *m, const char *s) {
  if (!m || !m->buf || !s) return;
  size_t len = strlen(s);
  if (m->count + len < m->size) {
    memcpy(m->buf + m->count, s, len);
    m->count += len;
  }
}

void seq_putc(struct seq_file *m, char c) {
  if (!m || !m->buf) return;
  if (m->count < m->size) {
    m->buf[m->count++] = c;
  }
}

int seq_write(struct seq_file *m, const void *data, size_t len) {
  if (!m || !m->buf || !data) return -1;
  if (m->count + len <= m->size) {
    memcpy(m->buf + m->count, data, len);
    m->count += len;
    return 0;
  }
  return -1;
}

int seq_open(struct file *file, const struct seq_operations *op) {
  if (!file) return -22;
  auto *m = static_cast<struct seq_file *>(calloc(1, sizeof(struct seq_file)));
  if (!m) return -12;  // -ENOMEM
  m->op = op;
  m->size = 4096;
  m->buf = static_cast<char *>(calloc(1, m->size));
  m->file = file;
  file->private_data = m;
  return 0;
}

int seq_release(struct inode *inode, struct file *file) {
  (void)inode;
  if (!file || !file->private_data) return 0;
  auto *m = static_cast<struct seq_file *>(file->private_data);
  free(m->buf);
  free(m);
  file->private_data = nullptr;
  return 0;
}

ssize_t seq_read(struct file *file, char *buf, size_t size, loff_t *ppos) {
  (void)file;
  (void)buf;
  (void)size;
  (void)ppos;
  // Simplified: in production this would walk the seq_operations.
  return 0;
}

loff_t seq_lseek(struct file *file, loff_t offset, int whence) {
  (void)file;
  (void)offset;
  (void)whence;
  return 0;
}

int single_open(struct file *file, int (*show)(struct seq_file *, void *),
                void *data) {
  if (!file) return -22;
  auto *m = static_cast<struct seq_file *>(calloc(1, sizeof(struct seq_file)));
  if (!m) return -12;
  m->size = 4096;
  m->buf = static_cast<char *>(calloc(1, m->size));
  m->priv = data;
  m->file = file;
  file->private_data = m;
  // Call show immediately so the data is ready.
  if (show) show(m, data);
  return 0;
}

int single_release(struct inode *inode, struct file *file) {
  return seq_release(inode, file);
}

// ============================================================
// procfs
// ============================================================

struct proc_dir_entry *proc_create(const char *name, unsigned short mode,
                                    struct proc_dir_entry *parent,
                                    const struct file_operations *proc_fops) {
  return proc_create_data(name, mode, parent, proc_fops, nullptr);
}

struct proc_dir_entry *proc_create_data(
    const char *name, unsigned short mode,
    struct proc_dir_entry *parent,
    const struct file_operations *proc_fops, void *data) {
  (void)mode;
  auto *entry = static_cast<struct proc_dir_entry *>(
      calloc(1, sizeof(struct proc_dir_entry)));
  if (!entry) return nullptr;

  std::string path;
  if (parent) {
    std::lock_guard<std::mutex> lock(g_fs_mu);
    auto it = g_proc_entries.find(parent);
    if (it != g_proc_entries.end()) {
      path = it->second.path + "/";
    }
  }
  path += name ? name : "";

  {
    std::lock_guard<std::mutex> lock(g_fs_mu);
    g_proc_entries[entry] = {name ? name : "", path, proc_fops, data, 0};
  }

  fprintf(stderr, "driverhub: proc: created /proc/%s\n", path.c_str());
  return entry;
}

struct proc_dir_entry *proc_mkdir(const char *name,
                                   struct proc_dir_entry *parent) {
  auto *entry = static_cast<struct proc_dir_entry *>(
      calloc(1, sizeof(struct proc_dir_entry)));
  if (!entry) return nullptr;

  std::string path;
  if (parent) {
    std::lock_guard<std::mutex> lock(g_fs_mu);
    auto it = g_proc_entries.find(parent);
    if (it != g_proc_entries.end()) {
      path = it->second.path + "/";
    }
  }
  path += name ? name : "";

  {
    std::lock_guard<std::mutex> lock(g_fs_mu);
    g_proc_entries[entry] = {name ? name : "", path, nullptr, nullptr, 1};
  }

  fprintf(stderr, "driverhub: proc: mkdir /proc/%s\n", path.c_str());
  return entry;
}

void proc_remove(struct proc_dir_entry *entry) {
  if (!entry) return;
  std::lock_guard<std::mutex> lock(g_fs_mu);
  auto it = g_proc_entries.find(entry);
  if (it != g_proc_entries.end()) {
    fprintf(stderr, "driverhub: proc: removed /proc/%s\n",
            it->second.path.c_str());
    g_proc_entries.erase(it);
  }
  free(entry);
}

void remove_proc_entry(const char *name, struct proc_dir_entry *parent) {
  std::lock_guard<std::mutex> lock(g_fs_mu);
  std::string target = name ? name : "";
  for (auto it = g_proc_entries.begin(); it != g_proc_entries.end(); ++it) {
    if (it->second.name == target) {
      // If parent is specified, check that parent matches by comparing
      // the path prefix. For simplicity, just match by name.
      (void)parent;
      fprintf(stderr, "driverhub: proc: removed /proc/%s\n",
              it->second.path.c_str());
      free(it->first);
      g_proc_entries.erase(it);
      return;
    }
  }
}

struct proc_dir_entry *proc_create_single_data(
    const char *name, unsigned short mode,
    struct proc_dir_entry *parent,
    int (*show)(struct seq_file *, void *), void *data) {
  (void)show;
  return proc_create_data(name, mode, parent, nullptr, data);
}

// ============================================================
// debugfs
// ============================================================

struct dentry *debugfs_create_dir(const char *name, struct dentry *parent) {
  auto *d = static_cast<struct dentry *>(calloc(1, sizeof(struct dentry)));
  if (!d) return nullptr;

  std::lock_guard<std::mutex> lock(g_fs_mu);
  g_debugfs_entries[d] = {name ? name : "", nullptr, nullptr, 1, parent};
  fprintf(stderr, "driverhub: debugfs: mkdir '%s'\n", name ? name : "");
  return d;
}

struct dentry *debugfs_create_file(const char *name, unsigned short mode,
                                    struct dentry *parent, void *data,
                                    const struct file_operations *fops) {
  (void)mode;
  auto *d = static_cast<struct dentry *>(calloc(1, sizeof(struct dentry)));
  if (!d) return nullptr;

  std::lock_guard<std::mutex> lock(g_fs_mu);
  g_debugfs_entries[d] = {name ? name : "", fops, data, 0, parent};
  fprintf(stderr, "driverhub: debugfs: created '%s'\n", name ? name : "");
  return d;
}

void debugfs_remove(struct dentry *dentry) {
  if (!dentry) return;
  std::lock_guard<std::mutex> lock(g_fs_mu);
  auto it = g_debugfs_entries.find(dentry);
  if (it != g_debugfs_entries.end()) {
    fprintf(stderr, "driverhub: debugfs: removed '%s'\n",
            it->second.name.c_str());
    g_debugfs_entries.erase(it);
  }
  free(dentry);
}

void debugfs_remove_recursive(struct dentry *dentry) {
  if (!dentry) return;
  std::lock_guard<std::mutex> lock(g_fs_mu);
  // Remove this entry and all children (entries whose parent is this dentry).
  std::vector<struct dentry *> to_remove;
  for (auto &kv : g_debugfs_entries) {
    if (kv.second.parent == dentry || kv.first == dentry) {
      to_remove.push_back(kv.first);
    }
  }
  for (auto *d : to_remove) {
    g_debugfs_entries.erase(d);
    if (d != dentry) free(d);
  }
  fprintf(stderr, "driverhub: debugfs: removed recursive (%zu entries)\n",
          to_remove.size());
  free(dentry);
}

void debugfs_create_u8(const char *name, unsigned short mode,
                       struct dentry *parent, uint8_t *value) {
  (void)value;
  debugfs_create_file(name, mode, parent, value, nullptr);
}

void debugfs_create_u16(const char *name, unsigned short mode,
                        struct dentry *parent, uint16_t *value) {
  (void)value;
  debugfs_create_file(name, mode, parent, value, nullptr);
}

void debugfs_create_u32(const char *name, unsigned short mode,
                        struct dentry *parent, uint32_t *value) {
  (void)value;
  debugfs_create_file(name, mode, parent, value, nullptr);
}

void debugfs_create_u64(const char *name, unsigned short mode,
                        struct dentry *parent, uint64_t *value) {
  (void)value;
  debugfs_create_file(name, mode, parent, value, nullptr);
}

void debugfs_create_bool(const char *name, unsigned short mode,
                         struct dentry *parent, int *value) {
  (void)value;
  debugfs_create_file(name, mode, parent, value, nullptr);
}

void debugfs_create_blob(const char *name, unsigned short mode,
                         struct dentry *parent,
                         struct debugfs_blob_wrapper *blob) {
  debugfs_create_file(name, mode, parent, blob, nullptr);
}

// ============================================================
// sysfs
// ============================================================

int sysfs_create_group(struct kobject *kobj,
                       const struct attribute_group *grp) {
  if (!kobj || !grp) return -22;
  std::lock_guard<std::mutex> lock(g_fs_mu);
  g_sysfs_groups.push_back({kobj, grp});
  fprintf(stderr, "driverhub: sysfs: created group '%s' on '%s'\n",
          grp->name ? grp->name : "(default)",
          kobj->name ? kobj->name : "");
  return 0;
}

void sysfs_remove_group(struct kobject *kobj,
                        const struct attribute_group *grp) {
  std::lock_guard<std::mutex> lock(g_fs_mu);
  for (auto it = g_sysfs_groups.begin(); it != g_sysfs_groups.end(); ++it) {
    if (it->first == kobj && it->second == grp) {
      g_sysfs_groups.erase(it);
      break;
    }
  }
  fprintf(stderr, "driverhub: sysfs: removed group\n");
}

int sysfs_create_file(struct kobject *kobj, const struct attribute *attr) {
  (void)kobj;
  fprintf(stderr, "driverhub: sysfs: created file '%s'\n",
          attr && attr->name ? attr->name : "");
  return 0;
}

void sysfs_remove_file(struct kobject *kobj, const struct attribute *attr) {
  (void)kobj;
  fprintf(stderr, "driverhub: sysfs: removed file '%s'\n",
          attr && attr->name ? attr->name : "");
}

int device_create_file(struct device *dev,
                       const struct device_attribute *attr) {
  (void)dev;
  fprintf(stderr, "driverhub: sysfs: device_create_file '%s'\n",
          attr && attr->attr.name ? attr->attr.name : "");
  return 0;
}

void device_remove_file(struct device *dev,
                        const struct device_attribute *attr) {
  (void)dev;
  fprintf(stderr, "driverhub: sysfs: device_remove_file '%s'\n",
          attr && attr->attr.name ? attr->attr.name : "");
}

// ============================================================
// kobject
// ============================================================

void kobject_init(struct kobject *kobj, void *ktype) {
  if (!kobj) return;
  kobj->ktype = ktype;
  kobj->state_initialized = 1;
}

int kobject_add(struct kobject *kobj, struct kobject *parent,
                const char *fmt, ...) {
  if (!kobj) return -22;
  kobj->parent = parent;

  char name[64];
  va_list args;
  va_start(args, fmt);
  vsnprintf(name, sizeof(name), fmt, args);
  va_end(args);
  kobj->name = strdup(name);
  kobj->state_in_sysfs = 1;

  std::lock_guard<std::mutex> lock(g_fs_mu);
  g_kobjects.push_back(kobj);
  fprintf(stderr, "driverhub: kobject: added '%s'\n", name);
  return 0;
}

struct kobject *kobject_create_and_add(const char *name,
                                       struct kobject *parent) {
  auto *kobj = static_cast<struct kobject *>(calloc(1, sizeof(struct kobject)));
  if (!kobj) return nullptr;
  kobject_init(kobj, nullptr);
  kobject_add(kobj, parent, "%s", name);
  return kobj;
}

void kobject_put(struct kobject *kobj) {
  if (!kobj) return;
  std::lock_guard<std::mutex> lock(g_fs_mu);
  for (auto it = g_kobjects.begin(); it != g_kobjects.end(); ++it) {
    if (*it == kobj) {
      g_kobjects.erase(it);
      break;
    }
  }
  free(const_cast<char *>(kobj->name));
  free(kobj);
}

void kobject_del(struct kobject *kobj) {
  if (!kobj) return;
  kobj->state_in_sysfs = 0;
  fprintf(stderr, "driverhub: kobject: del '%s'\n",
          kobj->name ? kobj->name : "");
}

void sysfs_notify(struct kobject *kobj, const char *dir, const char *attr) {
  (void)kobj;
  (void)dir;
  fprintf(stderr, "driverhub: sysfs: notify '%s'\n", attr ? attr : "");
}

}  // extern "C"
