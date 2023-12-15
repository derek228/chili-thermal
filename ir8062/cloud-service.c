#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <getopt.h>
#include <linux/types.h>
#include "spidev.h"

#include <sys/socket.h>
#include <net/if.h>
#include <stdint.h>
#include "curl_fun.h"
#include <curl/curl.h>
#include "cloud-service.h"
#include "http-igreent.h"
#include "ini-parse.h"
#include "rs485.h"

#define IGREENT_CLOUD 1
// global mac address 
char device_macno[18]={0};
tEthernet_t eth_config={0};
void get_macno() {
    int fd;
    struct ifreq ifr;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    strncpy(ifr.ifr_name, ETHERNET_DEVICE_NAME, IFNAMSIZ);

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("ioctl");
        exit(1);
    }

    sprintf(device_macno,"%02x:%02x:%02x:%02x:%02x:%02x",
            (unsigned char)ifr.ifr_hwaddr.sa_data[0],
            (unsigned char)ifr.ifr_hwaddr.sa_data[1],
            (unsigned char)ifr.ifr_hwaddr.sa_data[2],
            (unsigned char)ifr.ifr_hwaddr.sa_data[3],
            (unsigned char)ifr.ifr_hwaddr.sa_data[4],
            (unsigned char)ifr.ifr_hwaddr.sa_data[5]);

    close(fd);
	printf("Onboard MAC Address : %s\n", device_macno);
    
}


static void print_usage(const char *prog)
{
	printf("Usage: %s [-m]\n", prog);
	puts("  -m --macno   device mac number\n");
	//			"  -4 --quad     quad transfer\n");
	exit(1);
}
static void parse_opts(int argc, char *argv[])
{
	char *macno;
	int i=0;
	while (1) {
		int c;
		c = getopt(argc, argv, "m:");
		if (c == -1) {
			break;
		}
		switch (c) {
			case 'm':
				macno = optarg;
				memcpy(device_macno, macno, strlen(macno));
				device_macno[17]='\0';
				printf("SPECIFICT MAC NO = %s, len=%d\n",macno,strlen(macno));
				printf("Specific mac number=%s\n", device_macno);
				break;
//			case 'R':
//				mode |= SPI_READY;
//				break;
			default:
				printf("Unknow args, use original MAC NO=%s\n", device_macno);
				break;
		}
	}
}

void set_eth_data_format(int format) {
	eth_config.data_format = format;
}
void set_eth_alarm(int alarm) {
	eth_config.alarm_status = alarm;
}
void set_eth_over_temperature(int temp) {
	eth_config.over_temperature = temp;
}
void set_eth_under_temperature(int temp) {
	eth_config.under_temperature = temp;
}
void set_eth_alert_temperature(int temp) {
	eth_config.alert_temperature = temp;
}
int get_eth_alert_temerature() {
	return eth_config.alert_temperature;
}

int get_eth_data_format() {
	return eth_config.data_format;
}
int get_eth_alarm(){
	return eth_config.alarm_status;
}
int get_eth_over_temperature(){
	return eth_config.over_temperature;
}
int get_eth_under_temperature(){
	return eth_config.under_temperature;
}

int ir8062_cloud_service_init(int argc, char *argv[]) {
	curl_global_init(CURL_GLOBAL_DEFAULT);
	get_macno();
	parse_opts(argc, argv);
	rs485_init();
//	set_gateway_url(device_macno);

//	if (curl_exec_ir_gateway()	<0) {
//	  	printf("HTTP ERROR : can't get sensor_id\n");
//		return 1;
//	}
	return 0;
}

int ir8062_cloud_service_register() {
	#if (IGREENT_CLOUD==1)
	set_gateway_url(device_macno);
	#endif
	return 0;
}
int ir8062_cloud_service_post(char *data) {
//	printf("%s :DATA = %s, len=%ld\n",__FUNCTION__,data,strlen(data));
	#if (IGREENT_CLOUD==1)
		http_igreent_post(data);
	#endif
}

int ir8062_cloud_service_release() {
  curl_global_cleanup();
}
char* ir8062_get_mac_address() {
	return device_macno;
}
