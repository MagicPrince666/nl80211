#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/wireless.h>
#include <netinet/in.h>
#include <string>

class Netinfc {
 public:
  Netinfc(const char* netdev);
  ~Netinfc();

    bool IfRunning();
    bool GetInet(std::string& ip);
    bool GetMask(std::string& mac);
    bool GetHwaddr(char *mac);
    bool GetMacAddr(unsigned char *mac);
    int SetIpaddr(std::string &ipaddr);
    int SetEtherStatusUP();
    int SetEtherStatusDOWN();

 private:
    struct sockaddr_in *addr;
    struct ifreq ifr;
    char*address;
    int sockfd;
    struct iwreq wreq;
	 struct iw_statistics stats;
};

