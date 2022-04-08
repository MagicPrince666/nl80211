#include <iostream>
#include <signal.h>
#include <execinfo.h>

#include "wifi_interface.h"
//#include "net_interface.h"
#include "netwatch.h"
#include "config.h"  

#define PRINT_SIZE_ 20

static void _signal_handler(int signum)
{
    void *array[PRINT_SIZE_];
    char **strings;

    size_t size = ::backtrace(array, PRINT_SIZE_);
    strings = (char **)::backtrace_symbols(array, size);

    if (strings == NULL) {
	   fprintf(stderr, "backtrace_symbols");
	   exit(EXIT_FAILURE);
    }

    switch(signum) {
        case SIGSEGV:
        fprintf(stderr, "widebright received SIGSEGV! Stack trace:\n");
        break;

        case SIGPIPE:
        fprintf(stderr, "widebright received SIGPIPE! Stack trace:\n");
        break;

        case SIGFPE:
        fprintf(stderr, "widebright received SIGFPE! Stack trace:\n");
        break;

        case SIGABRT:
        fprintf(stderr, "widebright received SIGABRT! Stack trace:\n");
        break;

        default:
        break;
    }

    for (size_t i = 0; i < size; i++) {
        fprintf(stderr, "%d %s \n", i, strings[i]);
    }

    free(strings);
    signal(signum, SIG_DFL); /* 还原默认的信号处理handler */

    exit(1);
}

int main(int argc, char **argv) {
    signal(SIGPIPE, _signal_handler);  // SIGPIPE，管道破裂。
    signal(SIGSEGV, _signal_handler);  // SIGSEGV，非法内存访问
    signal(SIGFPE, _signal_handler);  // SIGFPE，数学相关的异常，如被0除，浮点溢出，等等
    signal(SIGABRT, _signal_handler);  // SIGABRT，由调用abort函数产生，进程非正常退出

    Wifi wifi_info("wlan0");
    wifi_msg wifi_massage;
    wifi_info.LinkMsg(wifi_massage);
    printf("Ssid: %s\n", wifi_massage.ssid);
    printf("ifname: %s\n", wifi_massage.ifname);
    printf("mac_addr: %s\n", wifi_massage.mac_addr);
    printf("signal: %d\n", wifi_massage.signal);
    printf("channel: %d\n", wifi_massage.channel);
    printf("txrate: %d\n", wifi_massage.txrate);

    Config cfg("/data/bin/dnsmasq.conf");
    auto ipaddr = cfg.Read("listen-address", std::string("bbb"));
    printf("ipaddr: %s\n", ipaddr.c_str());

    return 0;
}