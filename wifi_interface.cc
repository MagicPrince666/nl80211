#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <netlink/netlink.h>    //lots of netlink functions
#include <netlink/genl/genl.h>  //genl_connect, genlmsg_put
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>  //genl_ctrl_resolve
#include <linux/nl80211.h>      //NL80211 definitions
#include <iostream>

#include "wifi_interface.h"
#include "net_interface.h"

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

enum print_ie_type {
	PRINT_SCAN,
	PRINT_LINK,
};

static int ieee80211_frequency_to_channel(int freq)
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

static void mac_addr_n2a(char *mac_addr, const unsigned char *arg)
{
	int i, l;

	l = 0;
	for (i = 0; i < ETH_ALEN ; i++) {
		if (i == 0) {
			sprintf(mac_addr+l, "%02X", arg[i]);
			l += 2;
		} else {
			sprintf(mac_addr+l, ":%02X", arg[i]);
			l += 3;
		}
	}
}

static int callback(struct nl_msg *msg, void *arg) {
    struct genlmsghdr *gnlh = (genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));

    struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];

    nla_parse(tb_msg,
            NL80211_ATTR_MAX,
            genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0),
            NULL);

    if (tb_msg[NL80211_ATTR_IFNAME]) {
        strcpy(((wifi_msg*)arg)->ifname, nla_get_string(tb_msg[NL80211_ATTR_IFNAME]));
    }

    if (tb_msg[NL80211_ATTR_IFINDEX]) {
        ((wifi_msg*)arg)->ifindex = nla_get_u32(tb_msg[NL80211_ATTR_IFINDEX]);
    }

    if (tb_msg[NL80211_ATTR_MAC]) {
            mac_addr_n2a(((wifi_msg*)arg)->mac_addr, (uint8_t*)nla_data(tb_msg[NL80211_ATTR_MAC]));
    }

    if (tb_msg[NL80211_ATTR_SSID]) {
        strcpy(((wifi_msg*)arg)->ssid, nla_get_string(tb_msg[NL80211_ATTR_SSID]));
    }

    if (tb_msg[NL80211_ATTR_WIPHY_FREQ]) {
            uint32_t freq = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY_FREQ]);
        ((wifi_msg*)arg)->channel = ieee80211_frequency_to_channel(freq);
        ((wifi_msg*)arg)->band = freq;
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
        fprintf(stderr, "failed to parse nested attributes!\n");
        return NL_SKIP;
    }

    if (sinfo[NL80211_STA_INFO_SIGNAL]) {
        ((wifi_msg*)arg)->signal = (int8_t)nla_get_u8(sinfo[NL80211_STA_INFO_SIGNAL]);
    }

    if (sinfo[NL80211_STA_INFO_TX_BITRATE]) {  
        if (nla_parse_nested(rinfo, NL80211_RATE_INFO_MAX,
                            sinfo[NL80211_STA_INFO_TX_BITRATE], rate_policy)) {
            fprintf(stderr, "failed to parse nested rate attributes!\n"); }
        else {
            if (rinfo[NL80211_RATE_INFO_BITRATE]) {
                ((wifi_msg*)arg)->txrate = nla_get_u16(rinfo[NL80211_RATE_INFO_BITRATE]);
            }
        }
    }

    if (tb[NL80211_ATTR_SCHED_SCAN_MATCH]) {
        strcpy(((wifi_msg*)arg)->ssid, nla_get_string(tb[NL80211_SCHED_SCAN_MATCH_ATTR_SSID]));
	}

    if (tb[NL80211_ATTR_WIPHY_FREQ]) {
		uint32_t freq = nla_get_u32(tb[NL80211_ATTR_WIPHY_FREQ]);
        ((wifi_msg*)arg)->channel = ieee80211_frequency_to_channel(freq);
        ((wifi_msg*)arg)->band = freq;
	}

  return NL_SKIP;
}

struct link_result {
	uint8_t bssid[8];
	bool link_found;
	bool anything_found;
};

static void *nlmsg_data(const struct nlmsghdr *nlh)
{
	return (void *)(nlh + NLMSG_HDRLEN);
}

static inline struct nlmsghdr *nlmsg_hdr(struct nl_msg *n)
{
	return n->nm_nlh;
}

int mac_addr_a2n(unsigned char *mac_addr, char *arg)
{
	int i;

	for (i = 0; i < ETH_ALEN ; i++) {
		int temp;
		char *cp = strchr(arg, ':');
		if (cp) {
			*cp = 0;
			cp++;
		}
		if (sscanf(arg, "%x", &temp) != 1)
			return -1;
		if (temp < 0 || temp > 255)
			return -1;

		mac_addr[i] = temp;
		if (!cp)
			break;
		arg = cp;
	}
	if (i < ETH_ALEN - 1)
		return -1;

	return 0;
}

void print_ies(unsigned char *ie, int ielen, bool unknown,
	       enum print_ie_type ptype)
{
	struct print_ies_data ie_buffer = {
		.ie = ie,
		.ielen = ielen };

	if (ie == NULL || ielen < 0)
		return;

	while (ielen >= 2 && ielen - 2 >= ie[1]) {
		if (ie[0] < ARRAY_SIZE(ieprinters) &&
		    ieprinters[ie[0]].name &&
		    ieprinters[ie[0]].flags & BIT(ptype)) {
			print_ie(&ieprinters[ie[0]],
				 ie[0], ie[1], ie + 2, &ie_buffer);
		} else if (ie[0] == 221 /* vendor */) {
			print_vendor(ie[1], ie + 2, unknown, ptype);
		} else if (ie[0] == 255 /* extension */) {
			print_extension(ie[1], ie + 2, unknown, ptype);
		} else if (unknown) {
			int i;

			printf("\tUnknown IE (%d):", ie[0]);
			for (i=0; i<ie[1]; i++)
				printf(" %.2x", ie[2+i]);
			printf("\n");
		}
		ielen -= ie[1] + 2;
		ie += ie[1] + 2;
	}
}

static int link_bss_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *bss[NL80211_BSS_MAX + 1];
	static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
        [__NL80211_BSS_INVALID] = {},
		[NL80211_BSS_TSF] = { .type = NLA_U64 },
		[NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
		[NL80211_BSS_BSSID] = { },
		[NL80211_BSS_BEACON_INTERVAL] = { .type = NLA_U16 },
		[NL80211_BSS_CAPABILITY] = { .type = NLA_U16 },
		[NL80211_BSS_INFORMATION_ELEMENTS] = { },
		[NL80211_BSS_SIGNAL_MBM] = { .type = NLA_U32 },
		[NL80211_BSS_SIGNAL_UNSPEC] = { .type = NLA_U8 },
		[NL80211_BSS_STATUS] = { .type = NLA_U32 },
	};
	struct link_result *result = (struct link_result *)arg;
	char mac_addr[20], dev[20];

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_BSS]) {
		fprintf(stderr, "bss info missing!\n");
		return NL_SKIP;
	}
	if (nla_parse_nested(bss, NL80211_BSS_MAX,
			     tb[NL80211_ATTR_BSS],
			     bss_policy)) {
		fprintf(stderr, "failed to parse nested attributes!\n");
		return NL_SKIP;
	}

	if (!bss[NL80211_BSS_BSSID])
		return NL_SKIP;

	if (!bss[NL80211_BSS_STATUS])
		return NL_SKIP;

	mac_addr_n2a(mac_addr, nla_data(bss[NL80211_BSS_BSSID]));
	if_indextoname(nla_get_u32(tb[NL80211_ATTR_IFINDEX]), dev);

	switch (nla_get_u32(bss[NL80211_BSS_STATUS])) {
	case NL80211_BSS_STATUS_ASSOCIATED:
		printf("Connected to %s (on %s)\n", mac_addr, dev);
		break;
	case NL80211_BSS_STATUS_AUTHENTICATED:
		printf("Authenticated with %s (on %s)\n", mac_addr, dev);
		return NL_SKIP;
	case NL80211_BSS_STATUS_IBSS_JOINED:
		printf("Joined IBSS %s (on %s)\n", mac_addr, dev);
		break;
	default:
		return NL_SKIP;
	}

	result->anything_found = true;

	if (bss[NL80211_BSS_INFORMATION_ELEMENTS])
		print_ies(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]),
			  nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]),
			  false, PRINT_LINK);

	if (bss[NL80211_BSS_FREQUENCY])
		printf("\tfreq: %d\n",
			nla_get_u32(bss[NL80211_BSS_FREQUENCY]));

	if (nla_get_u32(bss[NL80211_BSS_STATUS]) != NL80211_BSS_STATUS_ASSOCIATED)
		return NL_SKIP;

	/* only in the assoc case do we want more info from station get */
	result->link_found = true;
	memcpy(result->bssid, nla_data(bss[NL80211_BSS_BSSID]), 6);
	return NL_SKIP;
}

Wifi::Wifi(std::string netdev)
{
    dev = &netdev;
    ifIndex = if_nametoindex(netdev.c_str());
    sk = nl_socket_alloc();
    genl_connect(sk);
    expected_id = genl_ctrl_resolve(sk, "nl80211");
}

Wifi::~Wifi()
{
    nl_socket_free(sk);
}

int Wifi::WifiInfo()
{
    msg = nlmsg_alloc();

    genlmsg_put(msg, 0, 0, expected_id, 0, NLM_F_DUMP, NL80211_CMD_GET_INTERFACE, 0);
    nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifIndex);

    nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
                        NULL, NULL);

    int ret = nl_send_auto_complete(sk, msg);
    nl_recvmsgs_default(sk);
    nlmsg_free(msg);
    return ret;
}

std::string Wifi::GetSsid()
{
    wifi_msg wifi_massage;
    msg = nlmsg_alloc();

    genlmsg_put(msg, 0, 0, expected_id, 0, NLM_F_DUMP, NL80211_CMD_GET_INTERFACE, 0);
    nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifIndex);

    nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
                        callback, &wifi_massage);

    nl_send_auto_complete(sk, msg);
    nl_recvmsgs_default(sk);
    nlmsg_free(msg);

    std::cout <<" Interface: " << wifi_massage.ifname << std::endl;
    std::cout <<" Ssid: " << wifi_massage.ssid << std::endl;
    std::cout <<" MacAddr: " << wifi_massage.mac_addr << std::endl;
    std::cout <<" Channal: " << wifi_massage.channel << std::endl;
    std::cout <<" Band: " << wifi_massage.band << std::endl;

    std::string ssid = wifi_massage.ssid;
    if(ssid.size() == 0) {
        char cmd[64] = {0};
        sprintf(cmd, "iw %s link | grep SSID", dev->c_str());
        FILE* pp = popen(cmd, "r");
        if (pp == NULL)
            return nullptr;
        char tmp[256] = {0};
        while (fgets(tmp, sizeof(tmp), pp) != NULL) {
            ssid = tmp;
            ssid = tmp + ssid.find("SSID: ") + 6;
        }
    }

    return ssid;
}

int Wifi::LinkStatus(wifi_msg &wifi_massage)
{
    msg = nlmsg_alloc();

    genlmsg_put(msg, 0, 0, expected_id, 0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0);

    nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifIndex);
    nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
                        link_bss_handler, &wifi_massage);

    int ret = nl_send_auto_complete(sk, msg);
    nl_recvmsgs_default(sk);
    nlmsg_free(msg);

    return ret;
}

int Wifi::ScanSsid()
{ // 扫描环境ssid会消耗大量资源，不建议频繁操作
    //std::cout << "Scanning SSID" << std::endl;
    //ld_wifi_scan(dev->c_str());
    return 0;
}

int Wifi::ConnectAp(std::string &ap, std::string &pwd)
{ //连接AP TO　DO　：wpa_supplicant　实现方式
    char cmd[128] = {0};

    int len = sprintf(cmd, "wpa_passphrase %s %s > /etc/wpa_supplicant.conf", ap.c_str(), pwd.c_str());
    cmd[len] = 0;
    system(cmd);

    len = sprintf(cmd, "wpa_supplicant -B -i %s -c /etc/wpa_supplicant.conf", dev->c_str());
    cmd[len] = 0;
    system(cmd);

    len = sprintf(cmd, "dhclient %s", dev->c_str());
    cmd[len] = 0;
    system(cmd);
    return 0;
}

int Wifi::CreatAp(std::string &ssid)
{ //TO DO 设置AP模式
    std::string ipaddr = "192.168.2.1";
    Netinfc netinfo(dev->c_str());
    netinfo.SetIpaddr(ipaddr);
    netinfo.SetEtherStatusUP();
    return 0;
}

int Wifi::PowerSave(bool power_)
{
    msg = nlmsg_alloc();

    enum nl80211_commands cmd = NL80211_CMD_GET_POWER_SAVE;
    int flags = 0;
    genlmsg_put(msg, 0, 0, expected_id, 0, flags, cmd, 0);
    nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifIndex);
    nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
                        NULL, NULL);
    int ret = nl_send_auto_complete(sk, msg);
    nl_recvmsgs_default(sk);
    nlmsg_free(msg);

    return ret;
}

int Wifi::ResetWifi()
{ //复位wifi
    Netinfc netinfo(dev->c_str());
    netinfo.SetEtherStatusDOWN();
    netinfo.SetEtherStatusUP();
    return 0;
}