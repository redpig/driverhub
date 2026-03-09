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

#include "src/shim/include/linux/android_kabi.h"
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

#include <sys/types.h>  // dev_t, ssize_t

// loff_t: defined by glibc on Linux, but not by Fuchsia's musl.
#if defined(__Fuchsia__) && !defined(_LOFF_T_DEFINED)
#define _LOFF_T_DEFINED
typedef long long loff_t;
#endif

#ifndef __fmode_t_defined
typedef unsigned int fmode_t;
#endif

// Must match the Linux 6.6 GKI kernel's struct file_operations layout exactly.
// Field offsets are ABI — precompiled .ko modules access fops at compiled-in
// offsets.
//
// GKI 6.6 adds 4 ANDROID_KABI_RESERVE slots at the end.
// Total: 33 function pointers (264 bytes) + 4 KABI reserves (32 bytes) = 296 bytes.
struct file_operations {
  void *owner;  // struct module *                                //   0
  loff_t (*llseek)(struct file *, loff_t, int);                   //   8
  ssize_t (*read)(struct file *, char *, size_t, loff_t *);       //  16
  ssize_t (*write)(struct file *, const char *, size_t, loff_t *);//  24
  void *read_iter;                                                //  32
  void *write_iter;                                               //  40
  void *iopoll;                                                   //  48
  void *iterate_shared;                                           //  56
  unsigned int (*poll)(struct file *, struct poll_table_struct *); //  64
  long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long); // 72
  long (*compat_ioctl)(struct file *, unsigned int, unsigned long);   // 80
  int (*mmap)(struct file *, struct vm_area_struct *);             //  88
  unsigned long mmap_supported_flags;                             //  96
  int (*open)(struct inode *, struct file *);                     // 104
  void *flush;                                                    // 112
  int (*release)(struct inode *, struct file *);                  // 120
  void *fsync;                                                    // 128
  int (*fasync)(int, struct file *, int);                         // 136
  void *lock;                                                     // 144
  void *sendpage;                                                 // 152
  void *get_unmapped_area;                                        // 160
  void *check_flags;                                              // 168
  void *flock;                                                    // 176
  void *splice_write;                                             // 184
  void *splice_read;                                              // 192
  void *splice_eof;                                               // 200
  void *setlease;                                                 // 208
  void *fallocate;                                                // 216
  void *show_fdinfo;                                              // 224
  void *copy_file_range;                                          // 232
  void *remap_file_range;                                         // 240
  void *fadvise;                                                  // 248
  void *uring_cmd;                                                // 256
  // GKI ANDROID_KABI_RESERVE slots.
  ANDROID_KABI_RESERVE(1);                                        // 264
  ANDROID_KABI_RESERVE(2);                                        // 272
  ANDROID_KABI_RESERVE(3);                                        // 280
  ANDROID_KABI_RESERVE(4);                                        // 288
};  // Total: 296 bytes

// struct file — must match the GKI 6.6 kernel layout at offsets that
// module code accesses.  Shim-only fields are placed in the initial
// padding region (kernel uses union/spinlock/fmode_t there, but modules
// don't access those directly).
//
// Key kernel offsets (from rfkill.ko disassembly):
//   f_flags:      0x58  (module reads byte 0x59 bit 3 for O_NONBLOCK)
//   private_data: 0xd8  (module reads/writes per-file data pointer)
//
// Shim-internal fields (f_op, f_mode, f_pos) are at the start where
// the kernel has f_llist/f_lock/f_mode/f_count — modules never access
// these directly.
//
// GKI 6.6 adds 4 ANDROID_KABI_RESERVE slots at the end of struct file,
// extending the total size to ~288 bytes.
struct file {
  // --- Shim-only region (0x00 – 0x17) ---
  const struct file_operations *f_op;    // 0x00 (shim dispatch)
  fmode_t f_mode;                        // 0x08 (shim-internal)
  unsigned int _pad0;                    // 0x0c
  loff_t f_pos;                          // 0x10 (shim-internal)

  // --- Padding to kernel f_flags offset ---
  char _pad1[0x58 - 0x18];              // 0x18 – 0x57

  // --- Module-accessed fields at kernel offsets ---
  unsigned int f_flags;                  // 0x58 (matches kernel)
  char _pad2[0xd8 - 0x5c];              // 0x5c – 0xd7
  void *private_data;                    // 0xd8 (matches kernel)

  // --- Tail padding + GKI KABI reserves ---
  char _pad3[0xe0 - 0xd8 - sizeof(void*)]; // 0xe0 pad
  ANDROID_KABI_RESERVE(1);               // 0xe0
  ANDROID_KABI_RESERVE(2);               // 0xe8
  ANDROID_KABI_RESERVE(3);               // 0xf0
  ANDROID_KABI_RESERVE(4);               // 0xf8
  char _pad4[0x120 - 0x100];            // tail padding to 288 bytes
};  // Total: 288 bytes

// struct inode — must be large enough for module code that may access
// fields at kernel offsets.  rfkill.ko only passes inode to stream_open
// (our shim) and doesn't dereference fields directly.  Other modules
// may access i_rdev, i_private, etc.
//
// GKI 6.6 struct inode on ARM64 is ~720 bytes with 4 KABI reserves.
// We use a padded struct with key fields at their correct offsets.
// TODO: Validate exact offsets against GKI 6.6 vmlinux when available.
struct inode {
  unsigned long i_ino;                   // 0x00
  void *i_private;                       // 0x08
  unsigned short i_mode;                 // 0x10
  unsigned short _pad0;                  // 0x12
  dev_t i_rdev;                          // 0x14
  char _pad1[0x2c0 - 0x14 - sizeof(dev_t) - 4 * sizeof(uint64_t)];
  // GKI KABI reserves at the end.
  ANDROID_KABI_RESERVE(1);
  ANDROID_KABI_RESERVE(2);
  ANDROID_KABI_RESERVE(3);
  ANDROID_KABI_RESERVE(4);
};  // Total: ~704 bytes (padded to accommodate GKI 6.6 layout)

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
  char _kabi_pad[64 - 4 * sizeof(void*) - 2 * sizeof(int) - 2 * sizeof(uint64_t)];
  ANDROID_KABI_RESERVE(1);
  ANDROID_KABI_RESERVE(2);
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

// sysfs_emit: print into a sysfs attribute page buffer (PAGE_SIZE limited).
int sysfs_emit(char *buf, const char *fmt, ...);

// kobject_uevent: send a uevent for hotplug notification.
int kobject_uevent(struct kobject *kobj, int action);

// uevent environment variable helper.
struct kobj_uevent_env {
  char buf[2048];
  int buflen;
  int envp_idx;
  char *envp[32];
};
int add_uevent_var(struct kobj_uevent_env *env, const char *fmt, ...);

// stream_open: mark a file as a stream (non-seekable).
int stream_open(struct inode *inode, struct file *filp);

// compat_ptr_ioctl: 32-bit compat ioctl passthrough.
long compat_ptr_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_FS_H_
