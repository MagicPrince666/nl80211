#pragma once
#include <string>
#include <netlink/netlink.h>    //lots of netlink functions
#include <netlink/genl/genl.h>  //genl_connect, genlmsg_put
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>  //genl_ctrl_resolve
#include <linux/nl80211.h>      //NL80211 definitions

#define NL_AUTO_PORT 0
#define ETH_ALEN 6

typedef struct {
  char ifname[30] = {0};
  char ssid[32] = {0};
  char mac_addr[20] = {0};
  int ifindex = 0;
  int channel = 0;
  int band = 0;
  int signal = 0;
  int txrate = 0;
} wifi_msg;

class Wifi
{
public:
    Wifi(std::string netdev);
    ~Wifi();

    int WifiInfo();
    int LinkStatus(wifi_msg &wifi_massage);
    std::string GetSsid();
    int ScanSsid();
    int ConnectAp(std::string &ap, std::string &pwd);
    int CreatAp(std::string &ssid);
    int PowerSave(bool power_);
    int ResetWifi();

private:
    std::string *dev;
    int ifIndex;
    struct nl_sock* sk;
    struct nl_msg* msg;
    int expected_id;
};