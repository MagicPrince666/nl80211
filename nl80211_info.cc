/*************************************************************
* Description: We get some wifi info using nl80211           *
*                                                            *
* Licence    : Public Domain.                                *
*                                                            *
* Author     : Antonios Tsolis (2016)                        *
*************************************************************/
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <netlink/netlink.h>    //lots of netlink functions
#include <netlink/genl/genl.h>  //genl_connect, genlmsg_put
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>  //genl_ctrl_resolve
#include <linux/nl80211.h>      //NL80211 definitions

#include <iostream>
#define NL_AUTO_PORT 0
#define ETH_ALEN 6
#undef nl_sock

static volatile int keepRunning = 1;

void ctrl_c_handler(int dummy) {
    keepRunning = 0;
}

typedef struct {
  int id;
  struct nl_sock* socket;
  struct nl_cb* cb1,* cb2;
  int result1, result2;
} Netlink;

typedef struct {
  char ifname[30];
  char ssid[32];
  char mac_addr[20];
  int ifindex;
  int channel;
  int band;
  int signal;
  int txrate;
} Wifi;

static struct nla_policy stats_policy[NL80211_STA_INFO_MAX + 1] = {
  [__NL80211_RATE_INFO_INVALID] = {},
  [NL80211_STA_INFO_INACTIVE_TIME] = { .type = NLA_U32 },
  [NL80211_STA_INFO_RX_BYTES] = { .type = NLA_U32 },
  [NL80211_STA_INFO_TX_BYTES] = { .type = NLA_U32 },
  [NL80211_STA_INFO_LLID] = { .type = NLA_U16 },
  [NL80211_STA_INFO_PLID] = { .type = NLA_U16 },
  [NL80211_STA_INFO_PLINK_STATE] = { .type = NLA_U8 },
  [NL80211_STA_INFO_SIGNAL] = { .type = NLA_U8 },
  [NL80211_STA_INFO_TX_BITRATE] = { .type = NLA_NESTED },
  [NL80211_STA_INFO_RX_PACKETS] = { .type = NLA_U32 },
  [NL80211_STA_INFO_TX_PACKETS] = { .type = NLA_U32 },
};

static struct nla_policy rate_policy[NL80211_RATE_INFO_MAX + 1] = {
  [__NL80211_RATE_INFO_INVALID] = {},
  [NL80211_RATE_INFO_BITRATE] = { type : NLA_U16 },
  [NL80211_RATE_INFO_MCS] = { type : NLA_U8 },
  [NL80211_RATE_INFO_40_MHZ_WIDTH] = { type : NLA_FLAG },
  [NL80211_RATE_INFO_SHORT_GI] = { type : NLA_FLAG },
};

static int initNl80211(Netlink* nl, Wifi* w);
static int finish_handler(struct nl_msg *msg, void *arg);
static int getWifiName_callback(struct nl_msg *msg, void *arg);
static int getWifiInfo_callback(struct nl_msg *msg, void *arg);
static int getWifiStatus(Netlink* nl, Wifi* w);


static int initNl80211(Netlink* nl, Wifi* w) {
  nl->socket = nl_socket_alloc();
  if (!nl->socket) {
    fprintf(stderr, "Failed to allocate netlink socket.\n");
    return -ENOMEM;
  }

  nl_socket_set_buffer_size(nl->socket, 8192, 8192);

  if (genl_connect(nl->socket)) {
    fprintf(stderr, "Failed to connect to netlink socket.\n");
    nl_close(nl->socket);
    nl_socket_free(nl->socket);
    return -ENOLINK;
  }

  nl->id = genl_ctrl_resolve(nl->socket, "nl80211");
  if (nl->id< 0) {
    fprintf(stderr, "Nl80211 interface not found.\n");
    nl_close(nl->socket);
    nl_socket_free(nl->socket);
    return -ENOENT;
  }

  nl->cb1 = nl_cb_alloc(NL_CB_DEFAULT);
  nl->cb2 = nl_cb_alloc(NL_CB_DEFAULT);
  if ((!nl->cb1) || (!nl->cb2)) {
     fprintf(stderr, "Failed to allocate netlink callback.\n");
     nl_close(nl->socket);
     nl_socket_free(nl->socket);
     return -ENOMEM;
  }

  nl_cb_set(nl->cb1, NL_CB_VALID , NL_CB_CUSTOM, getWifiName_callback, w);
  nl_cb_set(nl->cb1, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &(nl->result1));
  nl_cb_set(nl->cb2, NL_CB_VALID , NL_CB_CUSTOM, getWifiInfo_callback, w);
  nl_cb_set(nl->cb2, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &(nl->result2));

  return nl->id;
}


static int finish_handler(struct nl_msg *msg, void *arg) {
  int *ret = (int *)arg;
  *ret = 0;
  return NL_SKIP;
}

int ieee80211_frequency_to_channel(int freq)
{
	/* see 802.11-2007 17.3.8.3.2 and Annex J */
	if (freq == 2484)
		return 14;
	/* see 802.11ax D6.1 27.3.23.2 and Annex E */
	else if (freq == 5935)
		return 2;
	else if (freq < 2484)
		return (freq - 2407) / 5;
	else if (freq >= 4910 && freq <= 4980)
		return (freq - 4000) / 5;
	else if (freq < 5950)
		return (freq - 5000) / 5;
	else if (freq <= 45000) /* DMG band lower limit */
		/* see 802.11ax D6.1 27.3.23.2 */
		return (freq - 5950) / 5;
	else if (freq >= 58320 && freq <= 70200)
		return (freq - 56160) / 2160;
	else
		return 0;
}

void mac_addr_n2a(char *mac_addr, const unsigned char *arg)
{
	int i, l;

	l = 0;
	for (i = 0; i < ETH_ALEN ; i++) {
		if (i == 0) {
			sprintf(mac_addr+l, "%02x", arg[i]);
			l += 2;
		} else {
			sprintf(mac_addr+l, ":%02x", arg[i]);
			l += 3;
		}
	}
}

static int getWifiName_callback(struct nl_msg *msg, void *arg) {

  struct genlmsghdr *gnlh = (genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));

  struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];

  nla_parse(tb_msg,
            NL80211_ATTR_MAX,
            genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0),
            NULL);

  if (tb_msg[NL80211_ATTR_IFNAME]) {
    strcpy(((Wifi*)arg)->ifname, nla_get_string(tb_msg[NL80211_ATTR_IFNAME]));
  }

  if (tb_msg[NL80211_ATTR_IFINDEX]) {
    ((Wifi*)arg)->ifindex = nla_get_u32(tb_msg[NL80211_ATTR_IFINDEX]);
  }

  if (tb_msg[NL80211_ATTR_MAC]) {
		mac_addr_n2a(((Wifi*)arg)->mac_addr, (uint8_t*)nla_data(tb_msg[NL80211_ATTR_MAC]));
	}

  if (tb_msg[NL80211_ATTR_SSID]) {
    strcpy(((Wifi*)arg)->ssid, nla_get_string(tb_msg[NL80211_ATTR_SSID]));
	}

  if (tb_msg[NL80211_ATTR_WIPHY_FREQ]) {
		uint32_t freq = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY_FREQ]);
    ((Wifi*)arg)->channel = ieee80211_frequency_to_channel(freq);
    ((Wifi*)arg)->band = freq;
	}

  return NL_SKIP;
}


static int getWifiInfo_callback(struct nl_msg *msg, void *arg) {
  struct nlattr *tb[NL80211_ATTR_MAX + 1];
  struct genlmsghdr *gnlh = (genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
  struct nlattr *sinfo[NL80211_STA_INFO_MAX + 1];
  struct nlattr *rinfo[NL80211_RATE_INFO_MAX + 1];

  nla_parse(tb,
            NL80211_ATTR_MAX,
            genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0),
            NULL);
  /*
   * TODO: validate the interface and mac address!
   * Otherwise, there's a race condition as soon as
   * the kernel starts sending station notifications.
   */

  if (!tb[NL80211_ATTR_STA_INFO]) {
    fprintf(stderr, "sta stats missing!\n"); return NL_SKIP;
  }

  if (nla_parse_nested(sinfo, NL80211_STA_INFO_MAX,
                       tb[NL80211_ATTR_STA_INFO], stats_policy)) {
    fprintf(stderr, "failed to parse nested attributes!\n"); return NL_SKIP;
  }

  if (sinfo[NL80211_STA_INFO_SIGNAL]) {
    ((Wifi*)arg)->signal = (int8_t)nla_get_u8(sinfo[NL80211_STA_INFO_SIGNAL]);
  }

  if (sinfo[NL80211_STA_INFO_TX_BITRATE]) {  
    if (nla_parse_nested(rinfo, NL80211_RATE_INFO_MAX,
                         sinfo[NL80211_STA_INFO_TX_BITRATE], rate_policy)) {
      fprintf(stderr, "failed to parse nested rate attributes!\n"); }
    else {
      if (rinfo[NL80211_RATE_INFO_BITRATE]) {
        ((Wifi*)arg)->txrate = nla_get_u16(rinfo[NL80211_RATE_INFO_BITRATE]);
      }
    }
  }
  return NL_SKIP;
}


static int getWifiStatus(Netlink* nl, Wifi* w) {
  nl->result1 = 1;
  nl->result2 = 1;

  struct nl_msg* msg1 = nlmsg_alloc();
  if (!msg1) {
    fprintf(stderr, "Failed to allocate netlink message.\n");
    return -2;
  }

  genlmsg_put(msg1,
              NL_AUTO_PORT,
              NL_AUTO_SEQ,
              nl->id,
              0,
              NLM_F_DUMP,
              NL80211_CMD_GET_INTERFACE,
              0);

  nl_send_auto_complete(nl->socket, msg1);

  while (nl->result1 > 0) { nl_recvmsgs(nl->socket, nl->cb1); }
  nlmsg_free(msg1);

  if (w->ifindex < 0) { return -1; }

  struct nl_msg* msg2 = nlmsg_alloc();

  if (!msg2) {
    fprintf(stderr, "Failed to allocate netlink message.\n");
    return -2;
  }

  genlmsg_put(msg2,
              NL_AUTO_PORT,
              NL_AUTO_SEQ,
              nl->id,
              0,
              NLM_F_DUMP,
              NL80211_CMD_GET_STATION,
              0);

  nla_put_u32(msg2, NL80211_ATTR_IFINDEX, w->ifindex);
  nl_send_auto_complete(nl->socket, msg2);
  while (nl->result2 > 0) { nl_recvmsgs(nl->socket, nl->cb2); }
  nlmsg_free(msg2);

  return 0;
}


int main(int argc, char **argv) {
  Netlink nl;
  Wifi wifi;

  signal(SIGINT, ctrl_c_handler);

  nl.id = initNl80211(&nl, &wifi);
  if (nl.id < 0) {
    fprintf(stderr, "Error initializing netlink 802.11\n");
    return -1;
  }

  do {
    getWifiStatus(&nl, &wifi);
    std::cout <<" Interface: " << wifi.ifname << std::endl;
    std::cout <<" Ssid: " << wifi.ssid << std::endl;
    std::cout <<" MacAddr: " << wifi.mac_addr << std::endl;
    std::cout <<" Channal: " << wifi.channel << std::endl;
    std::cout <<" Band: " << wifi.band << std::endl;
    std::cout <<" RSSI: " << wifi.signal + 95 << std::endl;
    std::cout <<" Txrate: " << (float)wifi.txrate/10 << "\n" << std::endl;

    sleep(1);
  } while(keepRunning);

  nl_cb_put(nl.cb1);
  nl_cb_put(nl.cb2);
  nl_close(nl.socket);
  nl_socket_free(nl.socket);
  return 0;
}
