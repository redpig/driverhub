// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRIVERHUB_SRC_SHIM_SUBSYSTEM_CFG80211_H_
#define DRIVERHUB_SRC_SHIM_SUBSYSTEM_CFG80211_H_

// KMI shims for the Linux cfg80211/mac80211 wireless subsystem APIs.
//
// Provides: wiphy_new, wiphy_register, wiphy_unregister, wiphy_free,
//           ieee80211_alloc_hw, ieee80211_register_hw,
//           ieee80211_unregister_hw, ieee80211_free_hw,
//           cfg80211_scan_done, cfg80211_rx_mgmt,
//           cfg80211_connect_result, cfg80211_disconnected,
//           ieee80211_rx, ieee80211_tx_status, etc.
//
// On Fuchsia: maps to fuchsia.wlan.fullmac/WlanFullmacImpl FIDL protocol
// (for fullmac drivers) or fuchsia.wlan.softmac/WlanSoftmacBridge
// (for softmac/mac80211 drivers).

#include <stddef.h>
#include <stdint.h>

#include "src/shim/kernel/device.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Forward declarations ---

struct wiphy;
struct wireless_dev;
struct net_device;
struct cfg80211_ops;
struct ieee80211_hw;
struct ieee80211_ops;
struct ieee80211_vif;
struct ieee80211_sta;
struct sk_buff;
struct cfg80211_scan_request;

// --- IEEE 802.11 frequency bands ---

#define NL80211_BAND_2GHZ  0
#define NL80211_BAND_5GHZ  1
#define NL80211_BAND_6GHZ  2
#define NUM_NL80211_BANDS   3

// --- Interface types ---

#define NL80211_IFTYPE_STATION     2
#define NL80211_IFTYPE_AP          3
#define NL80211_IFTYPE_MONITOR     6
#define NL80211_IFTYPE_P2P_CLIENT  8
#define NL80211_IFTYPE_P2P_GO      9

// --- Channel definitions ---

struct ieee80211_channel {
  int band;
  uint16_t center_freq;  // MHz
  int hw_value;
  uint32_t flags;
  int max_antenna_gain;
  int max_power;  // dBm
};

#define IEEE80211_CHAN_DISABLED  (1 << 0)
#define IEEE80211_CHAN_NO_IR     (1 << 1)
#define IEEE80211_CHAN_RADAR     (1 << 3)

struct ieee80211_rate {
  uint32_t flags;
  uint16_t bitrate;  // in 100kbps units
  uint16_t hw_value;
  uint16_t hw_value_short;
};

struct ieee80211_supported_band {
  struct ieee80211_channel *channels;
  struct ieee80211_rate *bitrates;
  int n_channels;
  int n_bitrates;
  int band;
  // HT/VHT/HE capabilities (simplified).
  void *ht_cap;
  void *vht_cap;
};

// --- wiphy (wireless physical device) ---

struct wiphy {
  const char *name;
  struct device *dev;
  void *priv;  // Driver private data follows the struct.
  size_t priv_size;

  // Supported bands.
  struct ieee80211_supported_band *bands[NUM_NL80211_BANDS];

  // Interface modes bitmask.
  uint32_t interface_modes;

  // Cipher suites.
  const uint32_t *cipher_suites;
  int n_cipher_suites;

  // Max scan SSIDs and scan IE length.
  int max_scan_ssids;
  int max_scan_ie_len;

  // Signal type.
  int signal_type;

  // Flags.
  uint32_t flags;
};

#define WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL  (1 << 0)
#define WIPHY_FLAG_NETNS_OK               (1 << 2)
#define WIPHY_FLAG_PS_ON_BY_DEFAULT       (1 << 3)

#define CFG80211_SIGNAL_TYPE_MBM  1

// --- wireless_dev ---

struct wireless_dev {
  struct wiphy *wiphy;
  int iftype;
  struct net_device *netdev;
  // Current connection state.
  uint8_t current_bss_bssid[6];
  int connected;
};

// --- Minimal net_device ---

struct net_device {
  char name[16];
  struct device dev;
  struct wireless_dev *ieee80211_ptr;
  void *priv;  // netdev_priv()
  unsigned int flags;
  int mtu;
  uint8_t dev_addr[6];
  uint8_t perm_addr[6];
};

#define IFF_UP 1

// --- cfg80211_ops (fullmac driver operations) ---

struct cfg80211_ops {
  int (*scan)(struct wiphy *wiphy, struct cfg80211_scan_request *request);
  int (*connect)(struct wiphy *wiphy, struct net_device *ndev,
                 void *sme);  // struct cfg80211_connect_params
  int (*disconnect)(struct wiphy *wiphy, struct net_device *ndev,
                    uint16_t reason_code);
  int (*add_key)(struct wiphy *wiphy, struct net_device *ndev,
                 int link_id, uint8_t key_index, int pairwise,
                 const uint8_t *mac_addr, void *params);
  int (*del_key)(struct wiphy *wiphy, struct net_device *ndev,
                 int link_id, uint8_t key_index, int pairwise,
                 const uint8_t *mac_addr);
  int (*set_default_key)(struct wiphy *wiphy, struct net_device *ndev,
                         int link_id, uint8_t key_index, int unicast,
                         int multicast);
  int (*set_power_mgmt)(struct wiphy *wiphy, struct net_device *ndev,
                        int enabled, int timeout);
  int (*suspend)(struct wiphy *wiphy, void *wowlan);
  int (*resume)(struct wiphy *wiphy);
};

// --- Scan result reporting ---

struct cfg80211_scan_request {
  int n_ssids;
  int n_channels;
  struct ieee80211_channel **channels;
  struct wiphy *wiphy;
};

struct cfg80211_bss {
  struct ieee80211_channel *channel;
  uint8_t bssid[6];
  int signal;
};

// --- ieee80211_ops (mac80211/softmac driver operations) ---

struct ieee80211_ops {
  int (*start)(struct ieee80211_hw *hw);
  void (*stop)(struct ieee80211_hw *hw);
  int (*add_interface)(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
  void (*remove_interface)(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
  int (*config)(struct ieee80211_hw *hw, uint32_t changed);
  void (*configure_filter)(struct ieee80211_hw *hw, unsigned int changed,
                           unsigned int *total, uint64_t multicast);
  void (*bss_info_changed)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
                           void *info, uint64_t changed);
  int (*set_key)(struct ieee80211_hw *hw, int cmd,
                 struct ieee80211_vif *vif, struct ieee80211_sta *sta,
                 void *key);
  void (*tx)(struct ieee80211_hw *hw, void *control, struct sk_buff *skb);
  int (*ampdu_action)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
                      void *params);
  void (*flush)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
                uint32_t queues, int drop);
  void (*wake_tx_queue)(struct ieee80211_hw *hw, void *txq);
};

// --- ieee80211_hw ---

struct ieee80211_hw {
  struct wiphy *wiphy;
  void *priv;
  uint32_t flags;
  int queues;
  int max_rates;
  int max_rate_tries;
  int extra_tx_headroom;
  uint16_t max_listen_interval;
  uint8_t max_signal;
};

// Hardware flags.
#define IEEE80211_HW_SIGNAL_DBM           (1 << 0)
#define IEEE80211_HW_HAS_RATE_CONTROL     (1 << 1)
#define IEEE80211_HW_AMPDU_AGGREGATION    (1 << 2)
#define IEEE80211_HW_MFP_CAPABLE          (1 << 6)

// --- ieee80211_vif ---

struct ieee80211_vif {
  int type;
  uint8_t addr[6];
  void *drv_priv;
};

// --- ieee80211_sta ---

struct ieee80211_sta {
  uint8_t addr[6];
  void *drv_priv;
};

// --- sk_buff (minimal) ---

struct sk_buff {
  uint8_t *data;
  unsigned int len;
  uint8_t *head;
  uint8_t *tail;
  uint8_t *end;
  void *cb[6];
};

// --- wiphy lifecycle ---

struct wiphy *wiphy_new_nm(const struct cfg80211_ops *ops, int sizeof_priv,
                           const char *requested_name);
// Convenience wrapper.
struct wiphy *wiphy_new(const struct cfg80211_ops *ops, int sizeof_priv);
int wiphy_register(struct wiphy *wiphy);
void wiphy_unregister(struct wiphy *wiphy);
void wiphy_free(struct wiphy *wiphy);

// Priv accessor.
static inline void *wiphy_priv(struct wiphy *wiphy) {
  return wiphy ? wiphy->priv : 0;
}

// --- ieee80211_hw lifecycle (mac80211) ---

struct ieee80211_hw *ieee80211_alloc_hw(size_t priv_data_len,
                                         const struct ieee80211_ops *ops);
int ieee80211_register_hw(struct ieee80211_hw *hw);
void ieee80211_unregister_hw(struct ieee80211_hw *hw);
void ieee80211_free_hw(struct ieee80211_hw *hw);

// --- Event notification (cfg80211 → userspace/Fuchsia) ---

void cfg80211_scan_done(struct cfg80211_scan_request *request, int aborted);
struct cfg80211_bss *cfg80211_inform_bss_data(
    struct wiphy *wiphy, void *data, int ftype,
    const uint8_t *bssid, uint64_t tsf, uint16_t capability,
    uint16_t beacon_interval, const uint8_t *ie, size_t ielen,
    int signal, unsigned int mem_flags);

void cfg80211_connect_result(struct net_device *dev, const uint8_t *bssid,
                             const uint8_t *req_ie, size_t req_ie_len,
                             const uint8_t *resp_ie, size_t resp_ie_len,
                             uint16_t status, unsigned int flags);
void cfg80211_disconnected(struct net_device *dev, uint16_t reason,
                           const uint8_t *ie, size_t ie_len,
                           int locally_generated, unsigned int flags);

// --- mac80211 TX/RX ---

void ieee80211_rx(struct ieee80211_hw *hw, struct sk_buff *skb);
void ieee80211_rx_irqsafe(struct ieee80211_hw *hw, struct sk_buff *skb);
void ieee80211_tx_status(struct ieee80211_hw *hw, struct sk_buff *skb);
void ieee80211_tx_status_irqsafe(struct ieee80211_hw *hw,
                                  struct sk_buff *skb);

// --- Regulatory ---

int regulatory_hint(struct wiphy *wiphy, const char *alpha2);

// --- Net device helpers ---

struct net_device *alloc_netdev(int sizeof_priv, const char *name,
                                unsigned char name_assign_type,
                                void (*setup)(struct net_device *));
void free_netdev(struct net_device *dev);
int register_netdev(struct net_device *dev);
void unregister_netdev(struct net_device *dev);

static inline void *netdev_priv(const struct net_device *dev) {
  return dev ? dev->priv : 0;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DRIVERHUB_SRC_SHIM_SUBSYSTEM_CFG80211_H_
