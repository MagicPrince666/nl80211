/**
 * @file net_message.h
 * @author 黄李全 (hlq@ldrobot.com)
 * @brief 网速实时检测
 * @version 0.1
 * @date 2021-12-07
 * @copyright Copyright (c) {2021} 深圳乐动机器人版权所有
 */
#pragma once

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <iostream>

#define NET_INFO_PATH "/proc/net/dev"

#define NET_DIFF_TIME 1  //时间差阈值(单位：秒)

class NetWatch {
 public:
    NetWatch();
    ~NetWatch();
    int Loop();

 private:
    struct timeval tv_now,tv_pre;
	char netdevice[16]={0};
	int nDevLen;
	long recvpre = 0,recvcur = 0;
	long sendpre = 0,sendcur = 0;
	double sendrate;
	double recvrate;
    FILE* netfd;
    int GetNetRate(char *interface,long *recv,long *send);
};
