// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/shim/subsystem/cfg80211.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

// cfg80211/mac80211 wireless subsystem shim.
//
// Maintains simulated wireless device state. In userspace, operations succeed
// and log their actions for testing and debugging.
//
// On Fuchsia, cfg80211 operations would map to:
// - fuchsia.wlan.fullmac/WlanFullmacImpl for fullmac drivers
// - fuchsia.wlan.softmac/WlanSoftmacBridge for mac80211/softmac drivers

namespace {

std::mutex g_wifi_mu;
std::vector<struct wiphy*> g_wiphys;
std::vector<struct ieee80211_hw*> g_hw_list;
std::vector<struct net_device*> g_netdevs;

int g_wiphy_idx = 0;

}  // namespace

extern "C" {

// --- wiphy lifecycle ---

struct wiphy* wiphy_new_nm(const struct cfg80211_ops* ops, int sizeof_priv,
                           const char* requested_name) {
  (void)ops;
  size_t total = sizeof(struct wiphy) + sizeof_priv;
  auto* w = static_cast<struct wiphy*>(calloc(1, total));
  if (!w) return nullptr;

  if (sizeof_priv > 0) {
    w->priv = reinterpret_cast<void*>(
        reinterpret_cast<uint8_t*>(w) + sizeof(struct wiphy));
    w->priv_size = sizeof_priv;
  }

  char name_buf[32];
  if (requested_name) {
    snprintf(name_buf, sizeof(name_buf), "%s", requested_name);
  } else {
    snprintf(name_buf, sizeof(name_buf), "phy%d", g_wiphy_idx++);
  }
  w->name = strdup(name_buf);

  fprintf(stderr, "driverhub: cfg80211: wiphy_new '%s' (priv=%d)\n",
          w->name, sizeof_priv);
  return w;
}

struct wiphy* wiphy_new(const struct cfg80211_ops* ops, int sizeof_priv) {
  return wiphy_new_nm(ops, sizeof_priv, nullptr);
}

int wiphy_register(struct wiphy* wiphy) {
  if (!wiphy) return -22;
  std::lock_guard<std::mutex> lock(g_wifi_mu);
  g_wiphys.push_back(wiphy);
  fprintf(stderr, "driverhub: cfg80211: wiphy_register '%s'\n", wiphy->name);
  return 0;
}

void wiphy_unregister(struct wiphy* wiphy) {
  if (!wiphy) return;
  std::lock_guard<std::mutex> lock(g_wifi_mu);
  for (auto it = g_wiphys.begin(); it != g_wiphys.end(); ++it) {
    if (*it == wiphy) {
      g_wiphys.erase(it);
      break;
    }
  }
  fprintf(stderr, "driverhub: cfg80211: wiphy_unregister '%s'\n",
          wiphy->name ? wiphy->name : "");
}

void wiphy_free(struct wiphy* wiphy) {
  if (!wiphy) return;
  fprintf(stderr, "driverhub: cfg80211: wiphy_free '%s'\n",
          wiphy->name ? wiphy->name : "");
  free(const_cast<char*>(wiphy->name));
  free(wiphy);
}

// --- ieee80211_hw lifecycle (mac80211) ---

struct ieee80211_hw* ieee80211_alloc_hw(size_t priv_data_len,
                                         const struct ieee80211_ops* ops) {
  (void)ops;
  // Allocate hw + wiphy + priv in one block.
  auto* hw = static_cast<struct ieee80211_hw*>(
      calloc(1, sizeof(struct ieee80211_hw)));
  if (!hw) return nullptr;

  hw->wiphy = wiphy_new(nullptr, 0);
  if (!hw->wiphy) {
    free(hw);
    return nullptr;
  }

  if (priv_data_len > 0) {
    hw->priv = calloc(1, priv_data_len);
  }

  hw->queues = 4;
  hw->max_rates = 4;
  hw->max_rate_tries = 7;

  fprintf(stderr, "driverhub: mac80211: alloc_hw (priv=%zu)\n", priv_data_len);
  return hw;
}

int ieee80211_register_hw(struct ieee80211_hw* hw) {
  if (!hw) return -22;
  std::lock_guard<std::mutex> lock(g_wifi_mu);
  g_hw_list.push_back(hw);
  if (hw->wiphy) {
    wiphy_register(hw->wiphy);
  }
  fprintf(stderr, "driverhub: mac80211: register_hw\n");
  return 0;
}

void ieee80211_unregister_hw(struct ieee80211_hw* hw) {
  if (!hw) return;
  std::lock_guard<std::mutex> lock(g_wifi_mu);
  for (auto it = g_hw_list.begin(); it != g_hw_list.end(); ++it) {
    if (*it == hw) {
      g_hw_list.erase(it);
      break;
    }
  }
  fprintf(stderr, "driverhub: mac80211: unregister_hw\n");
}

void ieee80211_free_hw(struct ieee80211_hw* hw) {
  if (!hw) return;
  if (hw->wiphy) wiphy_free(hw->wiphy);
  free(hw->priv);
  free(hw);
  fprintf(stderr, "driverhub: mac80211: free_hw\n");
}

// --- Event notification ---

void cfg80211_scan_done(struct cfg80211_scan_request* request, int aborted) {
  (void)request;
  fprintf(stderr, "driverhub: cfg80211: scan_done (aborted=%d)\n", aborted);
}

struct cfg80211_bss* cfg80211_inform_bss_data(
    struct wiphy* wiphy, void* data, int ftype,
    const uint8_t* bssid, uint64_t tsf, uint16_t capability,
    uint16_t beacon_interval, const uint8_t* ie, size_t ielen,
    int signal, unsigned int mem_flags) {
  (void)wiphy;
  (void)data;
  (void)ftype;
  (void)tsf;
  (void)capability;
  (void)beacon_interval;
  (void)ie;
  (void)ielen;
  (void)mem_flags;

  auto* bss = static_cast<struct cfg80211_bss*>(
      calloc(1, sizeof(struct cfg80211_bss)));
  if (bss && bssid) {
    memcpy(bss->bssid, bssid, 6);
    bss->signal = signal;
  }
  fprintf(stderr, "driverhub: cfg80211: inform_bss %02x:%02x:%02x:%02x:%02x:%02x signal=%d\n",
          bssid ? bssid[0] : 0, bssid ? bssid[1] : 0,
          bssid ? bssid[2] : 0, bssid ? bssid[3] : 0,
          bssid ? bssid[4] : 0, bssid ? bssid[5] : 0, signal);
  return bss;
}

void cfg80211_connect_result(struct net_device* dev, const uint8_t* bssid,
                             const uint8_t* req_ie, size_t req_ie_len,
                             const uint8_t* resp_ie, size_t resp_ie_len,
                             uint16_t status, unsigned int flags) {
  (void)req_ie;
  (void)req_ie_len;
  (void)resp_ie;
  (void)resp_ie_len;
  (void)flags;
  fprintf(stderr, "driverhub: cfg80211: connect_result '%s' status=%u bssid=%02x:%02x:%02x:%02x:%02x:%02x\n",
          dev ? dev->name : "", status,
          bssid ? bssid[0] : 0, bssid ? bssid[1] : 0,
          bssid ? bssid[2] : 0, bssid ? bssid[3] : 0,
          bssid ? bssid[4] : 0, bssid ? bssid[5] : 0);
}

void cfg80211_disconnected(struct net_device* dev, uint16_t reason,
                           const uint8_t* ie, size_t ie_len,
                           int locally_generated, unsigned int flags) {
  (void)ie;
  (void)ie_len;
  (void)flags;
  fprintf(stderr, "driverhub: cfg80211: disconnected '%s' reason=%u local=%d\n",
          dev ? dev->name : "", reason, locally_generated);
}

// --- mac80211 TX/RX ---

void ieee80211_rx(struct ieee80211_hw* hw, struct sk_buff* skb) {
  (void)hw;
  // In production, forward the frame to the Fuchsia WLAN stack.
  if (skb) {
    free(skb->head);
    free(skb);
  }
}

void ieee80211_rx_irqsafe(struct ieee80211_hw* hw, struct sk_buff* skb) {
  ieee80211_rx(hw, skb);
}

void ieee80211_tx_status(struct ieee80211_hw* hw, struct sk_buff* skb) {
  (void)hw;
  if (skb) {
    free(skb->head);
    free(skb);
  }
}

void ieee80211_tx_status_irqsafe(struct ieee80211_hw* hw,
                                  struct sk_buff* skb) {
  ieee80211_tx_status(hw, skb);
}

// --- Regulatory ---

int regulatory_hint(struct wiphy* wiphy, const char* alpha2) {
  fprintf(stderr, "driverhub: cfg80211: regulatory_hint '%s' region='%s'\n",
          wiphy ? wiphy->name : "", alpha2 ? alpha2 : "");
  return 0;
}

// --- Net device ---

struct net_device* alloc_netdev(int sizeof_priv, const char* name,
                                unsigned char name_assign_type,
                                void (*setup)(struct net_device*)) {
  (void)name_assign_type;
  auto* dev = static_cast<struct net_device*>(
      calloc(1, sizeof(struct net_device)));
  if (!dev) return nullptr;

  if (name) {
    strncpy(dev->name, name, sizeof(dev->name) - 1);
  }
  if (sizeof_priv > 0) {
    dev->priv = calloc(1, sizeof_priv);
  }
  dev->mtu = 1500;

  if (setup) setup(dev);

  fprintf(stderr, "driverhub: net: alloc_netdev '%s'\n", dev->name);
  return dev;
}

void free_netdev(struct net_device* dev) {
  if (!dev) return;
  free(dev->priv);
  free(dev);
}

int register_netdev(struct net_device* dev) {
  if (!dev) return -22;
  std::lock_guard<std::mutex> lock(g_wifi_mu);
  g_netdevs.push_back(dev);
  fprintf(stderr, "driverhub: net: register_netdev '%s'\n", dev->name);
  return 0;
}

void unregister_netdev(struct net_device* dev) {
  if (!dev) return;
  std::lock_guard<std::mutex> lock(g_wifi_mu);
  for (auto it = g_netdevs.begin(); it != g_netdevs.end(); ++it) {
    if (*it == dev) {
      g_netdevs.erase(it);
      break;
    }
  }
  fprintf(stderr, "driverhub: net: unregister_netdev '%s'\n", dev->name);
}

}  // extern "C"
