// Auto-generated KMI stub implementations.
// Generated from GKI android15-6.6 kernel headers (build 14943902).
// Regenerate with: python3 tools/generate_stubs.py
//
// Found in headers: 830 symbols
// Blind stubs: 558 symbols
// Total: 1388 symbols

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Kernel type aliases for host compilation.
typedef unsigned int gfp_t;
typedef unsigned short umode_t;
typedef long long loff_t;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;
typedef _Bool bool;
typedef unsigned long phys_addr_t;
typedef unsigned long dma_addr_t;
typedef unsigned long resource_size_t;
typedef unsigned int dev_t;
typedef int atomic_t;
typedef unsigned int fmode_t;
typedef int kuid_t;
typedef int kgid_t;
typedef int pid_t;
typedef unsigned long sector_t;
typedef unsigned int __wsum;
typedef unsigned short __sum16;
typedef unsigned short __be16;
typedef unsigned int __be32;
typedef unsigned long long __be64;
typedef unsigned short __le16;
typedef unsigned int __le32;
typedef unsigned long long __le64;
typedef long long ktime_t;
#define __user
#define __iomem
#define __force
#define __bitwise
#define __percpu
#define __rcu

// Forward declarations for common kernel structs.
struct sk_buff;
struct net_device;
struct sock;
struct socket;
struct proto;
struct net;
struct iov_iter;
struct msghdr;
struct nla_policy;
struct nlattr;
struct dst_entry;
struct scatterlist;
struct tasklet_struct;
struct hrtimer;
struct file;
struct inode;
struct dentry;
struct super_block;
struct crypto_tfm;
struct crypto_shash;
struct work_struct;
struct delayed_work;
struct workqueue_struct;
struct timer_list;
struct kobject;
struct kobj_uevent_env;
struct device;
struct device_driver;
struct platform_device;
struct platform_driver;
struct class;
struct bus_type;
struct notifier_block;
struct seq_file;
struct page;
struct folio;
struct vm_area_struct;
struct mm_struct;
struct task_struct;
struct wait_queue_head;
struct wait_queue_entry;
struct idr;
struct ida;
struct xarray;
struct regmap;
struct regmap_config;
struct regmap_bus;
struct kunit;
struct kunit_case;
struct kunit_suite;
struct usb_interface;
struct usb_device;
struct urb;
struct ethtool_link_ksettings;
struct ethtool_ts_info;
struct kernel_param;
struct kernel_param_ops;
struct attribute_group;
struct bin_attribute;
struct pernet_operations;

// __ClearPageMovable — 2 module(s) — linux/migrate.h
void __ClearPageMovable(struct page *page) {
}

// __SetPageMovable — 2 module(s) — linux/migrate.h
void __SetPageMovable(struct page *page, const struct movable_operations *ops) {
}

// ___pskb_trim — 2 module(s) — linux/skbuff.h
int ___pskb_trim(struct sk_buff *skb, unsigned int len) {
  return 0;
}

// ___ratelimit — 8 module(s) — linux/ratelimit_types.h
int ___ratelimit(struct ratelimit_state *rs, const char *func) {
  return 0;
}

// __alloc_pages — 6 module(s) — linux/gfp.h
return __alloc_pages(gfp_mask, order, nid, NULL) {
  return 0;
}

// __alloc_percpu — 5 module(s) — linux/percpu.h
void __percpu *__alloc_percpu(size_t size, size_t align) {
  return NULL;
}

// __alloc_percpu_gfp — 4 module(s) — linux/bpf.h
return __alloc_percpu_gfp(size, align, flags) {
  return 0;
}

// __alloc_skb — 16 module(s) — linux/skbuff.h
return __alloc_skb(size, priority, 0, NUMA_NO_NODE) {
  return 0;
}

// __blk_alloc_disk — 1 module(s) — linux/blkdev.h
struct gendisk *__blk_alloc_disk(int node, struct lock_class_key *lkclass) {
  return NULL;
}

// __blk_rq_map_sg — 1 module(s) — linux/blk-mq.h
return __blk_rq_map_sg(q, rq, sglist, &last_sg) {
  return 0;
}

// __clk_determine_rate — 1 module(s) — linux/clk-provider.h
int __clk_determine_rate(struct clk_hw *core, struct clk_rate_request *req) {
  return 0;
}

// __const_udelay — 2 module(s) — asm-generic/delay.h
void __const_udelay(unsigned long xloops) {
}

// __cpu_online_mask — 9 module(s) — linux/cpumask.h
struct cpumask __cpu_online_mask = {0};

// __cpu_possible_mask — 6 module(s) — linux/cpumask.h
struct cpumask __cpu_possible_mask = {0};

// __cpuhp_remove_state — 3 module(s) — linux/cpuhotplug.h
void __cpuhp_remove_state(enum cpuhp_state state, bool invoke) {
}

// __cpuhp_setup_state — 3 module(s) — linux/cpuhotplug.h
return __cpuhp_setup_state(state, name, true, startup, teardown, false) {
  return 0;
}

// __crypto_memneq — 1 module(s) — crypto/utils.h
unsigned long __crypto_memneq(const void *a, const void *b, size_t size) {
  return 0;
}

// __dev_fwnode — 1 module(s) — linux/property.h
struct fwnode_handle *__dev_fwnode(struct device *dev) {
  return NULL;
}

// __dev_get_by_index — 5 module(s) — linux/netdevice.h
struct net_device *__dev_get_by_index(struct net *net, int ifindex) {
  return NULL;
}

// __dev_get_by_name — 1 module(s) — linux/netdevice.h
struct net_device *__dev_get_by_name(struct net *net, const char *name) {
  return NULL;
}

// __dev_queue_xmit — 6 module(s) — linux/netdevice.h
int __dev_queue_xmit(struct sk_buff *skb, struct net_device *sb_dev) {
  return 0;
}

// __fdget — 1 module(s) — linux/file.h
unsigned long __fdget(unsigned int fd) {
  return 0;
}

// __flush_workqueue — 4 module(s) — linux/workqueue.h
void __flush_workqueue(struct workqueue_struct *wq) {
}

// __folio_put — 6 module(s) — linux/mm.h
void __folio_put(struct folio *folio) {
}

// __free_pages — 3 module(s) — linux/gfp.h
void __free_pages(struct page *page, unsigned int order) {
}

// __get_task_comm — 1 module(s) — linux/sched.h
char *__get_task_comm(char *to, size_t len, struct task_struct *tsk) {
  return NULL;
}

// __hw_addr_sync_dev — 1 module(s) — linux/netdevice.h
return __hw_addr_sync_dev(&dev->uc, dev, sync, unsync) {
  return 0;
}

// __ip_dev_find — 1 module(s) — linux/inetdevice.h
struct net_device *__ip_dev_find(struct net *net, __be32 addr, bool devref) {
  return NULL;
}

// __ip_select_ident — 1 module(s) — net/ip.h
void __ip_select_ident(struct net *net, struct iphdr *iph, int segs) {
}

// __ipv6_addr_type — 2 module(s) — net/ipv6.h
int __ipv6_addr_type(const struct in6_addr *addr) {
  return 0;
}

// __kfifo_free — 1 module(s) — linux/kfifo.h
void __kfifo_free(struct __kfifo *fifo) {
}

// __kunit_add_resource — 1 module(s) — kunit/resource.h
return __kunit_add_resource(test, init, free, res, data) {
  return 0;
}

// __local_bh_enable_ip — 8 module(s) — linux/bottom_half.h
void __local_bh_enable_ip(unsigned long ip, unsigned int cnt) {
}

// __mdiobus_register — 1 module(s) — linux/phy.h
int __mdiobus_register(struct mii_bus *bus, struct module *owner) {
  return 0;
}

// __module_get — 4 module(s) — linux/module.h
void __module_get(struct module *module) {
}

// __msecs_to_jiffies — 8 module(s) — linux/jiffies.h
unsigned long __msecs_to_jiffies(const unsigned int m) {
  return 0;
}

// __napi_alloc_skb — 1 module(s) — linux/skbuff.h
return __napi_alloc_skb(napi, length, GFP_ATOMIC) {
  return 0;
}

// __napi_schedule — 2 module(s) — linux/netdevice.h
void __napi_schedule(struct napi_struct *n) {
}

// __netdev_alloc_skb — 8 module(s) — linux/skbuff.h
return __netdev_alloc_skb(dev, length, GFP_ATOMIC) {
  return 0;
}

// __netif_napi_del — 1 module(s) — linux/netdevice.h
void __netif_napi_del(struct napi_struct *napi) {
}

// __netif_rx — 1 module(s) — linux/netdevice.h
int __netif_rx(struct sk_buff *skb) {
  return 0;
}

// __netlink_dump_start — 1 module(s) — linux/netlink.h
return __netlink_dump_start(ssk, skb, nlh, control) {
  return 0;
}

// __nlmsg_put — 4 module(s) — net/netlink.h
return __nlmsg_put(skb, portid, seq, type, payload, flags) {
  return 0;
}

// __num_online_cpus — 1 module(s) — linux/cpumask.h
atomic_t __num_online_cpus = {0};

// __per_cpu_offset — 7 module(s) — asm-generic/percpu.h
unsigned long __per_cpu_offset[NR_CPUS] = {0};

// __percpu_down_read — 1 module(s) — linux/percpu-rwsem.h
bool __percpu_down_read(struct percpu_rw_semaphore *, bool) {
  return 0;
}

// __pm_runtime_disable — 2 module(s) — linux/pm_runtime.h
void __pm_runtime_disable(struct device *dev, bool check_resume) {
}

// __pm_runtime_idle — 1 module(s) — linux/pm_runtime.h
int __pm_runtime_idle(struct device *dev, int rpmflags) {
  return 0;
}

// __pm_runtime_resume — 2 module(s) — linux/pm_runtime.h
int __pm_runtime_resume(struct device *dev, int rpmflags) {
  return 0;
}

// __pm_runtime_suspend — 2 module(s) — linux/pm_runtime.h
int __pm_runtime_suspend(struct device *dev, int rpmflags) {
  return 0;
}

// __printk_ratelimit — 1 module(s) — linux/printk.h
int __printk_ratelimit(const char *func) {
  return 0;
}

// __pskb_copy_fclone — 3 module(s) — linux/skbuff.h
return __pskb_copy_fclone(skb, headroom, gfp_mask, false) {
  return 0;
}

// __pskb_pull_tail — 15 module(s) — linux/skbuff.h
void *__pskb_pull_tail(struct sk_buff *skb, int delta) {
  return NULL;
}

// __put_cred — 1 module(s) — linux/cred.h
void __put_cred(struct cred *) {
}

// __put_net — 3 module(s) — net/net_namespace.h
void __put_net(struct net *net) {
}

// __put_task_struct — 1 module(s) — linux/sched/task.h
void __put_task_struct(struct task_struct *t) {
}

// __register_chrdev — 3 module(s) — linux/fs.h
return __register_chrdev(major, 0, 256, name, fops) {
  return 0;
}

// __request_module — 6 module(s) — linux/kmod.h
int __request_module(bool wait, const char *name, ...) {
  return 0;
}

// __sk_flush_backlog — 1 module(s) — net/sock.h
void __sk_flush_backlog(struct sock *sk) {
}

// __sk_mem_reclaim — 1 module(s) — net/sock.h
void __sk_mem_reclaim(struct sock *sk, int amount) {
}

// __sk_receive_skb — 1 module(s) — net/sock.h
return __sk_receive_skb(sk, skb, nested, 1, true) {
  return 0;
}

// __skb_gso_segment — 1 module(s) — net/udp.h
__skb_gso_segment(skb, features, false) {
}

// __skb_pad — 1 module(s) — linux/skbuff.h
int __skb_pad(struct sk_buff *skb, int pad, bool free_on_error) {
  return 0;
}

// __sock_queue_rcv_skb — 1 module(s) — net/sock.h
int __sock_queue_rcv_skb(struct sock *sk, struct sk_buff *skb) {
  return 0;
}

// __sock_tx_timestamp — 1 module(s) — net/sock.h
void __sock_tx_timestamp(__u16 tsflags, __u8 *tx_flags) {
}

// __srcu_read_lock — 1 module(s) — linux/srcu.h
int __srcu_read_lock(struct srcu_struct *ssp) {
  return 0;
}

// __srcu_read_unlock — 1 module(s) — linux/srcu.h
void __srcu_read_unlock(struct srcu_struct *ssp, int idx) {
}

// __sysfs_match_string — 1 module(s) — linux/string.h
int __sysfs_match_string(const char * const *array, size_t n, const char *s) {
  return 0;
}

// __tasklet_schedule — 5 module(s) — linux/interrupt.h
void __tasklet_schedule(struct tasklet_struct *t) {
}

// __usecs_to_jiffies — 1 module(s) — linux/jiffies.h
unsigned long __usecs_to_jiffies(const unsigned int u) {
  return 0;
}

// __vmalloc — 1 module(s) — linux/vmalloc.h
void *__vmalloc(unsigned long size, gfp_t gfp_mask) {
  return NULL;
}

// __wake_up_sync_key — 1 module(s) — linux/wait.h
void __wake_up_sync_key(struct wait_queue_head *wq_head, unsigned int mode, void *key) {
}

// _copy_from_iter — 9 module(s) — linux/uio.h
size_t _copy_from_iter(void *addr, size_t bytes, struct iov_iter *i) {
  return 0;
}

// _copy_to_iter — 3 module(s) — linux/uio.h
size_t _copy_to_iter(const void *addr, size_t bytes, struct iov_iter *i) {
  return 0;
}

// _dev_notice — 1 module(s) — linux/dev_printk.h
void _dev_notice(const struct device *dev, const char *fmt, ...) {
}

// _find_first_bit — 1 module(s) — linux/find.h
unsigned long _find_first_bit(const unsigned long *addr, unsigned long size) {
  return 0;
}

// _find_first_zero_bit — 1 module(s) — linux/find.h
unsigned long _find_first_zero_bit(const unsigned long *addr, unsigned long size) {
  return 0;
}

// _find_next_bit — 1 module(s) — linux/find.h
return _find_next_bit(addr, size, offset) {
  return 0;
}

// _find_next_zero_bit — 2 module(s) — linux/find.h
return _find_next_zero_bit(addr, size, offset) {
  return 0;
}

// _proc_mkdir — 3 module(s) — linux/proc_fs.h
struct proc_dir_entry *_proc_mkdir(const char *, umode_t, struct proc_dir_entry *, void *, bool) {
  return NULL;
}

// _raw_spin_trylock — 1 module(s) — linux/spinlock_api_smp.h
int __lockfunc _raw_spin_trylock(raw_spinlock_t *lock) {
  return 0;
}

// _raw_spin_trylock_bh — 1 module(s) — linux/spinlock_api_smp.h
int __lockfunc _raw_spin_trylock_bh(raw_spinlock_t *lock) {
  return 0;
}

// _raw_spin_unlock_irq — 9 module(s) — linux/spinlock_api_smp.h
void __lockfunc _raw_spin_unlock_irq(raw_spinlock_t *lock) {
  return 0;
}

// _raw_write_lock — 4 module(s) — linux/rwlock_api_smp.h
void __lockfunc _raw_write_lock(rwlock_t *lock) {
  return 0;
}

// _raw_write_unlock — 4 module(s) — linux/rwlock_api_smp.h
void __lockfunc _raw_write_unlock(rwlock_t *lock) {
  return 0;
}

// add_taint — 1 module(s) — linux/panic.h
void add_taint(unsigned flag, enum lockdep_ok) {
}

// aes_encrypt — 1 module(s) — crypto/aes.h
void aes_encrypt(const struct crypto_aes_ctx *ctx, u8 *out, const u8 *in) {
}

// all_vm_events — 1 module(s) — linux/vmstat.h
void all_vm_events(unsigned long *) {
}

// alloc_can_skb — 1 module(s) — linux/can/skb.h
struct sk_buff *alloc_can_skb(struct net_device *dev, struct can_frame **cf) {
  return NULL;
}

// alloc_pages_exact — 1 module(s) — linux/gfp.h
void *alloc_pages_exact(size_t size, gfp_t gfp_mask) {
  return NULL;
}

// arc4_crypt — 1 module(s) — crypto/arc4.h
void arc4_crypt(struct arc4_ctx *ctx, u8 *out, const u8 *in, unsigned int len) {
}

// arc4_setkey — 1 module(s) — crypto/arc4.h
int arc4_setkey(struct arc4_ctx *ctx, const u8 *in_key, unsigned int key_len) {
  return 0;
}

// balloon_mops — 1 module(s) — linux/balloon_compaction.h
const struct movable_operations balloon_mops = {0};

// balloon_page_alloc — 1 module(s) — linux/balloon_compaction.h
struct page *balloon_page_alloc(void) {
  return NULL;
}

// balloon_page_dequeue — 1 module(s) — linux/balloon_compaction.h
struct page *balloon_page_dequeue(struct balloon_dev_info *b_dev_info) {
  return NULL;
}

// baswap — 3 module(s) — net/bluetooth/bluetooth.h
void baswap(bdaddr_t *dst, const bdaddr_t *src) {
}

// bcmp — 8 module(s) — linux/string.h
int bcmp(const void *,const void *,__kernel_size_t) {
  return 0;
}

// bin2hex — 1 module(s) — linux/hex.h
char *bin2hex(char *dst, const void *src, size_t count) {
  return NULL;
}

// bio_alloc_bioset — 1 module(s) — linux/bio.h
return bio_alloc_bioset(bdev, nr_vecs, opf, gfp_mask, &fs_bio_set) {
  return 0;
}

// bio_chain — 1 module(s) — linux/bio.h
void bio_chain(struct bio *, struct bio *) {
}

// bio_start_io_acct — 1 module(s) — linux/blkdev.h
unsigned long bio_start_io_acct(struct bio *bio) {
  return 0;
}

// bit_wait — 1 module(s) — linux/wait_bit.h
int bit_wait(struct wait_bit_key *key, int mode) {
  return 0;
}

// bit_wait_timeout — 1 module(s) — linux/wait_bit.h
int bit_wait_timeout(struct wait_bit_key *key, int mode) {
  return 0;
}

// blk_execute_rq — 1 module(s) — linux/blk-mq.h
blk_status_t blk_execute_rq(struct request *rq, bool at_head) {
  return 0;
}

// blk_mq_freeze_queue — 1 module(s) — linux/blk-mq.h
void blk_mq_freeze_queue(struct request_queue *q) {
}

// blk_mq_stop_hw_queue — 1 module(s) — linux/blk-mq.h
void blk_mq_stop_hw_queue(struct blk_mq_hw_ctx *hctx) {
}

// blk_mq_unfreeze_queue — 1 module(s) — linux/blk-mq.h
void blk_mq_unfreeze_queue(struct request_queue *q) {
}

// blk_queue_io_min — 2 module(s) — linux/blkdev.h
void blk_queue_io_min(struct request_queue *q, unsigned int min) {
}

// blk_queue_io_opt — 2 module(s) — linux/blkdev.h
void blk_queue_io_opt(struct request_queue *q, unsigned int opt) {
}

// blk_queue_write_cache — 1 module(s) — linux/blkdev.h
void blk_queue_write_cache(struct request_queue *q, bool enabled, bool fua) {
}

// blk_status_to_errno — 1 module(s) — linux/blkdev.h
int blk_status_to_errno(blk_status_t status) {
  return 0;
}

// blkdev_put — 1 module(s) — linux/blkdev.h
void blkdev_put(struct block_device *bdev, void *holder) {
}

// bpf_trace_run1 — 5 module(s) — linux/trace_events.h
void bpf_trace_run1(struct bpf_prog *prog, u64 arg1) {
}

// bpf_trace_run2 — 5 module(s) — linux/trace_events.h
void bpf_trace_run2(struct bpf_prog *prog, u64 arg1, u64 arg2) {
}

// bt_accept_dequeue — 1 module(s) — net/bluetooth/bluetooth.h
struct sock *bt_accept_dequeue(struct sock *parent, struct socket *newsock) {
  return NULL;
}

// bt_accept_enqueue — 1 module(s) — net/bluetooth/bluetooth.h
void bt_accept_enqueue(struct sock *parent, struct sock *sk, bool bh) {
}

// bt_accept_unlink — 1 module(s) — net/bluetooth/bluetooth.h
void bt_accept_unlink(struct sock *sk) {
}

// bt_debugfs — 1 module(s) — net/bluetooth/bluetooth.h
struct dentry *bt_debugfs = {0};

// bt_err — 5 module(s) — net/bluetooth/bluetooth.h
void bt_err(const char *fmt, ...) {
}

// bt_info — 5 module(s) — net/bluetooth/bluetooth.h
void bt_info(const char *fmt, ...) {
}

// bt_procfs_cleanup — 2 module(s) — net/bluetooth/bluetooth.h
void bt_procfs_cleanup(struct net *net, const char *name) {
}

// bt_sock_ioctl — 1 module(s) — net/bluetooth/bluetooth.h
int bt_sock_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg) {
  return 0;
}

// bt_sock_link — 2 module(s) — net/bluetooth/bluetooth.h
void bt_sock_link(struct bt_sock_list *l, struct sock *s) {
}

// bt_sock_poll — 1 module(s) — net/bluetooth/bluetooth.h
__poll_t bt_sock_poll(struct file *file, struct socket *sock, poll_table *wait) {
  return 0;
}

// bt_sock_register — 2 module(s) — net/bluetooth/bluetooth.h
int bt_sock_register(int proto, const struct net_proto_family *ops) {
  return 0;
}

// bt_sock_unlink — 2 module(s) — net/bluetooth/bluetooth.h
void bt_sock_unlink(struct bt_sock_list *l, struct sock *s) {
}

// bt_sock_unregister — 2 module(s) — net/bluetooth/bluetooth.h
void bt_sock_unregister(int proto) {
}

// bt_sock_wait_ready — 1 module(s) — net/bluetooth/bluetooth.h
int bt_sock_wait_ready(struct sock *sk, unsigned int msg_flags) {
  return 0;
}

// bt_sock_wait_state — 1 module(s) — net/bluetooth/bluetooth.h
int bt_sock_wait_state(struct sock *sk, int state, unsigned long timeo) {
  return 0;
}

// bt_warn — 1 module(s) — net/bluetooth/bluetooth.h
void bt_warn(const char *fmt, ...) {
}

// bus_register — 1 module(s) — linux/device/bus.h
int bus_register(const struct bus_type *bus) {
  return 0;
}

// bus_unregister — 1 module(s) — linux/device/bus.h
void bus_unregister(const struct bus_type *bus) {
}

// can_bus_off — 1 module(s) — linux/can/dev.h
void can_bus_off(struct net_device *dev) {
}

// can_change_mtu — 1 module(s) — linux/can/dev.h
int can_change_mtu(struct net_device *dev, int new_mtu) {
  return 0;
}

// can_proto_register — 2 module(s) — linux/can/core.h
int can_proto_register(const struct can_proto *cp) {
  return 0;
}

// can_proto_unregister — 2 module(s) — linux/can/core.h
void can_proto_unregister(const struct can_proto *cp) {
}

// can_send — 3 module(s) — linux/can/core.h
int can_send(struct sk_buff *skb, int loop) {
  return 0;
}

// cancel_work — 1 module(s) — linux/workqueue.h
bool cancel_work(struct work_struct *work) {
  return 0;
}

// check_zeroed_user — 1 module(s) — linux/sockptr.h
return check_zeroed_user(src.user + offset, size) {
  return 0;
}

// class_dev_iter_exit — 2 module(s) — linux/device/class.h
void class_dev_iter_exit(struct class_dev_iter *iter) {
}

// class_dev_iter_next — 2 module(s) — linux/device/class.h
struct device *class_dev_iter_next(struct class_dev_iter *iter) {
  return NULL;
}

// class_find_device — 3 module(s) — linux/device/class.h
return class_find_device(class, NULL, name, device_match_name) {
  return 0;
}

// cleanup_srcu_struct — 1 module(s) — linux/srcu.h
void cleanup_srcu_struct(struct srcu_struct *ssp) {
}

// clk_get_parent — 1 module(s) — linux/clk.h
struct clk *clk_get_parent(struct clk *clk) {
  return NULL;
}

// clk_has_parent — 1 module(s) — linux/clk.h
bool clk_has_parent(const struct clk *clk, const struct clk *parent) {
  return 0;
}

// clk_hw_get_clk — 1 module(s) — linux/clk-provider.h
struct clk *clk_hw_get_clk(struct clk_hw *hw, const char *con_id) {
  return NULL;
}

// clk_hw_register — 1 module(s) — linux/clk-provider.h
int clk_hw_register(struct device *dev, struct clk_hw *hw) {
  return 0;
}

// clk_hw_unregister — 1 module(s) — linux/clk-provider.h
void clk_hw_unregister(struct clk_hw *hw) {
}

// clk_is_match — 2 module(s) — linux/clk.h
bool clk_is_match(const struct clk *p, const struct clk *q) {
  return 0;
}

// clk_notifier_register — 1 module(s) — linux/clk.h
int clk_notifier_register(struct clk *clk, struct notifier_block *nb) {
  return 0;
}

// clk_set_parent — 1 module(s) — linux/clk.h
int clk_set_parent(struct clk *clk, struct clk *parent) {
  return 0;
}

// clk_set_rate_range — 1 module(s) — linux/clk.h
int clk_set_rate_range(struct clk *clk, unsigned long min, unsigned long max) {
  return 0;
}

// close_candev — 1 module(s) — linux/can/dev.h
void close_candev(struct net_device *dev) {
}

// consume_skb — 18 module(s) — linux/skbuff.h
void consume_skb(struct sk_buff *skb) {
}

// crc16 — 1 module(s) — linux/crc16.h
u16 crc16(u16 crc, const u8 *buffer, size_t len) {
  return 0;
}

// crc32_le — 5 module(s) — linux/crc32.h
u32 crc32_le(u32 crc, unsigned char const *p, size_t len) {
  return 0;
}

// crc_ccitt — 1 module(s) — linux/crc-ccitt.h
u16 crc_ccitt(u16 crc, const u8 *buffer, size_t len) {
  return 0;
}

// crypto_aead_decrypt — 4 module(s) — crypto/aead.h
int crypto_aead_decrypt(struct aead_request *req) {
  return 0;
}

// crypto_aead_encrypt — 4 module(s) — crypto/aead.h
int crypto_aead_encrypt(struct aead_request *req) {
  return 0;
}

// crypto_alloc_aead — 4 module(s) — crypto/aead.h
struct crypto_aead *crypto_alloc_aead(const char *alg_name, u32 type, u32 mask) {
  return NULL;
}

// crypto_alloc_base — 1 module(s) — linux/crypto.h
struct crypto_tfm *crypto_alloc_base(const char *alg_name, u32 type, u32 mask) {
  return NULL;
}

// crypto_alloc_kpp — 1 module(s) — crypto/kpp.h
struct crypto_kpp *crypto_alloc_kpp(const char *alg_name, u32 type, u32 mask) {
  return NULL;
}

// crypto_alloc_shash — 3 module(s) — crypto/internal/kdf_selftest.h
crypto_alloc_shash(name, 0, 0) {
}

// crypto_default_rng — 1 module(s) — crypto/rng.h
struct crypto_rng *crypto_default_rng = {0};

// crypto_destroy_tfm — 8 module(s) — linux/crypto.h
void crypto_destroy_tfm(void *mem, struct crypto_tfm *tfm) {
}

// crypto_ecdh_key_len — 1 module(s) — crypto/ecdh.h
unsigned int crypto_ecdh_key_len(const struct ecdh *params) {
  return 0;
}

// crypto_has_ahash — 1 module(s) — crypto/hash.h
int crypto_has_ahash(const char *alg_name, u32 type, u32 mask) {
  return 0;
}

// crypto_has_alg — 2 module(s) — linux/crypto.h
int crypto_has_alg(const char *name, u32 type, u32 mask) {
  return 0;
}

// crypto_req_done — 2 module(s) — linux/crypto.h
void crypto_req_done(void *req, int err) {
}

// crypto_shash_final — 1 module(s) — crypto/hash.h
int crypto_shash_final(struct shash_desc *desc, u8 *out) {
  return 0;
}

// crypto_shash_update — 1 module(s) — linux/jbd2.h
crypto_shash_update(&desc.shash, address, length) {
}

// csum_ipv6_magic — 1 module(s) — net/ip6_checksum.h
return csum_ipv6_magic(saddr, daddr, len, IPPROTO_TCP, base) {
  return 0;
}

// current_work — 1 module(s) — linux/workqueue.h
struct work_struct *current_work(void) {
  return NULL;
}

// debugfs_initialized — 1 module(s) — linux/debugfs.h
bool debugfs_initialized(void) {
  return 0;
}

// debugfs_lookup — 1 module(s) — linux/debugfs.h
struct dentry *debugfs_lookup(const char *name, struct dentry *parent) {
  return NULL;
}

// dec_zone_page_state — 1 module(s) — linux/vmstat.h
void dec_zone_page_state(struct page *, enum zone_stat_item) {
}

// default_llseek — 1 module(s) — linux/fs.h
loff_t default_llseek(struct file *file, loff_t offset, int whence) {
  return 0;
}

// default_wake_function — 6 module(s) — linux/wait.h
int default_wake_function(struct wait_queue_entry *wq_entry, unsigned mode, int flags, void *key) {
  return 0;
}

// dev_add_pack — 4 module(s) — linux/netdevice.h
void dev_add_pack(struct packet_type *pt) {
}

// dev_alloc_name — 1 module(s) — linux/netdevice.h
int dev_alloc_name(struct net_device *dev, const char *name) {
  return 0;
}

// dev_close_many — 1 module(s) — linux/netdevice.h
void dev_close_many(struct list_head *head, bool unlink) {
}

// dev_err_probe — 3 module(s) — linux/dev_printk.h
int dev_err_probe(const struct device *dev, int err, const char *fmt, ...) {
  return 0;
}

// dev_get_by_index — 5 module(s) — linux/netdevice.h
struct net_device *dev_get_by_index(struct net *net, int ifindex) {
  return NULL;
}

// dev_get_by_index_rcu — 1 module(s) — linux/netdevice.h
struct net_device *dev_get_by_index_rcu(struct net *net, int ifindex) {
  return NULL;
}

// dev_get_by_name — 3 module(s) — linux/netdevice.h
struct net_device *dev_get_by_name(struct net *net, const char *name) {
  return NULL;
}

// dev_get_flags — 1 module(s) — linux/netdevice.h
unsigned int dev_get_flags(const struct net_device *) {
  return 0;
}

// dev_get_tstats64 — 5 module(s) — linux/netdevice.h
void dev_get_tstats64(struct net_device *dev, struct rtnl_link_stats64 *s) {
}

// dev_getfirstbyhwtype — 1 module(s) — linux/netdevice.h
struct net_device *dev_getfirstbyhwtype(struct net *net, unsigned short type) {
  return NULL;
}

// dev_load — 1 module(s) — linux/netdevice.h
void dev_load(struct net *net, const char *name) {
}

// dev_mc_sync — 2 module(s) — linux/netdevice.h
int dev_mc_sync(struct net_device *to, struct net_device *from) {
  return 0;
}

// dev_mc_unsync — 2 module(s) — linux/netdevice.h
void dev_mc_unsync(struct net_device *to, struct net_device *from) {
}

// dev_nit_active — 1 module(s) — linux/netdevice.h
bool dev_nit_active(struct net_device *dev) {
  return 0;
}

// dev_remove_pack — 4 module(s) — linux/netdevice.h
void dev_remove_pack(struct packet_type *pt) {
}

// dev_set_allmulti — 2 module(s) — linux/netdevice.h
int dev_set_allmulti(struct net_device *dev, int inc) {
  return 0;
}

// dev_set_mtu — 2 module(s) — linux/netdevice.h
int dev_set_mtu(struct net_device *, int) {
  return 0;
}

// dev_set_promiscuity — 2 module(s) — linux/netdevice.h
int dev_set_promiscuity(struct net_device *dev, int inc) {
  return 0;
}

// dev_uc_add — 2 module(s) — linux/if_macvlan.h
return dev_uc_add(macvlan->lowerdev, dev->dev_addr) {
  return 0;
}

// dev_uc_add_excl — 1 module(s) — linux/netdevice.h
int dev_uc_add_excl(struct net_device *dev, const unsigned char *addr) {
  return 0;
}

// dev_uc_del — 3 module(s) — linux/netdevice.h
int dev_uc_del(struct net_device *dev, const unsigned char *addr) {
  return 0;
}

// dev_uc_sync — 2 module(s) — linux/netdevice.h
int dev_uc_sync(struct net_device *to, struct net_device *from) {
  return 0;
}

// dev_uc_unsync — 2 module(s) — linux/netdevice.h
void dev_uc_unsync(struct net_device *to, struct net_device *from) {
}

// device_add_disk — 2 module(s) — linux/blkdev.h
return device_add_disk(NULL, disk, NULL) {
  return 0;
}

// device_find_any_child — 1 module(s) — linux/device.h
struct device *device_find_any_child(struct device *parent) {
  return NULL;
}

// device_get_match_data — 1 module(s) — linux/property.h
const void *device_get_match_data(const struct device *dev) {
  return NULL;
}

// device_match_name — 1 module(s) — linux/device/bus.h
int device_match_name(struct device *dev, const void *name) {
  return 0;
}

// device_register — 1 module(s) — linux/device.h
int device_register(struct device *dev) {
  return 0;
}

// device_rename — 1 module(s) — linux/device.h
int device_rename(struct device *dev, const char *new_name) {
  return 0;
}

// device_unregister — 2 module(s) — linux/device.h
void device_unregister(struct device *dev) {
}

// device_wakeup_disable — 1 module(s) — linux/pm_wakeup.h
int device_wakeup_disable(struct device *dev) {
  return 0;
}

// device_wakeup_enable — 1 module(s) — linux/pm_wakeup.h
int device_wakeup_enable(struct device *dev) {
  return 0;
}

// devm_clk_put — 1 module(s) — linux/clk.h
void devm_clk_put(struct device *dev, struct clk *clk) {
}

// devm_free_irq — 1 module(s) — linux/interrupt.h
void devm_free_irq(struct device *dev, unsigned int irq, void *dev_id) {
}

// devm_hwrng_register — 1 module(s) — linux/hw_random.h
int devm_hwrng_register(struct device *dev, struct hwrng *rng) {
  return 0;
}

// devm_kfree — 1 module(s) — linux/device.h
void devm_kfree(struct device *dev, const void *p) {
}

// devm_kstrdup — 1 module(s) — sound/soc.h
devm_kstrdup(card->dev, platform_name, GFP_KERNEL) {
}

// devm_memunmap — 1 module(s) — linux/io.h
void devm_memunmap(struct device *dev, void *addr) {
}

// disk_set_zoned — 1 module(s) — linux/blkdev.h
void disk_set_zoned(struct gendisk *disk, enum blk_zoned_model model) {
}

// dma_alloc_attrs — 1 module(s) — linux/dma-mapping.h
return dma_alloc_attrs(dev, size, dma_addr, gfp, attrs) {
  return 0;
}

// dma_free_attrs — 1 module(s) — linux/dma-mapping.h
return dma_free_attrs(dev, size, cpu_addr, dma_handle, 0) {
  return 0;
}

// down_read — 4 module(s) — linux/rwsem.h
void down_read(struct rw_semaphore *sem) {
}

// down_write — 5 module(s) — linux/rwsem.h
void down_write(struct rw_semaphore *sem) {
}

// dput — 1 module(s) — linux/dcache.h
void dput(struct dentry *) {
}

// drain_workqueue — 1 module(s) — linux/workqueue.h
void drain_workqueue(struct workqueue_struct *wq) {
}

// driver_attach — 1 module(s) — linux/device.h
int driver_attach(struct device_driver *drv) {
  return 0;
}

// driver_register — 1 module(s) — linux/device/driver.h
int driver_register(struct device_driver *drv) {
  return 0;
}

// driver_unregister — 2 module(s) — linux/siox.h
return driver_unregister(&sdriver->driver) {
  return 0;
}

// dst_cache_destroy — 1 module(s) — net/dst_cache.h
void dst_cache_destroy(struct dst_cache *dst_cache) {
}

// dst_cache_get — 1 module(s) — net/dst_cache.h
struct dst_entry *dst_cache_get(struct dst_cache *dst_cache) {
  return NULL;
}

// dst_cache_init — 1 module(s) — net/dst_metadata.h
dst_cache_init(&new_md->u.tun_info.dst_cache, GFP_ATOMIC) {
}

// dst_release — 6 module(s) — net/dst.h
void dst_release(struct dst_entry *dst) {
}

// eth_header_parse — 1 module(s) — linux/if_ether.h
int eth_header_parse(const struct sk_buff *skb, unsigned char *haddr) {
  return 0;
}

// eth_mac_addr — 4 module(s) — linux/etherdevice.h
int eth_mac_addr(struct net_device *dev, void *p) {
  return 0;
}

// eth_type_trans — 5 module(s) — linux/etherdevice.h
__be16 eth_type_trans(struct sk_buff *skb, struct net_device *dev) {
  return 0;
}

// eth_validate_addr — 8 module(s) — linux/etherdevice.h
int eth_validate_addr(struct net_device *dev) {
  return 0;
}

// ether_setup — 2 module(s) — linux/netdevice.h
void ether_setup(struct net_device *dev) {
}

// ethtool_op_get_link — 6 module(s) — linux/ethtool.h
u32 ethtool_op_get_link(struct net_device *dev) {
  return 0;
}

// eventfd_ctx_do_read — 1 module(s) — linux/eventfd.h
void eventfd_ctx_do_read(struct eventfd_ctx *ctx, __u64 *cnt) {
}

// eventfd_ctx_fdget — 1 module(s) — linux/eventfd.h
struct eventfd_ctx *eventfd_ctx_fdget(int fd) {
  return NULL;
}

// eventfd_ctx_fileget — 1 module(s) — linux/eventfd.h
struct eventfd_ctx *eventfd_ctx_fileget(struct file *file) {
  return NULL;
}

// eventfd_ctx_put — 1 module(s) — linux/eventfd.h
void eventfd_ctx_put(struct eventfd_ctx *ctx) {
}

// eventfd_signal — 1 module(s) — linux/eventfd.h
__u64 eventfd_signal(struct eventfd_ctx *ctx, __u64 n) {
  return 0;
}

// fasync_helper — 2 module(s) — linux/fs.h
int fasync_helper(int, struct file *, int, struct fasync_struct **) {
  return 0;
}

// fget — 2 module(s) — linux/file.h
struct file *fget(unsigned int fd) {
  return NULL;
}

// file_path — 1 module(s) — linux/fs.h
char *file_path(struct file *, char *, int) {
  return NULL;
}

// filp_close — 1 module(s) — linux/fs.h
int filp_close(struct file *, fl_owner_t id) {
  return 0;
}

// filp_open_block — 1 module(s) — linux/fs.h
struct file *filp_open_block(const char *, int, umode_t) {
  return NULL;
}

// finish_rcuwait — 1 module(s) — linux/rcuwait.h
void finish_rcuwait(struct rcuwait *w) {
}

// flush_delayed_work — 1 module(s) — linux/workqueue.h
bool flush_delayed_work(struct delayed_work *dwork) {
  return 0;
}

// folio_wait_bit — 1 module(s) — linux/pagemap.h
void folio_wait_bit(struct folio *folio, int bit_nr) {
}

// fput — 5 module(s) — linux/file.h
void fput(struct file *) {
}

// fqdir_exit — 1 module(s) — net/inet_frag.h
void fqdir_exit(struct fqdir *fqdir) {
}

// fqdir_init — 1 module(s) — net/inet_frag.h
int fqdir_init(struct fqdir **fqdirp, struct inet_frags *f, struct net *net) {
  return 0;
}

// free_candev — 1 module(s) — linux/can/dev.h
void free_candev(struct net_device *dev) {
}

// free_pages — 3 module(s) — linux/gfp.h
void free_pages(unsigned long addr, unsigned int order) {
}

// free_pages_exact — 1 module(s) — linux/gfp.h
void free_pages_exact(void *virt, size_t size) {
}

// free_percpu — 7 module(s) — linux/percpu.h
void free_percpu(void __percpu *__pdata) {
}

// fs_bio_set — 1 module(s) — linux/bio.h
struct bio_set fs_bio_set = {0};

// genl_register_family — 4 module(s) — linux/genl_magic_func.h
return genl_register_family(&ZZZ_genl_family) {
  return 0;
}

// genphy_resume — 1 module(s) — linux/phy.h
int genphy_resume(struct phy_device *phydev) {
  return 0;
}

// get_net_ns_by_fd — 1 module(s) — net/net_namespace.h
struct net *get_net_ns_by_fd(int fd) {
  return NULL;
}

// get_net_ns_by_pid — 1 module(s) — net/net_namespace.h
struct net *get_net_ns_by_pid(pid_t pid) {
  return NULL;
}

// get_random_u32 — 1 module(s) — linux/random.h
u32 get_random_u32(void) {
  return 0;
}

// get_user_ifreq — 1 module(s) — linux/netdevice.h
int get_user_ifreq(struct ifreq *ifr, void __user **ifrdata, void __user *arg) {
  return 0;
}

// get_zeroed_page — 2 module(s) — linux/gfp.h
unsigned long get_zeroed_page(gfp_t gfp_mask) {
  return 0;
}

// glob_match — 1 module(s) — linux/glob.h
bool glob_match(char const *pat, char const *str) {
  return 0;
}

// gpiochip_get_data — 1 module(s) — linux/gpio/driver.h
void *gpiochip_get_data(struct gpio_chip *gc) {
  return NULL;
}

// gre_add_protocol — 1 module(s) — net/gre.h
int gre_add_protocol(const struct gre_protocol *proto, u8 version) {
  return 0;
}

// gre_del_protocol — 1 module(s) — net/gre.h
int gre_del_protocol(const struct gre_protocol *proto, u8 version) {
  return 0;
}

// gro_cells_destroy — 1 module(s) — net/gro_cells.h
void gro_cells_destroy(struct gro_cells *gcells) {
}

// gro_cells_init — 1 module(s) — net/gro_cells.h
int gro_cells_init(struct gro_cells *gcells, struct net_device *dev) {
  return 0;
}

// gro_cells_receive — 1 module(s) — net/gro_cells.h
int gro_cells_receive(struct gro_cells *gcells, struct sk_buff *skb) {
  return 0;
}

// hci_alloc_dev_priv — 2 module(s) — net/bluetooth/hci_core.h
struct hci_dev *hci_alloc_dev_priv(int sizeof_priv) {
  return NULL;
}

// hci_conn_check_secure — 1 module(s) — net/bluetooth/hci_core.h
int hci_conn_check_secure(struct hci_conn *conn, __u8 sec_level) {
  return 0;
}

// hci_conn_switch_role — 1 module(s) — net/bluetooth/hci_core.h
int hci_conn_switch_role(struct hci_conn *conn, __u8 role) {
  return 0;
}

// hci_devcd_abort — 1 module(s) — net/bluetooth/coredump.h
int hci_devcd_abort(struct hci_dev *hdev) {
  return 0;
}

// hci_devcd_append — 1 module(s) — net/bluetooth/coredump.h
int hci_devcd_append(struct hci_dev *hdev, struct sk_buff *skb) {
  return 0;
}

// hci_devcd_complete — 1 module(s) — net/bluetooth/coredump.h
int hci_devcd_complete(struct hci_dev *hdev) {
  return 0;
}

// hci_devcd_init — 1 module(s) — net/bluetooth/coredump.h
int hci_devcd_init(struct hci_dev *hdev, u32 dump_size) {
  return 0;
}

// hci_free_dev — 2 module(s) — net/bluetooth/hci_core.h
void hci_free_dev(struct hci_dev *hdev) {
}

// hci_get_route — 1 module(s) — net/bluetooth/hci_core.h
struct hci_dev *hci_get_route(bdaddr_t *dst, bdaddr_t *src, u8 src_type) {
  return NULL;
}

// hci_recv_diag — 1 module(s) — net/bluetooth/hci_core.h
int hci_recv_diag(struct hci_dev *hdev, struct sk_buff *skb) {
  return 0;
}

// hci_recv_frame — 3 module(s) — net/bluetooth/hci_core.h
int hci_recv_frame(struct hci_dev *hdev, struct sk_buff *skb) {
  return 0;
}

// hci_register_cb — 1 module(s) — net/bluetooth/hci_core.h
int hci_register_cb(struct hci_cb *hcb) {
  return 0;
}

// hci_register_dev — 2 module(s) — net/bluetooth/hci_core.h
int hci_register_dev(struct hci_dev *hdev) {
  return 0;
}

// hci_reset_dev — 1 module(s) — net/bluetooth/hci_core.h
int hci_reset_dev(struct hci_dev *hdev) {
  return 0;
}

// hci_set_fw_info — 1 module(s) — net/bluetooth/hci_core.h
void hci_set_fw_info(struct hci_dev *hdev, const char *fmt, ...) {
}

// hci_unregister_cb — 1 module(s) — net/bluetooth/hci_core.h
int hci_unregister_cb(struct hci_cb *hcb) {
  return 0;
}

// hci_unregister_dev — 2 module(s) — net/bluetooth/hci_core.h
void hci_unregister_dev(struct hci_dev *hdev) {
}

// hex2bin — 1 module(s) — linux/hex.h
int hex2bin(u8 *dst, const char *src, size_t count) {
  return 0;
}

// hex_asc_upper — 1 module(s) — linux/hex.h
const char hex_asc_upper[] = {0};

// hex_to_bin — 1 module(s) — linux/hex.h
int hex_to_bin(unsigned char ch) {
  return 0;
}

// hid_add_device — 1 module(s) — linux/hid.h
int hid_add_device(struct hid_device *) {
  return 0;
}

// hid_allocate_device — 1 module(s) — linux/hid.h
struct hid_device *hid_allocate_device(void) {
  return NULL;
}

// hid_destroy_device — 1 module(s) — linux/hid.h
void hid_destroy_device(struct hid_device *) {
}

// hid_ignore — 1 module(s) — linux/hid.h
bool hid_ignore(struct hid_device *) {
  return 0;
}

// hid_is_usb — 1 module(s) — linux/hid.h
bool hid_is_usb(const struct hid_device *hdev) {
  return 0;
}

// hid_parse_report — 1 module(s) — linux/hid.h
int hid_parse_report(struct hid_device *hid, __u8 *start, unsigned size) {
  return 0;
}

// hrtimer_active — 2 module(s) — linux/hrtimer.h
bool hrtimer_active(const struct hrtimer *timer) {
  return 0;
}

// hrtimer_cancel — 5 module(s) — linux/hrtimer.h
int hrtimer_cancel(struct hrtimer *timer) {
  return 0;
}

// hrtimer_forward — 2 module(s) — linux/hrtimer.h
return hrtimer_forward(timer, timer->base->get_time(), interval) {
  return 0;
}

// ida_alloc_range — 6 module(s) — linux/idr.h
int ida_alloc_range(struct ida *, unsigned int min, unsigned int max, gfp_t) {
  return 0;
}

// ida_destroy — 2 module(s) — linux/idr.h
void ida_destroy(struct ida *ida) {
}

// ida_free — 6 module(s) — linux/idr.h
void ida_free(struct ida *, unsigned int id) {
}

// idr_alloc — 8 module(s) — linux/idr.h
int idr_alloc(struct idr *, void *ptr, int start, int end, gfp_t) {
  return 0;
}

// idr_destroy — 7 module(s) — linux/idr.h
void idr_destroy(struct idr *) {
}

// idr_find — 8 module(s) — linux/idr.h
void *idr_find(const struct idr *, unsigned long id) {
  return NULL;
}

// idr_get_next — 5 module(s) — linux/idr.h
void *idr_get_next(struct idr *, int *nextid) {
  return NULL;
}

// idr_get_next_ul — 1 module(s) — linux/idr.h
void *idr_get_next_ul(struct idr *, unsigned long *nextid) {
  return NULL;
}

// idr_preload — 1 module(s) — linux/idr.h
void idr_preload(gfp_t gfp_mask) {
}

// idr_remove — 9 module(s) — linux/idr.h
void *idr_remove(struct idr *, unsigned long id) {
  return NULL;
}

// idr_replace — 1 module(s) — linux/idr.h
void *idr_replace(struct idr *, void *, unsigned long id) {
  return NULL;
}

// ieee802154_hdr_peek — 2 module(s) — net/ieee802154_netdev.h
int ieee802154_hdr_peek(const struct sk_buff *skb, struct ieee802154_hdr *hdr) {
  return 0;
}

// ieee802154_hdr_pull — 2 module(s) — net/ieee802154_netdev.h
int ieee802154_hdr_pull(struct sk_buff *skb, struct ieee802154_hdr *hdr) {
  return 0;
}

// ieee802154_hdr_push — 1 module(s) — net/ieee802154_netdev.h
int ieee802154_hdr_push(struct sk_buff *skb, struct ieee802154_hdr *hdr) {
  return 0;
}

// iio_format_value — 1 module(s) — linux/iio/iio.h
ssize_t iio_format_value(char *buf, unsigned int type, int size, int *vals) {
  return 0;
}

// in6addr_any — 1 module(s) — linux/in6.h
const struct in6_addr in6addr_any = {0};

// in_aton — 1 module(s) — linux/inet.h
__be32 in_aton(const char *str) {
  return 0;
}

// inc_zone_page_state — 1 module(s) — linux/vmstat.h
void inc_zone_page_state(struct page *, enum zone_stat_item) {
}

// inet6_csk_xmit — 1 module(s) — net/inet6_connection_sock.h
int inet6_csk_xmit(struct sock *sk, struct sk_buff *skb, struct flowi *fl) {
  return 0;
}

// inet_frag_destroy — 1 module(s) — net/inet_frag.h
void inet_frag_destroy(struct inet_frag_queue *q) {
}

// inet_frag_find — 1 module(s) — net/inet_frag.h
struct inet_frag_queue *inet_frag_find(struct fqdir *fqdir, void *key) {
  return NULL;
}

// inet_frag_kill — 1 module(s) — net/inet_frag.h
void inet_frag_kill(struct inet_frag_queue *q) {
}

// inet_frags_fini — 1 module(s) — net/inet_frag.h
void inet_frags_fini(struct inet_frags *) {
}

// inet_frags_init — 1 module(s) — net/inet_frag.h
int inet_frags_init(struct inet_frags *) {
  return 0;
}

// init_net — 9 module(s) — linux/seq_file_net.h
struct net init_net = {0};

// init_srcu_struct — 1 module(s) — linux/srcu.h
int init_srcu_struct(struct srcu_struct *ssp) {
  return 0;
}

// init_user_ns — 1 module(s) — linux/uidgid.h
struct user_namespace init_user_ns = {0};

// init_uts_ns — 1 module(s) — linux/utsname.h
struct uts_namespace init_uts_ns = {0};

// iov_iter_npages — 1 module(s) — linux/uio.h
int iov_iter_npages(const struct iov_iter *i, int maxpages) {
  return 0;
}

// iov_iter_revert — 10 module(s) — linux/uio.h
void iov_iter_revert(struct iov_iter *i, size_t bytes) {
}

// ip6_dst_hoplimit — 1 module(s) — net/ipv6.h
int ip6_dst_hoplimit(struct dst_entry *dst) {
  return 0;
}

// ip_local_out — 1 module(s) — net/ip.h
int ip_local_out(struct net *net, struct sock *sk, struct sk_buff *skb) {
  return 0;
}

// ip_mc_join_group — 1 module(s) — linux/igmp.h
int ip_mc_join_group(struct sock *sk, struct ip_mreqn *imr) {
  return 0;
}

// ip_queue_xmit — 1 module(s) — net/ip.h
int ip_queue_xmit(struct sock *sk, struct sk_buff *skb, struct flowi *fl) {
  return 0;
}

// ip_route_output_flow — 2 module(s) — net/route.h
return ip_route_output_flow(net, flp, NULL) {
  return 0;
}

// ip_send_check — 1 module(s) — net/ip.h
void ip_send_check(struct iphdr *ip) {
}

// irq_get_irq_data — 1 module(s) — linux/irq.h
struct irq_data *irq_get_irq_data(unsigned int irq) {
  return NULL;
}

// irq_set_irq_wake — 1 module(s) — linux/interrupt.h
int irq_set_irq_wake(unsigned int irq, unsigned int on) {
  return 0;
}

// jiffies_to_msecs — 6 module(s) — linux/jiffies.h
unsigned int jiffies_to_msecs(const unsigned long j) {
  return 0;
}

// jiffies_to_usecs — 1 module(s) — linux/jiffies.h
unsigned int jiffies_to_usecs(const unsigned long j) {
  return 0;
}

// kasprintf — 1 module(s) — linux/sprintf.h
char *kasprintf(gfp_t gfp, const char *fmt, ...) {
  return NULL;
}

// kernel_accept — 2 module(s) — linux/net.h
int kernel_accept(struct socket *sock, struct socket **newsock, int flags) {
  return 0;
}

// kernel_bind — 3 module(s) — linux/net.h
int kernel_bind(struct socket *sock, struct sockaddr *addr, int addrlen) {
  return 0;
}

// kernel_kobj — 2 module(s) — linux/kobject.h
struct kobject *kernel_kobj = {0};

// kernel_listen — 2 module(s) — linux/net.h
int kernel_listen(struct socket *sock, int backlog) {
  return 0;
}

// kernel_read — 1 module(s) — linux/fs.h
ssize_t kernel_read(struct file *, void *, size_t, loff_t *) {
  return 0;
}

// kernel_sock_shutdown — 2 module(s) — linux/net.h
int kernel_sock_shutdown(struct socket *sock, enum sock_shutdown_cmd how) {
  return 0;
}

// kernel_write — 1 module(s) — linux/fs.h
ssize_t kernel_write(struct file *, const void *, size_t, loff_t *) {
  return 0;
}

// kfree_const — 1 module(s) — linux/string.h
void kfree_const(const void *x) {
}

// kfree_sensitive — 5 module(s) — linux/slab.h
void kfree_sensitive(const void *objp) {
}

// kfree_skb_partial — 1 module(s) — linux/skbuff.h
void kfree_skb_partial(struct sk_buff *skb, bool head_stolen) {
}

// kill_fasync — 2 module(s) — linux/fs.h
void kill_fasync(struct fasync_struct **, int, int) {
}

// kmalloc_large — 3 module(s) — linux/slab.h
return kmalloc_large(size, flags) {
  return 0;
}

// kmalloc_large_node — 1 module(s) — linux/slab.h
return kmalloc_large_node(size, flags, node) {
  return 0;
}

// kmem_cache_alloc — 5 module(s) — linux/jbd2.h
return kmem_cache_alloc(jbd2_inode_cache, gfp_flags) {
  return 0;
}

// kmem_cache_destroy — 5 module(s) — linux/slab.h
void kmem_cache_destroy(struct kmem_cache *s) {
}

// kmem_cache_free — 5 module(s) — linux/slab.h
void kmem_cache_free(struct kmem_cache *s, void *objp) {
}

// kobject_get — 1 module(s) — linux/kobject.h
struct kobject *kobject_get(struct kobject *kobj) {
  return NULL;
}

// kstrndup — 1 module(s) — linux/string.h
char *kstrndup(const char *s, size_t len, gfp_t gfp) {
  return NULL;
}

// kstrtobool_from_user — 1 module(s) — linux/kstrtox.h
int kstrtobool_from_user(const char __user *s, size_t count, bool *res) {
  return 0;
}

// kstrtoll — 1 module(s) — linux/kstrtox.h
int kstrtoll(const char *s, unsigned int base, long long *res) {
  return 0;
}

// kstrtou16 — 1 module(s) — linux/kstrtox.h
int kstrtou16(const char *s, unsigned int base, u16 *res) {
  return 0;
}

// kstrtou8 — 1 module(s) — linux/kstrtox.h
int kstrtou8(const char *s, unsigned int base, u8 *res) {
  return 0;
}

// kthread_should_stop — 1 module(s) — linux/kthread.h
bool kthread_should_stop(void) {
  return 0;
}

// kthread_stop — 2 module(s) — linux/kthread.h
int kthread_stop(struct task_struct *k) {
  return 0;
}

// ktime_get_real_ts64 — 2 module(s) — linux/timekeeping.h
void ktime_get_real_ts64(struct timespec64 *tv) {
}

// ktime_get_snapshot — 2 module(s) — linux/timekeeping.h
void ktime_get_snapshot(struct system_time_snapshot *systime_snapshot) {
}

// ktime_get_ts64 — 2 module(s) — linux/timekeeping.h
void ktime_get_ts64(struct timespec64 *ts) {
}

// ktime_get_with_offset — 2 module(s) — linux/timekeeping.h
ktime_t ktime_get_with_offset(enum tk_offsets offs) {
  return 0;
}

// kunit_add_action — 1 module(s) — kunit/resource.h
int kunit_add_action(struct kunit *test, kunit_action_t *action, void *ctx) {
  return 0;
}

// kunit_cleanup — 1 module(s) — kunit/test.h
void kunit_cleanup(struct kunit *test) {
}

// kunit_init_test — 1 module(s) — kunit/test.h
void kunit_init_test(struct kunit *test, const char *name, char *log) {
}

// kunit_log_append — 2 module(s) — kunit/test.h
kunit_log_append(char *log, const char *fmt, ...) {
}

// kunit_remove_resource — 1 module(s) — kunit/resource.h
void kunit_remove_resource(struct kunit *test, struct kunit_resource *res) {
}

// kunit_try_catch_run — 1 module(s) — kunit/try-catch.h
void kunit_try_catch_run(struct kunit_try_catch *try_catch, void *context) {
}

// kunit_try_catch_throw — 3 module(s) — kunit/try-catch.h
void __noreturn kunit_try_catch_throw(struct kunit_try_catch *try_catch) {
  return 0;
}

// kvasprintf_const — 1 module(s) — linux/sprintf.h
const char *kvasprintf_const(gfp_t gfp, const char *fmt, va_list args) {
  return NULL;
}

// kvfree — 3 module(s) — linux/rcutiny.h
void kvfree(const void *addr) {
}

// kvfree_call_rcu — 6 module(s) — linux/rcutree.h
void kvfree_call_rcu(struct rcu_head *head, void *ptr) {
}

// kvmalloc_node — 2 module(s) — linux/slab.h
void *kvmalloc_node(size_t size, gfp_t flags, int node) {
  return NULL;
}

// l2cap_conn_get — 1 module(s) — net/bluetooth/l2cap.h
struct l2cap_conn *l2cap_conn_get(struct l2cap_conn *conn) {
  return NULL;
}

// l2cap_conn_put — 1 module(s) — net/bluetooth/l2cap.h
void l2cap_conn_put(struct l2cap_conn *conn) {
}

// l2cap_is_socket — 1 module(s) — net/bluetooth/l2cap.h
bool l2cap_is_socket(struct socket *sock) {
  return 0;
}

// l2cap_register_user — 1 module(s) — net/bluetooth/l2cap.h
int l2cap_register_user(struct l2cap_conn *conn, struct l2cap_user *user) {
  return 0;
}

// l2cap_unregister_user — 1 module(s) — net/bluetooth/l2cap.h
void l2cap_unregister_user(struct l2cap_conn *conn, struct l2cap_user *user) {
}

// linkwatch_fire_event — 2 module(s) — linux/netdevice.h
void linkwatch_fire_event(struct net_device *dev) {
}

// list_sort — 1 module(s) — linux/list_sort.h
void list_sort(void *priv, struct list_head *head, list_cmp_func_t cmp) {
}

// lock_sock_nested — 12 module(s) — net/sock.h
void lock_sock_nested(struct sock *sk, int subclass) {
}

// match_int — 2 module(s) — linux/parser.h
int match_int(substring_t *, int *result) {
  return 0;
}

// match_strdup — 1 module(s) — linux/parser.h
char *match_strdup(const substring_t *) {
  return NULL;
}

// match_token — 2 module(s) — linux/parser.h
int match_token(char *, const match_table_t table, substring_t args[]) {
  return 0;
}

// mdiobus_alloc_size — 1 module(s) — linux/phy.h
struct mii_bus *mdiobus_alloc_size(size_t size) {
  return NULL;
}

// mdiobus_free — 1 module(s) — linux/phy.h
void mdiobus_free(struct mii_bus *bus) {
}

// mdiobus_get_phy — 1 module(s) — linux/mdio.h
struct phy_device *mdiobus_get_phy(struct mii_bus *bus, int addr) {
  return NULL;
}

// mdiobus_unregister — 1 module(s) — linux/phy.h
void mdiobus_unregister(struct mii_bus *bus) {
}

// memchr — 1 module(s) — linux/string.h
void * memchr(const void *,int,__kernel_size_t) {
  return NULL;
}

// memchr_inv — 2 module(s) — linux/string.h
void *memchr_inv(const void *s, int c, size_t n) {
  return NULL;
}

// memdup_user — 2 module(s) — linux/string.h
void *memdup_user(const void __user *, size_t) {
  return NULL;
}

// memparse — 1 module(s) — linux/kernel.h
unsigned long long memparse(const char *ptr, char **retptr) {
  return 0;
}

// memscan — 1 module(s) — linux/string.h
void * memscan(void *,int,__kernel_size_t) {
  return NULL;
}

// memset64 — 1 module(s) — linux/string.h
void *memset64(uint64_t *, uint64_t, __kernel_size_t) {
  return NULL;
}

// metadata_dst_alloc — 1 module(s) — net/dst_metadata.h
metadata_dst_alloc(md_size, METADATA_IP_TUNNEL, GFP_ATOMIC) {
}

// mii_ethtool_gset — 2 module(s) — linux/mii.h
void mii_ethtool_gset(struct mii_if_info *mii, struct ethtool_cmd *ecmd) {
}

// mii_link_ok — 2 module(s) — linux/mii.h
int mii_link_ok (struct mii_if_info *mii) {
  return 0;
}

// mii_nway_restart — 4 module(s) — linux/mii.h
int mii_nway_restart (struct mii_if_info *mii) {
  return 0;
}

// mtree_load — 1 module(s) — linux/mm.h
return mtree_load(&mm->mm_mt, addr) {
  return 0;
}

// mutex_is_locked — 1 module(s) — linux/mutex.h
bool mutex_is_locked(struct mutex *lock) {
  return 0;
}

// napi_complete_done — 2 module(s) — linux/netdevice.h
bool napi_complete_done(struct napi_struct *n, int work_done) {
  return 0;
}

// napi_disable — 1 module(s) — linux/netdevice.h
void napi_disable(struct napi_struct *n) {
}

// napi_enable — 2 module(s) — linux/netdevice.h
void napi_enable(struct napi_struct *n) {
}

// napi_gro_receive — 1 module(s) — linux/netdevice.h
gro_result_t napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb) {
  return 0;
}

// napi_schedule_prep — 2 module(s) — linux/netdevice.h
bool napi_schedule_prep(struct napi_struct *n) {
  return 0;
}

// nd_tbl — 1 module(s) — net/ndisc.h
struct neigh_table nd_tbl = {0};

// neigh_destroy — 1 module(s) — net/neighbour.h
void neigh_destroy(struct neighbour *neigh) {
}

// neigh_lookup — 1 module(s) — net/neighbour.h
neigh_lookup(tbl, pkey, dev) {
}

// net_namespace_list — 1 module(s) — net/net_namespace.h
struct list_head net_namespace_list = {0};

// net_ratelimit — 5 module(s) — linux/net.h
int net_ratelimit(void) {
  return 0;
}

// netdev_err — 8 module(s) — net/net_debug.h
void netdev_err(const struct net_device *dev, const char *format, ...) {
}

// netdev_info — 8 module(s) — net/net_debug.h
void netdev_info(const struct net_device *dev, const char *format, ...) {
}

// netdev_name_in_use — 1 module(s) — linux/netdevice.h
bool netdev_name_in_use(struct net *net, const char *name) {
  return 0;
}

// netdev_notice — 2 module(s) — net/net_debug.h
void netdev_notice(const struct net_device *dev, const char *format, ...) {
}

// netdev_warn — 11 module(s) — net/net_debug.h
void netdev_warn(const struct net_device *dev, const char *format, ...) {
}

// netif_carrier_off — 9 module(s) — linux/netdevice.h
void netif_carrier_off(struct net_device *dev) {
}

// netif_carrier_on — 9 module(s) — linux/netdevice.h
void netif_carrier_on(struct net_device *dev) {
}

// netif_device_attach — 3 module(s) — linux/netdevice.h
void netif_device_attach(struct net_device *dev) {
}

// netif_device_detach — 3 module(s) — linux/netdevice.h
void netif_device_detach(struct net_device *dev) {
}

// netif_receive_skb — 2 module(s) — linux/netdevice.h
int netif_receive_skb(struct sk_buff *skb) {
  return 0;
}

// netif_rx — 9 module(s) — linux/netdevice.h
int netif_rx(struct sk_buff *skb) {
  return 0;
}

// netif_tx_lock — 2 module(s) — linux/netdevice.h
void netif_tx_lock(struct net_device *dev) {
}

// netif_tx_unlock — 2 module(s) — linux/netdevice.h
void netif_tx_unlock(struct net_device *dev) {
}

// netif_tx_wake_queue — 6 module(s) — linux/netdevice.h
void netif_tx_wake_queue(struct netdev_queue *dev_queue) {
}

// netlink_broadcast — 2 module(s) — net/netlink.h
netlink_broadcast(sk, skb, portid, group, flags) {
}

// netlink_capable — 1 module(s) — linux/netlink.h
bool netlink_capable(const struct sk_buff *skb, int cap) {
  return 0;
}

// netlink_net_capable — 1 module(s) — linux/netlink.h
bool netlink_net_capable(const struct sk_buff *skb, int cap) {
  return 0;
}

// netlink_unicast — 3 module(s) — linux/netlink.h
int netlink_unicast(struct sock *ssk, struct sk_buff *skb, __u32 portid, int nonblock) {
  return 0;
}

// next_arg — 1 module(s) — linux/kernel.h
char *next_arg(char *args, char **param, char **val) {
  return NULL;
}

// nf_conntrack_destroy — 2 module(s) — linux/netfilter/nf_conntrack_common.h
void nf_conntrack_destroy(struct nf_conntrack *nfct) {
}

// nla_memcpy — 4 module(s) — net/netlink.h
int nla_memcpy(void *dest, const struct nlattr *src, int count) {
  return 0;
}

// nla_put — 9 module(s) — net/netlink.h
int nla_put(struct sk_buff *skb, int attrtype, int attrlen, const void *data) {
  return 0;
}

// nla_put_64bit — 3 module(s) — linux/genl_magic_struct.h
return nla_put_64bit(skb, attrtype, sizeof(u64), &value, 0) {
  return 0;
}

// nla_strscpy — 3 module(s) — net/netlink.h
ssize_t nla_strscpy(char *dst, const struct nlattr *nla, size_t dstsize) {
  return 0;
}

// nonseekable_open — 1 module(s) — linux/fs.h
int nonseekable_open(struct inode * inode, struct file * filp) {
  return 0;
}

// nr_cpu_ids — 1 module(s) — linux/cpumask.h
unsigned int nr_cpu_ids = {0};

// ns_capable — 4 module(s) — linux/capability.h
bool ns_capable(struct user_namespace *ns, int cap) {
  return 0;
}

// ns_to_timespec64 — 2 module(s) — linux/time64.h
struct timespec64 ns_to_timespec64(s64 nsec) {
  return 0;
}

// of_irq_get_byname — 1 module(s) — linux/of_irq.h
int of_irq_get_byname(struct device_node *dev, const char *name) {
  return 0;
}

// open_candev — 1 module(s) — linux/can/dev.h
int open_candev(struct net_device *dev) {
  return 0;
}

// overflowgid — 1 module(s) — linux/highuid.h
int overflowgid = {0};

// overflowuid — 2 module(s) — linux/highuid.h
int overflowuid = {0};

// p9_client_cb — 1 module(s) — net/9p/client.h
void p9_client_cb(struct p9_client *c, struct p9_req_t *req, int status) {
}

// p9_req_put — 1 module(s) — net/9p/client.h
int p9_req_put(struct p9_client *c, struct p9_req_t *r) {
  return 0;
}

// p9_tag_lookup — 1 module(s) — net/9p/client.h
struct p9_req_t *p9_tag_lookup(struct p9_client *c, u16 tag) {
  return NULL;
}

// page_pinner_inited — 6 module(s) — linux/page_pinner.h
struct static_key_false page_pinner_inited = {0};

// param_ops_bool — 6 module(s) — linux/moduleparam.h
const struct kernel_param_ops param_ops_bool = {0};

// param_ops_charp — 1 module(s) — linux/moduleparam.h
const struct kernel_param_ops param_ops_charp = {0};

// param_ops_int — 6 module(s) — linux/moduleparam.h
const struct kernel_param_ops param_ops_int = {0};

// pci_device_is_present — 1 module(s) — linux/pci.h
bool pci_device_is_present(struct pci_dev *pdev) {
  return 0;
}

// pci_disable_device — 1 module(s) — linux/pci.h
void pci_disable_device(struct pci_dev *dev) {
}

// pci_disable_sriov — 1 module(s) — linux/pci.h
void pci_disable_sriov(struct pci_dev *dev) {
}

// pci_enable_device — 1 module(s) — linux/pci.h
int pci_enable_device(struct pci_dev *dev) {
  return 0;
}

// pci_enable_sriov — 1 module(s) — linux/pci.h
int pci_enable_sriov(struct pci_dev *dev, int nr_virtfn) {
  return 0;
}

// pci_find_capability — 2 module(s) — linux/pci.h
u8 pci_find_capability(struct pci_dev *dev, int cap) {
  return 0;
}

// pci_free_irq_vectors — 1 module(s) — linux/pci.h
void pci_free_irq_vectors(struct pci_dev *dev) {
}

// pci_iomap — 1 module(s) — asm-generic/pci_iomap.h
void __iomem *pci_iomap(struct pci_dev *dev, int bar, unsigned long max) {
  return NULL;
}

// pci_iounmap — 3 module(s) — asm-generic/pci_iomap.h
void pci_iounmap(struct pci_dev *dev, void __iomem *) {
}

// pci_irq_get_affinity — 1 module(s) — linux/pci.h
const struct cpumask *pci_irq_get_affinity(struct pci_dev *pdev, int vec) {
  return NULL;
}

// pci_irq_vector — 1 module(s) — linux/pci.h
int pci_irq_vector(struct pci_dev *dev, unsigned int nr) {
  return 0;
}

// pci_read_config_byte — 2 module(s) — linux/pci.h
int pci_read_config_byte(const struct pci_dev *dev, int where, u8 *val) {
  return 0;
}

// pci_read_config_dword — 2 module(s) — linux/pci.h
int pci_read_config_dword(const struct pci_dev *dev, int where, u32 *val) {
  return 0;
}

// pci_read_config_word — 1 module(s) — linux/pci.h
int pci_read_config_word(const struct pci_dev *dev, int where, u16 *val) {
  return 0;
}

// pci_release_region — 1 module(s) — linux/pci.h
void pci_release_region(struct pci_dev *, int) {
}

// pci_request_region — 1 module(s) — linux/pci.h
int pci_request_region(struct pci_dev *, int, const char *) {
  return 0;
}

// pci_set_master — 1 module(s) — linux/pci.h
void pci_set_master(struct pci_dev *dev) {
}

// pci_unregister_driver — 1 module(s) — linux/pci.h
void pci_unregister_driver(struct pci_driver *dev) {
}

// pci_vfs_assigned — 1 module(s) — linux/pci.h
int pci_vfs_assigned(struct pci_dev *dev) {
  return 0;
}

// percpu_down_write — 1 module(s) — linux/percpu-rwsem.h
void percpu_down_write(struct percpu_rw_semaphore *) {
}

// percpu_free_rwsem — 1 module(s) — linux/percpu-rwsem.h
void percpu_free_rwsem(struct percpu_rw_semaphore *) {
}

// percpu_up_write — 1 module(s) — linux/percpu-rwsem.h
void percpu_up_write(struct percpu_rw_semaphore *) {
}

// perf_trace_buf_alloc — 7 module(s) — linux/trace_events.h
void *perf_trace_buf_alloc(int size, struct pt_regs **regs, int *rctxp) {
  return NULL;
}

// phy_attached_info — 1 module(s) — linux/phy.h
void phy_attached_info(struct phy_device *phydev) {
}

// phy_disconnect — 1 module(s) — linux/phy.h
void phy_disconnect(struct phy_device *phydev) {
}

// phy_do_ioctl_running — 1 module(s) — linux/phy.h
int phy_do_ioctl_running(struct net_device *dev, struct ifreq *ifr, int cmd) {
  return 0;
}

// phy_print_status — 1 module(s) — linux/phy.h
void phy_print_status(struct phy_device *phydev) {
}

// phy_start — 1 module(s) — linux/phy.h
void phy_start(struct phy_device *phydev) {
}

// phy_stop — 1 module(s) — linux/phy.h
void phy_stop(struct phy_device *phydev) {
}

// phy_suspend — 1 module(s) — linux/phy.h
int phy_suspend(struct phy_device *phydev) {
  return 0;
}

// phylink_connect_phy — 1 module(s) — linux/phylink.h
int phylink_connect_phy(struct phylink *, struct phy_device *) {
  return 0;
}

// phylink_destroy — 1 module(s) — linux/phylink.h
void phylink_destroy(struct phylink *) {
}

// phylink_resume — 1 module(s) — linux/phylink.h
void phylink_resume(struct phylink *pl) {
}

// phylink_start — 1 module(s) — linux/phylink.h
void phylink_start(struct phylink *) {
}

// phylink_stop — 1 module(s) — linux/phylink.h
void phylink_stop(struct phylink *) {
}

// phylink_suspend — 1 module(s) — linux/phylink.h
void phylink_suspend(struct phylink *pl, bool mac_wol) {
}

// pid_vnr — 1 module(s) — linux/pid.h
pid_t pid_vnr(struct pid *pid) {
  return 0;
}

// pipe_lock — 1 module(s) — linux/pipe_fs_i.h
void pipe_lock(struct pipe_inode_info *) {
}

// pipe_unlock — 1 module(s) — linux/pipe_fs_i.h
void pipe_unlock(struct pipe_inode_info *) {
}

// posix_clock_register — 1 module(s) — linux/posix-clock.h
int posix_clock_register(struct posix_clock *clk, struct device *dev) {
  return 0;
}

// ppp_channel_index — 1 module(s) — linux/ppp_channel.h
int ppp_channel_index(struct ppp_channel *) {
  return 0;
}

// ppp_dev_name — 1 module(s) — linux/ppp_channel.h
char *ppp_dev_name(struct ppp_channel *) {
  return NULL;
}

// ppp_input — 2 module(s) — linux/ppp_channel.h
void ppp_input(struct ppp_channel *, struct sk_buff *) {
}

// ppp_register_channel — 1 module(s) — linux/ppp_channel.h
int ppp_register_channel(struct ppp_channel *) {
  return 0;
}

// pppox_compat_ioctl — 2 module(s) — linux/if_pppox.h
int pppox_compat_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg) {
  return 0;
}

// pppox_ioctl — 2 module(s) — linux/if_pppox.h
int pppox_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg) {
  return 0;
}

// pppox_unbind_sock — 2 module(s) — linux/if_pppox.h
void pppox_unbind_sock(struct sock *sk) {
}

// pps_unregister_source — 1 module(s) — linux/pps_kernel.h
void pps_unregister_source(struct pps_device *pps) {
}

// proc_dointvec_jiffies — 1 module(s) — linux/sysctl.h
int proc_dointvec_jiffies(struct ctl_table *, int, void *, size_t *, loff_t *) {
  return 0;
}

// proc_dointvec_minmax — 1 module(s) — linux/sysctl.h
int proc_dointvec_minmax(struct ctl_table *, int, void *, size_t *, loff_t *) {
  return 0;
}

// proto_register — 9 module(s) — net/sock.h
int proto_register(struct proto *prot, int alloc_slab) {
  return 0;
}

// proto_unregister — 9 module(s) — net/sock.h
void proto_unregister(struct proto *prot) {
}

// pskb_expand_head — 8 module(s) — linux/skbuff.h
int pskb_expand_head(struct sk_buff *skb, int nhead, int ntail, gfp_t gfp_mask) {
  return 0;
}

// pskb_put — 1 module(s) — linux/skbuff.h
void *pskb_put(struct sk_buff *skb, struct sk_buff *tail, int len) {
  return NULL;
}

// ptp_clock_unregister — 1 module(s) — linux/ptp_clock_kernel.h
int ptp_clock_unregister(struct ptp_clock *ptp) {
  return 0;
}

// put_cmsg — 4 module(s) — linux/socket.h
int put_cmsg(struct msghdr*, int level, int type, int len, void *data) {
  return 0;
}

// put_pid — 1 module(s) — linux/pid.h
void put_pid(struct pid *pid) {
}

// put_user_ifreq — 1 module(s) — linux/netdevice.h
int put_user_ifreq(struct ifreq *ifr, void __user *arg) {
  return 0;
}

// radix_tree_tagged — 1 module(s) — linux/radix-tree.h
int radix_tree_tagged(const struct radix_tree_root *, unsigned int tag) {
  return 0;
}

// rb_erase — 2 module(s) — linux/rbtree.h
void rb_erase(struct rb_node *, struct rb_root *) {
}

// rb_first — 2 module(s) — linux/rbtree.h
struct rb_node *rb_first(const struct rb_root *) {
  return NULL;
}

// rb_first_postorder — 1 module(s) — linux/rbtree.h
struct rb_node *rb_first_postorder(const struct rb_root *) {
  return NULL;
}

// rb_insert_color — 2 module(s) — linux/rbtree.h
void rb_insert_color(struct rb_node *, struct rb_root *) {
}

// rb_next — 2 module(s) — linux/kvm_host.h
rb_next(iter->node) {
}

// rb_next_postorder — 1 module(s) — linux/rbtree.h
struct rb_node *rb_next_postorder(const struct rb_node *) {
  return NULL;
}

// rcuref_get_slowpath — 3 module(s) — linux/rcuref.h
bool rcuref_get_slowpath(rcuref_t *ref) {
  return 0;
}

// rcuwait_wake_up — 2 module(s) — linux/rcuwait.h
int rcuwait_wake_up(struct rcuwait *w) {
  return 0;
}

// recalc_sigpending — 1 module(s) — linux/sched/signal.h
void recalc_sigpending(void) {
}

// refcount_dec_if_one — 1 module(s) — linux/refcount.h
bool refcount_dec_if_one(refcount_t *r) {
  return 0;
}

// regcache_cache_bypass — 1 module(s) — linux/regmap.h
void regcache_cache_bypass(struct regmap *map, bool enable) {
}

// regcache_cache_only — 1 module(s) — linux/regmap.h
void regcache_cache_only(struct regmap *map, bool enable) {
}

// regcache_mark_dirty — 1 module(s) — linux/regmap.h
void regcache_mark_dirty(struct regmap *map) {
}

// regcache_reg_cached — 1 module(s) — linux/regmap.h
bool regcache_reg_cached(struct regmap *map, unsigned int reg) {
  return 0;
}

// regcache_sync — 1 module(s) — linux/regmap.h
int regcache_sync(struct regmap *map) {
  return 0;
}

// register_candev — 1 module(s) — linux/can/dev.h
int register_candev(struct net_device *dev) {
  return 0;
}

// register_netdevice — 6 module(s) — linux/netdevice.h
int register_netdevice(struct net_device *dev) {
  return 0;
}

// register_oom_notifier — 1 module(s) — linux/oom.h
int register_oom_notifier(struct notifier_block *nb) {
  return 0;
}

// register_pm_notifier — 2 module(s) — linux/suspend.h
int register_pm_notifier(struct notifier_block *nb) {
  return 0;
}

// register_pppox_proto — 2 module(s) — linux/if_pppox.h
int register_pppox_proto(int proto_num, const struct pppox_proto *pp) {
  return 0;
}

// regmap_bulk_read — 1 module(s) — linux/mfd/max14577-private.h
return regmap_bulk_read(map, reg, buf, count) {
  return 0;
}

// regmap_bulk_write — 1 module(s) — linux/mfd/max14577-private.h
return regmap_bulk_write(map, reg, buf, count) {
  return 0;
}

// regmap_exit — 1 module(s) — linux/regmap.h
void regmap_exit(struct regmap *map) {
}

// regmap_raw_write — 1 module(s) — linux/mfd/da9055/core.h
return regmap_raw_write(da9055->regmap, reg, val, reg_cnt) {
  return 0;
}

// regmap_read — 1 module(s) — linux/mfd/tps65090.h
regmap_read(tps->rmap, reg, &temp_val) {
}

// regmap_write — 1 module(s) — linux/mfd/tps65090.h
return regmap_write(tps->rmap, reg, val) {
  return 0;
}

// release_firmware — 4 module(s) — linux/firmware.h
void release_firmware(const struct firmware *fw) {
}

// release_sock — 12 module(s) — net/sock.h
void release_sock(struct sock *sk) {
}

// request_firmware — 3 module(s) — linux/ihex.h
request_firmware(&lfw, fw_name, dev) {
}

// rfkill_blocked — 2 module(s) — linux/rfkill.h
bool rfkill_blocked(struct rfkill *rfkill) {
  return 0;
}

// rfkill_destroy — 2 module(s) — linux/rfkill.h
void rfkill_destroy(struct rfkill *rfkill) {
}

// rfkill_register — 2 module(s) — linux/rfkill.h
int rfkill_register(struct rfkill *rfkill) {
  return 0;
}

// rfkill_unregister — 2 module(s) — linux/rfkill.h
void rfkill_unregister(struct rfkill *rfkill) {
}

// rhashtable_destroy — 1 module(s) — linux/rhashtable.h
void rhashtable_destroy(struct rhashtable *ht) {
}

// rhashtable_walk_enter — 1 module(s) — linux/rhashtable.h
return rhashtable_walk_enter(&hlt->ht, iter) {
  return 0;
}

// rhashtable_walk_exit — 1 module(s) — linux/rhashtable.h
void rhashtable_walk_exit(struct rhashtable_iter *iter) {
}

// rhashtable_walk_next — 1 module(s) — linux/rhashtable.h
void *rhashtable_walk_next(struct rhashtable_iter *iter) {
  return NULL;
}

// rhashtable_walk_stop — 1 module(s) — linux/rhashtable.h
void rhashtable_walk_stop(struct rhashtable_iter *iter) {
}

// round_jiffies — 1 module(s) — linux/timer.h
unsigned long round_jiffies(unsigned long j) {
  return 0;
}

// rtc_month_days — 1 module(s) — linux/rtc.h
int rtc_month_days(unsigned int month, unsigned int year) {
  return 0;
}

// rtl8152_get_version — 1 module(s) — linux/usb/r8152.h
u8 rtl8152_get_version(struct usb_interface *intf) {
  return 0;
}

// rtnl_is_locked — 6 module(s) — linux/rtnetlink.h
int rtnl_is_locked(void) {
  return 0;
}

// rtnl_link_register — 7 module(s) — net/rtnetlink.h
int rtnl_link_register(struct rtnl_link_ops *ops) {
  return 0;
}

// rtnl_link_unregister — 7 module(s) — net/rtnetlink.h
void rtnl_link_unregister(struct rtnl_link_ops *ops) {
}

// rtnl_lock — 14 module(s) — linux/rtnetlink.h
void rtnl_lock(void) {
}

// rtnl_unlock — 14 module(s) — linux/rtnetlink.h
void rtnl_unlock(void) {
}

// rtnl_unregister — 1 module(s) — net/rtnetlink.h
int rtnl_unregister(int protocol, int msgtype) {
  return 0;
}

// rtnl_unregister_all — 1 module(s) — net/rtnetlink.h
void rtnl_unregister_all(int protocol) {
}

// schedule_timeout — 8 module(s) — linux/sched.h
long schedule_timeout(long timeout) {
  return 0;
}

// scnprintf — 8 module(s) — linux/sprintf.h
int scnprintf(char *buf, size_t size, const char *fmt, ...) {
  return 0;
}

// sdio_claim_host — 1 module(s) — linux/mmc/sdio_func.h
void sdio_claim_host(struct sdio_func *func) {
}

// sdio_claim_irq — 1 module(s) — linux/mmc/sdio_func.h
int sdio_claim_irq(struct sdio_func *func, sdio_irq_handler_t *handler) {
  return 0;
}

// sdio_disable_func — 1 module(s) — linux/mmc/sdio_func.h
int sdio_disable_func(struct sdio_func *func) {
  return 0;
}

// sdio_enable_func — 1 module(s) — linux/mmc/sdio_func.h
int sdio_enable_func(struct sdio_func *func) {
  return 0;
}

// sdio_readb — 1 module(s) — linux/mmc/sdio_func.h
u8 sdio_readb(struct sdio_func *func, unsigned int addr, int *err_ret) {
  return 0;
}

// sdio_register_driver — 1 module(s) — linux/mmc/sdio_func.h
int sdio_register_driver(struct sdio_driver *) {
  return 0;
}

// sdio_release_host — 1 module(s) — linux/mmc/sdio_func.h
void sdio_release_host(struct sdio_func *func) {
}

// sdio_release_irq — 1 module(s) — linux/mmc/sdio_func.h
int sdio_release_irq(struct sdio_func *func) {
  return 0;
}

// security_sk_clone — 3 module(s) — linux/security.h
void security_sk_clone(const struct sock *sk, struct sock *newsk) {
}

// security_sock_graft — 2 module(s) — linux/security.h
void security_sock_graft(struct sock*sk, struct socket *parent) {
}

// serdev_device_close — 1 module(s) — linux/serdev.h
void serdev_device_close(struct serdev_device *) {
}

// serdev_device_open — 1 module(s) — linux/serdev.h
int serdev_device_open(struct serdev_device *) {
  return 0;
}

// set_disk_ro — 1 module(s) — linux/blkdev.h
void set_disk_ro(struct gendisk *disk, bool read_only) {
}

// set_page_private — 2 module(s) — linux/mm_types.h
void set_page_private(struct page *page, unsigned long private) {
}

// set_user_nice — 2 module(s) — linux/sched.h
void set_user_nice(struct task_struct *p, long nice) {
}

// sg_init_one — 6 module(s) — linux/scatterlist.h
void sg_init_one(struct scatterlist *, const void *, unsigned int) {
}

// sg_init_table — 5 module(s) — linux/scatterlist.h
void sg_init_table(struct scatterlist *, unsigned int) {
}

// sg_nents — 1 module(s) — linux/scatterlist.h
int sg_nents(struct scatterlist *sg) {
  return 0;
}

// sg_next — 2 module(s) — linux/scatterlist.h
struct scatterlist *sg_next(struct scatterlist *) {
  return NULL;
}

// si_mem_available — 1 module(s) — linux/mm.h
long si_mem_available(void) {
  return 0;
}

// si_meminfo — 1 module(s) — linux/mm.h
void si_meminfo(struct sysinfo * val) {
}

// simple_attr_open — 1 module(s) — linux/debugfs.h
return simple_attr_open(inode, file, __get, __set, __fmt) {
  return 0;
}

// simple_attr_release — 1 module(s) — linux/fs.h
int simple_attr_release(struct inode *inode, struct file *file) {
  return 0;
}

// simple_open — 2 module(s) — linux/fs.h
int simple_open(struct inode *inode, struct file *file) {
  return 0;
}

// sk_capable — 1 module(s) — net/sock.h
bool sk_capable(const struct sock *sk, int cap) {
  return 0;
}

// sk_common_release — 1 module(s) — net/sock.h
void sk_common_release(struct sock *sk) {
}

// sk_error_report — 5 module(s) — net/sock.h
void sk_error_report(struct sock *sk) {
}

// sk_filter_trim_cap — 1 module(s) — linux/filter.h
int sk_filter_trim_cap(struct sock *sk, struct sk_buff *skb, unsigned int cap) {
  return 0;
}

// sk_free — 12 module(s) — net/sock.h
void sk_free(struct sock *sk) {
}

// sk_ioctl — 1 module(s) — net/sock.h
int sk_ioctl(struct sock *sk, unsigned int cmd, void __user *arg) {
  return 0;
}

// sk_msg_free — 1 module(s) — linux/skmsg.h
int sk_msg_free(struct sock *sk, struct sk_msg *msg) {
  return 0;
}

// sk_msg_free_nocharge — 1 module(s) — linux/skmsg.h
int sk_msg_free_nocharge(struct sock *sk, struct sk_msg *msg) {
  return 0;
}

// sk_msg_free_partial — 1 module(s) — linux/skmsg.h
void sk_msg_free_partial(struct sock *sk, struct sk_msg *msg, u32 bytes) {
}

// sk_msg_return_zero — 1 module(s) — linux/skmsg.h
void sk_msg_return_zero(struct sock *sk, struct sk_msg *msg, int bytes) {
}

// sk_msg_trim — 1 module(s) — linux/skmsg.h
void sk_msg_trim(struct sock *sk, struct sk_msg *msg, int len) {
}

// sk_psock_drop — 1 module(s) — linux/skmsg.h
void sk_psock_drop(struct sock *sk, struct sk_psock *psock) {
}

// sk_setup_caps — 1 module(s) — net/sock.h
void sk_setup_caps(struct sock *sk, struct dst_entry *dst) {
}

// sk_stop_timer — 1 module(s) — net/sock.h
void sk_stop_timer(struct sock *sk, struct timer_list *timer) {
}

// sk_stream_error — 1 module(s) — net/sock.h
int sk_stream_error(struct sock *sk, int flags, int err) {
  return 0;
}

// sk_stream_wait_memory — 1 module(s) — net/sock.h
int sk_stream_wait_memory(struct sock *sk, long *timeo_p) {
  return 0;
}

// skb_checksum_help — 1 module(s) — linux/netdevice.h
int skb_checksum_help(struct sk_buff *skb) {
  return 0;
}

// skb_clone — 14 module(s) — linux/netlink.h
skb_clone(skb, gfp_mask) {
}

// skb_copy — 6 module(s) — linux/skbuff.h
struct sk_buff *skb_copy(const struct sk_buff *skb, gfp_t priority) {
  return NULL;
}

// skb_copy_bits — 4 module(s) — linux/skbuff.h
int skb_copy_bits(const struct sk_buff *skb, int offset, void *to, int len) {
  return 0;
}

// skb_copy_header — 1 module(s) — linux/skbuff.h
void skb_copy_header(struct sk_buff *new, const struct sk_buff *old) {
}

// skb_cow_data — 2 module(s) — linux/skbuff.h
int skb_cow_data(struct sk_buff *skb, int tailbits, struct sk_buff **trailer) {
  return 0;
}

// skb_dequeue — 12 module(s) — linux/skbuff.h
struct sk_buff *skb_dequeue(struct sk_buff_head *list) {
  return NULL;
}

// skb_free_datagram — 5 module(s) — linux/skbuff.h
void skb_free_datagram(struct sock *sk, struct sk_buff *skb) {
}

// skb_pull — 19 module(s) — linux/skbuff.h
void *skb_pull(struct sk_buff *skb, unsigned int len) {
  return NULL;
}

// skb_pull_data — 2 module(s) — linux/skbuff.h
void *skb_pull_data(struct sk_buff *skb, size_t len) {
  return NULL;
}

// skb_pull_rcsum — 1 module(s) — linux/skbuff.h
void *skb_pull_rcsum(struct sk_buff *skb, unsigned int len) {
  return NULL;
}

// skb_push — 19 module(s) — linux/skbuff.h
void *skb_push(struct sk_buff *skb, unsigned int len) {
  return NULL;
}

// skb_put — 28 module(s) — linux/skbuff.h
void *skb_put(struct sk_buff *skb, unsigned int len) {
  return NULL;
}

// skb_queue_head — 7 module(s) — linux/skbuff.h
void skb_queue_head(struct sk_buff_head *list, struct sk_buff *newsk) {
}

// skb_queue_tail — 13 module(s) — linux/skbuff.h
void skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk) {
}

// skb_recv_datagram — 6 module(s) — linux/skbuff.h
struct sk_buff *skb_recv_datagram(struct sock *sk, unsigned int flags, int *err) {
  return NULL;
}

// skb_scrub_packet — 1 module(s) — linux/skbuff.h
void skb_scrub_packet(struct sk_buff *skb, bool xnet) {
}

// skb_set_owner_w — 1 module(s) — net/sock.h
void skb_set_owner_w(struct sk_buff *skb, struct sock *sk) {
}

// skb_trim — 13 module(s) — linux/skbuff.h
void skb_trim(struct sk_buff *skb, unsigned int len) {
}

// skb_unlink — 2 module(s) — linux/skbuff.h
void skb_unlink(struct sk_buff *skb, struct sk_buff_head *list) {
}

// skip_spaces — 1 module(s) — linux/string.h
char * skip_spaces(const char *) {
  return NULL;
}

// slhc_free — 1 module(s) — net/slhc_vj.h
void slhc_free(struct slcompress *comp) {
}

// slhc_init — 1 module(s) — net/slhc_vj.h
struct slcompress *slhc_init(int rslots, int tslots) {
  return NULL;
}

// slhc_remember — 1 module(s) — net/slhc_vj.h
int slhc_remember(struct slcompress *comp, unsigned char *icp, int isize) {
  return 0;
}

// slhc_toss — 1 module(s) — net/slhc_vj.h
int slhc_toss(struct slcompress *comp) {
  return 0;
}

// slhc_uncompress — 1 module(s) — net/slhc_vj.h
int slhc_uncompress(struct slcompress *comp, unsigned char *icp, int isize) {
  return 0;
}

// snd_soc_register_card — 1 module(s) — sound/soc.h
int snd_soc_register_card(struct snd_soc_card *card) {
  return 0;
}

// sock_alloc_file — 1 module(s) — linux/net.h
struct file *sock_alloc_file(struct socket *sock, int flags, const char *dname) {
  return NULL;
}

// sock_alloc_send_pskb — 5 module(s) — net/sock.h
return sock_alloc_send_pskb(sk, size, 0, noblock, errcode, 0) {
  return 0;
}

// sock_create_kern — 3 module(s) — linux/net.h
int sock_create_kern(struct net *net, int family, int type, int proto, struct socket **res) {
  return 0;
}

// sock_diag_register — 1 module(s) — linux/sock_diag.h
int sock_diag_register(const struct sock_diag_handler *h) {
  return 0;
}

// sock_diag_save_cookie — 1 module(s) — linux/sock_diag.h
void sock_diag_save_cookie(struct sock *sk, __u32 *cookie) {
}

// sock_diag_unregister — 1 module(s) — linux/sock_diag.h
void sock_diag_unregister(const struct sock_diag_handler *h) {
}

// sock_efree — 4 module(s) — net/sock.h
void sock_efree(struct sk_buff *skb) {
}

// sock_i_ino — 3 module(s) — net/sock.h
unsigned long sock_i_ino(struct sock *sk) {
  return 0;
}

// sock_i_uid — 2 module(s) — net/sock.h
kuid_t sock_i_uid(struct sock *sk) {
  return 0;
}

// sock_init_data — 7 module(s) — net/sock.h
void sock_init_data(struct socket *sock, struct sock *sk) {
}

// sock_no_accept — 9 module(s) — net/sock.h
int sock_no_accept(struct socket *, struct socket *, int, bool) {
  return 0;
}

// sock_no_bind — 5 module(s) — net/sock.h
int sock_no_bind(struct socket *, struct sockaddr *, int) {
  return 0;
}

// sock_no_connect — 4 module(s) — net/sock.h
int sock_no_connect(struct socket *, struct sockaddr *, int, int) {
  return 0;
}

// sock_no_getname — 4 module(s) — net/sock.h
int sock_no_getname(struct socket *, struct sockaddr *, int) {
  return 0;
}

// sock_no_ioctl — 1 module(s) — net/sock.h
int sock_no_ioctl(struct socket *, unsigned int, unsigned long) {
  return 0;
}

// sock_no_listen — 9 module(s) — net/sock.h
int sock_no_listen(struct socket *, int) {
  return 0;
}

// sock_no_recvmsg — 2 module(s) — net/sock.h
int sock_no_recvmsg(struct socket *, struct msghdr *, size_t, int) {
  return 0;
}

// sock_no_sendmsg — 3 module(s) — net/sock.h
int sock_no_sendmsg(struct socket *, struct msghdr *, size_t) {
  return 0;
}

// sock_no_shutdown — 8 module(s) — net/sock.h
int sock_no_shutdown(struct socket *, int) {
  return 0;
}

// sock_no_socketpair — 9 module(s) — net/sock.h
int sock_no_socketpair(struct socket *, struct socket *) {
  return 0;
}

// sock_recvmsg — 1 module(s) — linux/net.h
int sock_recvmsg(struct socket *sock, struct msghdr *msg, int flags) {
  return 0;
}

// sock_register — 6 module(s) — linux/net.h
int sock_register(const struct net_proto_family *fam) {
  return 0;
}

// sock_release — 4 module(s) — linux/net.h
void sock_release(struct socket *sock) {
}

// sock_rfree — 1 module(s) — net/sock.h
void sock_rfree(struct sk_buff *skb) {
}

// sock_unregister — 6 module(s) — linux/net.h
void sock_unregister(int family) {
}

// sockfd_lookup — 2 module(s) — linux/net.h
struct socket *sockfd_lookup(int fd, int *err) {
  return NULL;
}

// static_key_slow_dec — 1 module(s) — linux/jump_label.h
void static_key_slow_dec(struct static_key *key) {
}

// static_key_slow_inc — 1 module(s) — linux/jump_label.h
bool static_key_slow_inc(struct static_key *key) {
  return 0;
}

// strlcat — 1 module(s) — linux/string.h
size_t strlcat(char *, const char *, __kernel_size_t) {
  return 0;
}

// strscpy_pad — 1 module(s) — linux/string.h
ssize_t strscpy_pad(char *dest, const char *src, size_t count) {
  return 0;
}

// strsep — 2 module(s) — linux/string.h
char * strsep(char **,const char *) {
  return NULL;
}

// submit_bio_wait — 1 module(s) — linux/bio.h
int submit_bio_wait(struct bio *bio) {
  return 0;
}

// sync_blockdev — 1 module(s) — linux/blkdev.h
int sync_blockdev(struct block_device *bdev) {
  return 0;
}

// synchronize_irq — 1 module(s) — linux/hardirq.h
void synchronize_irq(unsigned int irq) {
}

// synchronize_net — 5 module(s) — linux/netdevice.h
void synchronize_net(void) {
}

// synchronize_srcu — 1 module(s) — linux/srcu.h
void synchronize_srcu(struct srcu_struct *ssp) {
}

// sysctl_vals — 1 module(s) — linux/sysctl.h
const int sysctl_vals[] = {0};

// sysfs_create_file_ns — 1 module(s) — linux/sysfs.h
return sysfs_create_file_ns(kobj, attr, NULL) {
  return 0;
}

// sysfs_streq — 1 module(s) — linux/string.h
bool sysfs_streq(const char *s1, const char *s2) {
  return 0;
}

// system_freezable_wq — 1 module(s) — linux/workqueue.h
struct workqueue_struct *system_freezable_wq = {0};

// system_long_wq — 1 module(s) — linux/workqueue.h
struct workqueue_struct *system_long_wq = {0};

// tasklet_kill — 5 module(s) — linux/interrupt.h
void tasklet_kill(struct tasklet_struct *t) {
}

// tasklet_unlock_wait — 2 module(s) — linux/interrupt.h
void tasklet_unlock_wait(struct tasklet_struct *t) {
}

// tcp_memory_pressure — 1 module(s) — net/tcp.h
unsigned long tcp_memory_pressure = {0};

// tcp_read_done — 1 module(s) — net/tcp.h
void tcp_read_done(struct sock *sk, size_t len) {
}

// tcp_recv_skb — 1 module(s) — net/tcp.h
struct sk_buff *tcp_recv_skb(struct sock *sk, u32 seq, u32 *off) {
  return NULL;
}

// tcp_register_ulp — 1 module(s) — net/tcp.h
int tcp_register_ulp(struct tcp_ulp_ops *type) {
  return 0;
}

// tcp_sendmsg_locked — 1 module(s) — net/tcp.h
int tcp_sendmsg_locked(struct sock *sk, struct msghdr *msg, size_t size) {
  return 0;
}

// tcp_unregister_ulp — 1 module(s) — net/tcp.h
void tcp_unregister_ulp(struct tcp_ulp_ops *type) {
}

// timecounter_read — 1 module(s) — linux/timecounter.h
u64 timecounter_read(struct timecounter *tc) {
  return 0;
}

// timer_delete_sync — 7 module(s) — linux/timer.h
int timer_delete_sync(struct timer_list *timer) {
  return 0;
}

// timer_shutdown_sync — 2 module(s) — linux/timer.h
int timer_shutdown_sync(struct timer_list *timer) {
  return 0;
}

// trace_event_printf — 7 module(s) — linux/trace_events.h
void trace_event_printf(struct trace_iterator *iter, const char *fmt, ...) {
}

// trace_event_raw_init — 7 module(s) — linux/trace_events.h
int trace_event_raw_init(struct trace_event_call *call) {
  return 0;
}

// trace_handle_return — 7 module(s) — linux/trace_events.h
enum print_line_t trace_handle_return(struct trace_seq *s) {
  return 0;
}

// trace_output_call — 2 module(s) — trace/trace_events.h
* return trace_output_call(iter, <call>, <TP_printk> "\n") {
  return NULL;
}

// trace_raw_output_prep — 7 module(s) — trace/trace_custom_events.h
trace_raw_output_prep(iter, trace_event) {
}

// tty_driver_kref_put — 3 module(s) — linux/tty_driver.h
void tty_driver_kref_put(struct tty_driver *driver) {
}

// tty_flip_buffer_push — 4 module(s) — linux/tty_flip.h
void tty_flip_buffer_push(struct tty_port *port) {
}

// tty_get_char_size — 1 module(s) — linux/tty.h
unsigned char tty_get_char_size(unsigned int cflag) {
  return 0;
}

// tty_hangup — 1 module(s) — linux/tty.h
void tty_hangup(struct tty_struct *tty) {
}

// tty_kref_put — 4 module(s) — linux/tty.h
void tty_kref_put(struct tty_struct *tty) {
}

// tty_ldisc_deref — 2 module(s) — linux/tty_ldisc.h
void tty_ldisc_deref(struct tty_ldisc *) {
}

// tty_ldisc_flush — 1 module(s) — linux/tty_ldisc.h
void tty_ldisc_flush(struct tty_struct *tty) {
}

// tty_ldisc_ref — 2 module(s) — linux/tty_ldisc.h
struct tty_ldisc *tty_ldisc_ref(struct tty_struct *) {
  return NULL;
}

// tty_mode_ioctl — 1 module(s) — linux/tty.h
int tty_mode_ioctl(struct tty_struct *tty, unsigned int cmd, unsigned long arg) {
  return 0;
}

// tty_port_destroy — 1 module(s) — linux/tty_port.h
void tty_port_destroy(struct tty_port *port) {
}

// tty_port_hangup — 3 module(s) — linux/tty_port.h
void tty_port_hangup(struct tty_port *port) {
}

// tty_port_init — 3 module(s) — linux/tty_port.h
void tty_port_init(struct tty_port *port) {
}

// tty_port_put — 2 module(s) — linux/tty_port.h
void tty_port_put(struct tty_port *port) {
}

// tty_port_tty_get — 4 module(s) — linux/tty_port.h
struct tty_struct *tty_port_tty_get(struct tty_port *port) {
  return NULL;
}

// tty_port_tty_hangup — 2 module(s) — linux/tty_port.h
void tty_port_tty_hangup(struct tty_port *port, bool check_clocal) {
}

// tty_port_tty_wakeup — 3 module(s) — linux/tty_port.h
void tty_port_tty_wakeup(struct tty_port *port) {
}

// tty_register_driver — 3 module(s) — linux/tty_driver.h
int tty_register_driver(struct tty_driver *driver) {
  return 0;
}

// tty_register_ldisc — 2 module(s) — linux/tty_ldisc.h
int tty_register_ldisc(struct tty_ldisc_ops *new_ldisc) {
  return 0;
}

// tty_set_termios — 1 module(s) — linux/tty.h
int tty_set_termios(struct tty_struct *tty, struct ktermios *kt) {
  return 0;
}

// tty_std_termios — 3 module(s) — linux/tty.h
struct ktermios tty_std_termios = {0};

// tty_termios_baud_rate — 4 module(s) — linux/tty.h
speed_t tty_termios_baud_rate(const struct ktermios *termios) {
  return 0;
}

// tty_termios_copy_hw — 1 module(s) — linux/tty.h
void tty_termios_copy_hw(struct ktermios *new, const struct ktermios *old) {
}

// tty_unregister_device — 3 module(s) — linux/tty_driver.h
void tty_unregister_device(struct tty_driver *driver, unsigned index) {
}

// tty_unregister_driver — 3 module(s) — linux/tty_driver.h
void tty_unregister_driver(struct tty_driver *driver) {
}

// tty_unregister_ldisc — 2 module(s) — linux/tty_ldisc.h
void tty_unregister_ldisc(struct tty_ldisc_ops *ldisc) {
}

// tty_unthrottle — 1 module(s) — linux/tty.h
void tty_unthrottle(struct tty_struct *tty) {
}

// tty_vhangup — 3 module(s) — linux/tty.h
void tty_vhangup(struct tty_struct *tty) {
}

// tty_wakeup — 1 module(s) — linux/tty.h
void tty_wakeup(struct tty_struct *tty) {
}

// udp_sock_create4 — 2 module(s) — net/udp_tunnel.h
return udp_sock_create4(net, cfg, sockp) {
  return 0;
}

// udp_sock_create6 — 2 module(s) — net/udp_tunnel.h
return udp_sock_create6(net, cfg, sockp) {
  return 0;
}

// unlock_page — 2 module(s) — linux/pagemap.h
void unlock_page(struct page *page) {
}

// unpin_user_pages — 1 module(s) — linux/mm.h
void unpin_user_pages(struct page **pages, unsigned long npages) {
}

// unregister_candev — 1 module(s) — linux/can/dev.h
void unregister_candev(struct net_device *dev) {
}

// unregister_shrinker — 2 module(s) — linux/shrinker.h
void unregister_shrinker(struct shrinker *shrinker) {
}

// up_read — 4 module(s) — linux/rwsem.h
void up_read(struct rw_semaphore *sem) {
}

// up_write — 5 module(s) — linux/rwsem.h
void up_write(struct rw_semaphore *sem) {
}

// usb_anchor_urb — 2 module(s) — linux/usb.h
void usb_anchor_urb(struct urb *urb, struct usb_anchor *anchor) {
}

// usb_bus_idr — 1 module(s) — linux/usb/hcd.h
struct idr usb_bus_idr = {0};

// usb_bus_idr_lock — 1 module(s) — linux/usb/hcd.h
struct mutex usb_bus_idr_lock = {0};

// usb_clear_halt — 2 module(s) — linux/usb.h
int usb_clear_halt(struct usb_device *dev, int pipe) {
  return 0;
}

// usb_debug_root — 1 module(s) — linux/usb.h
struct dentry *usb_debug_root = {0};

// usb_disabled — 1 module(s) — linux/usb.h
int usb_disabled(void) {
  return 0;
}

// usb_enable_lpm — 1 module(s) — linux/usb.h
void usb_enable_lpm(struct usb_device *udev) {
}

// usb_get_dev — 1 module(s) — linux/usb.h
struct usb_device *usb_get_dev(struct usb_device *dev) {
  return NULL;
}

// usb_get_from_anchor — 2 module(s) — linux/usb.h
struct urb *usb_get_from_anchor(struct usb_anchor *anchor) {
  return NULL;
}

// usb_get_intf — 2 module(s) — linux/usb.h
struct usb_interface *usb_get_intf(struct usb_interface *intf) {
  return NULL;
}

// usb_get_urb — 1 module(s) — linux/usb.h
struct urb *usb_get_urb(struct urb *urb) {
  return NULL;
}

// usb_mon_deregister — 1 module(s) — linux/usb/hcd.h
void usb_mon_deregister(void) {
}

// usb_mon_register — 1 module(s) — linux/usb/hcd.h
int usb_mon_register(const struct usb_mon_operations *ops) {
  return 0;
}

// usb_poison_urb — 2 module(s) — linux/usb.h
void usb_poison_urb(struct urb *urb) {
}

// usb_put_dev — 1 module(s) — linux/usb.h
void usb_put_dev(struct usb_device *dev) {
}

// usb_put_intf — 2 module(s) — linux/usb.h
void usb_put_intf(struct usb_interface *intf) {
}

// usb_register_notify — 1 module(s) — linux/usb.h
void usb_register_notify(struct notifier_block *nb) {
}

// usb_reset_device — 1 module(s) — linux/usb.h
int usb_reset_device(struct usb_device *dev) {
  return 0;
}

// usb_set_configuration — 1 module(s) — linux/usb.h
int usb_set_configuration(struct usb_device *dev, int configuration) {
  return 0;
}

// usb_set_interface — 2 module(s) — linux/usb.h
int usb_set_interface(struct usb_device *dev, int ifnum, int alternate) {
  return 0;
}

// usb_show_dynids — 1 module(s) — linux/usb.h
ssize_t usb_show_dynids(struct usb_dynids *dynids, char *buf) {
  return 0;
}

// usb_unlink_urb — 3 module(s) — linux/usb.h
int usb_unlink_urb(struct urb *urb) {
  return 0;
}

// usb_unpoison_urb — 2 module(s) — linux/usb.h
void usb_unpoison_urb(struct urb *urb) {
}

// usb_unregister_notify — 1 module(s) — linux/usb.h
void usb_unregister_notify(struct notifier_block *nb) {
}

// usbnet_cdc_bind — 1 module(s) — linux/usb/usbnet.h
int usbnet_cdc_bind(struct usbnet *, struct usb_interface *) {
  return 0;
}

// usbnet_cdc_status — 1 module(s) — linux/usb/usbnet.h
void usbnet_cdc_status(struct usbnet *, struct urb *) {
}

// usbnet_cdc_unbind — 1 module(s) — linux/usb/usbnet.h
void usbnet_cdc_unbind(struct usbnet *, struct usb_interface *) {
}

// usbnet_change_mtu — 1 module(s) — linux/usb/usbnet.h
int usbnet_change_mtu(struct net_device *net, int new_mtu) {
  return 0;
}

// usbnet_defer_kevent — 1 module(s) — linux/usb/usbnet.h
void usbnet_defer_kevent(struct usbnet *, int) {
}

// usbnet_disconnect — 7 module(s) — linux/usb/usbnet.h
void usbnet_disconnect(struct usb_interface *) {
}

// usbnet_get_drvinfo — 4 module(s) — linux/usb/usbnet.h
void usbnet_get_drvinfo(struct net_device *, struct ethtool_drvinfo *) {
}

// usbnet_get_endpoints — 5 module(s) — linux/usb/usbnet.h
int usbnet_get_endpoints(struct usbnet *, struct usb_interface *) {
  return 0;
}

// usbnet_get_link — 3 module(s) — linux/usb/usbnet.h
u32 usbnet_get_link(struct net_device *net) {
  return 0;
}

// usbnet_get_msglevel — 5 module(s) — linux/usb/usbnet.h
u32 usbnet_get_msglevel(struct net_device *) {
  return 0;
}

// usbnet_link_change — 4 module(s) — linux/usb/usbnet.h
void usbnet_link_change(struct usbnet *, bool, bool) {
}

// usbnet_manage_power — 3 module(s) — linux/usb/usbnet.h
int usbnet_manage_power(struct usbnet *, int) {
  return 0;
}

// usbnet_nway_reset — 4 module(s) — linux/usb/usbnet.h
int usbnet_nway_reset(struct net_device *net) {
  return 0;
}

// usbnet_open — 4 module(s) — linux/usb/usbnet.h
int usbnet_open(struct net_device *net) {
  return 0;
}

// usbnet_probe — 7 module(s) — linux/usb/usbnet.h
int usbnet_probe(struct usb_interface *, const struct usb_device_id *) {
  return 0;
}

// usbnet_resume — 7 module(s) — linux/usb/usbnet.h
int usbnet_resume(struct usb_interface *) {
  return 0;
}

// usbnet_set_msglevel — 5 module(s) — linux/usb/usbnet.h
void usbnet_set_msglevel(struct net_device *, u32) {
}

// usbnet_set_rx_mode — 1 module(s) — linux/usb/usbnet.h
void usbnet_set_rx_mode(struct net_device *net) {
}

// usbnet_skb_return — 5 module(s) — linux/usb/usbnet.h
void usbnet_skb_return(struct usbnet *, struct sk_buff *) {
}

// usbnet_stop — 4 module(s) — linux/usb/usbnet.h
int usbnet_stop(struct net_device *net) {
  return 0;
}

// usbnet_suspend — 7 module(s) — linux/usb/usbnet.h
int usbnet_suspend(struct usb_interface *, pm_message_t) {
  return 0;
}

// usbnet_tx_timeout — 4 module(s) — linux/usb/usbnet.h
void usbnet_tx_timeout(struct net_device *net, unsigned int txqueue) {
}

// usbnet_unlink_rx_urbs — 2 module(s) — linux/usb/usbnet.h
void usbnet_unlink_rx_urbs(struct usbnet *) {
}

// v9fs_register_trans — 1 module(s) — net/9p/transport.h
void v9fs_register_trans(struct p9_trans_module *m) {
}

// v9fs_unregister_trans — 1 module(s) — net/9p/transport.h
void v9fs_unregister_trans(struct p9_trans_module *m) {
}

// virtio_break_device — 2 module(s) — linux/virtio.h
void virtio_break_device(struct virtio_device *dev) {
}

// virtio_config_changed — 1 module(s) — linux/virtio.h
void virtio_config_changed(struct virtio_device *dev) {
}

// virtio_device_freeze — 1 module(s) — linux/virtio.h
int virtio_device_freeze(struct virtio_device *dev) {
  return 0;
}

// virtio_device_restore — 1 module(s) — linux/virtio.h
int virtio_device_restore(struct virtio_device *dev) {
  return 0;
}

// virtio_max_dma_size — 1 module(s) — linux/virtio.h
size_t virtio_max_dma_size(const struct virtio_device *vdev) {
  return 0;
}

// virtio_reset_device — 4 module(s) — linux/virtio.h
void virtio_reset_device(struct virtio_device *dev) {
}

// virtqueue_disable_cb — 3 module(s) — linux/virtio.h
void virtqueue_disable_cb(struct virtqueue *vq) {
}

// virtqueue_enable_cb — 2 module(s) — linux/virtio.h
bool virtqueue_enable_cb(struct virtqueue *vq) {
  return 0;
}

// virtqueue_get_buf — 4 module(s) — linux/virtio.h
void *virtqueue_get_buf(struct virtqueue *vq, unsigned int *len) {
  return NULL;
}

// virtqueue_is_broken — 2 module(s) — linux/virtio.h
bool virtqueue_is_broken(const struct virtqueue *vq) {
  return 0;
}

// virtqueue_kick — 4 module(s) — linux/virtio.h
bool virtqueue_kick(struct virtqueue *vq) {
  return 0;
}

// virtqueue_notify — 1 module(s) — linux/virtio.h
bool virtqueue_notify(struct virtqueue *vq) {
  return 0;
}

// vlan_dev_vlan_id — 1 module(s) — linux/if_vlan.h
u16 vlan_dev_vlan_id(const struct net_device *dev) {
  return 0;
}

// vlan_ioctl_set — 1 module(s) — linux/if_vlan.h
void vlan_ioctl_set(int (*hook)(struct net *, void __user *)) {
}

// vlan_uses_dev — 1 module(s) — linux/if_vlan.h
bool vlan_uses_dev(const struct net_device *dev) {
  return 0;
}

// vlan_vid_add — 1 module(s) — linux/if_vlan.h
int vlan_vid_add(struct net_device *dev, __be16 proto, u16 vid) {
  return 0;
}

// vlan_vid_del — 1 module(s) — linux/if_vlan.h
void vlan_vid_del(struct net_device *dev, __be16 proto, u16 vid) {
}

// vm_iomap_memory — 1 module(s) — linux/mm.h
int vm_iomap_memory(struct vm_area_struct *vma, phys_addr_t start, unsigned long len) {
  return 0;
}

// vm_node_stat — 1 module(s) — linux/vmstat.h
atomic_long_t vm_node_stat[NR_VM_NODE_STAT_ITEMS] = {0};

// vp_legacy_get_status — 1 module(s) — linux/virtio_pci_legacy.h
u8 vp_legacy_get_status(struct virtio_pci_legacy_device *ldev) {
  return 0;
}

// vp_legacy_probe — 1 module(s) — linux/virtio_pci_legacy.h
int vp_legacy_probe(struct virtio_pci_legacy_device *ldev) {
  return 0;
}

// vp_legacy_remove — 1 module(s) — linux/virtio_pci_legacy.h
void vp_legacy_remove(struct virtio_pci_legacy_device *ldev) {
}

// vp_modern_generation — 1 module(s) — linux/virtio_pci_modern.h
u32 vp_modern_generation(struct virtio_pci_modern_device *mdev) {
  return 0;
}

// vp_modern_get_status — 1 module(s) — linux/virtio_pci_modern.h
u8 vp_modern_get_status(struct virtio_pci_modern_device *mdev) {
  return 0;
}

// vp_modern_probe — 1 module(s) — linux/virtio_pci_modern.h
int vp_modern_probe(struct virtio_pci_modern_device *mdev) {
  return 0;
}

// vp_modern_remove — 1 module(s) — linux/virtio_pci_modern.h
void vp_modern_remove(struct virtio_pci_modern_device *mdev) {
}

// vring_del_virtqueue — 1 module(s) — linux/virtio_ring.h
void vring_del_virtqueue(struct virtqueue *vq) {
}

// vring_interrupt — 1 module(s) — linux/virtio_ring.h
irqreturn_t vring_interrupt(int irq, void *_vq) {
  return 0;
}

// vscnprintf — 1 module(s) — linux/sprintf.h
int vscnprintf(char *buf, size_t size, const char *fmt, va_list args) {
  return 0;
}

// vsock_core_register — 1 module(s) — net/af_vsock.h
int vsock_core_register(const struct vsock_transport *t, int features) {
  return 0;
}

// vsock_core_unregister — 1 module(s) — net/af_vsock.h
void vsock_core_unregister(const struct vsock_transport *t) {
}

// wait_woken — 5 module(s) — linux/wait.h
long wait_woken(struct wait_queue_entry *wq_entry, unsigned mode, long timeout) {
  return 0;
}

// wake_up_bit — 2 module(s) — linux/wait_bit.h
void wake_up_bit(void *word, int bit) {
}

// wake_up_process — 3 module(s) — linux/sched.h
int wake_up_process(struct task_struct *tsk) {
  return 0;
}

// woken_wake_function — 5 module(s) — linux/wait.h
int woken_wake_function(struct wait_queue_entry *wq_entry, unsigned mode, int sync, void *key) {
  return 0;
}

// work_busy — 1 module(s) — linux/workqueue.h
unsigned int work_busy(struct work_struct *work) {
  return 0;
}

// wpan_phy_free — 1 module(s) — net/cfg802154.h
void wpan_phy_free(struct wpan_phy *phy) {
}

// wpan_phy_register — 1 module(s) — net/cfg802154.h
int wpan_phy_register(struct wpan_phy *phy) {
  return 0;
}

// wpan_phy_unregister — 1 module(s) — net/cfg802154.h
void wpan_phy_unregister(struct wpan_phy *phy) {
}

// zlib_deflate — 1 module(s) — linux/zlib.h
int zlib_deflate (z_streamp strm, int flush) {
  return 0;
}

// zlib_deflateEnd — 1 module(s) — linux/zlib.h
int zlib_deflateEnd (z_streamp strm) {
  return 0;
}

// zlib_deflateReset — 1 module(s) — linux/zlib.h
int zlib_deflateReset (z_streamp strm) {
  return 0;
}

// zlib_inflate — 1 module(s) — linux/zlib.h
int zlib_inflate (z_streamp strm, int flush) {
  return 0;
}

// zlib_inflateIncomp — 1 module(s) — linux/zlib.h
int zlib_inflateIncomp (z_stream *strm) {
  return 0;
}

// zlib_inflateInit2 — 1 module(s) — linux/zlib.h
int zlib_inflateInit2(z_streamp strm, int windowBits) {
  return 0;
}

// zlib_inflateReset — 1 module(s) — linux/zlib.h
int zlib_inflateReset (z_streamp strm) {
  return 0;
}

// zs_compact — 1 module(s) — linux/zsmalloc.h
unsigned long zs_compact(struct zs_pool *pool) {
  return 0;
}

// zs_create_pool — 1 module(s) — linux/zsmalloc.h
struct zs_pool *zs_create_pool(const char *name) {
  return NULL;
}

// zs_destroy_pool — 1 module(s) — linux/zsmalloc.h
void zs_destroy_pool(struct zs_pool *pool) {
}

// zs_free — 1 module(s) — linux/zsmalloc.h
void zs_free(struct zs_pool *pool, unsigned long obj) {
}

// zs_get_total_pages — 1 module(s) — linux/zsmalloc.h
unsigned long zs_get_total_pages(struct zs_pool *pool) {
  return 0;
}

// zs_huge_class_size — 1 module(s) — linux/zsmalloc.h
size_t zs_huge_class_size(struct zs_pool *pool) {
  return 0;
}

// zs_lookup_class_index — 1 module(s) — linux/zsmalloc.h
unsigned int zs_lookup_class_index(struct zs_pool *pool, unsigned int size) {
  return 0;
}

// zs_malloc — 1 module(s) — linux/zsmalloc.h
unsigned long zs_malloc(struct zs_pool *pool, size_t size, gfp_t flags) {
  return 0;
}

// zs_pool_stats — 1 module(s) — linux/zsmalloc.h
void zs_pool_stats(struct zs_pool *pool, struct zs_pool_stats *stats) {
}

// zs_unmap_object — 1 module(s) — linux/zsmalloc.h
void zs_unmap_object(struct zs_pool *pool, unsigned long handle) {
}


// ---- Symbols without header declarations ----
// Compiler builtins, inline expansions, or module-provided symbols.

// virtio_transport[...] — 35 module(s)
void virtio_transport[...](void) { }

// refcount_warn_sa[...] — 29 module(s)
void refcount_warn_sa[...](void) { }

// kfree_skb_reason — 29 module(s)
void kfree_skb_reason(void) { }

// __list_add_valid[...] — 27 module(s)
void __list_add_valid[...](void) { }

// __list_del_entry[...] — 27 module(s)
void __list_del_entry[...](void) { }

// _raw_spin_lock_i[...] — 26 module(s)
void _raw_spin_lock_i[...](void) { }

// _raw_spin_unlock[...] — 26 module(s)
void _raw_spin_unlock[...](void) { }

// unregister_netde[...] — 20 module(s)
void unregister_netde[...](void) { }

// __tracepoint_rwm[...] — 18 module(s)
void __tracepoint_rwm[...](void) { }

// preempt_schedule[...] — 17 module(s)
void preempt_schedule[...](void) { }

// skb_queue_purge_[...] — 15 module(s)
void skb_queue_purge_[...](void) { }

// __kunit_do_faile[...] — 15 module(s)
void __kunit_do_faile[...](void) { }

// trace_event_buff[...] — 14 module(s)
void trace_event_buff[...](void) { }

// kunit_binary_ass[...] — 14 module(s)
void kunit_binary_ass[...](void) { }

// register_pernet_[...] — 12 module(s)
void register_pernet_[...](void) { }

// unregister_perne[...] — 12 module(s)
void unregister_perne[...](void) { }

// dev_addr_mod — 12 module(s)
void dev_addr_mod(void) { }

// usb_autopm_get_i[...] — 11 module(s)
void usb_autopm_get_i[...](void) { }

// register_netdevi[...] — 10 module(s)
void register_netdevi[...](void) { }

// alloc_workqueue — 10 module(s)
void alloc_workqueue(void) { }

// sock_no_mmap — 10 module(s)
void sock_no_mmap(void) { }

// _raw_spin_lock_irq — 9 module(s)
void _raw_spin_lock_irq(void) { }

// usb_autopm_put_i[...] — 9 module(s)
void usb_autopm_put_i[...](void) { }

// dev_kfree_skb_an[...] — 9 module(s)
void dev_kfree_skb_an[...](void) { }

// kasan_flag_enabled — 9 module(s)
void kasan_flag_enabled(void) { }

// kunit_ptr_not_er[...] — 8 module(s)
void kunit_ptr_not_er[...](void) { }

// __trace_trigger_[...] — 7 module(s)
void __trace_trigger_[...](void) { }

// perf_trace_run_b[...] — 7 module(s)
void perf_trace_run_b[...](void) { }

// trace_event_reg — 7 module(s)
void trace_event_reg(void) { }

// skb_copy_expand — 7 module(s)
void skb_copy_expand(void) { }

// sk_alloc — 7 module(s)
void sk_alloc(void) { }

// skb_copy_datagra[...] — 7 module(s)
void skb_copy_datagra[...](void) { }

// cancel_delayed_w[...] — 7 module(s)
void cancel_delayed_w[...](void) { }

// sock_queue_rcv_s[...] — 7 module(s)
void sock_queue_rcv_s[...](void) { }

// lowpan_nhc_add — 7 module(s)
void lowpan_nhc_add(void) { }

// lowpan_nhc_del — 7 module(s)
void lowpan_nhc_del(void) { }

// do_trace_netlink[...] — 6 module(s)
void do_trace_netlink[...](void) { }

// __page_pinner_pu[...] — 6 module(s)
void __page_pinner_pu[...](void) { }

// ethtool_op_get_t[...] — 6 module(s)
void ethtool_op_get_t[...](void) { }

// datagram_poll — 6 module(s)
void datagram_poll(void) { }

// __nla_parse — 6 module(s)
void __nla_parse(void) { }

// kunit_unary_asse[...] — 6 module(s)
void kunit_unary_asse[...](void) { }

// __tracepoint_and[...] — 6 module(s)
void __tracepoint_and[...](void) { }

// __traceiter_andr[...] — 6 module(s)
void __traceiter_andr[...](void) { }

// alloc_netdev_mqs — 5 module(s)
void alloc_netdev_mqs(void) { }

// bpf_trace_run3 — 5 module(s)
void bpf_trace_run3(void) { }

// bpf_trace_run4 — 5 module(s)
void bpf_trace_run4(void) { }

// kmem_cache_create — 5 module(s)
void kmem_cache_create(void) { }

// usbnet_read_cmd — 5 module(s)
void usbnet_read_cmd(void) { }

// sock_gettstamp — 5 module(s)
void sock_gettstamp(void) { }

// of_property_read[...] — 5 module(s)
void of_property_read[...](void) { }

// hrtimer_start_ra[...] — 5 module(s)
void hrtimer_start_ra[...](void) { }

// hrtimer_init — 5 module(s)
void hrtimer_init(void) { }

// skb_tstamp_tx — 5 module(s)
void skb_tstamp_tx(void) { }

// device_property_[...] — 5 module(s)
void device_property_[...](void) { }

// __platform_drive[...] — 5 module(s)
void __platform_drive[...](void) { }

// platform_driver_[...] — 5 module(s)
void platform_driver_[...](void) { }

// ktime_get_mono_f[...] — 5 module(s)
void ktime_get_mono_f[...](void) { }

// log_write_mmio — 5 module(s)
void log_write_mmio(void) { }

// log_post_write_mmio — 5 module(s)
void log_post_write_mmio(void) { }

// tasklet_setup — 5 module(s)
void tasklet_setup(void) { }

// usb_serial_gener[...] — 5 module(s)
void usb_serial_gener[...](void) { }

// register_virtio_[...] — 5 module(s)
void register_virtio_[...](void) { }

// unregister_virti[...] — 5 module(s)
void unregister_virti[...](void) { }

// kstrdup — 4 module(s)
void kstrdup(void) { }

// trace_print_symb[...] — 4 module(s)
void trace_print_symb[...](void) { }

// usbnet_start_xmit — 4 module(s)
void usbnet_start_xmit(void) { }

// usleep_range_state — 4 module(s)
void usleep_range_state(void) { }

// usbnet_write_cmd — 4 module(s)
void usbnet_write_cmd(void) { }

// __sock_recv_cmsgs — 4 module(s)
void __sock_recv_cmsgs(void) { }

// dev_kfree_skb_ir[...] — 4 module(s)
void dev_kfree_skb_ir[...](void) { }

// ppp_unregister_c[...] — 4 module(s)
void ppp_unregister_c[...](void) { }

// log_read_mmio — 4 module(s)
void log_read_mmio(void) { }

// log_post_read_mmio — 4 module(s)
void log_post_read_mmio(void) { }

// usb_driver_claim[...] — 4 module(s)
void usb_driver_claim[...](void) { }

// usb_driver_relea[...] — 4 module(s)
void usb_driver_relea[...](void) { }

// __tty_insert_fli[...] — 4 module(s)
void __tty_insert_fli[...](void) { }

// kunit_binary_ptr[...] — 4 module(s)
void kunit_binary_ptr[...](void) { }

// alloc_etherdev_mqs — 4 module(s)
void alloc_etherdev_mqs(void) { }

// kunit_fail_asser[...] — 4 module(s)
void kunit_fail_asser[...](void) { }

// memstart_addr — 4 module(s)
void memstart_addr(void) { }

// mutex_lock_inter[...] — 4 module(s)
void mutex_lock_inter[...](void) { }

// genlmsg_put — 4 module(s)
void genlmsg_put(void) { }

// genl_unregister_[...] — 4 module(s)
void genl_unregister_[...](void) { }

// crypto_aead_setkey — 4 module(s)
void crypto_aead_setkey(void) { }

// crypto_aead_seta[...] — 4 module(s)
void crypto_aead_seta[...](void) { }

// virtio_check_dri[...] — 4 module(s)
void virtio_check_dri[...](void) { }

// iov_iter_kvec — 3 module(s)
void iov_iter_kvec(void) { }

// netif_set_tso_ma[...] — 3 module(s)
void netif_set_tso_ma[...](void) { }

// usbnet_write_cmd[...] — 3 module(s)
void usbnet_write_cmd[...](void) { }

// usbnet_read_cmd_nopm — 3 module(s)
void usbnet_read_cmd_nopm(void) { }

// usbnet_update_ma[...] — 3 module(s)
void usbnet_update_ma[...](void) { }

// eth_platform_get[...] — 3 module(s)
void eth_platform_get[...](void) { }

// usbnet_get_link_[...] — 3 module(s)
void usbnet_get_link_[...](void) { }

// mii_ethtool_get_[...] — 3 module(s)
void mii_ethtool_get_[...](void) { }

// add_wait_queue_e[...] — 3 module(s)
void add_wait_queue_e[...](void) { }

// __init_swait_que[...] — 3 module(s)
void __init_swait_que[...](void) { }

// simple_read_from[...] — 3 module(s)
void simple_read_from[...](void) { }

// ppp_register_com[...] — 3 module(s)
void ppp_register_com[...](void) { }

// __hci_cmd_sync — 3 module(s)
void __hci_cmd_sync(void) { }

// can_rx_unregister — 3 module(s)
void can_rx_unregister(void) { }

// schedule_timeout[...] — 3 module(s)
void schedule_timeout[...](void) { }

// can_rx_register — 3 module(s)
void can_rx_register(void) { }

// __tty_alloc_driver — 3 module(s)
void __tty_alloc_driver(void) { }

// tty_port_open — 3 module(s)
void tty_port_open(void) { }

// tty_port_close — 3 module(s)
void tty_port_close(void) { }

// usb_ifnum_to_if — 3 module(s)
void usb_ifnum_to_if(void) { }

// cdc_parse_cdc_header — 3 module(s)
void cdc_parse_cdc_header(void) { }

// tty_port_registe[...] — 3 module(s)
void tty_port_registe[...](void) { }

// kunit_binary_str[...] — 3 module(s)
void kunit_binary_str[...](void) { }

// kunit_mem_assert[...] — 3 module(s)
void kunit_mem_assert[...](void) { }

// __tracepoint_mma[...] — 3 module(s)
void __tracepoint_mma[...](void) { }

// __mmap_lock_do_t[...] — 3 module(s)
void __mmap_lock_do_t[...](void) { }

// serdev_device_se[...] — 3 module(s)
void serdev_device_se[...](void) { }

// kthread_create_o[...] — 3 module(s)
void kthread_create_o[...](void) { }

// kernel_sendmsg — 3 module(s)
void kernel_sendmsg(void) { }

// kunit_running — 3 module(s)
void kunit_running(void) { }

// kunit_hooks — 3 module(s)
void kunit_hooks(void) { }

// crypto_skcipher_[...] — 3 module(s)
void crypto_skcipher_[...](void) { }

// skb_to_sgvec — 3 module(s)
void skb_to_sgvec(void) { }

// ethtool_convert_[...] — 3 module(s)
void ethtool_convert_[...](void) { }

// __unregister_chrdev — 3 module(s)
void __unregister_chrdev(void) { }

// unregister_chrde[...] — 3 module(s)
void unregister_chrde[...](void) { }

// __tracepoint_sk_[...] — 3 module(s)
void __tracepoint_sk_[...](void) { }

// __traceiter_sk_d[...] — 3 module(s)
void __traceiter_sk_d[...](void) { }

// virtqueue_get_vr[...] — 3 module(s)
void virtqueue_get_vr[...](void) { }

// virtqueue_add_inbuf — 3 module(s)
void virtqueue_add_inbuf(void) { }

// blk_queue_max_se[...] — 3 module(s)
void blk_queue_max_se[...](void) { }

// blk_queue_max_di[...] — 3 module(s)
void blk_queue_max_di[...](void) { }

// vp_modern_get_qu[...] — 3 module(s)
void vp_modern_get_qu[...](void) { }

// vp_modern_set_qu[...] — 3 module(s)
void vp_modern_set_qu[...](void) { }

// netdev_upper_dev[...] — 2 module(s)
void netdev_upper_dev[...](void) { }

// netdev_upper_dev_link — 2 module(s)
void netdev_upper_dev_link(void) { }

// netif_stacked_tr[...] — 2 module(s)
void netif_stacked_tr[...](void) { }

// generic_hwtstamp[...] — 2 module(s)
void generic_hwtstamp[...](void) { }

// proc_create_net_data — 2 module(s)
void proc_create_net_data(void) { }

// idr_alloc_u32 — 2 module(s)
void idr_alloc_u32(void) { }

// generic_mii_ioctl — 2 module(s)
void generic_mii_ioctl(void) { }

// phylink_ethtool_[...] — 2 module(s)
void phylink_ethtool_[...](void) { }

// net_selftest_get[...] — 2 module(s)
void net_selftest_get[...](void) { }

// usbnet_write_cmd_nopm — 2 module(s)
void usbnet_write_cmd_nopm(void) { }

// mii_ethtool_set_[...] — 2 module(s)
void mii_ethtool_set_[...](void) { }

// out_of_line_wait[...] — 2 module(s)
void out_of_line_wait[...](void) { }

// rfkill_alloc — 2 module(s)
void rfkill_alloc(void) { }

// unregister_pm_no[...] — 2 module(s)
void unregister_pm_no[...](void) { }

// __sock_recv_timestamp — 2 module(s)
void __sock_recv_timestamp(void) { }

// __sock_recv_wifi[...] — 2 module(s)
void __sock_recv_wifi[...](void) { }

// device_move — 2 module(s)
void device_move(void) { }

// __hci_cmd_send — 2 module(s)
void __hci_cmd_send(void) { }

// proc_create_net_[...] — 2 module(s)
void proc_create_net_[...](void) { }

// devm_gpiod_get_o[...] — 2 module(s)
void devm_gpiod_get_o[...](void) { }

// gpiod_set_value_[...] — 2 module(s)
void gpiod_set_value_[...](void) { }

// netif_napi_add_weight — 2 module(s)
void netif_napi_add_weight(void) { }

// sock_recv_errqueue — 2 module(s)
void sock_recv_errqueue(void) { }

// devm_request_thr[...] — 2 module(s)
void devm_request_thr[...](void) { }

// pm_runtime_set_a[...] — 2 module(s)
void pm_runtime_set_a[...](void) { }

// __pm_runtime_use[...] — 2 module(s)
void __pm_runtime_use[...](void) { }

// __pm_runtime_set[...] — 2 module(s)
void __pm_runtime_set[...](void) { }

// tty_standard_install — 2 module(s)
void tty_standard_install(void) { }

// usb_find_common_[...] — 2 module(s)
void usb_find_common_[...](void) { }

// usbnet_get_ether[...] — 2 module(s)
void usbnet_get_ether[...](void) { }

// __clk_hw_registe[...] — 2 module(s)
void __clk_hw_registe[...](void) { }

// clk_hw_unregiste[...] — 2 module(s)
void clk_hw_unregiste[...](void) { }

// usb_control_msg_recv — 2 module(s)
void usb_control_msg_recv(void) { }

// __kfifo_out — 2 module(s)
void __kfifo_out(void) { }

// serdev_device_wr[...] — 2 module(s)
void serdev_device_wr[...](void) { }

// device_set_wakeu[...] — 2 module(s)
void device_set_wakeu[...](void) { }

// wait_for_complet[...] — 2 module(s)
void wait_for_complet[...](void) { }

// usb_string — 2 module(s)
void usb_string(void) { }

// input_unregister[...] — 2 module(s)
void input_unregister[...](void) { }

// bt_procfs_init — 2 module(s)
void bt_procfs_init(void) { }

// bt_sock_alloc — 2 module(s)
void bt_sock_alloc(void) { }

// dev_set_mac_address — 2 module(s)
void dev_set_mac_address(void) { }

// ieee802154_hdr_p[...] — 2 module(s)
void ieee802154_hdr_p[...](void) { }

// inet_frag_reasm_[...] — 2 module(s)
void inet_frag_reasm_[...](void) { }

// register_net_sys[...] — 2 module(s)
void register_net_sys[...](void) { }

// unregister_net_s[...] — 2 module(s)
void unregister_net_s[...](void) { }

// proc_doulongvec_[...] — 2 module(s)
void proc_doulongvec_[...](void) { }

// ieee802154_max_p[...] — 2 module(s)
void ieee802154_max_p[...](void) { }

// set_normalized_t[...] — 2 module(s)
void set_normalized_t[...](void) { }

// kernel_connect — 2 module(s)
void kernel_connect(void) { }

// setup_udp_tunnel_sock — 2 module(s)
void setup_udp_tunnel_sock(void) { }

// unregister_pppox[...] — 2 module(s)
void unregister_pppox[...](void) { }

// ieee802154_mac_c[...] — 2 module(s)
void ieee802154_mac_c[...](void) { }

// netdev_rx_handle[...] — 2 module(s)
void netdev_rx_handle[...](void) { }

// class_dev_iter_init — 2 module(s)
void class_dev_iter_init(void) { }

// netdev_printk — 2 module(s)
void netdev_printk(void) { }

// __init_rwsem — 2 module(s)
void __init_rwsem(void) { }

// device_for_each_child — 2 module(s)
void device_for_each_child(void) { }

// __regmap_init — 2 module(s)
void __regmap_init(void) { }

// usb_check_bulk_e[...] — 2 module(s)
void usb_check_bulk_e[...](void) { }

// can_dropped_inva[...] — 2 module(s)
void can_dropped_inva[...](void) { }

// snd_soc_unregist[...] — 2 module(s)
void snd_soc_unregist[...](void) { }

// snd_soc_tplg_com[...] — 2 module(s)
void snd_soc_tplg_com[...](void) { }

// alloc_skb_with_frags — 2 module(s)
void alloc_skb_with_frags(void) { }

// register_shrinker — 2 module(s)
void register_shrinker(void) { }

// virtqueue_add_outbuf — 2 module(s)
void virtqueue_add_outbuf(void) { }

// __register_blkdev — 2 module(s)
void __register_blkdev(void) { }

// blk_queue_logica[...] — 2 module(s)
void blk_queue_logica[...](void) { }

// blk_queue_physic[...] — 2 module(s)
void blk_queue_physic[...](void) { }

// blk_queue_max_wr[...] — 2 module(s)
void blk_queue_max_wr[...](void) { }

// set_capacity_and[...] — 2 module(s)
void set_capacity_and[...](void) { }

// blk_mq_complete_[...] — 2 module(s)
void blk_mq_complete_[...](void) { }

// virtqueue_add_sgs — 2 module(s)
void virtqueue_add_sgs(void) { }

// virtqueue_detach[...] — 2 module(s)
void virtqueue_detach[...](void) { }

// vp_modern_queue_[...] — 2 module(s)
void vp_modern_queue_[...](void) { }

// pci_find_next_ca[...] — 2 module(s)
void pci_find_next_ca[...](void) { }

// vp_legacy_get_qu[...] — 2 module(s)
void vp_legacy_get_qu[...](void) { }

// addrconf_add_lin[...] — 1 module(s)
void addrconf_add_lin[...](void) { }

// __ndisc_fill_add[...] — 1 module(s)
void __ndisc_fill_add[...](void) { }

// addrconf_prefix_[...] — 1 module(s)
void addrconf_prefix_[...](void) { }

// netdev_update_fe[...] — 1 module(s)
void netdev_update_fe[...](void) { }

// call_netdevice_n[...] — 1 module(s)
void call_netdevice_n[...](void) { }

// dev_change_flags — 1 module(s)
void dev_change_flags(void) { }

// netif_inherit_tso_max — 1 module(s)
void netif_inherit_tso_max(void) { }

// vlan_filter_push_vids — 1 module(s)
void vlan_filter_push_vids(void) { }

// vlan_filter_drop_vids — 1 module(s)
void vlan_filter_drop_vids(void) { }

// __ethtool_get_li[...] — 1 module(s)
void __ethtool_get_li[...](void) { }

// __nla_validate — 1 module(s)
void __nla_validate(void) { }

// proc_create_sing[...] — 1 module(s)
void proc_create_sing[...](void) { }

// dev_get_stats — 1 module(s)
void dev_get_stats(void) { }

// kmem_cache_creat[...] — 1 module(s)
void kmem_cache_creat[...](void) { }

// __sock_create — 1 module(s)
void __sock_create(void) { }

// p9_parse_header — 1 module(s)
void p9_parse_header(void) { }

// usb_reset_config[...] — 1 module(s)
void usb_reset_config[...](void) { }

// usb_driver_set_c[...] — 1 module(s)
void usb_driver_set_c[...](void) { }

// mii_check_media — 1 module(s)
void mii_check_media(void) { }

// phylink_disconne[...] — 1 module(s)
void phylink_disconne[...](void) { }

// phylink_create — 1 module(s)
void phylink_create(void) { }

// usbnet_set_link_[...] — 1 module(s)
void usbnet_set_link_[...](void) { }

// phy_ethtool_nway[...] — 1 module(s)
void phy_ethtool_nway[...](void) { }

// net_selftest — 1 module(s)
void net_selftest(void) { }

// phy_ethtool_get_[...] — 1 module(s)
void phy_ethtool_get_[...](void) { }

// phy_ethtool_set_[...] — 1 module(s)
void phy_ethtool_set_[...](void) { }

// phy_connect — 1 module(s)
void phy_connect(void) { }

// proc_create_seq_[...] — 1 module(s)
void proc_create_seq_[...](void) { }

// seq_hlist_start_head — 1 module(s)
void seq_hlist_start_head(void) { }

// seq_hlist_next — 1 module(s)
void seq_hlist_next(void) { }

// __get_random_u32[...] — 1 module(s)
void __get_random_u32[...](void) { }

// memcpy_and_pad — 1 module(s)
void memcpy_and_pad(void) { }

// security_secid_t[...] — 1 module(s)
void security_secid_t[...](void) { }

// security_release[...] — 1 module(s)
void security_release[...](void) { }

// ns_to_kernel_old[...] — 1 module(s)
void ns_to_kernel_old[...](void) { }

// aes_expandkey — 1 module(s)
void aes_expandkey(void) { }

// crypto_shash_setkey — 1 module(s)
void crypto_shash_setkey(void) { }

// crypto_shash_tfm[...] — 1 module(s)
void crypto_shash_tfm[...](void) { }

// crypto_ecdh_enco[...] — 1 module(s)
void crypto_ecdh_enco[...](void) { }

// fwnode_property_[...] — 1 module(s)
void fwnode_property_[...](void) { }

// dev_coredumpv — 1 module(s)
void dev_coredumpv(void) { }

// debugfs_attr_read — 1 module(s)
void debugfs_attr_read(void) { }

// debugfs_attr_write — 1 module(s)
void debugfs_attr_write(void) { }

// efi — 1 module(s)
void efi(void) { }

// of_find_node_opt[...] — 1 module(s)
void of_find_node_opt[...](void) { }

// firmware_request[...] — 1 module(s)
void firmware_request[...](void) { }

// __hci_cmd_sync_ev — 1 module(s)
void __hci_cmd_sync_ev(void) { }

// sdio_unregister_[...] — 1 module(s)
void sdio_unregister_[...](void) { }

// sdio_writesb — 1 module(s)
void sdio_writesb(void) { }

// sdio_writeb — 1 module(s)
void sdio_writeb(void) { }

// sdio_readsb — 1 module(s)
void sdio_readsb(void) { }

// of_get_child_by_name — 1 module(s)
void of_get_child_by_name(void) { }

// rtnl_register_module — 1 module(s)
void rtnl_register_module(void) { }

// sock_cmsg_send — 1 module(s)
void sock_cmsg_send(void) { }

// __kmalloc_node_t[...] — 1 module(s)
void __kmalloc_node_t[...](void) { }

// devm_platform_io[...] — 1 module(s)
void devm_platform_io[...](void) { }

// devm_clk_get_opt[...] — 1 module(s)
void devm_clk_get_opt[...](void) { }

// usb_alloc_coherent — 1 module(s)
void usb_alloc_coherent(void) { }

// usb_free_coherent — 1 module(s)
void usb_free_coherent(void) { }

// usbnet_device_su[...] — 1 module(s)
void usbnet_device_su[...](void) { }

// usb_altnum_to_al[...] — 1 module(s)
void usb_altnum_to_al[...](void) { }

// usbnet_cdc_updat[...] — 1 module(s)
void usbnet_cdc_updat[...](void) { }

// platform_device_[...] — 1 module(s)
void platform_device_[...](void) { }

// clk_hw_get_num_p[...] — 1 module(s)
void clk_hw_get_num_p[...](void) { }

// clk_hw_init_rate[...] — 1 module(s)
void clk_hw_init_rate[...](void) { }

// clk_notifier_unr[...] — 1 module(s)
void clk_notifier_unr[...](void) { }

// __clk_mux_determ[...] — 1 module(s)
void __clk_mux_determ[...](void) { }

// clk_hw_determine[...] — 1 module(s)
void clk_hw_determine[...](void) { }

// dev_addr_add — 1 module(s)
void dev_addr_add(void) { }

// dev_addr_del — 1 module(s)
void dev_addr_del(void) { }

// tipc_nl_sk_walk — 1 module(s)
void tipc_nl_sk_walk(void) { }

// tipc_sk_fill_soc[...] — 1 module(s)
void tipc_sk_fill_soc[...](void) { }

// tipc_dump_start — 1 module(s)
void tipc_dump_start(void) { }

// tipc_dump_done — 1 module(s)
void tipc_dump_done(void) { }

// fat_time_fat2unix — 1 module(s)
void fat_time_fat2unix(void) { }

// fat_time_unix2fat — 1 module(s)
void fat_time_unix2fat(void) { }

// usb_serial_regis[...] — 1 module(s)
void usb_serial_regis[...](void) { }

// usb_serial_dereg[...] — 1 module(s)
void usb_serial_dereg[...](void) { }

// gpiochip_add_dat[...] — 1 module(s)
void gpiochip_add_dat[...](void) { }

// tty_encode_baud_rate — 1 module(s)
void tty_encode_baud_rate(void) { }

// usb_serial_handl[...] — 1 module(s)
void usb_serial_handl[...](void) { }

// __bitmap_complement — 1 module(s)
void __bitmap_complement(void) { }

// kobject_create_a[...] — 1 module(s)
void kobject_create_a[...](void) { }

// unpin_user_pages[...] — 1 module(s)
void unpin_user_pages[...](void) { }

// anon_inode_getfd — 1 module(s)
void anon_inode_getfd(void) { }

// kimage_voffset — 1 module(s)
void kimage_voffset(void) { }

// eventfd_ctx_remo[...] — 1 module(s)
void eventfd_ctx_remo[...](void) { }

// add_wait_queue_p[...] — 1 module(s)
void add_wait_queue_p[...](void) { }

// pin_user_pages — 1 module(s)
void pin_user_pages(void) { }

// arm_smccc_1_2_hvc — 1 module(s)
void arm_smccc_1_2_hvc(void) { }

// clocks_calc_mult[...] — 1 module(s)
void clocks_calc_mult[...](void) { }

// _trace_android_v[...] — 1 module(s)
void _trace_android_v[...](void) { }

// tty_termios_enco[...] — 1 module(s)
void tty_termios_enco[...](void) { }

// __percpu_init_rwsem — 1 module(s)
void __percpu_init_rwsem(void) { }

// tty_driver_flush[...] — 1 module(s)
void tty_driver_flush[...](void) { }

// n_tty_ioctl_helper — 1 module(s)
void n_tty_ioctl_helper(void) { }

// btbcm_set_bdaddr — 1 module(s)
void btbcm_set_bdaddr(void) { }

// btbcm_check_bdaddr — 1 module(s)
void btbcm_check_bdaddr(void) { }

// __serdev_device_[...] — 1 module(s)
void __serdev_device_[...](void) { }

// serdev_device_ge[...] — 1 module(s)
void serdev_device_ge[...](void) { }

// devm_regulator_b[...] — 1 module(s)
void devm_regulator_b[...](void) { }

// regulator_bulk_enable — 1 module(s)
void regulator_bulk_enable(void) { }

// regulator_bulk_d[...] — 1 module(s)
void regulator_bulk_d[...](void) { }

// btbcm_initialize — 1 module(s)
void btbcm_initialize(void) { }

// btbcm_read_pcm_i[...] — 1 module(s)
void btbcm_read_pcm_i[...](void) { }

// btbcm_write_pcm_[...] — 1 module(s)
void btbcm_write_pcm_[...](void) { }

// btbcm_finalize — 1 module(s)
void btbcm_finalize(void) { }

// serdev_device_wa[...] — 1 module(s)
void serdev_device_wa[...](void) { }

// qca_send_pre_shu[...] — 1 module(s)
void qca_send_pre_shu[...](void) { }

// gpiod_get_value_[...] — 1 module(s)
void gpiod_get_value_[...](void) { }

// qca_read_soc_version — 1 module(s)
void qca_read_soc_version(void) { }

// qca_uart_setup — 1 module(s)
void qca_uart_setup(void) { }

// qca_set_bdaddr — 1 module(s)
void qca_set_bdaddr(void) { }

// qca_set_bdaddr_rome — 1 module(s)
void qca_set_bdaddr_rome(void) { }

// hci_devcd_register — 1 module(s)
void hci_devcd_register(void) { }

// hci_devcd_append[...] — 1 module(s)
void hci_devcd_append[...](void) { }

// usb_interrupt_msg — 1 module(s)
void usb_interrupt_msg(void) { }

// __module_put_and[...] — 1 module(s)
void __module_put_and[...](void) { }

// hid_input_report — 1 module(s)
void hid_input_report(void) { }

// class_for_each_device — 1 module(s)
void class_for_each_device(void) { }

// __dev_change_net[...] — 1 module(s)
void __dev_change_net[...](void) { }

// lowpan_register_[...] — 1 module(s)
void lowpan_register_[...](void) { }

// lowpan_unregiste[...] — 1 module(s)
void lowpan_unregiste[...](void) { }

// lowpan_header_de[...] — 1 module(s)
void lowpan_header_de[...](void) { }

// inet_frag_queue_[...] — 1 module(s)
void inet_frag_queue_[...](void) { }

// lowpan_header_co[...] — 1 module(s)
void lowpan_header_co[...](void) { }

// dev_getbyhwaddr_rcu — 1 module(s)
void dev_getbyhwaddr_rcu(void) { }

// sock_common_sets[...] — 1 module(s)
void sock_common_sets[...](void) { }

// sock_common_gets[...] — 1 module(s)
void sock_common_gets[...](void) { }

// sock_common_recvmsg — 1 module(s)
void sock_common_recvmsg(void) { }

// input_get_poll_i[...] — 1 module(s)
void input_get_poll_i[...](void) { }

// input_set_poll_i[...] — 1 module(s)
void input_set_poll_i[...](void) { }

// sysfs_create_bin_file — 1 module(s)
void sysfs_create_bin_file(void) { }

// sysfs_remove_bin_file — 1 module(s)
void sysfs_remove_bin_file(void) { }

// __kunit_activate[...] — 1 module(s)
void __kunit_activate[...](void) { }

// kunit_deactivate[...] — 1 module(s)
void kunit_deactivate[...](void) { }

// kunit_destroy_re[...] — 1 module(s)
void kunit_destroy_re[...](void) { }

// kunit_remove_action — 1 module(s)
void kunit_remove_action(void) { }

// kunit_release_action — 1 module(s)
void kunit_release_action(void) { }

// register_module_[...] — 1 module(s)
void register_module_[...](void) { }

// unregister_modul[...] — 1 module(s)
void unregister_modul[...](void) { }

// pfn_is_map_memory — 1 module(s)
void pfn_is_map_memory(void) { }

// kthread_complete[...] — 1 module(s)
void kthread_complete[...](void) { }

// udp_set_csum — 1 module(s)
void udp_set_csum(void) { }

// udp6_set_csum — 1 module(s)
void udp6_set_csum(void) { }

// l2tp_session_dec[...] — 1 module(s)
void l2tp_session_dec[...](void) { }

// l2tp_session_get_nth — 1 module(s)
void l2tp_session_get_nth(void) { }

// l2tp_tunnel_dec_[...] — 1 module(s)
void l2tp_tunnel_dec_[...](void) { }

// l2tp_tunnel_get_nth — 1 module(s)
void l2tp_tunnel_get_nth(void) { }

// l2tp_udp_encap_recv — 1 module(s)
void l2tp_udp_encap_recv(void) { }

// l2tp_session_delete — 1 module(s)
void l2tp_session_delete(void) { }

// l2tp_tunnel_get — 1 module(s)
void l2tp_tunnel_get(void) { }

// l2tp_tunnel_create — 1 module(s)
void l2tp_tunnel_create(void) { }

// l2tp_tunnel_inc_[...] — 1 module(s)
void l2tp_tunnel_inc_[...](void) { }

// l2tp_tunnel_register — 1 module(s)
void l2tp_tunnel_register(void) { }

// l2tp_tunnel_delete — 1 module(s)
void l2tp_tunnel_delete(void) { }

// l2tp_tunnel_get_[...] — 1 module(s)
void l2tp_tunnel_get_[...](void) { }

// l2tp_session_create — 1 module(s)
void l2tp_session_create(void) { }

// l2tp_session_inc[...] — 1 module(s)
void l2tp_session_inc[...](void) { }

// l2tp_session_register — 1 module(s)
void l2tp_session_register(void) { }

// ppp_register_net[...] — 1 module(s)
void ppp_register_net[...](void) { }

// l2tp_session_set[...] — 1 module(s)
void l2tp_session_set[...](void) { }

// sock_wmalloc — 1 module(s)
void sock_wmalloc(void) { }

// l2tp_xmit_skb — 1 module(s)
void l2tp_xmit_skb(void) { }

// wpan_phy_new — 1 module(s)
void wpan_phy_new(void) { }

// crypto_alloc_syn[...] — 1 module(s)
void crypto_alloc_syn[...](void) { }

// nl802154_scan_done — 1 module(s)
void nl802154_scan_done(void) { }

// nl802154_scan_started — 1 module(s)
void nl802154_scan_started(void) { }

// nl802154_scan_event — 1 module(s)
void nl802154_scan_event(void) { }

// ieee802154_beaco[...] — 1 module(s)
void ieee802154_beaco[...](void) { }

// nl802154_beaconi[...] — 1 module(s)
void nl802154_beaconi[...](void) { }

// dev_fetch_sw_netstats — 1 module(s)
void dev_fetch_sw_netstats(void) { }

// netlink_register[...] — 1 module(s)
void netlink_register[...](void) { }

// netlink_unregist[...] — 1 module(s)
void netlink_unregist[...](void) { }

// of_reserved_mem_[...] — 1 module(s)
void of_reserved_mem_[...](void) { }

// devm_memremap — 1 module(s)
void devm_memremap(void) { }

// zlib_deflate_wor[...] — 1 module(s)
void zlib_deflate_wor[...](void) { }

// zlib_deflateInit2 — 1 module(s)
void zlib_deflateInit2(void) { }

// zlib_inflate_wor[...] — 1 module(s)
void zlib_inflate_wor[...](void) { }

// slhc_compress — 1 module(s)
void slhc_compress(void) { }

// iov_iter_init — 1 module(s)
void iov_iter_init(void) { }

// security_sk_clas[...] — 1 module(s)
void security_sk_clas[...](void) { }

// skb_realloc_headroom — 1 module(s)
void skb_realloc_headroom(void) { }

// kthread_delayed_[...] — 1 module(s)
void kthread_delayed_[...](void) { }

// kthread_create_worker — 1 module(s)
void kthread_create_worker(void) { }

// pps_register_source — 1 module(s)
void pps_register_source(void) { }

// kthread_destroy_[...] — 1 module(s)
void kthread_destroy_[...](void) { }

// kthread_queue_de[...] — 1 module(s)
void kthread_queue_de[...](void) { }

// kthread_cancel_d[...] — 1 module(s)
void kthread_cancel_d[...](void) { }

// posix_clock_unre[...] — 1 module(s)
void posix_clock_unre[...](void) { }

// pps_event — 1 module(s)
void pps_event(void) { }

// kthread_mod_dela[...] — 1 module(s)
void kthread_mod_dela[...](void) { }

// device_for_each_[...] — 1 module(s)
void device_for_each_[...](void) { }

// timecounter_init — 1 module(s)
void timecounter_init(void) { }

// timecounter_cyc2time — 1 module(s)
void timecounter_cyc2time(void) { }

// kvm_arm_hyp_serv[...] — 1 module(s)
void kvm_arm_hyp_serv[...](void) { }

// kvm_arch_ptp_get[...] — 1 module(s)
void kvm_arch_ptp_get[...](void) { }

// ptp_clock_register — 1 module(s)
void ptp_clock_register(void) { }

// get_device_syste[...] — 1 module(s)
void get_device_syste[...](void) { }

// usb_register_dev[...] — 1 module(s)
void usb_register_dev[...](void) { }

// usb_deregister_d[...] — 1 module(s)
void usb_deregister_d[...](void) { }

// crypto_shash_digest — 1 module(s)
void crypto_shash_digest(void) { }

// skb_add_rx_frag — 1 module(s)
void skb_add_rx_frag(void) { }

// usb_queue_reset_[...] — 1 module(s)
void usb_queue_reset_[...](void) { }

// kmalloc_node_trace — 1 module(s)
void kmalloc_node_trace(void) { }

// regmap_register_patch — 1 module(s)
void regmap_register_patch(void) { }

// regcache_drop_region — 1 module(s)
void regcache_drop_region(void) { }

// regmap_raw_read — 1 module(s)
void regmap_raw_read(void) { }

// __regmap_init_ram — 1 module(s)
void __regmap_init_ram(void) { }

// __regmap_init_raw_ram — 1 module(s)
void __regmap_init_raw_ram(void) { }

// hci_conn_security — 1 module(s)
void hci_conn_security(void) { }

// bt_sock_reclassi[...] — 1 module(s)
void bt_sock_reclassi[...](void) { }

// bt_sock_stream_r[...] — 1 module(s)
void bt_sock_stream_r[...](void) { }

// tty_port_install — 1 module(s)
void tty_port_install(void) { }

// system_power_eff[...] — 1 module(s)
void system_power_eff[...](void) { }

// round_jiffies_re[...] — 1 module(s)
void round_jiffies_re[...](void) { }

// led_trigger_unre[...] — 1 module(s)
void led_trigger_unre[...](void) { }

// usb_check_int_en[...] — 1 module(s)
void usb_check_int_en[...](void) { }

// usb_control_msg_send — 1 module(s)
void usb_control_msg_send(void) { }

// alloc_candev_mqs — 1 module(s)
void alloc_candev_mqs(void) { }

// alloc_can_err_skb — 1 module(s)
void alloc_can_err_skb(void) { }

// can_change_state — 1 module(s)
void can_change_state(void) { }

// __root_device_re[...] — 1 module(s)
void __root_device_re[...](void) { }

// root_device_unre[...] — 1 module(s)
void root_device_unre[...](void) { }

// snd_soc_componen[...] — 1 module(s)
void snd_soc_componen[...](void) { }

// snd_soc_add_component — 1 module(s)
void snd_soc_add_component(void) { }

// _snd_pcm_hw_para[...] — 1 module(s)
void _snd_pcm_hw_para[...](void) { }

// snd_soc_params_t[...] — 1 module(s)
void snd_soc_params_t[...](void) { }

// snd_soc_tdm_para[...] — 1 module(s)
void snd_soc_tdm_para[...](void) { }

// skb_try_coalesce — 1 module(s)
void skb_try_coalesce(void) { }

// __rb_insert_augmented — 1 module(s)
void __rb_insert_augmented(void) { }

// __rb_erase_color — 1 module(s)
void __rb_erase_color(void) { }

// rht_bucket_nested — 1 module(s)
void rht_bucket_nested(void) { }

// rhashtable_walk_[...] — 1 module(s)
void rhashtable_walk_[...](void) { }

// rhashtable_init — 1 module(s)
void rhashtable_init(void) { }

// sk_reset_timer — 1 module(s)
void sk_reset_timer(void) { }

// rht_bucket_neste[...] — 1 module(s)
void rht_bucket_neste[...](void) { }

// rhashtable_inser[...] — 1 module(s)
void rhashtable_inser[...](void) { }

// __rht_bucket_nested — 1 module(s)
void __rht_bucket_nested(void) { }

// ipv6_dev_find — 1 module(s)
void ipv6_dev_find(void) { }

// udp_tunnel_sock_[...] — 1 module(s)
void udp_tunnel_sock_[...](void) { }

// dst_cache_set_ip4 — 1 module(s)
void dst_cache_set_ip4(void) { }

// udp_tunnel_xmit_skb — 1 module(s)
void udp_tunnel_xmit_skb(void) { }

// ipv6_stub — 1 module(s)
void ipv6_stub(void) { }

// dst_cache_set_ip6 — 1 module(s)
void dst_cache_set_ip6(void) { }

// udp_tunnel6_xmit_skb — 1 module(s)
void udp_tunnel6_xmit_skb(void) { }

// crypto_get_defau[...] — 1 module(s)
void crypto_get_defau[...](void) { }

// crypto_put_defau[...] — 1 module(s)
void crypto_put_defau[...](void) { }

// tcp_rate_check_a[...] — 1 module(s)
void tcp_rate_check_a[...](void) { }

// iov_iter_bvec — 1 module(s)
void iov_iter_bvec(void) { }

// tcp_poll — 1 module(s)
void tcp_poll(void) { }

// sk_msg_alloc — 1 module(s)
void sk_msg_alloc(void) { }

// sk_msg_zerocopy_[...] — 1 module(s)
void sk_msg_zerocopy_[...](void) { }

// sk_msg_clone — 1 module(s)
void sk_msg_clone(void) { }

// sk_msg_memcopy_f[...] — 1 module(s)
void sk_msg_memcopy_f[...](void) { }

// iov_iter_extract[...] — 1 module(s)
void iov_iter_extract[...](void) { }

// sk_psock_msg_verdict — 1 module(s)
void sk_psock_msg_verdict(void) { }

// tcp_bpf_sendmsg_redir — 1 module(s)
void tcp_bpf_sendmsg_redir(void) { }

// iov_iter_get_pages2 — 1 module(s)
void iov_iter_get_pages2(void) { }

// sk_msg_recvmsg — 1 module(s)
void sk_msg_recvmsg(void) { }

// sk_psock_tls_str[...] — 1 module(s)
void sk_psock_tls_str[...](void) { }

// skb_splice_bits — 1 module(s)
void skb_splice_bits(void) { }

// bpf_trace_run5 — 1 module(s)
void bpf_trace_run5(void) { }

// bpf_trace_run6 — 1 module(s)
void bpf_trace_run6(void) { }

// tcp_read_sock — 1 module(s)
void tcp_read_sock(void) { }

// kfree_skb_list_reason — 1 module(s)
void kfree_skb_list_reason(void) { }

// memcg_sockets_en[...] — 1 module(s)
void memcg_sockets_en[...](void) { }

// memory_cgrp_subs[...] — 1 module(s)
void memory_cgrp_subs[...](void) { }

// usb_match_id — 1 module(s)
void usb_match_id(void) { }

// usb_match_one_id — 1 module(s)
void usb_match_one_id(void) { }

// __kfifo_alloc — 1 module(s)
void __kfifo_alloc(void) { }

// __kfifo_in — 1 module(s)
void __kfifo_in(void) { }

// usb_store_new_id — 1 module(s)
void usb_store_new_id(void) { }

// __devm_alloc_percpu — 1 module(s)
void __devm_alloc_percpu(void) { }

// devm_platform_ge[...] — 1 module(s)
void devm_platform_ge[...](void) { }

// init_on_free — 1 module(s)
void init_on_free(void) { }

// page_reporting_r[...] — 1 module(s)
void page_reporting_r[...](void) { }

// unregister_oom_n[...] — 1 module(s)
void unregister_oom_n[...](void) { }

// page_reporting_u[...] — 1 module(s)
void page_reporting_u[...](void) { }

// balloon_page_enqueue — 1 module(s)
void balloon_page_enqueue(void) { }

// adjust_managed_p[...] — 1 module(s)
void adjust_managed_p[...](void) { }

// virtqueue_disabl[...] — 1 module(s)
void virtqueue_disabl[...](void) { }

// page_relinquish — 1 module(s)
void page_relinquish(void) { }

// vm_event_states — 1 module(s)
void vm_event_states(void) { }

// post_page_relinq[...] — 1 module(s)
void post_page_relinq[...](void) { }

// __blk_mq_alloc_disk — 1 module(s)
void __blk_mq_alloc_disk(void) { }

// blk_queue_max_hw[...] — 1 module(s)
void blk_queue_max_hw[...](void) { }

// blk_queue_alignm[...] — 1 module(s)
void blk_queue_alignm[...](void) { }

// blk_mq_quiesce_q[...] — 1 module(s)
void blk_mq_quiesce_q[...](void) { }

// blk_mq_unquiesce[...] — 1 module(s)
void blk_mq_unquiesce[...](void) { }

// blk_revalidate_d[...] — 1 module(s)
void blk_revalidate_d[...](void) { }

// string_get_size — 1 module(s)
void string_get_size(void) { }

// blk_queue_chunk_[...] — 1 module(s)
void blk_queue_chunk_[...](void) { }

// blk_queue_max_zo[...] — 1 module(s)
void blk_queue_max_zo[...](void) { }

// blk_mq_start_sto[...] — 1 module(s)
void blk_mq_start_sto[...](void) { }

// sg_free_table_chained — 1 module(s)
void sg_free_table_chained(void) { }

// virtqueue_kick_p[...] — 1 module(s)
void virtqueue_kick_p[...](void) { }

// blk_mq_virtio_ma[...] — 1 module(s)
void blk_mq_virtio_ma[...](void) { }

// sg_alloc_table_c[...] — 1 module(s)
void sg_alloc_table_c[...](void) { }

// blk_mq_requeue_r[...] — 1 module(s)
void blk_mq_requeue_r[...](void) { }

// blk_mq_end_reque[...] — 1 module(s)
void blk_mq_end_reque[...](void) { }

// blk_mq_alloc_request — 1 module(s)
void blk_mq_alloc_request(void) { }

// blk_rq_map_kern — 1 module(s)
void blk_rq_map_kern(void) { }

// hvc_instantiate — 1 module(s)
void hvc_instantiate(void) { }

// __hvc_resize — 1 module(s)
void __hvc_resize(void) { }

// hvc_remove — 1 module(s)
void hvc_remove(void) { }

// hvc_alloc — 1 module(s)
void hvc_alloc(void) { }

// hvc_poll — 1 module(s)
void hvc_poll(void) { }

// hvc_kick — 1 module(s)
void hvc_kick(void) { }

// __splice_from_pipe — 1 module(s)
void __splice_from_pipe(void) { }

// vp_modern_config[...] — 1 module(s)
void vp_modern_config[...](void) { }

// vp_modern_get_nu[...] — 1 module(s)
void vp_modern_get_nu[...](void) { }

// vring_create_vir[...] — 1 module(s)
void vring_create_vir[...](void) { }

// vp_modern_map_vq[...] — 1 module(s)
void vp_modern_map_vq[...](void) { }

// vp_modern_set_status — 1 module(s)
void vp_modern_set_status(void) { }

// vp_modern_get_fe[...] — 1 module(s)
void vp_modern_get_fe[...](void) { }

// vring_transport_[...] — 1 module(s)
void vring_transport_[...](void) { }

// pci_find_ext_cap[...] — 1 module(s)
void pci_find_ext_cap[...](void) { }

// vp_modern_set_fe[...] — 1 module(s)
void vp_modern_set_fe[...](void) { }

// virtqueue_get_de[...] — 1 module(s)
void virtqueue_get_de[...](void) { }

// virtqueue_get_av[...] — 1 module(s)
void virtqueue_get_av[...](void) { }

// virtqueue_get_us[...] — 1 module(s)
void virtqueue_get_us[...](void) { }

// vring_notificati[...] — 1 module(s)
void vring_notificati[...](void) { }

// __irq_apply_affi[...] — 1 module(s)
void __irq_apply_affi[...](void) { }

// pci_alloc_irq_ve[...] — 1 module(s)
void pci_alloc_irq_ve[...](void) { }

// __pci_register_driver — 1 module(s)
void __pci_register_driver(void) { }

// vp_legacy_config[...] — 1 module(s)
void vp_legacy_config[...](void) { }

// vp_legacy_set_qu[...] — 1 module(s)
void vp_legacy_set_qu[...](void) { }

// vp_legacy_queue_[...] — 1 module(s)
void vp_legacy_queue_[...](void) { }

// vp_legacy_set_status — 1 module(s)
void vp_legacy_set_status(void) { }

// vp_legacy_get_fe[...] — 1 module(s)
void vp_legacy_get_fe[...](void) { }

// vp_legacy_set_fe[...] — 1 module(s)
void vp_legacy_set_fe[...](void) { }

// pci_request_sele[...] — 1 module(s)
void pci_request_sele[...](void) { }

// pci_release_sele[...] — 1 module(s)
void pci_release_sele[...](void) { }

// pci_iomap_range — 1 module(s)
void pci_iomap_range(void) { }

// vsock_for_each_c[...] — 1 module(s)
void vsock_for_each_c[...](void) { }

// device_find_chil[...] — 1 module(s)
void device_find_chil[...](void) { }

// rtnl_create_link — 1 module(s)
void rtnl_create_link(void) { }

// rtnl_configure_link — 1 module(s)
void rtnl_configure_link(void) { }

// crypto_comp_compress — 1 module(s)
void crypto_comp_compress(void) { }

// crypto_comp_deco[...] — 1 module(s)
void crypto_comp_deco[...](void) { }

// __cpuhp_state_re[...] — 1 module(s)
void __cpuhp_state_re[...](void) { }

// __cpuhp_state_ad[...] — 1 module(s)
void __cpuhp_state_ad[...](void) { }

// idr_for_each — 1 module(s)
void idr_for_each(void) { }

// zs_map_object — 1 module(s)
void zs_map_object(void) { }

// flush_dcache_page — 1 module(s)
void flush_dcache_page(void) { }

// bio_end_io_acct_[...] — 1 module(s)
void bio_end_io_acct_[...](void) { }

// __bio_add_page — 1 module(s)
void __bio_add_page(void) { }

// blkdev_get_by_dev — 1 module(s)
void blkdev_get_by_dev(void) { }

// bio_init — 1 module(s)
void bio_init(void) { }


// ---- Additional stubs for remaining 365 symbols ----
// Added to cover full-width symbol names.

// __cpuhp_state_add_instance -- 1 module(s)
return __cpuhp_state_add_instance(state, node, true) {
  return 0;
}

// __cpuhp_state_remove_instance -- 1 module(s)
return __cpuhp_state_remove_instance(state, node, true) {
  return 0;
}

// __dev_change_net_namespace -- 1 module(s)
return __dev_change_net_namespace(dev, net, pat, 0) {
  return 0;
}

// __get_random_u32_below -- 1 module(s)
u32 __get_random_u32_below(u32 ceil) {
  return 0;
}

// __irq_apply_affinity_hint -- 1 module(s)
return __irq_apply_affinity_hint(irq, m, false) {
  return 0;
}

// __mmap_lock_do_trace_released -- 1 module(s)
void __mmap_lock_do_trace_released(struct mm_struct *mm, bool write) {
}

// __mmap_lock_do_trace_start_locking -- 1 module(s)
void __mmap_lock_do_trace_start_locking(struct mm_struct *mm, bool write) {
}

// __page_pinner_put_page -- 6 module(s)
void __page_pinner_put_page(struct page *page) {
}

// __pm_runtime_set_status -- 2 module(s)
int __pm_runtime_set_status(struct device *dev, unsigned int status) {
  return 0;
}

// __pm_runtime_use_autosuspend -- 2 module(s)
void __pm_runtime_use_autosuspend(struct device *dev, bool use) {
}

// __root_device_register -- 1 module(s)
struct device *__root_device_register(const char *name, struct module *owner) {
  return NULL;
}

// __serdev_device_driver_register -- 1 module(s)
int __serdev_device_driver_register(struct serdev_device_driver *, struct module *) {
  return 0;
}

// __trace_trigger_soft_disabled -- 7 module(s)
bool __trace_trigger_soft_disabled(struct trace_event_file *file) {
  return 0;
}

// __tty_insert_flip_string_flags -- 4 module(s)
return __tty_insert_flip_string_flags(port, chars, &flag, false, size) {
  return 0;
}

// _snd_pcm_hw_params_any -- 1 module(s)
void _snd_pcm_hw_params_any(struct snd_pcm_hw_params *params) {
}

// add_wait_queue_exclusive -- 3 module(s)
void add_wait_queue_exclusive(struct wait_queue_head *wq_head, struct wait_queue_entry *wq_entry) {
}

// add_wait_queue_priority -- 1 module(s)
void add_wait_queue_priority(struct wait_queue_head *wq_head, struct wait_queue_entry *wq_entry) {
}

// adjust_managed_page_count -- 1 module(s)
void adjust_managed_page_count(struct page *page, long count) {
}

// bio_end_io_acct_remapped -- 1 module(s)
return bio_end_io_acct_remapped(bio, start_time, bio->bi_bdev) {
  return 0;
}

// blk_mq_complete_request_remote -- 1 module(s)
bool blk_mq_complete_request_remote(struct request *rq) {
  return 0;
}

// blk_mq_end_request_batch -- 1 module(s)
void blk_mq_end_request_batch(struct io_comp_batch *ib) {
}

// blk_mq_quiesce_queue_nowait -- 1 module(s)
void blk_mq_quiesce_queue_nowait(struct request_queue *q) {
}

// blk_mq_unquiesce_queue -- 1 module(s)
void blk_mq_unquiesce_queue(struct request_queue *q) {
}

// blk_queue_chunk_sectors -- 1 module(s)
void blk_queue_chunk_sectors(struct request_queue *, unsigned int) {
}

// bt_sock_reclassify_lock -- 1 module(s)
void bt_sock_reclassify_lock(struct sock *sk, int proto) {
}

// call_netdevice_notifiers -- 1 module(s)
int call_netdevice_notifiers(unsigned long val, struct net_device *dev) {
  return 0;
}

// can_dropped_invalid_skb -- 2 module(s)
return can_dropped_invalid_skb(dev, skb) {
  return 0;
}

// clk_hw_get_num_parents -- 1 module(s)
unsigned int clk_hw_get_num_parents(const struct clk_hw *hw) {
  return 0;
}

// clk_notifier_unregister -- 1 module(s)
int clk_notifier_unregister(struct clk *clk, struct notifier_block *nb) {
  return 0;
}

// clocks_calc_mult_shift -- 1 module(s)
return clocks_calc_mult_shift(&ce->mult, &ce->shift, NSEC_PER_SEC, freq, maxsec) {
  return 0;
}

// crypto_aead_setauthsize -- 4 module(s)
int crypto_aead_setauthsize(struct crypto_aead *tfm, unsigned int authsize) {
  return 0;
}

// crypto_ecdh_encode_key -- 1 module(s)
int crypto_ecdh_encode_key(char *buf, unsigned int len, const struct ecdh *p) {
  return 0;
}

// crypto_get_default_rng -- 1 module(s)
int crypto_get_default_rng(void) {
  return 0;
}

// crypto_put_default_rng -- 1 module(s)
void crypto_put_default_rng(void) {
}

// crypto_skcipher_decrypt -- 1 module(s)
int crypto_skcipher_decrypt(struct skcipher_request *req) {
  return 0;
}

// crypto_skcipher_encrypt -- 1 module(s)
int crypto_skcipher_encrypt(struct skcipher_request *req) {
  return 0;
}

// crypto_skcipher_setkey -- 1 module(s)
return crypto_skcipher_setkey(&tfm->base, key, keylen) {
  return 0;
}

// dev_kfree_skb_any_reason -- 9 module(s)
void dev_kfree_skb_any_reason(struct sk_buff *skb, enum skb_drop_reason reason) {
}

// dev_kfree_skb_irq_reason -- 4 module(s)
void dev_kfree_skb_irq_reason(struct sk_buff *skb, enum skb_drop_reason reason) {
}

// device_property_present -- 1 module(s)
bool device_property_present(const struct device *dev, const char *propname) {
  return 0;
}

// device_property_read_u32_array -- 2 module(s)
return device_property_read_u32_array(dev, propname, val, 1) {
  return 0;
}

// device_property_read_u8_array -- 1 module(s)
return device_property_read_u8_array(dev, propname, val, 1) {
  return 0;
}

// device_set_wakeup_capable -- 1 module(s)
void device_set_wakeup_capable(struct device *dev, bool capable) {
}

// device_set_wakeup_enable -- 1 module(s)
int device_set_wakeup_enable(struct device *dev, bool enable) {
  return 0;
}

// devm_clk_get_optional_enabled -- 1 module(s)
struct clk *devm_clk_get_optional_enabled(struct device *dev, const char *id) {
  return NULL;
}

// do_trace_netlink_extack -- 6 module(s)
void do_trace_netlink_extack(const char *msg) {
}

// eth_platform_get_mac_address -- 3 module(s)
int eth_platform_get_mac_address(struct device *dev, u8 *mac_addr) {
  return 0;
}

// ethtool_op_get_ts_info -- 6 module(s)
int ethtool_op_get_ts_info(struct net_device *dev, struct ethtool_ts_info *eti) {
  return 0;
}

// fwnode_property_read_u8_array -- 1 module(s)
return fwnode_property_read_u8_array(fwnode, propname, val, 1) {
  return 0;
}

// genl_unregister_family -- 4 module(s)
int genl_unregister_family(const struct genl_family *family) {
  return 0;
}

// gpiod_get_value_cansleep -- 1 module(s)
int gpiod_get_value_cansleep(const struct gpio_desc *desc) {
  return 0;
}

// gpiod_set_value_cansleep -- 2 module(s)
void gpiod_set_value_cansleep(struct gpio_desc *desc, int value) {
}

// hci_devcd_append_pattern -- 1 module(s)
int hci_devcd_append_pattern(struct hci_dev *hdev, u8 pattern, u32 len) {
  return 0;
}

// ieee802154_max_payload -- 2 module(s)
int ieee802154_max_payload(const struct ieee802154_hdr *hdr) {
  return 0;
}

// kthread_cancel_delayed_work_sync -- 1 module(s)
bool kthread_cancel_delayed_work_sync(struct kthread_delayed_work *work) {
  return 0;
}

// kthread_delayed_work_timer_fn -- 1 module(s)
void kthread_delayed_work_timer_fn(struct timer_list *t) {
}

// kthread_destroy_worker -- 1 module(s)
void kthread_destroy_worker(struct kthread_worker *worker) {
}

// ktime_get_mono_fast_ns -- 5 module(s)
ktime_get_mono_fast_ns()) {
}

// kunit_deactivate_static_stub -- 1 module(s)
void kunit_deactivate_static_stub(struct kunit *test, void *real_fn_addr) {
}

// lowpan_unregister_netdevice -- 1 module(s)
void lowpan_unregister_netdevice(struct net_device *dev) {
}

// memcg_sockets_enabled_key -- 1 module(s)
struct static_key_false memcg_sockets_enabled_key = {0};

// mutex_lock_interruptible -- 4 module(s)
return mutex_lock_interruptible(&dev->mutex) {
  return 0;
}

// net_selftest_get_count -- 1 module(s)
int net_selftest_get_count(void) {
  return 0;
}

// net_selftest_get_strings -- 1 module(s)
void net_selftest_get_strings(u8 *data) {
}

// netdev_rx_handler_unregister -- 1 module(s)
void netdev_rx_handler_unregister(struct net_device *dev) {
}

// netdev_update_features -- 1 module(s)
void netdev_update_features(struct net_device *dev) {
}

// netif_set_tso_max_size -- 3 module(s)
void netif_set_tso_max_size(struct net_device *dev, unsigned int size) {
}

// netlink_register_notifier -- 1 module(s)
int netlink_register_notifier(struct notifier_block *nb) {
  return 0;
}

// netlink_unregister_notifier -- 1 module(s)
int netlink_unregister_notifier(struct notifier_block *nb) {
  return 0;
}

// ns_to_kernel_old_timeval -- 1 module(s)
ns_to_kernel_old_timeval(nsec) {
}

// of_find_node_opts_by_path -- 1 module(s)
return of_find_node_opts_by_path(path, NULL) {
  return 0;
}

// of_property_read_string_helper -- 1 module(s)
return of_property_read_string_helper(np, propname, out_strs, sz, 0) {
  return 0;
}

// of_reserved_mem_lookup -- 1 module(s)
struct reserved_mem *of_reserved_mem_lookup(struct device_node *np) {
  return NULL;
}

// out_of_line_wait_on_bit -- 1 module(s)
int out_of_line_wait_on_bit(void *word, int, wait_bit_action_f *action, unsigned int mode) {
  return 0;
}

// out_of_line_wait_on_bit_timeout -- 1 module(s)
int out_of_line_wait_on_bit_timeout(void *word, int, wait_bit_action_f *action, unsigned int mode, unsigned long timeout) {
  return 0;
}

// page_reporting_register -- 1 module(s)
int page_reporting_register(struct page_reporting_dev_info *prdev) {
  return 0;
}

// page_reporting_unregister -- 1 module(s)
void page_reporting_unregister(struct page_reporting_dev_info *prdev) {
}

// pci_find_ext_capability -- 1 module(s)
u16 pci_find_ext_capability(struct pci_dev *dev, int cap) {
  return 0;
}

// pci_find_next_capability -- 2 module(s)
u8 pci_find_next_capability(struct pci_dev *dev, u8 pos, int cap) {
  return 0;
}

// pci_release_selected_regions -- 1 module(s)
void pci_release_selected_regions(struct pci_dev *, int) {
}

// pci_request_selected_regions -- 1 module(s)
int pci_request_selected_regions(struct pci_dev *, int, const char *) {
  return 0;
}

// phy_ethtool_nway_reset -- 1 module(s)
int phy_ethtool_nway_reset(struct net_device *ndev) {
  return 0;
}

// phylink_disconnect_phy -- 1 module(s)
void phylink_disconnect_phy(struct phylink *) {
}

// posix_clock_unregister -- 1 module(s)
void posix_clock_unregister(struct posix_clock *clk) {
}

// ppp_register_compressor -- 3 module(s)
int ppp_register_compressor(struct compressor *) {
  return 0;
}

// ppp_register_net_channel -- 1 module(s)
int ppp_register_net_channel(struct net *, struct ppp_channel *) {
  return 0;
}

// ppp_unregister_channel -- 1 module(s)
void ppp_unregister_channel(struct ppp_channel *) {
}

// ppp_unregister_compressor -- 3 module(s)
void ppp_unregister_compressor(struct compressor *) {
}

// proc_doulongvec_minmax -- 2 module(s)
int proc_doulongvec_minmax(struct ctl_table *, int, void *, size_t *, loff_t *) {
  return 0;
}

// register_module_notifier -- 1 module(s)
int register_module_notifier(struct notifier_block *nb) {
  return 0;
}

// register_netdevice_notifier -- 10 module(s)
int register_netdevice_notifier(struct notifier_block *nb) {
  return 0;
}

// register_pernet_device -- 5 module(s)
int register_pernet_device(struct pernet_operations *) {
  return 0;
}

// register_pernet_subsys -- 7 module(s)
int register_pernet_subsys(struct pernet_operations *) {
  return 0;
}

// register_virtio_device -- 1 module(s)
int register_virtio_device(struct virtio_device *dev) {
  return 0;
}

// register_virtio_driver -- 4 module(s)
int register_virtio_driver(struct virtio_driver *drv) {
  return 0;
}

// rhashtable_insert_slow -- 1 module(s)
return rhashtable_insert_slow(ht, key, obj) {
  return 0;
}

// rhashtable_walk_start_check -- 1 module(s)
int rhashtable_walk_start_check(struct rhashtable_iter *iter) {
  return 0;
}

// root_device_unregister -- 1 module(s)
void root_device_unregister(struct device *root) {
}

// schedule_timeout_interruptible -- 1 module(s)
long schedule_timeout_interruptible(long timeout) {
  return 0;
}

// schedule_timeout_uninterruptible -- 2 module(s)
long schedule_timeout_uninterruptible(long timeout) {
  return 0;
}

// sdio_unregister_driver -- 1 module(s)
void sdio_unregister_driver(struct sdio_driver *) {
}

// security_release_secctx -- 1 module(s)
void security_release_secctx(char *secdata, u32 seclen) {
}

// security_secid_to_secctx -- 1 module(s)
int security_secid_to_secctx(u32 secid, char **secdata, u32 *seclen) {
  return 0;
}

// serdev_device_get_tiocm -- 1 module(s)
int serdev_device_get_tiocm(struct serdev_device *) {
  return 0;
}

// serdev_device_set_baudrate -- 1 module(s)
unsigned int serdev_device_set_baudrate(struct serdev_device *, unsigned int) {
  return 0;
}

// serdev_device_set_flow_control -- 1 module(s)
void serdev_device_set_flow_control(struct serdev_device *, bool) {
}

// serdev_device_set_tiocm -- 1 module(s)
int serdev_device_set_tiocm(struct serdev_device *, int, int) {
  return 0;
}

// serdev_device_wait_until_sent -- 1 module(s)
void serdev_device_wait_until_sent(struct serdev_device *, long) {
}

// serdev_device_write_buf -- 1 module(s)
int serdev_device_write_buf(struct serdev_device *, const unsigned char *, size_t) {
  return 0;
}

// serdev_device_write_flush -- 1 module(s)
void serdev_device_write_flush(struct serdev_device *) {
}

// set_capacity_and_notify -- 2 module(s)
bool set_capacity_and_notify(struct gendisk *disk, sector_t size) {
  return 0;
}

// set_normalized_timespec64 -- 2 module(s)
void set_normalized_timespec64(struct timespec64 *ts, time64_t sec, s64 nsec) {
}

// sk_psock_tls_strp_read -- 1 module(s)
int sk_psock_tls_strp_read(struct sk_psock *psock, struct sk_buff *skb) {
  return 0;
}

// skb_copy_datagram_iter -- 7 module(s)
return skb_copy_datagram_iter(from, offset, &msg->msg_iter, size) {
  return 0;
}

// snd_soc_params_to_bclk -- 1 module(s)
int snd_soc_params_to_bclk(struct snd_pcm_hw_params *parms) {
  return 0;
}

// snd_soc_tplg_component_remove -- 1 module(s)
int snd_soc_tplg_component_remove(struct snd_soc_component *comp) {
  return 0;
}

// snd_soc_unregister_card -- 1 module(s)
void snd_soc_unregister_card(struct snd_soc_card *card) {
}

// snd_soc_unregister_component -- 1 module(s)
void snd_soc_unregister_component(struct device *dev) {
}

// sock_queue_rcv_skb_reason -- 7 module(s)
return sock_queue_rcv_skb_reason(sk, skb, NULL) {
  return 0;
}

// tcp_rate_check_app_limited -- 1 module(s)
void tcp_rate_check_app_limited(struct sock *sk) {
}

// trace_event_buffer_commit -- 7 module(s)
void trace_event_buffer_commit(struct trace_event_buffer *fbuffer) {
}

// tty_driver_flush_buffer -- 1 module(s)
void tty_driver_flush_buffer(struct tty_struct *tty) {
}

// udp_tunnel_sock_release -- 1 module(s)
void udp_tunnel_sock_release(struct socket *sock) {
}

// unregister_module_notifier -- 1 module(s)
int unregister_module_notifier(struct notifier_block *nb) {
  return 0;
}

// unregister_net_sysctl_table -- 2 module(s)
void unregister_net_sysctl_table(struct ctl_table_header *header) {
}

// unregister_netdevice_many -- 4 module(s)
void unregister_netdevice_many(struct list_head *head) {
}

// unregister_netdevice_notifier -- 10 module(s)
int unregister_netdevice_notifier(struct notifier_block *nb) {
  return 0;
}

// unregister_netdevice_queue -- 6 module(s)
void unregister_netdevice_queue(struct net_device *dev, struct list_head *head) {
}

// unregister_oom_notifier -- 1 module(s)
int unregister_oom_notifier(struct notifier_block *nb) {
  return 0;
}

// unregister_pernet_device -- 5 module(s)
void unregister_pernet_device(struct pernet_operations *) {
}

// unregister_pernet_subsys -- 7 module(s)
void unregister_pernet_subsys(struct pernet_operations *) {
}

// unregister_pm_notifier -- 2 module(s)
int unregister_pm_notifier(struct notifier_block *nb) {
  return 0;
}

// unregister_pppox_proto -- 2 module(s)
void unregister_pppox_proto(int proto_num) {
}

// unregister_virtio_device -- 1 module(s)
void unregister_virtio_device(struct virtio_device *dev) {
}

// unregister_virtio_driver -- 4 module(s)
void unregister_virtio_driver(struct virtio_driver *drv) {
}

// usb_autopm_get_interface -- 6 module(s)
int usb_autopm_get_interface(struct usb_interface *intf) {
  return 0;
}

// usb_autopm_get_interface_async -- 3 module(s)
int usb_autopm_get_interface_async(struct usb_interface *intf) {
  return 0;
}

// usb_autopm_get_interface_no_resume -- 2 module(s)
void usb_autopm_get_interface_no_resume(struct usb_interface *intf) {
}

// usb_autopm_put_interface -- 6 module(s)
void usb_autopm_put_interface(struct usb_interface *intf) {
}

// usb_autopm_put_interface_async -- 3 module(s)
void usb_autopm_put_interface_async(struct usb_interface *intf) {
}

// usb_deregister_device_driver -- 1 module(s)
void usb_deregister_device_driver(struct usb_device_driver *) {
}

// usb_driver_set_configuration -- 1 module(s)
int usb_driver_set_configuration(struct usb_device *udev, int config) {
  return 0;
}

// usb_find_common_endpoints -- 2 module(s)
return usb_find_common_endpoints(alt, bulk_in, NULL, NULL, NULL) {
  return 0;
}

// usb_queue_reset_device -- 1 module(s)
void usb_queue_reset_device(struct usb_interface *dev) {
}

// usb_reset_configuration -- 1 module(s)
int usb_reset_configuration(struct usb_device *dev) {
  return 0;
}

// usb_serial_deregister_drivers -- 1 module(s)
void usb_serial_deregister_drivers(struct usb_serial_driver *const serial_drivers[]) {
}

// usb_serial_generic_get_icount -- 1 module(s)
int usb_serial_generic_get_icount(struct tty_struct *tty, struct serial_icounter_struct *icount) {
  return 0;
}

// usb_serial_generic_open -- 1 module(s)
int usb_serial_generic_open(struct tty_struct *tty, struct usb_serial_port *port) {
  return 0;
}

// usb_serial_generic_throttle -- 1 module(s)
void usb_serial_generic_throttle(struct tty_struct *tty) {
}

// usb_serial_generic_tiocmiwait -- 1 module(s)
int usb_serial_generic_tiocmiwait(struct tty_struct *tty, unsigned long arg) {
  return 0;
}

// usb_serial_generic_unthrottle -- 1 module(s)
void usb_serial_generic_unthrottle(struct tty_struct *tty) {
}

// usbnet_cdc_update_filter -- 1 module(s)
void usbnet_cdc_update_filter(struct usbnet *dev) {
}

// usbnet_device_suggests_idle -- 1 module(s)
void usbnet_device_suggests_idle(struct usbnet *dev) {
}

// usbnet_get_ethernet_addr -- 2 module(s)
int usbnet_get_ethernet_addr(struct usbnet *, int) {
  return 0;
}

// usbnet_update_max_qlen -- 3 module(s)
void usbnet_update_max_qlen(struct usbnet *dev) {
}

// virtio_transport_connect -- 1 module(s)
int virtio_transport_connect(struct vsock_sock *vsk) {
  return 0;
}

// virtio_transport_deliver_tap_pkt -- 1 module(s)
void virtio_transport_deliver_tap_pkt(struct sk_buff *skb) {
}

// virtio_transport_destruct -- 1 module(s)
void virtio_transport_destruct(struct vsock_sock *vsk) {
}

// virtio_transport_dgram_allow -- 1 module(s)
bool virtio_transport_dgram_allow(u32 cid, u32 port) {
  return 0;
}

// virtio_transport_notify_buffer_size -- 1 module(s)
void virtio_transport_notify_buffer_size(struct vsock_sock *vsk, u64 *val) {
}

// virtio_transport_notify_set_rcvlowat -- 1 module(s)
int virtio_transport_notify_set_rcvlowat(struct vsock_sock *vsk, int val) {
  return 0;
}

// virtio_transport_purge_skbs -- 1 module(s)
int virtio_transport_purge_skbs(void *vsk, struct sk_buff_head *list) {
  return 0;
}

// virtio_transport_read_skb -- 1 module(s)
int virtio_transport_read_skb(struct vsock_sock *vsk, skb_read_actor_t read_actor) {
  return 0;
}

// virtio_transport_release -- 1 module(s)
void virtio_transport_release(struct vsock_sock *vsk) {
}

// virtio_transport_seqpacket_has_data -- 1 module(s)
u32 virtio_transport_seqpacket_has_data(struct vsock_sock *vsk) {
  return 0;
}

// virtio_transport_shutdown -- 1 module(s)
int virtio_transport_shutdown(struct vsock_sock *vsk, int mode) {
  return 0;
}

// virtio_transport_stream_allow -- 1 module(s)
bool virtio_transport_stream_allow(u32 cid, u32 port) {
  return 0;
}

// virtio_transport_stream_has_data -- 1 module(s)
s64 virtio_transport_stream_has_data(struct vsock_sock *vsk) {
  return 0;
}

// virtio_transport_stream_has_space -- 1 module(s)
s64 virtio_transport_stream_has_space(struct vsock_sock *vsk) {
  return 0;
}

// virtio_transport_stream_is_active -- 1 module(s)
bool virtio_transport_stream_is_active(struct vsock_sock *vsk) {
  return 0;
}

// virtio_transport_stream_rcvhiwat -- 1 module(s)
u64 virtio_transport_stream_rcvhiwat(struct vsock_sock *vsk) {
  return 0;
}

// virtqueue_detach_unused_buf -- 2 module(s)
void *virtqueue_detach_unused_buf(struct virtqueue *vq) {
  return NULL;
}

// virtqueue_disable_dma_api_for_buffers -- 1 module(s)
void virtqueue_disable_dma_api_for_buffers(struct virtqueue *vq) {
}

// virtqueue_get_avail_addr -- 1 module(s)
dma_addr_t virtqueue_get_avail_addr(const struct virtqueue *vq) {
  return 0;
}

// virtqueue_get_desc_addr -- 1 module(s)
dma_addr_t virtqueue_get_desc_addr(const struct virtqueue *vq) {
  return 0;
}

// virtqueue_get_used_addr -- 1 module(s)
dma_addr_t virtqueue_get_used_addr(const struct virtqueue *vq) {
  return 0;
}

// virtqueue_get_vring_size -- 3 module(s)
unsigned int virtqueue_get_vring_size(const struct virtqueue *vq) {
  return 0;
}

// virtqueue_kick_prepare -- 1 module(s)
bool virtqueue_kick_prepare(struct virtqueue *vq) {
  return 0;
}

// vp_legacy_get_features -- 1 module(s)
u64 vp_legacy_get_features(struct virtio_pci_legacy_device *ldev) {
  return 0;
}

// vp_modern_get_features -- 1 module(s)
u64 vp_modern_get_features(struct virtio_pci_modern_device *mdev) {
  return 0;
}

// vp_modern_get_num_queues -- 1 module(s)
u16 vp_modern_get_num_queues(struct virtio_pci_modern_device *mdev) {
  return 0;
}

// vp_modern_get_queue_reset -- 1 module(s)
int vp_modern_get_queue_reset(struct virtio_pci_modern_device *mdev, u16 index) {
  return 0;
}

// vp_modern_set_queue_reset -- 1 module(s)
void vp_modern_set_queue_reset(struct virtio_pci_modern_device *mdev, u16 index) {
}

// vring_notification_data -- 1 module(s)
u32 vring_notification_data(struct virtqueue *_vq) {
  return 0;
}

// vring_transport_features -- 1 module(s)
void vring_transport_features(struct virtio_device *vdev) {
}

// zlib_deflate_workspacesize -- 1 module(s)
int zlib_deflate_workspacesize (int windowBits, int memLevel) {
  return 0;
}

// zlib_inflate_workspacesize -- 1 module(s)
int zlib_inflate_workspacesize (void) {
  return 0;
}


// ---- Blind stubs for remaining symbols not in headers ----

// skb_queue_purge_reason -- 15 module(s)
void skb_queue_purge_reason(void) { }

// trace_event_buffer_reserve -- 7 module(s)
void trace_event_buffer_reserve(void) { }

// perf_trace_run_bpf_submit -- 7 module(s)
void perf_trace_run_bpf_submit(void) { }

// hrtimer_start_range_ns -- 5 module(s)
void hrtimer_start_range_ns(void) { }

// __tracepoint_rwmmio_write -- 5 module(s)
void __tracepoint_rwmmio_write(void) { }

// __tracepoint_rwmmio_post_write -- 5 module(s)
void __tracepoint_rwmmio_post_write(void) { }

// trace_print_symbols_seq -- 4 module(s)
void trace_print_symbols_seq(void) { }

// of_property_read_variable_u32_array -- 4 module(s)
void of_property_read_variable_u32_array(void) { }

// __tracepoint_rwmmio_read -- 4 module(s)
void __tracepoint_rwmmio_read(void) { }

// __tracepoint_rwmmio_post_read -- 4 module(s)
void __tracepoint_rwmmio_post_read(void) { }

// usb_driver_claim_interface -- 4 module(s)
void usb_driver_claim_interface(void) { }

// usb_driver_release_interface -- 4 module(s)
void usb_driver_release_interface(void) { }

// virtio_check_driver_offered_feature -- 4 module(s)
void virtio_check_driver_offered_feature(void) { }

// usbnet_write_cmd_async -- 3 module(s)
void usbnet_write_cmd_async(void) { }

// mii_ethtool_get_link_ksettings -- 3 module(s)
void mii_ethtool_get_link_ksettings(void) { }

// __init_swait_queue_head -- 3 module(s)
void __init_swait_queue_head(void) { }

// simple_read_from_buffer -- 3 module(s)
void simple_read_from_buffer(void) { }

// tty_port_register_device -- 3 module(s)
void tty_port_register_device(void) { }

// kunit_mem_assert_format -- 3 module(s)
void kunit_mem_assert_format(void) { }

// kthread_create_on_node -- 3 module(s)
void kthread_create_on_node(void) { }

// __tracepoint_sk_data_ready -- 3 module(s)
void __tracepoint_sk_data_ready(void) { }

// __traceiter_sk_data_ready -- 3 module(s)
void __traceiter_sk_data_ready(void) { }

// netdev_upper_dev_unlink -- 2 module(s)
void netdev_upper_dev_unlink(void) { }

// netif_stacked_transfer_operstate -- 2 module(s)
void netif_stacked_transfer_operstate(void) { }

// mii_ethtool_set_link_ksettings -- 2 module(s)
void mii_ethtool_set_link_ksettings(void) { }

// __sock_recv_wifi_status -- 2 module(s)
void __sock_recv_wifi_status(void) { }

// proc_create_net_single -- 2 module(s)
void proc_create_net_single(void) { }

// usbnet_get_link_ksettings_internal -- 2 module(s)
void usbnet_get_link_ksettings_internal(void) { }

// ieee802154_hdr_peek_addrs -- 2 module(s)
void ieee802154_hdr_peek_addrs(void) { }

// register_net_sysctl_sz -- 2 module(s)
void register_net_sysctl_sz(void) { }

// ethtool_convert_legacy_u32_to_link_mode -- 2 module(s)
void ethtool_convert_legacy_u32_to_link_mode(void) { }

// usb_check_bulk_endpoints -- 2 module(s)
void usb_check_bulk_endpoints(void) { }

// blk_queue_max_discard_sectors -- 2 module(s)
void blk_queue_max_discard_sectors(void) { }

// blk_queue_max_write_zeroes_sectors -- 2 module(s)
void blk_queue_max_write_zeroes_sectors(void) { }

// addrconf_add_linklocal -- 1 module(s)
void addrconf_add_linklocal(void) { }

// __ndisc_fill_addr_option -- 1 module(s)
void __ndisc_fill_addr_option(void) { }

// addrconf_prefix_rcv_add_addr -- 1 module(s)
void addrconf_prefix_rcv_add_addr(void) { }

// generic_hwtstamp_get_lower -- 1 module(s)
void generic_hwtstamp_get_lower(void) { }

// generic_hwtstamp_set_lower -- 1 module(s)
void generic_hwtstamp_set_lower(void) { }

// __ethtool_get_link_ksettings -- 1 module(s)
void __ethtool_get_link_ksettings(void) { }

// kmem_cache_create_usercopy -- 1 module(s)
void kmem_cache_create_usercopy(void) { }

// phylink_ethtool_get_pauseparam -- 1 module(s)
void phylink_ethtool_get_pauseparam(void) { }

// phylink_ethtool_set_pauseparam -- 1 module(s)
void phylink_ethtool_set_pauseparam(void) { }

// usbnet_get_link_ksettings_mii -- 1 module(s)
void usbnet_get_link_ksettings_mii(void) { }

// usbnet_set_link_ksettings_mii -- 1 module(s)
void usbnet_set_link_ksettings_mii(void) { }

// phy_ethtool_get_link_ksettings -- 1 module(s)
void phy_ethtool_get_link_ksettings(void) { }

// phy_ethtool_set_link_ksettings -- 1 module(s)
void phy_ethtool_set_link_ksettings(void) { }

// proc_create_seq_private -- 1 module(s)
void proc_create_seq_private(void) { }

// crypto_shash_tfm_digest -- 1 module(s)
void crypto_shash_tfm_digest(void) { }

// firmware_request_nowarn -- 1 module(s)
void firmware_request_nowarn(void) { }

// __kmalloc_node_track_caller -- 1 module(s)
void __kmalloc_node_track_caller(void) { }

// devm_platform_ioremap_resource -- 1 module(s)
void devm_platform_ioremap_resource(void) { }

// usb_altnum_to_altsetting -- 1 module(s)
void usb_altnum_to_altsetting(void) { }

// clk_hw_init_rate_request -- 1 module(s)
void clk_hw_init_rate_request(void) { }

// __clk_mux_determine_rate_closest -- 1 module(s)
void __clk_mux_determine_rate_closest(void) { }

// clk_hw_determine_rate_no_reparent -- 1 module(s)
void clk_hw_determine_rate_no_reparent(void) { }

// tipc_sk_fill_sock_diag -- 1 module(s)
void tipc_sk_fill_sock_diag(void) { }

// usb_serial_register_drivers -- 1 module(s)
void usb_serial_register_drivers(void) { }

// gpiochip_add_data_with_key -- 1 module(s)
void gpiochip_add_data_with_key(void) { }

// usb_serial_handle_dcd_change -- 1 module(s)
void usb_serial_handle_dcd_change(void) { }

// __tracepoint_android_vh_gzvm_destroy_vm_post_process -- 1 module(s)
void __tracepoint_android_vh_gzvm_destroy_vm_post_process(void) { }

// unpin_user_pages_dirty_lock -- 1 module(s)
void unpin_user_pages_dirty_lock(void) { }

// __traceiter_android_vh_gzvm_destroy_vm_post_process -- 1 module(s)
void __traceiter_android_vh_gzvm_destroy_vm_post_process(void) { }

// __tracepoint_android_vh_gzvm_vcpu_exit_reason -- 1 module(s)
void __tracepoint_android_vh_gzvm_vcpu_exit_reason(void) { }

// __traceiter_android_vh_gzvm_vcpu_exit_reason -- 1 module(s)
void __traceiter_android_vh_gzvm_vcpu_exit_reason(void) { }

// eventfd_ctx_remove_wait_queue -- 1 module(s)
void eventfd_ctx_remove_wait_queue(void) { }

// __tracepoint_mmap_lock_start_locking -- 1 module(s)
void __tracepoint_mmap_lock_start_locking(void) { }

// __tracepoint_mmap_lock_acquire_returned -- 1 module(s)
void __tracepoint_mmap_lock_acquire_returned(void) { }

// __tracepoint_mmap_lock_released -- 1 module(s)
void __tracepoint_mmap_lock_released(void) { }

// __mmap_lock_do_trace_acquire_returned -- 1 module(s)
void __mmap_lock_do_trace_acquire_returned(void) { }

// __tracepoint_android_vh_gzvm_handle_demand_page_pre -- 1 module(s)
void __tracepoint_android_vh_gzvm_handle_demand_page_pre(void) { }

// __tracepoint_android_vh_gzvm_handle_demand_page_post -- 1 module(s)
void __tracepoint_android_vh_gzvm_handle_demand_page_post(void) { }

// __traceiter_android_vh_gzvm_handle_demand_page_pre -- 1 module(s)
void __traceiter_android_vh_gzvm_handle_demand_page_pre(void) { }

// __traceiter_android_vh_gzvm_handle_demand_page_post -- 1 module(s)
void __traceiter_android_vh_gzvm_handle_demand_page_post(void) { }

// _trace_android_vh_record_pcpu_rwsem_starttime -- 1 module(s)
void _trace_android_vh_record_pcpu_rwsem_starttime(void) { }

// tty_termios_encode_baud_rate -- 1 module(s)
void tty_termios_encode_baud_rate(void) { }

// devm_regulator_bulk_get -- 1 module(s)
void devm_regulator_bulk_get(void) { }

// regulator_bulk_disable -- 1 module(s)
void regulator_bulk_disable(void) { }

// btbcm_read_pcm_int_params -- 1 module(s)
void btbcm_read_pcm_int_params(void) { }

// btbcm_write_pcm_int_params -- 1 module(s)
void btbcm_write_pcm_int_params(void) { }

// device_property_read_string -- 1 module(s)
void device_property_read_string(void) { }

// qca_send_pre_shutdown_cmd -- 1 module(s)
void qca_send_pre_shutdown_cmd(void) { }

// __module_put_and_kthread_exit -- 1 module(s)
void __module_put_and_kthread_exit(void) { }

// lowpan_register_netdevice -- 1 module(s)
void lowpan_register_netdevice(void) { }

// lowpan_header_decompress -- 1 module(s)
void lowpan_header_decompress(void) { }

// inet_frag_queue_insert -- 1 module(s)
void inet_frag_queue_insert(void) { }

// inet_frag_reasm_prepare -- 1 module(s)
void inet_frag_reasm_prepare(void) { }

// inet_frag_reasm_finish -- 1 module(s)
void inet_frag_reasm_finish(void) { }

// lowpan_header_compress -- 1 module(s)
void lowpan_header_compress(void) { }

// sock_common_setsockopt -- 1 module(s)
void sock_common_setsockopt(void) { }

// sock_common_getsockopt -- 1 module(s)
void sock_common_getsockopt(void) { }

// __kunit_activate_static_stub -- 1 module(s)
void __kunit_activate_static_stub(void) { }

// kunit_destroy_resource -- 1 module(s)
void kunit_destroy_resource(void) { }

// kthread_complete_and_exit -- 1 module(s)
void kthread_complete_and_exit(void) { }

// l2tp_session_dec_refcount -- 1 module(s)
void l2tp_session_dec_refcount(void) { }

// l2tp_tunnel_dec_refcount -- 1 module(s)
void l2tp_tunnel_dec_refcount(void) { }

// l2tp_tunnel_inc_refcount -- 1 module(s)
void l2tp_tunnel_inc_refcount(void) { }

// l2tp_tunnel_get_session -- 1 module(s)
void l2tp_tunnel_get_session(void) { }

// l2tp_session_inc_refcount -- 1 module(s)
void l2tp_session_inc_refcount(void) { }

// l2tp_session_set_header_len -- 1 module(s)
void l2tp_session_set_header_len(void) { }

// ieee802154_mac_cmd_pl_pull -- 1 module(s)
void ieee802154_mac_cmd_pl_pull(void) { }

// crypto_alloc_sync_skcipher -- 1 module(s)
void crypto_alloc_sync_skcipher(void) { }

// ieee802154_mac_cmd_push -- 1 module(s)
void ieee802154_mac_cmd_push(void) { }

// ieee802154_beacon_push -- 1 module(s)
void ieee802154_beacon_push(void) { }

// nl802154_beaconing_done -- 1 module(s)
void nl802154_beaconing_done(void) { }

// netdev_rx_handler_register -- 1 module(s)
void netdev_rx_handler_register(void) { }

// ethtool_convert_link_mode_to_legacy_u32 -- 1 module(s)
void ethtool_convert_link_mode_to_legacy_u32(void) { }

// __platform_driver_probe -- 1 module(s)
void __platform_driver_probe(void) { }

// security_sk_classify_flow -- 1 module(s)
void security_sk_classify_flow(void) { }

// kthread_queue_delayed_work -- 1 module(s)
void kthread_queue_delayed_work(void) { }

// kthread_mod_delayed_work -- 1 module(s)
void kthread_mod_delayed_work(void) { }

// device_for_each_child_reverse -- 1 module(s)
void device_for_each_child_reverse(void) { }

// kvm_arm_hyp_service_available -- 1 module(s)
void kvm_arm_hyp_service_available(void) { }

// kvm_arch_ptp_get_crosststamp -- 1 module(s)
void kvm_arch_ptp_get_crosststamp(void) { }

// get_device_system_crosststamp -- 1 module(s)
void get_device_system_crosststamp(void) { }

// usb_register_device_driver -- 1 module(s)
void usb_register_device_driver(void) { }

// bt_sock_stream_recvmsg -- 1 module(s)
void bt_sock_stream_recvmsg(void) { }

// usb_check_int_endpoints -- 1 module(s)
void usb_check_int_endpoints(void) { }

// snd_soc_component_initialize -- 1 module(s)
void snd_soc_component_initialize(void) { }

// snd_soc_tplg_component_load -- 1 module(s)
void snd_soc_tplg_component_load(void) { }

// snd_soc_tdm_params_to_bclk -- 1 module(s)
void snd_soc_tdm_params_to_bclk(void) { }

// rht_bucket_nested_insert -- 1 module(s)
void rht_bucket_nested_insert(void) { }

// sk_msg_zerocopy_from_iter -- 1 module(s)
void sk_msg_zerocopy_from_iter(void) { }

// sk_msg_memcopy_from_iter -- 1 module(s)
void sk_msg_memcopy_from_iter(void) { }

// iov_iter_extract_pages -- 1 module(s)
void iov_iter_extract_pages(void) { }

// memory_cgrp_subsys_on_dfl_key -- 1 module(s)
void memory_cgrp_subsys_on_dfl_key(void) { }

// devm_platform_get_and_ioremap_resource -- 1 module(s)
void devm_platform_get_and_ioremap_resource(void) { }

// post_page_relinquish_tlb_inv -- 1 module(s)
void post_page_relinquish_tlb_inv(void) { }

// blk_queue_alignment_offset -- 1 module(s)
void blk_queue_alignment_offset(void) { }

// blk_queue_max_secure_erase_sectors -- 1 module(s)
void blk_queue_max_secure_erase_sectors(void) { }

// blk_queue_max_discard_segments -- 1 module(s)
void blk_queue_max_discard_segments(void) { }

// blk_revalidate_disk_zones -- 1 module(s)
void blk_revalidate_disk_zones(void) { }

// blk_queue_max_zone_append_sectors -- 1 module(s)
void blk_queue_max_zone_append_sectors(void) { }

// blk_mq_virtio_map_queues -- 1 module(s)
void blk_mq_virtio_map_queues(void) { }

// sg_alloc_table_chained -- 1 module(s)
void sg_alloc_table_chained(void) { }

// vp_modern_config_vector -- 1 module(s)
void vp_modern_config_vector(void) { }

// vp_modern_get_queue_size -- 1 module(s)
void vp_modern_get_queue_size(void) { }

// vp_modern_get_queue_enable -- 1 module(s)
void vp_modern_get_queue_enable(void) { }

// vring_create_virtqueue -- 1 module(s)
void vring_create_virtqueue(void) { }

// vp_modern_map_vq_notify -- 1 module(s)
void vp_modern_map_vq_notify(void) { }

// vp_modern_queue_vector -- 1 module(s)
void vp_modern_queue_vector(void) { }

// vp_modern_set_queue_enable -- 1 module(s)
void vp_modern_set_queue_enable(void) { }

// vp_modern_set_features -- 1 module(s)
void vp_modern_set_features(void) { }

// vp_modern_set_queue_size -- 1 module(s)
void vp_modern_set_queue_size(void) { }

// vp_modern_queue_address -- 1 module(s)
void vp_modern_queue_address(void) { }

// pci_alloc_irq_vectors_affinity -- 1 module(s)
void pci_alloc_irq_vectors_affinity(void) { }

// vp_legacy_config_vector -- 1 module(s)
void vp_legacy_config_vector(void) { }

// vp_legacy_get_queue_size -- 1 module(s)
void vp_legacy_get_queue_size(void) { }

// vp_legacy_get_queue_enable -- 1 module(s)
void vp_legacy_get_queue_enable(void) { }

// vp_legacy_set_queue_address -- 1 module(s)
void vp_legacy_set_queue_address(void) { }

// vp_legacy_queue_vector -- 1 module(s)
void vp_legacy_queue_vector(void) { }

// vp_legacy_set_features -- 1 module(s)
void vp_legacy_set_features(void) { }

// virtio_transport_recv_pkt -- 1 module(s)
void virtio_transport_recv_pkt(void) { }

// vsock_for_each_connected_socket -- 1 module(s)
void vsock_for_each_connected_socket(void) { }

// virtio_transport_do_socket_init -- 1 module(s)
void virtio_transport_do_socket_init(void) { }

// virtio_transport_dgram_bind -- 1 module(s)
void virtio_transport_dgram_bind(void) { }

// virtio_transport_dgram_dequeue -- 1 module(s)
void virtio_transport_dgram_dequeue(void) { }

// virtio_transport_dgram_enqueue -- 1 module(s)
void virtio_transport_dgram_enqueue(void) { }

// virtio_transport_stream_dequeue -- 1 module(s)
void virtio_transport_stream_dequeue(void) { }

// virtio_transport_stream_enqueue -- 1 module(s)
void virtio_transport_stream_enqueue(void) { }

// virtio_transport_seqpacket_dequeue -- 1 module(s)
void virtio_transport_seqpacket_dequeue(void) { }

// virtio_transport_seqpacket_enqueue -- 1 module(s)
void virtio_transport_seqpacket_enqueue(void) { }

// virtio_transport_notify_poll_in -- 1 module(s)
void virtio_transport_notify_poll_in(void) { }

// virtio_transport_notify_poll_out -- 1 module(s)
void virtio_transport_notify_poll_out(void) { }

// virtio_transport_notify_recv_init -- 1 module(s)
void virtio_transport_notify_recv_init(void) { }

// virtio_transport_notify_recv_pre_block -- 1 module(s)
void virtio_transport_notify_recv_pre_block(void) { }

// virtio_transport_notify_recv_pre_dequeue -- 1 module(s)
void virtio_transport_notify_recv_pre_dequeue(void) { }

// virtio_transport_notify_recv_post_dequeue -- 1 module(s)
void virtio_transport_notify_recv_post_dequeue(void) { }

// virtio_transport_notify_send_init -- 1 module(s)
void virtio_transport_notify_send_init(void) { }

// virtio_transport_notify_send_pre_block -- 1 module(s)
void virtio_transport_notify_send_pre_block(void) { }

// virtio_transport_notify_send_pre_enqueue -- 1 module(s)
void virtio_transport_notify_send_pre_enqueue(void) { }

// virtio_transport_notify_send_post_enqueue -- 1 module(s)
void virtio_transport_notify_send_post_enqueue(void) { }

// device_find_child_by_name -- 1 module(s)
void device_find_child_by_name(void) { }

// crypto_comp_decompress -- 1 module(s)
void crypto_comp_decompress(void) { }

// __tracepoint_android_vh_zs_shrinker_bypass -- 1 module(s)
void __tracepoint_android_vh_zs_shrinker_bypass(void) { }

// __tracepoint_android_vh_zs_shrinker_adjust -- 1 module(s)
void __tracepoint_android_vh_zs_shrinker_adjust(void) { }

// __traceiter_android_vh_zs_shrinker_bypass -- 1 module(s)
void __traceiter_android_vh_zs_shrinker_bypass(void) { }

// __traceiter_android_vh_zs_shrinker_adjust -- 1 module(s)
void __traceiter_android_vh_zs_shrinker_adjust(void) { }


#ifdef __cplusplus
}  // extern "C"
#endif
