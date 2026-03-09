// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_NET_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_NET_H_

// KMI shims for the Linux networking subsystem APIs.
//
// Provides: alloc_netdev_mqs, register_netdev, unregister_netdev,
//           free_netdev, __alloc_skb, kfree_skb_reason, consume_skb,
//           skb_put, skb_pull, skb_push, skb_trim, skb_clone,
//           netif_rx, netif_carrier_on/off, napi_*, ethtool_*,
//           sock_init_data, proto_register/unregister, etc.
//
// On Fuchsia: maps to fuchsia.hardware.network FIDL protocol.
//
// Note: struct net_device and struct sk_buff are defined in cfg80211.h
// to avoid redefinition conflicts. This header uses forward declarations.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations — full definitions in cfg80211.h or net.cc.
struct net_device;
struct sk_buff;
struct sk_buff_head;
struct napi_struct;
struct proto;
struct ethtool_ops;

// ------------------------------------------------------------
// Network device API
// ------------------------------------------------------------

struct net_device *alloc_netdev_mqs(int sizeof_priv, const char *name,
                                     unsigned char name_assign_type,
                                     void (*setup)(struct net_device *),
                                     unsigned int txqs,
                                     unsigned int rxqs);
int register_netdev(struct net_device *dev);
void unregister_netdev(struct net_device *dev);
int register_netdevice(struct net_device *dev);
void unregister_netdevice_queue(struct net_device *dev, void *head);
void unregister_netdevice_many(void *head);
void free_netdev(struct net_device *dev);
// netdev_priv: defined as static inline in cfg80211.h

int dev_addr_mod(struct net_device *dev, unsigned int offset,
                 const void *addr, size_t len);

void netif_carrier_on(struct net_device *dev);
void netif_carrier_off(struct net_device *dev);
void netif_tx_wake_queue(struct net_device *dev);
void netif_wake_queue(struct net_device *dev);
int netif_rx(struct sk_buff *skb);
int netif_receive_skb(struct sk_buff *skb);

void netif_napi_add_weight(struct net_device *dev, struct napi_struct *napi,
                            void *poll, int weight);
void napi_enable(struct napi_struct *n);
void napi_disable(struct napi_struct *n);
int napi_schedule_prep(struct napi_struct *n);
int napi_complete_done(struct napi_struct *n, int work_done);
int napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb);

void netif_set_tso_max_size(struct net_device *dev, unsigned int size);
void netif_device_attach(struct net_device *dev);
void netif_device_detach(struct net_device *dev);
void netif_stacked_transfer_operstate(const struct net_device *rootdev,
                                       struct net_device *dev);

int netdev_upper_dev_link(struct net_device *dev,
                           struct net_device *upper_dev, void *extack);
void netdev_upper_dev_unlink(struct net_device *dev,
                              struct net_device *upper_dev);
void netdev_update_features(struct net_device *dev);

int netdev_rx_handler_register(struct net_device *dev, void *rx_handler,
                                void *rx_handler_data);
void netdev_rx_handler_unregister(struct net_device *dev);

void netdev_warn(const struct net_device *dev, const char *fmt, ...);
void netdev_info(const struct net_device *dev, const char *fmt, ...);
void netdev_err(const struct net_device *dev, const char *fmt, ...);
void netdev_notice(const struct net_device *dev, const char *fmt, ...);
void netdev_printk(const char *level, const struct net_device *dev,
                    const char *fmt, ...);

void dev_get_tstats64(struct net_device *dev, void *s);

// ------------------------------------------------------------
// sk_buff API
// ------------------------------------------------------------

struct sk_buff *__alloc_skb(unsigned int size, unsigned int priority,
                             int flags, int node);
struct sk_buff *alloc_skb_with_frags(unsigned long header_len,
                                      unsigned long data_len,
                                      int max_page_order,
                                      int *errcode, unsigned int gfp);
struct sk_buff *__netdev_alloc_skb(struct net_device *dev,
                                    unsigned int len, unsigned int gfp);

void kfree_skb_reason(struct sk_buff *skb, unsigned int reason);
void kfree_skb_list_reason(struct sk_buff *segs, unsigned int reason);
void kfree_skb_partial(struct sk_buff *skb, int head_stolen);
void consume_skb(struct sk_buff *skb);

void *skb_put(struct sk_buff *skb, unsigned int len);
void *skb_push(struct sk_buff *skb, unsigned int len);
void *skb_pull(struct sk_buff *skb, unsigned int len);
void skb_trim(struct sk_buff *skb, unsigned int len);
void skb_reserve(struct sk_buff *skb, int len);

struct sk_buff *skb_clone(struct sk_buff *skb, unsigned int gfp);
struct sk_buff *skb_copy_expand(const struct sk_buff *skb,
                                 int newheadroom, int newtailroom,
                                 unsigned int gfp);
int skb_copy_datagram_iter(const struct sk_buff *from, int offset,
                            void *iter, int len);

void skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk);
struct sk_buff *skb_dequeue(struct sk_buff_head *list);
void skb_queue_purge_reason(struct sk_buff_head *list, unsigned int reason);
void skb_queue_head_init(struct sk_buff_head *list);

int pskb_expand_head(struct sk_buff *skb, int nhead, int ntail,
                      unsigned int gfp);

unsigned short eth_type_trans(struct sk_buff *skb, struct net_device *dev);
void ether_setup(struct net_device *dev);

int ethtool_op_get_link(struct net_device *dev);
int ethtool_op_get_ts_info(struct net_device *dev, void *info);
int __ethtool_get_link_ksettings(struct net_device *dev, void *cmd);
int ethtool_convert_legacy_u32_to_link_mode(void *dst,
                                             unsigned int legacy_u32);
int ethtool_convert_link_mode_to_legacy_u32(unsigned int *legacy_u32,
                                             const void *src);

int __dev_queue_xmit(struct sk_buff *skb, void *accel_priv);
int dev_kfree_skb_any_reason(struct sk_buff *skb, unsigned int reason);
int dev_kfree_skb_irq_reason(struct sk_buff *skb, unsigned int reason);

// ------------------------------------------------------------
// Socket / protocol
// ------------------------------------------------------------

int proto_register(struct proto *prot, int alloc_slab);
void proto_unregister(struct proto *prot);

void sock_init_data(void *sock, void *sk);
int sock_register(const void *ops);
void sock_unregister(int family);
int sock_queue_rcv_skb_reason(void *sk, struct sk_buff *skb,
                               unsigned int *reason);
void *sock_alloc_send_pskb(void *sk, unsigned long header_len,
                            unsigned long data_len, int noblock,
                            int *errcode, int max_page_order);

void sk_free(void *sk);
void *sk_alloc(void *net, int family, unsigned int priority,
               struct proto *prot, int kern);
void sk_error_report(void *sk);

int sock_no_mmap(void *file, void *sock, void *vma);
int sock_no_socketpair(void *sock1, void *sock2);
int sock_no_accept(void *sock, void *newsock, int flags, int kern);
int sock_no_listen(void *sock, int backlog);
int sock_no_shutdown(void *sock, int how);

int register_pernet_subsys(void *ops);
void unregister_pernet_subsys(void *ops);
int register_pernet_device(void *ops);
void unregister_pernet_device(void *ops);

void rtnl_lock(void);
void rtnl_unlock(void);
int rtnl_is_locked(void);
int rtnl_link_register(const void *ops);
void rtnl_link_unregister(const void *ops);

int genl_register_family(void *family);
int genl_unregister_family(const void *family);
int netlink_unicast(void *ssk, struct sk_buff *skb, unsigned int portid,
                     int nonblock);
int netlink_broadcast(void *ssk, struct sk_buff *skb, unsigned int portid,
                       unsigned int group, unsigned int allocation);
int nla_put(struct sk_buff *skb, int attrtype, int attrlen, const void *data);
int nla_put_64bit(struct sk_buff *skb, int attrtype, int attrlen,
                   const void *data, int padattr);
int nla_memcpy(void *dest, const void *nla, int count);
int nla_strscpy(char *dst, const void *nla, size_t dstsize);
void *genlmsg_put(struct sk_buff *skb, unsigned int portid, unsigned int seq,
                   const void *family, int flags, unsigned char cmd);

int register_netdevice_notifier(void *nb);
int unregister_netdevice_notifier(void *nb);

struct net_device *__dev_get_by_index(void *net, int ifindex);
struct net_device *__dev_get_by_name(void *net, const char *name);
int __dev_change_net_namespace(struct net_device *dev, void *net,
                                const char *pat);

void dev_add_pack(void *pt);
void dev_remove_pack(void *pt);
void dev_uc_del(struct net_device *dev, const void *addr);
void dst_release(void *dst);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_NET_H_
