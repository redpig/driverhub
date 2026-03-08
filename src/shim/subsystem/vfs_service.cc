// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/subsystem/vfs_service.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "src/shim/subsystem/fs.h"

namespace driverhub {

// Full definition of OpenFile (needs struct file/inode from fs.h).
struct DevFs::OpenFile {
  const VfsNode* node;
  struct file file_obj;
  struct inode inode_obj;
};

// ============================================================
// VfsTree
// ============================================================

VfsTree::VfsTree(const std::string& root_name) {
  root_.name = root_name;
  root_.path = "/" + root_name;
  root_.type = VfsNodeType::kDirectory;
  root_.mode = S_IRUGO | S_IXUSR;
}

VfsNode* VfsTree::MkdirP(const std::string& path) {
  std::lock_guard<std::mutex> lock(mu_);

  // Split path on '/'.
  std::istringstream iss(path);
  std::string component;
  VfsNode* current = &root_;

  while (std::getline(iss, component, '/')) {
    if (component.empty()) continue;
    auto it = current->children.find(component);
    if (it == current->children.end()) {
      auto node = std::make_unique<VfsNode>();
      node->name = component;
      node->path = current->path + "/" + component;
      node->type = VfsNodeType::kDirectory;
      node->mode = S_IRUGO | S_IXUSR;
      node->parent = current;
      auto* raw = node.get();
      current->children[component] = std::move(node);
      current = raw;
    } else {
      current = it->second.get();
    }
  }
  return current;
}

VfsNode* VfsTree::AddNode(const std::string& parent_path,
                           const std::string& name,
                           VfsNodeType type) {
  std::lock_guard<std::mutex> lock(mu_);

  // Find parent (walk from root).
  VfsNode* parent = &root_;
  if (!parent_path.empty()) {
    std::istringstream iss(parent_path);
    std::string component;
    while (std::getline(iss, component, '/')) {
      if (component.empty()) continue;
      auto it = parent->children.find(component);
      if (it == parent->children.end()) {
        // Auto-create intermediate directories.
        auto dir = std::make_unique<VfsNode>();
        dir->name = component;
        dir->path = parent->path + "/" + component;
        dir->type = VfsNodeType::kDirectory;
        dir->mode = S_IRUGO | S_IXUSR;
        dir->parent = parent;
        auto* raw = dir.get();
        parent->children[component] = std::move(dir);
        parent = raw;
      } else {
        parent = it->second.get();
      }
    }
  }

  auto node = std::make_unique<VfsNode>();
  node->name = name;
  node->path = parent->path + "/" + name;
  node->type = type;
  node->parent = parent;
  auto* raw = node.get();
  parent->children[name] = std::move(node);
  return raw;
}

void VfsTree::RemoveNode(const std::string& path) {
  std::lock_guard<std::mutex> lock(mu_);

  // Walk to find the node's parent, then erase.
  VfsNode* current = &root_;
  std::string last_component;
  std::istringstream iss(path);
  std::string component;
  VfsNode* parent = nullptr;

  while (std::getline(iss, component, '/')) {
    if (component.empty()) continue;
    parent = current;
    last_component = component;
    auto it = current->children.find(component);
    if (it == current->children.end()) return;  // Not found.
    current = it->second.get();
  }

  if (parent && !last_component.empty()) {
    parent->children.erase(last_component);
  }
}

VfsNode* VfsTree::Lookup(const std::string& path) const {
  std::lock_guard<std::mutex> lock(mu_);

  VfsNode* current = const_cast<VfsNode*>(&root_);
  std::istringstream iss(path);
  std::string component;

  while (std::getline(iss, component, '/')) {
    if (component.empty()) continue;
    auto it = current->children.find(component);
    if (it == current->children.end()) return nullptr;
    current = it->second.get();
  }
  return current;
}

std::vector<const VfsNode*> VfsTree::ListAll() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<const VfsNode*> result;
  CollectAll(&root_, &result);
  return result;
}

std::vector<const VfsNode*> VfsTree::ListDir(const std::string& path) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<const VfsNode*> result;

  // Inline path lookup (can't call Lookup which also takes mu_).
  const VfsNode* dir = &root_;
  if (!path.empty()) {
    std::istringstream iss(path);
    std::string component;
    while (std::getline(iss, component, '/')) {
      if (component.empty()) continue;
      auto it = dir->children.find(component);
      if (it == dir->children.end()) { dir = nullptr; break; }
      dir = it->second.get();
    }
  }
  if (!dir) dir = &root_;
  for (const auto& [name, child] : dir->children) {
    result.push_back(child.get());
  }
  return result;
}

void VfsTree::CollectAll(const VfsNode* node,
                          std::vector<const VfsNode*>* out) const {
  out->push_back(node);
  for (const auto& [name, child] : node->children) {
    CollectAll(child.get(), out);
  }
}

// ============================================================
// DevFs
// ============================================================

DevFs::DevFs() : tree_("dev") {}

DevFs& DevFs::Instance() {
  static DevFs instance;
  return instance;
}

void DevFs::RegisterMisc(const char* name, int minor,
                          const struct file_operations* fops,
                          struct device* this_device) {
  auto* node = tree_.AddNode("", name ? name : "misc",
                              VfsNodeType::kMiscDevice);
  node->fops = fops;
  node->major = 10;  // Linux misc major.
  node->minor = static_cast<unsigned int>(minor);
  node->mode = S_IRUGO | S_IWUGO;
  node->private_data = this_device;

  fprintf(stderr, "driverhub: devfs: added /dev/%s (misc, minor=%d)\n",
          name ? name : "misc", minor);
}

void DevFs::DeregisterMisc(const char* name) {
  if (!name) return;
  tree_.RemoveNode(std::string(name));
  fprintf(stderr, "driverhub: devfs: removed /dev/%s\n", name);
}

void DevFs::RegisterChrdev(const char* name, unsigned int major,
                            unsigned int minor, unsigned int count,
                            const struct file_operations* fops) {
  auto* node = tree_.AddNode("", name ? name : "chrdev",
                              VfsNodeType::kCharDevice);
  node->fops = fops;
  node->major = major;
  node->minor = minor;
  node->mode = S_IRUGO | S_IWUGO;

  fprintf(stderr, "driverhub: devfs: added /dev/%s (chrdev %u:%u)\n",
          name ? name : "chrdev", major, minor);
}

void DevFs::DeregisterChrdev(const char* name) {
  if (!name) return;
  tree_.RemoveNode(std::string(name));
  fprintf(stderr, "driverhub: devfs: removed /dev/%s\n", name);
}

int DevFs::Open(const std::string& path) {
  auto* node = tree_.Lookup(path);
  if (!node) return -2;  // -ENOENT

  std::lock_guard<std::mutex> lock(fd_mu_);
  int fd = next_fd_++;

  auto of = std::make_unique<OpenFile>();
  of->node = node;
  memset(&of->file_obj, 0, sizeof(of->file_obj));
  memset(&of->inode_obj, 0, sizeof(of->inode_obj));
  of->file_obj.f_op = node->fops;
  of->file_obj.private_data = nullptr;
  of->inode_obj.i_rdev = (node->major << 20) | node->minor;
  of->inode_obj.i_private = node->private_data;

  // Call the module's open callback.
  if (node->fops && node->fops->open) {
    int ret = node->fops->open(&of->inode_obj, &of->file_obj);
    if (ret != 0) return ret;
  }

  open_files_[fd] = std::move(of);
  return fd;
}

ssize_t DevFs::Read(int fd, char* buf, size_t count) {
  std::lock_guard<std::mutex> lock(fd_mu_);
  auto it = open_files_.find(fd);
  if (it == open_files_.end()) return -9;  // -EBADF

  auto* of = it->second.get();
  if (!of->node->fops || !of->node->fops->read) return -22;  // -EINVAL

  return of->node->fops->read(&of->file_obj, buf, count, &of->file_obj.f_pos);
}

ssize_t DevFs::Write(int fd, const char* buf, size_t count) {
  std::lock_guard<std::mutex> lock(fd_mu_);
  auto it = open_files_.find(fd);
  if (it == open_files_.end()) return -9;

  auto* of = it->second.get();
  if (!of->node->fops || !of->node->fops->write) return -22;

  return of->node->fops->write(&of->file_obj, buf, count, &of->file_obj.f_pos);
}

long DevFs::Ioctl(int fd, unsigned int cmd, unsigned long arg) {
  std::lock_guard<std::mutex> lock(fd_mu_);
  auto it = open_files_.find(fd);
  if (it == open_files_.end()) return -9;

  auto* of = it->second.get();
  if (!of->node->fops || !of->node->fops->unlocked_ioctl) return -25;  // -ENOTTY

  return of->node->fops->unlocked_ioctl(&of->file_obj, cmd, arg);
}

int DevFs::Close(int fd) {
  std::lock_guard<std::mutex> lock(fd_mu_);
  auto it = open_files_.find(fd);
  if (it == open_files_.end()) return -9;

  auto* of = it->second.get();
  if (of->node->fops && of->node->fops->release) {
    of->node->fops->release(&of->inode_obj, &of->file_obj);
  }

  open_files_.erase(it);
  return 0;
}

std::vector<const VfsNode*> DevFs::ListDevices() const {
  return tree_.ListAll();
}

const VfsNode* DevFs::Lookup(const std::string& path) const {
  return tree_.Lookup(path);
}

// ============================================================
// SysFs
// ============================================================

SysFs::SysFs() : tree_("sys") {}

SysFs& SysFs::Instance() {
  static SysFs instance;
  return instance;
}

std::string SysFs::KobjPath(struct kobject* kobj) {
  if (!kobj) return "";
  std::string path;
  // Walk up the parent chain to build path.
  std::vector<std::string> parts;
  for (auto* k = kobj; k; k = k->parent) {
    if (k->name) parts.push_back(k->name);
  }
  for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
    if (!path.empty()) path += "/";
    path += *it;
  }
  return path;
}

void SysFs::AddGroup(struct kobject* kobj,
                      const struct attribute_group* grp) {
  std::string base = KobjPath(kobj);
  std::string dir_path = base;
  if (grp->name) {
    dir_path += "/";
    dir_path += grp->name;
    tree_.MkdirP(dir_path);
  }

  if (grp->attrs) {
    for (struct attribute** ap = grp->attrs; *ap; ap++) {
      auto* node = tree_.AddNode(dir_path, (*ap)->name,
                                  VfsNodeType::kSysfsAttr);
      node->mode = (*ap)->mode;
      fprintf(stderr, "driverhub: sysfs: added %s\n", node->path.c_str());
    }
  }
}

void SysFs::RemoveGroup(struct kobject* kobj,
                         const struct attribute_group* grp) {
  std::string base = KobjPath(kobj);
  if (grp->name) {
    tree_.RemoveNode(base + "/" + grp->name);
  }
}

void SysFs::AddDeviceAttr(struct device* dev,
                           const struct device_attribute* attr) {
  // Build path from device name.
  std::string dev_name = dev && dev->init_name ? dev->init_name : "unknown";
  std::string dir_path = "devices/" + dev_name;
  tree_.MkdirP(dir_path);

  auto* node = tree_.AddNode(dir_path, attr->attr.name,
                              VfsNodeType::kSysfsAttr);
  node->dev_attr = attr;
  node->dev = dev;
  node->mode = attr->attr.mode;

  fprintf(stderr, "driverhub: sysfs: added %s\n", node->path.c_str());
}

void SysFs::RemoveDeviceAttr(struct device* dev,
                              const struct device_attribute* attr) {
  std::string dev_name = dev && dev->init_name ? dev->init_name : "unknown";
  tree_.RemoveNode("devices/" + dev_name + "/" + attr->attr.name);
}

void SysFs::AddKobject(struct kobject* kobj, struct kobject* parent,
                        const char* name) {
  std::string parent_path = parent ? KobjPath(parent) : "";
  tree_.MkdirP(parent_path.empty() ? std::string(name)
                                     : parent_path + "/" + name);
}

void SysFs::RemoveKobject(struct kobject* kobj) {
  tree_.RemoveNode(KobjPath(kobj));
}

ssize_t SysFs::ReadAttr(const std::string& path, char* buf, size_t size) {
  auto* node = tree_.Lookup(path);
  if (!node) return -2;
  if (node->type != VfsNodeType::kSysfsAttr) return -21;  // -EISDIR

  if (node->dev_attr && node->dev_attr->show) {
    return node->dev_attr->show(node->dev,
                                 const_cast<struct device_attribute*>(
                                     node->dev_attr),
                                 buf);
  }
  return -22;
}

ssize_t SysFs::WriteAttr(const std::string& path,
                          const char* buf, size_t count) {
  auto* node = tree_.Lookup(path);
  if (!node) return -2;
  if (node->type != VfsNodeType::kSysfsAttr) return -21;

  if (node->dev_attr && node->dev_attr->store) {
    return node->dev_attr->store(node->dev,
                                  const_cast<struct device_attribute*>(
                                      node->dev_attr),
                                  buf, count);
  }
  return -1;  // -EPERM (read-only attribute)
}

std::vector<const VfsNode*> SysFs::ListAll() const {
  return tree_.ListAll();
}

// ============================================================
// ProcFs
// ============================================================

ProcFs::ProcFs() : tree_("proc") {}

ProcFs& ProcFs::Instance() {
  static ProcFs instance;
  return instance;
}

void ProcFs::AddEntry(const char* name, const char* parent_path,
                       unsigned short mode,
                       const struct file_operations* fops, void* data) {
  std::string pp = parent_path ? parent_path : "";
  auto* node = tree_.AddNode(pp, name ? name : "entry",
                              VfsNodeType::kProcEntry);
  node->fops = fops;
  node->private_data = data;
  node->mode = mode;

  fprintf(stderr, "driverhub: procfs: added %s\n", node->path.c_str());
}

void ProcFs::AddSingleEntry(const char* name, const char* parent_path,
                              unsigned short mode,
                              int (*show)(struct seq_file*, void*),
                              void* data) {
  std::string pp = parent_path ? parent_path : "";
  auto* node = tree_.AddNode(pp, name ? name : "entry",
                              VfsNodeType::kProcEntry);
  node->single_show = show;
  node->private_data = data;
  node->mode = mode;

  fprintf(stderr, "driverhub: procfs: added %s (single)\n",
          node->path.c_str());
}

void ProcFs::AddDir(const char* name, const char* parent_path) {
  std::string pp = parent_path ? parent_path : "";
  std::string full = pp.empty() ? std::string(name) : pp + "/" + name;
  tree_.MkdirP(full);

  fprintf(stderr, "driverhub: procfs: mkdir %s\n", full.c_str());
}

void ProcFs::RemoveEntry(const char* path) {
  if (!path) return;
  tree_.RemoveNode(path);
  fprintf(stderr, "driverhub: procfs: removed %s\n", path);
}

ssize_t ProcFs::Read(const std::string& path, char* buf, size_t size) {
  auto* node = tree_.Lookup(path);
  if (!node) return -2;

  // Single-show proc entries: call show into a seq_file buffer.
  if (node->single_show) {
    struct seq_file sf;
    memset(&sf, 0, sizeof(sf));
    sf.buf = static_cast<char*>(malloc(4096));
    sf.size = 4096;
    sf.count = 0;
    sf.priv = node->private_data;

    node->single_show(&sf, node->private_data);

    ssize_t to_copy = sf.count < size ? sf.count : size;
    memcpy(buf, sf.buf, to_copy);
    free(sf.buf);
    return to_copy;
  }

  // File-operations-based entries.
  if (node->fops && node->fops->read) {
    struct file f;
    memset(&f, 0, sizeof(f));
    f.f_op = node->fops;
    f.private_data = node->private_data;

    // If the entry uses seq_file, do single_open + seq_read.
    if (node->fops->open) {
      struct inode ino;
      memset(&ino, 0, sizeof(ino));
      node->fops->open(&ino, &f);
    }

    loff_t pos = 0;
    ssize_t ret = node->fops->read(&f, buf, size, &pos);

    if (node->fops->release) {
      struct inode ino;
      memset(&ino, 0, sizeof(ino));
      node->fops->release(&ino, &f);
    }

    return ret;
  }

  return -22;
}

ssize_t ProcFs::Write(const std::string& path,
                       const char* buf, size_t count) {
  auto* node = tree_.Lookup(path);
  if (!node) return -2;

  if (node->fops && node->fops->write) {
    struct file f;
    memset(&f, 0, sizeof(f));
    f.f_op = node->fops;
    f.private_data = node->private_data;

    loff_t pos = 0;
    return node->fops->write(&f, buf, count, &pos);
  }

  return -1;
}

std::vector<const VfsNode*> ProcFs::ListAll() const {
  return tree_.ListAll();
}

}  // namespace driverhub
