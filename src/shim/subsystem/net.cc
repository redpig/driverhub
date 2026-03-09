// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/subsystem/net.h"
#include "src/shim/subsystem/cfg80211.h"  // For net_device, sk_buff definitions

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// sk_buff_head is not defined in cfg80211.h, define it here.
struct sk_buff_head {
  struct sk_buff *next;
  struct sk_buff *prev;
  unsigned int qlen;
  void *lock;
};

// All functions use opaque pointers. Internally we allocate appropriately
// sized buffers. The struct layouts in cfg80211.h or kernel headers don't
// need to match exactly — modules access these through our shim functions.

// Minimum net_device alloc size — covers all commonly accessed fields.
static constexpr size_t kNetDevBaseSize = 4096;

// ============================================================
// net_device allocation and registration
// ============================================================

extern "C" struct net_device *alloc_netdev_mqs(int sizeof_priv,
                                                const char *name,
                                                unsigned char name_assign_type,
                                                void (*setup)(struct net_device *),
                                                unsigned int txqs,
                                                unsigned int rxqs) {
  (void)name_assign_type;
  size_t alloc_size = kNetDevBaseSize + ((sizeof_priv + 31) & ~31);
  auto *dev = static_cast<struct net_device *>(calloc(1, alloc_size));
  if (!dev) return nullptr;

  // name is at offset 0, 16 bytes
  if (name) strncpy(dev->name, name, 15);

  // Set basic defaults through the setup callback.
  if (setup) setup(dev);

  fprintf(stderr, "driverhub: net: alloc_netdev_mqs(%s, priv=%d, txqs=%u, rxqs=%u)\n",
          dev->name, sizeof_priv, txqs, rxqs);
  return dev;
}

// register_netdev: provided by cfg80211.cc

// unregister_netdev: provided by cfg80211.cc

extern "C" int register_netdevice(struct net_device *dev) {
  return register_netdev(dev);
}

extern "C" void unregister_netdevice_queue(struct net_device *dev, void *head) {
  (void)head;
  unregister_netdev(dev);
}

extern "C" void unregister_netdevice_many(void *head) { (void)head; }

// free_netdev: provided by cfg80211.cc


extern "C" int dev_addr_mod(struct net_device *dev, unsigned int offset,
                             const void *addr, size_t len) {
  if (!dev || !addr) return -22;
  memcpy(dev->dev_addr + offset, addr, len > 6 ? 6 : len);
  return 0;
}

// ============================================================
// Carrier and queue state
// ============================================================

extern "C" void netif_carrier_on(struct net_device *dev) { (void)dev; }
extern "C" void netif_carrier_off(struct net_device *dev) { (void)dev; }
extern "C" void netif_tx_wake_queue(struct net_device *dev) { (void)dev; }
extern "C" void netif_tx_stop_queue(void *txq) { (void)txq; }
extern "C" void netif_tx_start_queue(void *txq) { (void)txq; }
extern "C" void netif_wake_queue(struct net_device *dev) { (void)dev; }
extern "C" void netif_device_attach(struct net_device *dev) { (void)dev; }
extern "C" void netif_device_detach(struct net_device *dev) { (void)dev; }

extern "C" void netif_set_tso_max_size(struct net_device *dev,
                                        unsigned int size) {
  (void)dev; (void)size;
}

extern "C" void netif_stacked_transfer_operstate(
    const struct net_device *rootdev, struct net_device *dev) {
  (void)rootdev; (void)dev;
}

extern "C" int netdev_upper_dev_link(struct net_device *dev,
                                      struct net_device *upper_dev,
                                      void *extack) {
  (void)dev; (void)upper_dev; (void)extack;
  return 0;
}

extern "C" void netdev_upper_dev_unlink(struct net_device *dev,
                                         struct net_device *upper_dev) {
  (void)dev; (void)upper_dev;
}

extern "C" void netdev_update_features(struct net_device *dev) { (void)dev; }

extern "C" int netdev_rx_handler_register(struct net_device *dev,
                                           void *rx_handler,
                                           void *rx_handler_data) {
  (void)dev; (void)rx_handler; (void)rx_handler_data;
  return 0;
}

extern "C" void netdev_rx_handler_unregister(struct net_device *dev) {
  (void)dev;
}

// ============================================================
// sk_buff allocation and manipulation
// ============================================================

// Internal sk_buff layout with head/data pointers and integer offsets.
struct skb_alloc {
  struct sk_buff hdr;     // Matches the opaque sk_buff pointer
  unsigned char *head_ptr;
  unsigned char *data_ptr;
  unsigned int tail_off;
  unsigned int end_off;
  unsigned int total_len;
  struct net_device *skb_dev;
  struct skb_alloc *skb_next;
  struct skb_alloc *skb_prev;
  unsigned short skb_protocol;
  unsigned char skb_cloned;
};

static struct sk_buff *alloc_skb_internal(unsigned int size) {
  auto *sa = static_cast<struct skb_alloc *>(calloc(1, sizeof(struct skb_alloc)));
  if (!sa) return nullptr;

  unsigned int alloc_size = (size + 63) & ~63;
  sa->head_ptr = static_cast<unsigned char *>(calloc(1, alloc_size));
  if (!sa->head_ptr) {
    free(sa);
    return nullptr;
  }
  sa->data_ptr = sa->head_ptr;
  sa->tail_off = 0;
  sa->end_off = alloc_size;
  sa->total_len = 0;

  // Also set the fields in the opaque sk_buff header so that modules
  // accessing ->data, ->len, ->head via the cfg80211.h layout work.
  sa->hdr.data = sa->data_ptr;
  sa->hdr.len = 0;
  sa->hdr.head = sa->head_ptr;

  return &sa->hdr;
}

static struct skb_alloc *skb_to_sa(struct sk_buff *skb) {
  return reinterpret_cast<struct skb_alloc *>(skb);
}

static void free_skb(struct sk_buff *skb) {
  if (!skb) return;
  auto *sa = skb_to_sa(skb);
  free(sa->head_ptr);
  free(sa);
}

extern "C" struct sk_buff *__alloc_skb(unsigned int size,
                                        unsigned int priority,
                                        int flags, int node) {
  (void)priority; (void)flags; (void)node;
  return alloc_skb_internal(size);
}

extern "C" struct sk_buff *alloc_skb_with_frags(unsigned long header_len,
                                                 unsigned long data_len,
                                                 int max_page_order,
                                                 int *errcode,
                                                 unsigned int gfp) {
  (void)max_page_order; (void)gfp;
  auto *skb = alloc_skb_internal(header_len + data_len);
  if (!skb && errcode) *errcode = -12;
  return skb;
}

extern "C" struct sk_buff *__netdev_alloc_skb(struct net_device *dev,
                                               unsigned int len,
                                               unsigned int gfp) {
  (void)gfp;
  auto *skb = alloc_skb_internal(len);
  if (skb) skb_to_sa(skb)->skb_dev = dev;
  return skb;
}

extern "C" void kfree_skb_reason(struct sk_buff *skb, unsigned int reason) {
  (void)reason;
  free_skb(skb);
}

extern "C" void kfree_skb_list_reason(struct sk_buff *segs,
                                       unsigned int reason) {
  while (segs) {
    auto *sa = skb_to_sa(segs);
    struct sk_buff *next = reinterpret_cast<struct sk_buff *>(sa->skb_next);
    kfree_skb_reason(segs, reason);
    segs = next;
  }
}

extern "C" void kfree_skb_partial(struct sk_buff *skb, int head_stolen) {
  (void)head_stolen;
  free_skb(skb);
}

extern "C" void consume_skb(struct sk_buff *skb) {
  free_skb(skb);
}

extern "C" void *skb_put(struct sk_buff *skb, unsigned int len) {
  if (!skb) return nullptr;
  auto *sa = skb_to_sa(skb);
  void *tmp = sa->head_ptr + sa->tail_off;
  sa->tail_off += len;
  sa->total_len += len;
  skb->len = sa->total_len;
  return tmp;
}

extern "C" void *skb_push(struct sk_buff *skb, unsigned int len) {
  if (!skb) return nullptr;
  auto *sa = skb_to_sa(skb);
  sa->data_ptr -= len;
  sa->total_len += len;
  skb->data = sa->data_ptr;
  skb->len = sa->total_len;
  return sa->data_ptr;
}

extern "C" void *skb_pull(struct sk_buff *skb, unsigned int len) {
  if (!skb) return nullptr;
  auto *sa = skb_to_sa(skb);
  sa->data_ptr += len;
  sa->total_len -= len;
  skb->data = sa->data_ptr;
  skb->len = sa->total_len;
  return sa->data_ptr;
}

extern "C" void skb_trim(struct sk_buff *skb, unsigned int len) {
  if (!skb) return;
  auto *sa = skb_to_sa(skb);
  if (sa->total_len > len) {
    sa->total_len = len;
    sa->tail_off = (unsigned int)(sa->data_ptr - sa->head_ptr) + len;
    skb->len = len;
  }
}

extern "C" void skb_reserve(struct sk_buff *skb, int len) {
  if (!skb) return;
  auto *sa = skb_to_sa(skb);
  sa->data_ptr += len;
  sa->tail_off += len;
  skb->data = sa->data_ptr;
}

extern "C" struct sk_buff *skb_clone(struct sk_buff *skb, unsigned int gfp) {
  (void)gfp;
  if (!skb) return nullptr;
  auto *sa = skb_to_sa(skb);
  auto *n = alloc_skb_internal(sa->end_off);
  if (!n) return nullptr;
  auto *na = skb_to_sa(n);
  memcpy(na->head_ptr, sa->head_ptr, sa->end_off);
  na->data_ptr = na->head_ptr + (sa->data_ptr - sa->head_ptr);
  na->tail_off = sa->tail_off;
  na->total_len = sa->total_len;
  na->skb_protocol = sa->skb_protocol;
  na->skb_dev = sa->skb_dev;
  na->skb_cloned = 1;
  n->data = na->data_ptr;
  n->len = na->total_len;
  return n;
}

extern "C" struct sk_buff *skb_copy_expand(const struct sk_buff *skb,
                                            int newheadroom, int newtailroom,
                                            unsigned int gfp) {
  (void)gfp;
  if (!skb) return nullptr;
  unsigned int newsize = newheadroom + skb->len + newtailroom;
  auto *n = alloc_skb_internal(newsize);
  if (!n) return nullptr;
  skb_reserve(n, newheadroom);
  memcpy(skb_put(n, skb->len), skb->data, skb->len);
  return n;
}

extern "C" int skb_copy_datagram_iter(const struct sk_buff *from, int offset,
                                       void *iter, int len) {
  (void)from; (void)offset; (void)iter; (void)len;
  return 0;
}

extern "C" void skb_queue_head_init(struct sk_buff_head *list) {
  if (list) memset(list, 0, sizeof(*list));
}

extern "C" void skb_queue_tail(struct sk_buff_head *list,
                                struct sk_buff *newsk) {
  (void)list; (void)newsk;
  // Simplified — just track count
}

extern "C" struct sk_buff *skb_dequeue(struct sk_buff_head *list) {
  (void)list;
  return nullptr;
}

extern "C" void skb_queue_purge_reason(struct sk_buff_head *list,
                                        unsigned int reason) {
  (void)list; (void)reason;
}

extern "C" int pskb_expand_head(struct sk_buff *skb, int nhead, int ntail,
                                 unsigned int gfp) {
  (void)gfp;
  if (!skb) return -22;
  auto *sa = skb_to_sa(skb);
  unsigned int new_size = nhead + sa->end_off + ntail;
  auto *new_head = static_cast<unsigned char *>(calloc(1, new_size));
  if (!new_head) return -12;
  unsigned int data_off = (unsigned int)(sa->data_ptr - sa->head_ptr);
  memcpy(new_head + nhead, sa->head_ptr, sa->end_off);
  free(sa->head_ptr);
  sa->head_ptr = new_head;
  sa->data_ptr = new_head + nhead + data_off;
  sa->tail_off += nhead;
  sa->end_off = new_size;
  skb->head = new_head;
  skb->data = sa->data_ptr;
  return 0;
}

// ============================================================
// Receive path
// ============================================================

extern "C" int netif_rx(struct sk_buff *skb) {
  consume_skb(skb);
  return 0;
}

extern "C" int netif_receive_skb(struct sk_buff *skb) {
  return netif_rx(skb);
}

// ============================================================
// Ethernet helpers
// ============================================================

extern "C" unsigned short eth_type_trans(struct sk_buff *skb,
                                          struct net_device *dev) {
  if (skb) {
    skb_to_sa(skb)->skb_dev = dev;
    skb_to_sa(skb)->skb_protocol = 0x0800;
    skb_pull(skb, 14);  // ETH_HLEN
  }
  return 0x0800;
}

extern "C" void ether_setup(struct net_device *dev) {
  if (!dev) return;
  dev->mtu = 1500;
  dev->flags = 0x1103;  // IFF_UP | IFF_BROADCAST | IFF_MULTICAST
}

extern "C" int ethtool_op_get_link(struct net_device *dev) {
  (void)dev;
  return 1;  // Link up
}

extern "C" int ethtool_op_get_ts_info(struct net_device *dev, void *info) {
  (void)dev; (void)info;
  return 0;
}

extern "C" int ethtool_convert_legacy_u32_to_link_mode(void *dst,
                                                        unsigned int legacy_u32) {
  (void)dst; (void)legacy_u32;
  return 0;
}

extern "C" int ethtool_convert_link_mode_to_legacy_u32(unsigned int *legacy_u32,
                                                        const void *src) {
  if (legacy_u32) *legacy_u32 = 0;
  (void)src;
  return 0;
}

extern "C" int __ethtool_get_link_ksettings(struct net_device *dev,
                                             void *cmd) {
  (void)dev; (void)cmd;
  return 0;
}

// ============================================================
// NAPI
// ============================================================

extern "C" void netif_napi_add_weight(struct net_device *dev,
                                       struct napi_struct *napi,
                                       void *poll, int weight) {
  (void)dev; (void)napi; (void)poll; (void)weight;
}

extern "C" void napi_enable(struct napi_struct *n) { (void)n; }
extern "C" void napi_disable(struct napi_struct *n) { (void)n; }

extern "C" int napi_schedule_prep(struct napi_struct *n) {
  (void)n;
  return 1;
}

extern "C" int napi_complete_done(struct napi_struct *n, int work_done) {
  (void)n; (void)work_done;
  return 1;
}

extern "C" int napi_gro_receive(struct napi_struct *napi,
                                 struct sk_buff *skb) {
  (void)napi;
  consume_skb(skb);
  return 0;
}

// ============================================================
// TX path
// ============================================================

extern "C" int __dev_queue_xmit(struct sk_buff *skb, void *accel_priv) {
  (void)accel_priv;
  consume_skb(skb);
  return 0;
}

extern "C" int dev_kfree_skb_any_reason(struct sk_buff *skb,
                                         unsigned int reason) {
  (void)reason;
  free_skb(skb);
  return 0;
}

extern "C" int dev_kfree_skb_irq_reason(struct sk_buff *skb,
                                         unsigned int reason) {
  (void)reason;
  free_skb(skb);
  return 0;
}

// ============================================================
// Socket / protocol
// ============================================================

extern "C" int proto_register(struct proto *prot, int alloc_slab) {
  (void)alloc_slab;
  if (prot) {
    fprintf(stderr, "driverhub: net: proto_register(%s)\n",
            reinterpret_cast<char *>(prot));  // name is at offset 0
  }
  return 0;
}

extern "C" void proto_unregister(struct proto *prot) {
  if (prot) {
    fprintf(stderr, "driverhub: net: proto_unregister(%s)\n",
            reinterpret_cast<char *>(prot));
  }
}

extern "C" void sock_init_data(void *sock, void *sk) {
  (void)sock; (void)sk;
}

extern "C" int sock_register(const void *ops) { (void)ops; return 0; }
extern "C" void sock_unregister(int family) { (void)family; }

extern "C" int sock_queue_rcv_skb_reason(void *sk, struct sk_buff *skb,
                                          unsigned int *reason) {
  (void)sk; (void)skb; (void)reason;
  return 0;
}

extern "C" void *sock_alloc_send_pskb(void *sk, unsigned long header_len,
                                       unsigned long data_len, int noblock,
                                       int *errcode, int max_page_order) {
  (void)sk; (void)noblock; (void)max_page_order;
  auto *skb = alloc_skb_internal(header_len + data_len);
  if (!skb && errcode) *errcode = -12;
  return skb;
}

extern "C" void sk_free(void *sk) { free(sk); }

extern "C" void *sk_alloc(void *net, int family, unsigned int priority,
                           struct proto *prot, int kern) {
  (void)net; (void)family; (void)priority; (void)kern;
  // proto->obj_size is at a known offset; use 512 as safe default
  size_t size = 512;
  return calloc(1, size);
}

extern "C" void sk_error_report(void *sk) { (void)sk; }

extern "C" int sock_no_mmap(void *file, void *sock, void *vma) {
  (void)file; (void)sock; (void)vma;
  return -95;
}

extern "C" int sock_no_socketpair(void *sock1, void *sock2) {
  (void)sock1; (void)sock2;
  return -95;
}

extern "C" int sock_no_accept(void *sock, void *newsock, int flags, int kern) {
  (void)sock; (void)newsock; (void)flags; (void)kern;
  return -95;
}

extern "C" int sock_no_listen(void *sock, int backlog) {
  (void)sock; (void)backlog;
  return -95;
}

extern "C" int sock_no_shutdown(void *sock, int how) {
  (void)sock; (void)how;
  return -95;
}

// ============================================================
// Per-net namespace
// ============================================================

extern "C" int register_pernet_subsys(void *ops) { (void)ops; return 0; }
extern "C" void unregister_pernet_subsys(void *ops) { (void)ops; }
extern "C" int register_pernet_device(void *ops) { (void)ops; return 0; }
extern "C" void unregister_pernet_device(void *ops) { (void)ops; }

// ============================================================
// RTNL lock
// ============================================================

extern "C" void rtnl_lock(void) {}
extern "C" void rtnl_unlock(void) {}
extern "C" int rtnl_is_locked(void) { return 0; }

extern "C" int rtnl_link_register(const void *ops) { (void)ops; return 0; }
extern "C" void rtnl_link_unregister(const void *ops) { (void)ops; }

// ============================================================
// Netlink / genetlink
// ============================================================

extern "C" int genl_register_family(void *family) { (void)family; return 0; }
extern "C" int genl_unregister_family(const void *family) {
  (void)family;
  return 0;
}

extern "C" int netlink_unicast(void *ssk, struct sk_buff *skb,
                                unsigned int portid, int nonblock) {
  (void)ssk; (void)portid; (void)nonblock;
  consume_skb(skb);
  return 0;
}

extern "C" int netlink_broadcast(void *ssk, struct sk_buff *skb,
                                  unsigned int portid, unsigned int group,
                                  unsigned int allocation) {
  (void)ssk; (void)portid; (void)group; (void)allocation;
  consume_skb(skb);
  return 0;
}

extern "C" int nla_put(struct sk_buff *skb, int attrtype, int attrlen,
                        const void *data) {
  (void)skb; (void)attrtype; (void)attrlen; (void)data;
  return 0;
}

extern "C" int nla_put_64bit(struct sk_buff *skb, int attrtype, int attrlen,
                              const void *data, int padattr) {
  (void)skb; (void)attrtype; (void)attrlen; (void)data; (void)padattr;
  return 0;
}

extern "C" int nla_memcpy(void *dest, const void *nla, int count) {
  (void)dest; (void)nla; (void)count;
  return 0;
}

extern "C" int nla_strscpy(char *dst, const void *nla, size_t dstsize) {
  if (dst && dstsize > 0) dst[0] = '\0';
  (void)nla;
  return 0;
}

extern "C" void *genlmsg_put(struct sk_buff *skb, unsigned int portid,
                              unsigned int seq, const void *family,
                              int flags, unsigned char cmd) {
  (void)portid; (void)seq; (void)family; (void)flags; (void)cmd;
  if (!skb) return nullptr;
  return skb_put(skb, 16);
}

// ============================================================
// Notifier chain
// ============================================================

extern "C" int register_netdevice_notifier(void *nb) { (void)nb; return 0; }
extern "C" int unregister_netdevice_notifier(void *nb) { (void)nb; return 0; }

// ============================================================
// Device lookup
// ============================================================

extern "C" struct net_device *__dev_get_by_index(void *net, int ifindex) {
  (void)net; (void)ifindex;
  return nullptr;
}

extern "C" struct net_device *__dev_get_by_name(void *net, const char *name) {
  (void)net; (void)name;
  return nullptr;
}

extern "C" int __dev_change_net_namespace(struct net_device *dev, void *net,
                                           const char *pat) {
  (void)dev; (void)net; (void)pat;
  return 0;
}

extern "C" void dev_add_pack(void *pt) { (void)pt; }
extern "C" void dev_remove_pack(void *pt) { (void)pt; }
extern "C" void dev_uc_del(struct net_device *dev, const void *addr) {
  (void)dev; (void)addr;
}

extern "C" void dev_get_tstats64(struct net_device *dev, void *s) {
  if (s) memset(s, 0, 184);  // sizeof(struct rtnl_link_stats64)
  (void)dev;
}

extern "C" void dst_release(void *dst) { (void)dst; }

// ============================================================
// Print helpers
// ============================================================

extern "C" void netdev_warn(const struct net_device *dev, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "driverhub: net[%s]: WARN: ", dev ? dev->name : "?");
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

extern "C" void netdev_info(const struct net_device *dev, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "driverhub: net[%s]: ", dev ? dev->name : "?");
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

extern "C" void netdev_err(const struct net_device *dev, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "driverhub: net[%s]: ERR: ", dev ? dev->name : "?");
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

extern "C" void netdev_notice(const struct net_device *dev, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "driverhub: net[%s]: ", dev ? dev->name : "?");
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

extern "C" void netdev_printk(const char *level, const struct net_device *dev,
                               const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "driverhub: net[%s]: ", dev ? dev->name : "?");
  (void)level;
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}
