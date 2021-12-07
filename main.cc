#include <iostream>
#include <signal.h>
#include <execinfo.h>

//#include "wifi_interface.h"
//#include "net_interface.h"
#include "netwatch.h"

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
    // Netinfc net_info(argv[1]);
    // if(!net_info.IfRunning()) { //网卡是否启动
    //     std::cout << argv[1] << " not running" << std::endl;
    // } else {
    //     std::cout << argv[1] << " is running" << std::endl;
    // }
    // std::string ipaddress;
    // net_info.GetInet(ipaddress);
    // if(ipaddress.size() == 0) {
    //     std::cout << argv[1] <<" ip address not set, please check" << std::endl;
    // } else {
    //     std::cout << argv[1] <<" ip address is " << ipaddress << std::endl;
    // }
    // Wifi wifiinfo(argv[1]);//init wifi interface
    // // std::string ssid = wifiinfo.GetSsid();
    // wifi_msg wifi_massage;
    // int ret = wifiinfo.LinkStatus(wifi_massage);
    // if(ret) {
    //     std::cout <<" Ssid: " << wifi_massage.ssid << std::endl;
    //     std::cout <<" signal: " << wifi_massage.signal << std::endl;
    //     std::cout <<" Txrate: " << (float)wifi_massage.txrate/1000000 << std::endl;
    // } else {
    //     std::cout << "ret = " << ret << std::endl;
    // }
    // std::cout << "SSID = " << ssid << std::endl;
    // wifiinfo.LinkStatus();

    NetWatch netinfo;
    netinfo.Loop();
    return 0;
}