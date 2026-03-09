// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_VFS_SERVICE_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_VFS_SERVICE_H_

// Generic virtual filesystem services for DriverHub.
//
// Inspired by Starnix's FsNode/FsNodeOps pattern, this provides three
// unified virtual filesystem services that automatically serve any device,
// sysfs attribute, or proc entry registered by loaded .ko modules:
//
//   DevFs  — /dev (misc_register, cdev_add, register_chrdev)
//   SysFs  — /sys (sysfs_create_file, device_create_file, kobject_add)
//   ProcFs — /proc (proc_create, proc_create_data, proc_mkdir)
//
// Each service maintains a virtual directory tree.  When a module registers
// a device/attribute/entry, a VfsNode is created in the appropriate tree.
// Client tools (devctl) can open nodes and perform read/write/ioctl via
// the IPC protocol, which dispatches to the module's file_operations or
// show/store callbacks.
//
// This is the same model Starnix uses: virtual filesystem nodes backed by
// kernel (shim) callbacks, served to userspace via a channel protocol.

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations from fs.h (avoid include cycle).
struct file_operations;
struct file;
struct inode;
struct seq_file;
struct device;
struct device_attribute;
struct kobject;
struct attribute;
struct attribute_group;

namespace driverhub {

// ============================================================
// VfsNode — a single node in the virtual filesystem tree.
// Mirrors Starnix's FsNode concept.
// ============================================================

enum class VfsNodeType : uint8_t {
  kDirectory,      // Directory node (can contain children).
  kCharDevice,     // Character device (has file_operations).
  kMiscDevice,     // Misc device (has file_operations).
  kProcEntry,      // Proc entry (has file_operations or show callback).
  kSysfsAttr,      // Sysfs attribute (has show/store callbacks).
  kDebugfsEntry,   // Debugfs entry (has file_operations).
};

struct VfsNode {
  std::string name;
  std::string path;        // Full path from root (e.g., "/dev/rfkill").
  VfsNodeType type;
  unsigned short mode = 0; // Permission bits.

  // Parent and children for tree traversal.
  VfsNode* parent = nullptr;
  std::unordered_map<std::string, std::unique_ptr<VfsNode>> children;

  // Backing callbacks — set depending on type.
  // For kCharDevice, kMiscDevice, kProcEntry, kDebugfsEntry:
  const struct file_operations* fops = nullptr;
  void* private_data = nullptr;  // data pointer for proc/debugfs.

  // For kSysfsAttr:
  const struct device_attribute* dev_attr = nullptr;
  struct device* dev = nullptr;

  // For kProcEntry with single_open pattern:
  int (*single_show)(struct seq_file*, void*) = nullptr;

  // Device numbers (for char/misc devices).
  unsigned int major = 0;
  unsigned int minor = 0;
};

// ============================================================
// VfsTree — a virtual filesystem tree (one per service).
// ============================================================

class VfsTree {
 public:
  explicit VfsTree(const std::string& root_name);

  // Create or get a directory node at the given path.
  VfsNode* MkdirP(const std::string& path);

  // Add a node under a parent path.
  VfsNode* AddNode(const std::string& parent_path,
                    const std::string& name,
                    VfsNodeType type);

  // Remove a node by path.
  void RemoveNode(const std::string& path);

  // Lookup a node by path.
  VfsNode* Lookup(const std::string& path) const;

  // Get the root node.
  VfsNode* root() { return &root_; }

  // List all nodes (for debugging / listing).
  std::vector<const VfsNode*> ListAll() const;

  // List children of a directory.
  std::vector<const VfsNode*> ListDir(const std::string& path) const;

 private:
  void CollectAll(const VfsNode* node,
                  std::vector<const VfsNode*>* out) const;

  VfsNode root_;
  mutable std::mutex mu_;
};

// ============================================================
// DevFs — /dev service
// ============================================================

class DevFs {
 public:
  static DevFs& Instance();

  // Called by misc_register().
  void RegisterMisc(const char* name, int minor,
                     const struct file_operations* fops,
                     struct device* this_device);
  void DeregisterMisc(const char* name);

  // Called by cdev_add().
  void RegisterChrdev(const char* name, unsigned int major,
                       unsigned int minor, unsigned int count,
                       const struct file_operations* fops);
  void DeregisterChrdev(const char* name);

  // File operations on a device node (dispatches to module's fops).
  int Open(const std::string& path);
  ssize_t Read(int fd, char* buf, size_t count);
  ssize_t Write(int fd, const char* buf, size_t count);
  long Ioctl(int fd, unsigned int cmd, unsigned long arg);
  int Close(int fd);

  // List all registered devices.
  std::vector<const VfsNode*> ListDevices() const;

  // Lookup a device by path.
  const VfsNode* Lookup(const std::string& path) const;

  VfsTree& tree() { return tree_; }

 private:
  DevFs();

  VfsTree tree_;

  // Open file descriptors (defined in vfs_service.cc, needs full struct defs).
  struct OpenFile;
  std::mutex fd_mu_;
  std::unordered_map<int, std::unique_ptr<OpenFile>> open_files_;
  int next_fd_ = 100;
};

// ============================================================
// SysFs — /sys service
// ============================================================

class SysFs {
 public:
  static SysFs& Instance();

  // Called by sysfs_create_group().
  void AddGroup(struct kobject* kobj, const struct attribute_group* grp);
  void RemoveGroup(struct kobject* kobj, const struct attribute_group* grp);

  // Called by device_create_file().
  void AddDeviceAttr(struct device* dev,
                      const struct device_attribute* attr);
  void RemoveDeviceAttr(struct device* dev,
                         const struct device_attribute* attr);

  // Called by kobject_add().
  void AddKobject(struct kobject* kobj, struct kobject* parent,
                   const char* name);
  void RemoveKobject(struct kobject* kobj);

  // Read a sysfs attribute (dispatches to show callback).
  ssize_t ReadAttr(const std::string& path, char* buf, size_t size);

  // Write a sysfs attribute (dispatches to store callback).
  ssize_t WriteAttr(const std::string& path, const char* buf, size_t count);

  // List all sysfs entries.
  std::vector<const VfsNode*> ListAll() const;

  VfsTree& tree() { return tree_; }

 private:
  SysFs();

  std::string KobjPath(struct kobject* kobj);

  VfsTree tree_;
};

// ============================================================
// ProcFs — /proc service
// ============================================================

class ProcFs {
 public:
  static ProcFs& Instance();

  // Called by proc_create_data().
  void AddEntry(const char* name, const char* parent_path,
                 unsigned short mode,
                 const struct file_operations* fops, void* data);

  // Called by proc_create_single_data().
  void AddSingleEntry(const char* name, const char* parent_path,
                       unsigned short mode,
                       int (*show)(struct seq_file*, void*), void* data);

  // Called by proc_mkdir().
  void AddDir(const char* name, const char* parent_path);

  // Called by proc_remove() / remove_proc_entry().
  void RemoveEntry(const char* path);

  // Read a proc entry (dispatches to fops->read or single_show).
  ssize_t Read(const std::string& path, char* buf, size_t size);

  // Write a proc entry (dispatches to fops->write).
  ssize_t Write(const std::string& path, const char* buf, size_t count);

  // List all proc entries.
  std::vector<const VfsNode*> ListAll() const;

  VfsTree& tree() { return tree_; }

 private:
  ProcFs();

  VfsTree tree_;
};

}  // namespace driverhub

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_VFS_SERVICE_H_
