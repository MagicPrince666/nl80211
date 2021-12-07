#include "netwatch.h"

#include <sys/types.h>  
#include <sys/stat.h> 
#include <sys/ioctl.h> 
#include <sys/socket.h>
#include <sys/wait.h> 
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h> 
#include <stdio.h>  
#include <stdlib.h>
#include <string.h>  
#include <pthread.h>   
#include <dirent.h> 
#include <time.h>
#include <fcntl.h> 
#include <errno.h>
 
#ifdef debugprintf
	#define debugpri(mesg, args...) fprintf(stderr, "[NetRate print:%s:%d:] " mesg "\n", __FILE__, __LINE__, ##args) 
#else
	#define debugpri(mesg, args...)
#endif

NetWatch::NetWatch() {
    netfd = fopen("/proc/net/dev","r+");
	if (NULL == netfd)
	{
		printf("/proc/net/dev not exists!\n");
		exit(-1);
	}
    printf("NetWorkRate Statistic Verson 0.0.1\n");
}

NetWatch::~NetWatch() {
    fclose(netfd);
}

int NetWatch::GetNetRate(char *interface,long *recv,long *send)
{
	char buf[1024];
	char tempstr[16][16]={0};

	memset(buf, 0, sizeof(buf));
	memset(tempstr,0,sizeof(tempstr));
	fseek(netfd, 0, SEEK_SET);
	int nBytes = fread(buf, 1, sizeof(buf)-1, netfd);
	if (-1 == nBytes)
	{
		debugpri("fread error");
		fclose(netfd);
		return -1;
	}
	buf[nBytes] = '\0';
	char* pDev = strstr(buf, interface);
	if (NULL == pDev)
	{
		printf("don't find dev %s\n", interface);
		fclose(netfd);
		return -1;
	}
	sscanf(pDev,"%[^' ']\t%[^' ']\t%[^' ']\t%[^' ']\t%[^' ']\t%[^' ']\t%[^' ']\t%[^' ']\t%[^' ']\t%[^' ']\t%[^' ']\t%[^' ']\t",\
	tempstr[0],tempstr[1],tempstr[2],tempstr[3],tempstr[4],tempstr[5],tempstr[6],tempstr[7],tempstr[8],tempstr[9],tempstr[10],tempstr[11]);
	*recv = atol(tempstr[1]);
	*send = atol(tempstr[9]);
    return 0;
}

int NetWatch::Loop()
{
	while(true)
	{
		gettimeofday(&tv_pre,NULL);
		GetNetRate(netdevice,&recvpre,&sendpre);
		sleep(2);
		gettimeofday(&tv_now,NULL);
		GetNetRate(netdevice,&recvcur,&sendcur);
		recvrate= (recvcur - recvpre)/(1024*(tv_now.tv_sec+tv_now.tv_usec*0.000001-tv_pre.tv_sec+tv_pre.tv_usec*0.000001));
		if(recvrate < 0) {
			recvrate = 0;
		}
		sendrate= (sendcur - sendpre)/(1024*(tv_now.tv_sec+tv_now.tv_usec*0.000001-tv_pre.tv_sec+tv_pre.tv_usec*0.000001));
		if(sendrate < 0) {
			sendrate = 0;
		}
		printf("Net Device	receive rate	send rate\n");
		printf("%-10s\t%-6.2fKB/sec\t%-6.2fKB/sec\n",netdevice,recvrate,recvrate);
	}
	return 0;
}
