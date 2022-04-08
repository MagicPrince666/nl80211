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
  int ifindex = -1;
  int channel = -1;
  int band = -1;
  int signal = -1;
  int txrate = -1;
} wifi_msg;

typedef struct {
  int id;
  struct nl_sock* socket;
  struct nl_cb* name_cb,* info_cb;
  int name_result = -1, info_result = -1;
} Net_link;

class Wifi
{
public:
    Wifi(std::string netdev);
    ~Wifi();

    /**
     * @brief 获取连接状态
     * @param wifi_massage 
     * @return int 
     */
    int LinkStatus();

    /**
     * @brief 获取无线连接信息
     * @param wifi_massage 
     * @return int 
     */
    int LinkMsg(wifi_msg &wifi_massage);

    /**
     * @brief 获取ssid
     * @return std::string 
     */
    std::string GetSsid();

    /**
     * @brief 扫描ssid
     * @return int 
     */
    int ScanSsid();

    /**
     * @brief 连接ap
     * @return int 
     */
    int ConnectAp();
    int ConnectAp(std::string &ap, std::string &pwd);

    /**
     * @brief 创建热点
     * @param ssid 
     * @return int 
     */
    int CreatAp(std::string &ssid);

    /**
     * @brief 节能模式
     * @param power_ 
     * @return int 
     */
    int PowerSave(bool power_);

    /**
     * @brief 重启wifi
     * @return int 
     */
    int ResetWifi();

private:
    Net_link nl_;
    wifi_msg wifi_;
    std::string dev_;
    std::string readFileIntoString(const std::string path);
    int WifiInfoUpdate();
    bool InitNl80211();
};
