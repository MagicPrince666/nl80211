/*************************************************************
* Description: We get some wifi info using nl80211           *
*                                                            *
* Licence    : Public Domain.                                *
*                                                            *
* Author     : Antonios Tsolis (2016)                        *
*************************************************************/
#include <signal.h>
#include <iostream>

#include "wifi_interface.h"

int main(int argc, char **argv) {
  Wifi wifi_info("wlan0");
  wifi_msg wifi_massage;
  wifi_info.LinkMsg(wifi_massage);
  printf("Ssid: %s\n", wifi_massage.ssid);
  printf("ifname: %s\n", wifi_massage.ifname);
  printf("ifindex: %d\n", wifi_massage.ifindex);
  printf("mac_addr: %s\n", wifi_massage.mac_addr);
  printf("signal: %d\n", wifi_massage.signal);
  printf("channel: %d\n", wifi_massage.channel);
  printf("txrate: %d\n", wifi_massage.txrate);
  printf("band: %d\n", wifi_massage.band);

  return 0;
}
