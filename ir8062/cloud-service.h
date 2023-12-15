#ifndef __CLOUD_SERVICE__
#define __CLOUD_SERVICE__


typedef struct {
	int data_format;
	int alarm_status;
	int over_temperature;
	int under_temperature;
	int alert_temperature;
}tEthernet_t;

int ir8062_cloud_service_init(int argc, char *argv[]);
char* ir8062_get_mac_address();
int ir8062_cloud_service_release();
int ir8062_cloud_service_post(char *data);
void set_eth_data_format(int format);
void set_eth_alarm(int alarm);
void set_eth_over_temperature(int temp);
void set_eth_under_temperature(int temp);
int get_eth_data_format();
int get_eth_alarm();
int get_eth_over_temperature();
int get_eth_under_temperature();
#endif
