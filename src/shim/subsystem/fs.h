// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_FS_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_FS_H_

// KMI shims for the Linux VFS subsystem APIs.
//
// Provides: file_operations, cdev, chrdev registration, miscdevice,
//           procfs (proc_create, proc_mkdir, seq_file),
//           debugfs (debugfs_create_dir, debugfs_create_file),
//           sysfs (sysfs_create_group, sysfs_remove_group),
//           kobject basics.
//
// On Fuchsia: procfs/debugfs/sysfs nodes map to Fuchsia devfs or
// diagnostics inspect nodes. Char devices map to
// fuchsia.io/fuchsia.hardware.pty or custom FIDL protocols.

#include <stddef.h>
#include <stdint.h>

#include "src/shim/kernel/device.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// File operations and inode basics
// ============================================================

struct inode;
struct file;
struct poll_table_struct;
struct vm_area_struct;
struct seq_file;
struct dentry;

#include <sys/types.h>  // loff_t, dev_t, ssize_t

#ifndef __fmode_t_defined
typedef unsigned int fmode_t;
#endif

struct file_operations {
  void *owner;  // struct module *
  loff_t (*llseek)(struct file *, loff_t, int);
  ssize_t (*read)(struct file *, char *, size_t, loff_t *);
  ssize_t (*write)(struct file *, const char *, size_t, loff_t *);
  long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
  long (*compat_ioctl)(struct file *, unsigned int, unsigned long);
  int (*mmap)(struct file *, struct vm_area_struct *);
  int (*open)(struct inode *, struct file *);
  int (*release)(struct inode *, struct file *);
  int (*fasync)(int, struct file *, int);
  unsigned int (*poll)(struct file *, struct poll_table_struct *);
};

// Minimal struct file.
struct file {
  const struct file_operations *f_op;
  void *private_data;
  fmode_t f_mode;
  loff_t f_pos;
  unsigned int f_flags;
};

// Minimal struct inode.
struct inode {
  unsigned long i_ino;
  void *i_private;
  unsigned short i_mode;
  dev_t i_rdev;
};

// File mode bits.
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IRUGO (S_IRUSR | S_IRGRP | S_IROTH)
#define S_IWUGO (S_IWUSR | S_IWGRP | S_IWOTH)
#define S_IRWXUGO (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

// ============================================================
// Character device (cdev)
// ============================================================

// dev_t provided by sys/types.h

#define MAJOR(dev) ((unsigned int)((dev) >> 20))
#define MINOR(dev) ((unsigned int)((dev) & 0xfffff))
#define MKDEV(ma, mi) (((ma) << 20) | (mi))

struct cdev {
  const struct file_operations *ops;
  void *owner;
  dev_t dev;
  unsigned int count;
};

void cdev_init(struct cdev *cdev, const struct file_operations *fops);
struct cdev *cdev_alloc(void);
int cdev_add(struct cdev *cdev, dev_t dev, unsigned int count);
void cdev_del(struct cdev *cdev);

int register_chrdev_region(dev_t from, unsigned int count, const char *name);
int alloc_chrdev_region(dev_t *dev, unsigned int baseminor,
                        unsigned int count, const char *name);
void unregister_chrdev_region(dev_t from, unsigned int count);

int register_chrdev(unsigned int major, const char *name,
                    const struct file_operations *fops);
void unregister_chrdev(unsigned int major, const char *name);

// ============================================================
// class / class_create / device_create
// ============================================================

// Linux uses "struct class" but that's a C++ keyword. GKI modules are C
// and see "struct class" — we typedef it in the shim include headers that
// modules use, so at the KMI boundary the symbol names match.
// Internally we use struct dh_class as the opaque type.
struct dh_class;

struct dh_class *class_create(void *owner, const char *name);
void class_destroy(struct dh_class *cls);

struct device *device_create(struct dh_class *cls, struct device *parent,
                             dev_t devt, void *drvdata, const char *fmt, ...);
void device_destroy(struct dh_class *cls, dev_t devt);

// ============================================================
// Misc device
// ============================================================

#define MISC_DYNAMIC_MINOR 255

struct miscdevice {
  int minor;
  const char *name;
  const struct file_operations *fops;
  struct device *this_device;
  void *parent;  // struct device *
  int mode;
};

int misc_register(struct miscdevice *misc);
void misc_deregister(struct miscdevice *misc);

// ============================================================
// procfs
// ============================================================

struct proc_dir_entry;

// seq_file interface (used by most /proc entries).
struct seq_file {
  char *buf;
  size_t size;
  size_t count;  // Bytes written so far.
  loff_t index;
  void *priv;  // "private" in Linux, renamed for C++ compat.
  const struct seq_operations *op;
  struct file *file;
};

struct seq_operations {
  void *(*start)(struct seq_file *m, loff_t *pos);
  void (*stop)(struct seq_file *m, void *v);
  void *(*next)(struct seq_file *m, void *v, loff_t *pos);
  int (*show)(struct seq_file *m, void *v);
};

// seq_file helpers.
void seq_printf(struct seq_file *m, const char *fmt, ...);
void seq_puts(struct seq_file *m, const char *s);
void seq_putc(struct seq_file *m, char c);
int seq_write(struct seq_file *m, const void *data, size_t len);

int seq_open(struct file *file, const struct seq_operations *op);
int seq_release(struct inode *inode, struct file *file);
ssize_t seq_read(struct file *file, char *buf, size_t size, loff_t *ppos);
loff_t seq_lseek(struct file *file, loff_t offset, int whence);

// Single-open seq_file (most common pattern).
int single_open(struct file *file, int (*show)(struct seq_file *, void *),
                void *data);
int single_release(struct inode *inode, struct file *file);

// procfs entry management.
struct proc_dir_entry *proc_create(const char *name, unsigned short mode,
                                    struct proc_dir_entry *parent,
                                    const struct file_operations *proc_fops);
struct proc_dir_entry *proc_create_data(
    const char *name, unsigned short mode,
    struct proc_dir_entry *parent,
    const struct file_operations *proc_fops, void *data);
struct proc_dir_entry *proc_mkdir(const char *name,
                                   struct proc_dir_entry *parent);
void proc_remove(struct proc_dir_entry *entry);
void remove_proc_entry(const char *name, struct proc_dir_entry *parent);

// proc_create_single_data: convenience for single-show proc entries.
struct proc_dir_entry *proc_create_single_data(
    const char *name, unsigned short mode,
    struct proc_dir_entry *parent,
    int (*show)(struct seq_file *, void *), void *data);

// ============================================================
// debugfs
// ============================================================

struct debugfs_blob_wrapper {
  void *data;
  unsigned long size;
};

struct dentry *debugfs_create_dir(const char *name, struct dentry *parent);
struct dentry *debugfs_create_file(const char *name, unsigned short mode,
                                    struct dentry *parent, void *data,
                                    const struct file_operations *fops);
void debugfs_remove(struct dentry *dentry);
void debugfs_remove_recursive(struct dentry *dentry);

// Typed debugfs helpers.
void debugfs_create_u8(const char *name, unsigned short mode,
                       struct dentry *parent, uint8_t *value);
void debugfs_create_u16(const char *name, unsigned short mode,
                        struct dentry *parent, uint16_t *value);
void debugfs_create_u32(const char *name, unsigned short mode,
                        struct dentry *parent, uint32_t *value);
void debugfs_create_u64(const char *name, unsigned short mode,
                        struct dentry *parent, uint64_t *value);
void debugfs_create_bool(const char *name, unsigned short mode,
                         struct dentry *parent, int *value);
void debugfs_create_blob(const char *name, unsigned short mode,
                         struct dentry *parent,
                         struct debugfs_blob_wrapper *blob);

// ============================================================
// sysfs
// ============================================================

struct kobject {
  const char *name;
  struct kobject *parent;
  void *ktype;
  void *sd;  // struct kernfs_node *
  int state_initialized;
  int state_in_sysfs;
};

struct attribute {
  const char *name;
  unsigned short mode;
};

struct device_attribute {
  struct attribute attr;
  ssize_t (*show)(struct device *dev, struct device_attribute *attr, char *buf);
  ssize_t (*store)(struct device *dev, struct device_attribute *attr,
                   const char *buf, size_t count);
};

struct attribute_group {
  const char *name;  // Optional subdirectory name.
  struct attribute **attrs;
};

// Convenience macros for declaring device attributes.
#define DEVICE_ATTR(_name, _mode, _show, _store) \
  struct device_attribute dev_attr_##_name = { \
    .attr = { .name = #_name, .mode = _mode }, \
    .show = _show, .store = _store }

#define DEVICE_ATTR_RO(_name) \
  DEVICE_ATTR(_name, S_IRUGO, _name##_show, NULL)

#define DEVICE_ATTR_RW(_name) \
  DEVICE_ATTR(_name, S_IRUGO | S_IWUSR, _name##_show, _name##_store)

int sysfs_create_group(struct kobject *kobj,
                       const struct attribute_group *grp);
void sysfs_remove_group(struct kobject *kobj,
                        const struct attribute_group *grp);
int sysfs_create_file(struct kobject *kobj, const struct attribute *attr);
void sysfs_remove_file(struct kobject *kobj, const struct attribute *attr);

int device_create_file(struct device *dev,
                       const struct device_attribute *attr);
void device_remove_file(struct device *dev,
                        const struct device_attribute *attr);

// kobject basics.
void kobject_init(struct kobject *kobj, void *ktype);
int kobject_add(struct kobject *kobj, struct kobject *parent,
                const char *fmt, ...);
struct kobject *kobject_create_and_add(const char *name,
                                       struct kobject *parent);
void kobject_put(struct kobject *kobj);
void kobject_del(struct kobject *kobj);

// sysfs_notify: wake up poll waiters on a sysfs attribute.
void sysfs_notify(struct kobject *kobj, const char *dir, const char *attr);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_FS_H_
