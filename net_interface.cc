#include <iostream>
#include <iomanip>

#include "net_interface.h"

Netinfc::Netinfc(const char* netdev)
{
    if (strlen(netdev) >= IFNAMSIZ)
    {
        perror("device name is error"), exit(0);
    }
    strcpy(ifr.ifr_name, netdev);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0) {
        perror("socket");
        exit(0);
    }

    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) != 0)
    {
        perror("ioctl");
        exit(0);
    }
}

Netinfc::~Netinfc()
{
    close(sockfd);
}

bool Netinfc::IfRunning()
{
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
        perror("ioctl error");
        return false;
    }
    if (ifr.ifr_flags & IFF_RUNNING)
        return true; // 网卡已插上网线
    else
        return false;
}

bool Netinfc::GetInet(std::string &ip)
{
    if (ioctl(sockfd, SIOCGIFADDR, &ifr) == -1) {
        perror("ioctl error");
        return false;
    }
    addr = (struct sockaddr_in *)&(ifr.ifr_addr);
    address = inet_ntoa(addr->sin_addr);

    ip = std::string(address);

    return true;
}

bool Netinfc::GetMask(std::string &mac)
{
    if (ioctl(sockfd, SIOCGIFNETMASK, &ifr) == -1) {
        perror("ioctl error");
        return false;
    }
    addr = (struct sockaddr_in *)&ifr.ifr_addr;
    address = inet_ntoa(addr->sin_addr);

    return true;
}

bool Netinfc::GetHwaddr(char *mac)
{
    u_int8_t hd[6];
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == -1) {
        perror("hwaddr error");
        return false;
    }
    memcpy(hd, ifr.ifr_hwaddr.sa_data, sizeof(hd));

    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", hd[0], hd[1], hd[2], hd[3], hd[4], hd[5]);

    return true;
}

bool Netinfc::GetMacAddr(unsigned char *mac)
{
    u_int8_t hd[6];
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == -1) {
        perror("hwaddr error");
        return false;
    }
    memcpy(hd, ifr.ifr_hwaddr.sa_data, sizeof(hd));

    return true;
}

int Netinfc::SetIpaddr(std::string &ipaddr)
{
    struct sockaddr_in *p = (struct sockaddr_in *)&(ifr.ifr_addr);

    p->sin_family = AF_INET;
    inet_aton(ipaddr.c_str(), &(p->sin_addr));

    if (ioctl(sockfd, SIOCSIFADDR, &ifr) == -1) {
        perror("ioctl error");
        return -1;
    }
    return 0;
}

//启动网卡接口
int Netinfc::SetEtherStatusUP()
{
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
        perror("ioctl SIOCSIFFLAGS fails!");
        return -1;
    }
    return 0;
}

//关闭网卡接口
int Netinfc::SetEtherStatusDOWN()
{
    ifr.ifr_flags &= ~IFF_UP;
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
        perror("ioctl SIOCSIFFLAGS fails!");
        return -1;
    }

    return 0;
}

int test(void)
{
    Netinfc netinfo("eth0");
    //netinfo.IfRunning();
    //netinfo.GetHwaddr();
    //netinfo.GetMask();
    return 0;
}
