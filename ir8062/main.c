#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ini-parse.h"
#include "dummy_temperature.h"
#include "led_control.h"

#define CUSTOM_INI_FILENAME "/mnt/mtdblock1/ir8062.ini"
#define DEFAULT_INI_FILENAME "ir8062.ini"
#define FW_VERSION	"1.00.04"
static char *ini_filename;
static int ir8062_config_init()
{
	char *filename=NULL;
	size_t len=0;
	printf("FW Version : %s", FW_VERSION);
	if (access(CUSTOM_INI_FILENAME,F_OK) != -1) {
		filename=CUSTOM_INI_FILENAME;
		len=strlen(filename);
		printf("Get custom ini file %s, len=%d\n", filename,len);
	}
	else {
		if (access(DEFAULT_INI_FILENAME,F_OK) !=-1) {
			filename = DEFAULT_INI_FILENAME;
			len=strlen(filename);
			printf("Get default ini file %s, len=%d\n", filename,len);
		}
	}
	if (filename==NULL) {
		printf("ERROR : ini file not found\n");
		return 1;
	}
	else {
		ini_filename = (char *)malloc((len+1) * sizeof(char));
		if (ini_filename !=NULL) {
			strcpy(ini_filename,filename);
			printf("ini config file is %s\n",ini_filename);
		}
		else {
			printf("ERROR: can't found ini file %s\n", filename);
			return 1;
		}
	}
	if (!parse_ini_file(ini_filename)) {
		fprintf(stderr, "Error parsing file: %s\n", DEFAULT_INI_FILENAME);
		return 1;
	}
	return 0;

}

int main(int argc, char *argv[]) {
	int cnt=0;
	pthread_t tid_sensor;
	if (ir8062_config_init())
		return -1;
	
	switch (ir8062_get_connectivity()) {
		case CONNECTION_RJ45:
			printf("Thermal sensor output data to RJ45\n");
			while (cnt<10) {
				if (system("udhcpc -n")) {
					cnt++;
					printf("ERROR : Ethernet connection failure %d times\n", cnt);
					continue;
				}
				else 
					break;
			}
			if (cnt>=10) {
				printf("ERROR : DHCP failed...\n");
			}
			else {
				led_msg_init();
			}
			// initial cloud service here
			if (ir8062_cloud_service_init(argc,argv)) {
				printf("ERROR : CURL initial failed...\n");
				return -1;
			}
			//
			break;
		case CONNECTION_RS485:
			printf("Thermal sensor output data to RS485\n");
			// TODO: Initial UART8 here
			break;
		default :
			printf("Error: Unknow output device\n");
			break;
	}
	sensor_init(argc,argv);
}

