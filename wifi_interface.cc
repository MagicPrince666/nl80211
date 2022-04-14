
#include <iostream>
#include <fstream>
#include <sstream>

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


#include "wifi_interface.h"

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
			sprintf(mac_addr+l, "%02x", arg[i]);
			l += 2;
		} else {
			sprintf(mac_addr+l, ":%02x", arg[i]);
			l += 3;
		}
	}
}

static int finish_handler(struct nl_msg *msg, void *arg) {
    int *ret = (int *)arg;
    *ret = 0;
    return NL_SKIP;
}

static int get_wifi_name_callback(struct nl_msg *msg, void *arg) {
    struct genlmsghdr *gnlh = (genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
    wifi_msg *wifi = (wifi_msg*)arg;

    nla_parse(tb_msg,
            NL80211_ATTR_MAX,
            genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0),
            NULL);

    if (tb_msg[NL80211_ATTR_IFNAME]) {
        strcpy(wifi->ifname, nla_get_string(tb_msg[NL80211_ATTR_IFNAME]));
    }

    if (tb_msg[NL80211_ATTR_IFINDEX]) {
        wifi->ifindex = nla_get_u32(tb_msg[NL80211_ATTR_IFINDEX]);
    }

    if (tb_msg[NL80211_ATTR_MAC]) {
		mac_addr_n2a(wifi->mac_addr, (uint8_t*)nla_data(tb_msg[NL80211_ATTR_MAC]));
	}

    if (tb_msg[NL80211_ATTR_SSID]) {
        strcpy(wifi->ssid, nla_get_string(tb_msg[NL80211_ATTR_SSID]));
    }

    if (tb_msg[NL80211_ATTR_WIPHY_FREQ]) {
		uint32_t freq = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY_FREQ]);
        wifi->channel = ieee80211_frequency_to_channel(freq);
        wifi->band = freq;
	}

    return NL_SKIP;
}

static int get_wifi_info_callback(struct nl_msg *msgi, void *argv) {
    struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = (genlmsghdr *)nlmsg_data(nlmsg_hdr(msgi));
    struct nlattr *sinfo[NL80211_STA_INFO_MAX + 1];
    struct nlattr *rinfo[NL80211_RATE_INFO_MAX + 1];
    wifi_msg *wifi = (wifi_msg*)argv;

    printf("call %s line %d\n", __FUNCTION__, __LINE__);

  nla_parse(tb_msg,
            NL80211_ATTR_MAX,
            genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0),
            NULL);
  /*
   * TODO: validate the interface and mac address!
   * Otherwise, there's a race condition as soon as
   * the kernel starts sending station notifications.
   */

    if (!tb_msg[NL80211_ATTR_STA_INFO]) {
        fprintf(stderr, "sta stats missing!\n");
        return NL_SKIP;
    }

    if (nla_parse_nested(sinfo, NL80211_STA_INFO_MAX,
                        tb_msg[NL80211_ATTR_STA_INFO], stats_policy)) {
        fprintf(stderr, "failed to parse nested attributes!\n");
        return NL_SKIP;
    }

    if (sinfo[NL80211_STA_INFO_SIGNAL]) {
        wifi->signal = nla_get_u8(sinfo[NL80211_STA_INFO_SIGNAL]);
        printf("signal %d\n", wifi->signal);
    }

    if (sinfo[NL80211_STA_INFO_TX_BITRATE]) {  
        if (nla_parse_nested(rinfo, NL80211_RATE_INFO_MAX,
                            sinfo[NL80211_STA_INFO_TX_BITRATE], rate_policy)) {
            fprintf(stderr, "failed to parse nested rate attributes!\n"); }
        else {
            if (rinfo[NL80211_RATE_INFO_BITRATE]) {
                wifi->txrate = nla_get_u16(rinfo[NL80211_RATE_INFO_BITRATE]);
                printf("txrate %d\n", wifi->txrate);
            }
        }
    }

  return NL_SKIP;
}

Wifi::Wifi(std::string netdev)
:dev_(netdev)
{
    WifiNameUpdate();
    WifiInfoUpdate();
}

Wifi::~Wifi() {
}

bool Wifi::InitNl80211(Net_link &nl) {
    nl.socket      = nl_socket_alloc();
    if (!nl.socket) {
        fprintf(stderr, "Failed to allocate netlink socket.\n");
        return false;
    }

    nl_socket_set_buffer_size(nl.socket, 8192, 8192);

    if (genl_connect(nl.socket)) {
        fprintf(stderr, "Failed to connect to netlink socket.\n");
        nl_close(nl.socket);
        nl_socket_free(nl.socket);
        return false;
    }

    nl.id = genl_ctrl_resolve(nl.socket, "nl80211");
    if (nl.id < 0) {
        fprintf(stderr, "Nl80211 interface not found.\n");
        nl_close(nl.socket);
        nl_socket_free(nl.socket);
        return false;
    }

    return true;
}

int Wifi::WifiNameUpdate() {
    Net_link nl_name;
    InitNl80211(nl_name);
    nl_name.result = 1;

    struct nl_msg* msg1 = nlmsg_alloc();
    if (!msg1) {
        fprintf(stderr, "Failed to allocate netlink message.\n");
        return -2;
    }

    nl_name.callback = nl_cb_alloc(NL_CB_DEFAULT);
    if (!nl_name.callback) {
        fprintf(stderr, "Failed to allocate netlink callback.\n");
        nl_close(nl_name.socket);
        nl_socket_free(nl_name.socket);
        return false;
    }
    nl_cb_set(nl_name.callback, NL_CB_VALID , NL_CB_CUSTOM, get_wifi_name_callback, &wifi_);
    nl_cb_set(nl_name.callback, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &(nl_name.result));

    

    genlmsg_put(msg1,
                NL_AUTO_PORT,
                NL_AUTO_SEQ,
                nl_name.id,
                0,
                NLM_F_DUMP,
                NL80211_CMD_GET_INTERFACE,
                0);

    nl_send_auto_complete(nl_name.socket, msg1);

    while (nl_name.result > 0) { nl_recvmsgs(nl_name.socket, nl_name.callback); }
    nlmsg_free(msg1);
    nl_cb_put(nl_name.callback);
    nl_close(nl_name.socket);
    nl_socket_free(nl_name.socket);

    if (wifi_.ifindex < 0) { return -1; }

    return 0;
}

int Wifi::WifiInfoUpdate() {
    Net_link nl_info;
    InitNl80211(nl_info);
    struct nl_msg* msg2 = nlmsg_alloc();
    if (!msg2) {
        fprintf(stderr, "Failed to allocate netlink message.\n");
        return -2;
    }

    nl_info.callback = nl_cb_alloc(NL_CB_DEFAULT);
    if (!nl_info.callback) {
        fprintf(stderr, "Failed to allocate netlink callback.\n");
        nl_close(nl_info.socket);
        nl_socket_free(nl_info.socket);
        return false;
    }
    nl_cb_set(nl_info.callback, NL_CB_VALID , NL_CB_CUSTOM, get_wifi_info_callback, &wifi_);
    nl_cb_set(nl_info.callback, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &(nl_info.result));

    
    genlmsg_put(msg2,
                NL_AUTO_PORT,
                NL_AUTO_SEQ,
                nl_info.id,
                0,
                NLM_F_DUMP,
                NL80211_CMD_GET_STATION,
                0);

    if (wifi_.ifindex < 0) {
        // return -1;
    } else {
        nla_put_u32(msg2, NL80211_ATTR_IFINDEX, wifi_.ifindex);
        nl_send_auto_complete(nl_info.socket, msg2);
        nl_info.result = 1;
        while (nl_info.result > 0) { nl_recvmsgs(nl_info.socket, nl_info.callback); }
    }

    nlmsg_free(msg2);
    nl_cb_put(nl_info.callback);
    nl_close(nl_info.socket);
    nl_socket_free(nl_info.socket);

    return 0;
}

std::string Wifi::GetSsid() {
    std::string ssid =  wifi_.ssid;
    // popen 临时解决SSID获取失败问题
    if (ssid.size() == 0) {
        char cmd[64] = {0};
        sprintf(cmd, "iw %s link | grep SSID", dev_.c_str());
        FILE *pp = popen(cmd, "r");
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

int Wifi::LinkStatus() {
    return wifi_.signal;
}

int Wifi::LinkMsg(wifi_msg &wifi_massage) {
    wifi_massage = wifi_;
    return 0;
}

int Wifi::ScanSsid() {   // 扫描环境ssid会消耗大量资源，不建议频繁操作
    // std::cout << "Scanning SSID" << std::endl;
    // ld_wifi_scan(dev_.c_str());
    return 0;
}

std::string Wifi::readFileIntoString(const std::string path) {
    std::ifstream input_file(path);
    if (!input_file.is_open()) {
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
}

int Wifi::ConnectAp() {
    // std::string ap = readFileIntoString(STA_NAME_FILE);
    // std::string pwd = readFileIntoString(STA_PWD_FILE);
    // char cmd[128] = {0};
    // sprintf(cmd, "cleanpack_mode -m sta -s %s -p %s", ap.c_str(), pwd.c_str());
    // system(cmd);
    return 0;
}

int Wifi::ConnectAp(std::string &ap, std::string &pwd) {   //连接AP TO　DO　：wpa_supplicant　实现方式
    char cmd[128] = {0};

    int len  = sprintf(cmd, "wpa_passphrase %s %s > /etc/wpa_supplicant.conf", ap.c_str(), pwd.c_str());
    cmd[len] = 0;
    system(cmd);

    len      = sprintf(cmd, "wpa_supplicant -B -i %s -c /etc/wpa_supplicant.conf", dev_.c_str());
    cmd[len] = 0;
    system(cmd);

    len      = sprintf(cmd, "dhclient %s", dev_.c_str());
    cmd[len] = 0;
    system(cmd);
    return 0;
}

int Wifi::CreatAp(std::string &ssid) {   // TO DO 设置AP模式
    // std::string ipaddr = "192.168.78.1";
    // Netinfc netinfo(dev_.c_str());
    // netinfo.SetIpaddr(ipaddr);
    // netinfo.SetEtherStatusUP();
    return 0;
}

int Wifi::PowerSave(bool power_) {
    return 0;
}

int Wifi::ResetWifi() {   //复位wifi
    // Netinfc netinfo(dev_.c_str());
    // netinfo.SetEtherStatusDOWN();
    // netinfo.SetEtherStatusUP();
    return 0;
}
