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
#include "ini-parse.h"
#include "sensor.h"
#include "../leds/pwm_message.h"
#include "led_control.h"
// Thermal sensor hardware signal setting
#define CAP_SIG_ID          0x0a 
#define CMD_SIG_TASK_REG    _IOW(CAP_SIG_ID, 0, int32_t*)
#define CAP_MAX_LINE                32
#define CAP_INFO_LEN_UNIT_BYTE      12
#define CAP_MAX_LEN                 (CAP_MAX_LINE * CAP_INFO_LEN_UNIT_BYTE)
#define GPIO_INFO_SIZE_INT          3
#define CAP_MAX_LEN_INT             (CAP_MAX_LEN/4)
// sensor data length
#define RAW_DATA_LEN	9920 // 80x62x2
#define THERMAL_STRING_LEN	29760  // 80x62x2x3 ( 3 char per 1 byte)

int sensor_debug = 0;
char thermeal_image_1B_raw[ RAW_DATA_LEN ]; //80*62 pixel , 1 pixel = 2byte , spi odd=dummy even=real data
#if 0
char thermal_data_string[THERMAL_STRING_LEN]={0};
#endif

//gpio
int state_change = 0;
#if 0
// global mac address 
char device_macno[18]={0};
#endif

// SPI interface config
static const char *device = "/dev/spidev1.0";
//static const char *device = "/dev/spidev0.1";
//static const char *device = "/dev/spidev1.1";
static uint32_t mode;
static uint8_t bits = 8;
static uint32_t speed = 4000000; //500000    4000000
static uint16_t delay;
static int verbose;
char *input_tx;
static int fd_spi;
static int fd_capture;
static uint8_t *tx;
static uint8_t *rx;
static int size;
static char full_frame_max_temperature[2]={0};
static char full_frame_min_temperature[2]={0};
#if 0
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
#endif

void sig_event_handler(int sig_id, siginfo_t *sig_info, void *unused)
{
	if ( sig_id == CAP_SIG_ID ) {
		state_change = 1;
	}
}


#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
static void pabort(const char *s)
{
	perror(s);
	abort();
}

static void transfer(int fd, uint8_t const *tx, uint8_t const *rx, size_t len)
{
	int i;
	int ret;
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = len,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};
	if (mode & SPI_TX_QUAD)
		tr.tx_nbits = 4;
	else if (mode & SPI_TX_DUAL)
		tr.tx_nbits = 2;
	if (mode & SPI_RX_QUAD)
		tr.rx_nbits = 4;
	else if (mode & SPI_RX_DUAL)
		tr.rx_nbits = 2;
	if (!(mode & SPI_LOOP)) {
		if (mode & (SPI_TX_QUAD | SPI_TX_DUAL))
			tr.rx_buf = 0;
		else if (mode & (SPI_RX_QUAD | SPI_RX_DUAL))
			tr.tx_buf = 0;
	}

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
}

static int memory_print() {
    unsigned long long free_memory;
    FILE *fp = fopen("/proc/meminfo", "r");
    if(fp == NULL) {
        printf("Error: Failed to open /proc/meminfo\n");
        return 1;
    }
    char line[128];
    while(fgets(line, sizeof(line), fp)) {
        if(sscanf(line, "MemFree: %llu kB", &free_memory) == 1) {
            free_memory *= 1024;
            break;
        }
    }
    fclose(fp);
    logd(sensor_debug,"Free memory: %llu bytes\n", free_memory);
    return 0;
}

static int ir8062_hwinit()
{
	//SPI
	int ret = 0;
	// GPIO
	struct sigaction act;
	int i;

	sigemptyset(&act.sa_mask);
	act.sa_flags = (SA_SIGINFO | SA_RESTART);
	act.sa_sigaction = sig_event_handler;
	sigaction(CAP_SIG_ID, &act, NULL);

	printf("signal handler= %d, spi device=%s\n", CAP_SIG_ID,device);

	fd_capture = open("/dev/gpio_cap", O_RDWR);
	if(fd_capture < 0) {
		printf("Open device fail\n");
		return 0;
	}
/*
	int gpio;
	int state;
	unsigned int count;
	unsigned int time;
	int gpio_info_nums;

	unsigned int pre_count;

*/

	fd_spi = open(device, O_RDWR);
	if (fd_spi < 0)
		pabort("can't open device");
	/*
	 * spi mode
	 */
	ret = ioctl(fd_spi, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("can't set spi mode");
	ret = ioctl(fd_spi, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");
	/*
	 * bits per word
	 */
	ret = ioctl(fd_spi, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");
	ret = ioctl(fd_spi, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");
	/*
	 * max speed hz
	 */
	ret = ioctl(fd_spi, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");
	ret = ioctl(fd_spi, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");

	printf("spi mode: 0x%x\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

	input_tx = "./spidev_test -D /dev/spidev1.0 -p 11111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111";  //98 byte
	size = 160;
	tx = malloc(size);
	rx = malloc(size);
	for (i=0; i<size; i++)
	{
		tx[i] = 0;//i;
		//printf("%x ", tx[i] );
	}
	printf("Reset MI48 \n");
	system("./i2cset -f -y 1 0x40 0 1");
	printf("MI48 reset done\n");
	system("./i2cset -f -y 1 0x40 0xB4 0x03");
	system("./i2cset -f -y 1 0x40 0xD0 0x02");
	system("./i2cset -f -y 1 0x40 0xD0 0x03");

	system("./i2cset -f -y 1 0x40 0xB1 0x03");
#if 1
	system("./i2cget -f -y 1 0x40 0xB2");
	system("./i2cget -f -y 1 0x40 0xB3");
	system("./i2cget -f -y 1 0x40 0xE0");
	system("./i2cget -f -y 1 0x40 0xE1");
	system("./i2cget -f -y 1 0x40 0xE2");
	system("./i2cget -f -y 1 0x40 0xE3");
	system("./i2cget -f -y 1 0x40 0xE4");
	system("./i2cget -f -y 1 0x40 0xE5");
#else	
	system("./i2cset -f -y 1 0x40 0xB2");
	system("./i2cget -f -y 1 0x40");
	system("./i2cset -f -y 1 0x40 0xB3");
	system("./i2cget -f -y 1 0x40");

	system("./i2cset -f -y 1 0x40 0xE0");
	system("./i2cget -f -y 1 0x40");
	system("./i2cset -f -y 1 0x40 0xE1");
	system("./i2cget -f -y 1 0x40");

	system("./i2cset -f -y 1 0x40 0xE2");
	system("./i2cget -f -y 1 0x40");
	system("./i2cset -f -y 1 0x40 0xE3");
	system("./i2cget -f -y 1 0x40");

	system("./i2cset -f -y 1 0x40 0xE4");
	system("./i2cget -f -y 1 0x40");

	system("./i2cset -f -y 1 0x40 0xE5");
	system("./i2cget -f -y 1 0x40");
#endif

}

static void ir8062_send_data(char *data)
{
	//printf("%s :DATA = %s, len=%ld\n",__FUNCTION__,data,strlen(data));
	switch (ir8062_get_connectivity()) {
		case CONNECTION_RJ45:
			logd(sensor_debug,"Upload thermal sensor data to cloud\n");
			ir8062_cloud_service_post(data);
			break;
		case CONNECTION_RS485:
			logd(sensor_debug,"Upload thermal sensor data to RS485\n");
			// TODO: Initial UART8 here
			break;
		default :
			printf("Error: Unknow output device\n");
			break;
	}
}
//static show_thermal_header(uint8_t *buf) {
static thermal_header_parse(int index, uint8_t *buf) {
	#if 1
		switch (index) {
		case 0:
			logd(sensor_debug,"Frame counter : %02X",buf[index]);
			break;
		case 1:
			logd(sensor_debug,"%02X\n",buf[index]);
			break;
		case 2:
			logd(sensor_debug,"SenXor VDD : %02X",buf[index]);
			break;
		case 3:
			logd(sensor_debug,"%02X\n",buf[index]);
			break;
		case 4:
			logd(sensor_debug,"Die Temperature : %02X",buf[index]);
			break;
		case 5:
			logd(sensor_debug,"%02X\n",buf[index]);
			break;
		case 10:
			logd(sensor_debug,"Max : %02X",buf[index]);
			full_frame_max_temperature[0]=buf[index];
			break;
		case 11:
			full_frame_max_temperature[1]=buf[index];
			logd(sensor_debug,"%02X\n",buf[index]);
			break;
		case 12:
			full_frame_min_temperature[0]=buf[index];
			logd(sensor_debug,"Min : %02X",buf[index]);
			break;
		case 13:
			full_frame_min_temperature[1]=buf[index];
			logd(sensor_debug,"%02X\n",buf[index]);
			break;

		}

	#else
	printf("Frame counter=%2X%2X\n",buf[0],buf[1]);
	printf("SenXor VDD=%2X%2x\n",buf[2],buf[3]);
	printf("Die temperature=%2X%2X\n",buf[4],buf[5]);
	printf("Time stamp=%2X%2X%2X%2X\n",buf[6],buf[7],buf[8],buf[9]);
	printf("Max=%2X%2X\n",buf[10],buf[11]);
	printf("Min=%2X%2X\n",buf[12],buf[13]);
	#endif
}
static int show_thermal_raw(char *raw) {
	int i,j;
	printf("=============== Show thermal sensor RAW data ===============\n");
	for (i=0; i<62; i++) {
		for (j=0; j<40;j++) {
			printf("%02X%02X ",thermeal_image_1B_raw[i*80+j*2],thermeal_image_1B_raw[i*80+j*2+1]);
		}
		printf("\n");
	}
	printf("\n=============== Thermal sensor RAW data END ===============\n");
}
char* get_full_frame_max_temperature() {
	return full_frame_max_temperature;
}

char* get_full_frame_min_temperature() {
	return full_frame_min_temperature;
}

int sensor_init(int argc, char *argv[])
//int main(int argc, char *argv[])
{
	int spi_count = 0;
	int count_break = 0;
	unsigned int index = 0;
	int i,ret = 0;
	//-------remove front 80 byte--------
	static uint16_t remove_front_count = 0;
	//static uint8_t remove_flag = 1;
	//static uint16_t remove_number = 160;
	char key_val;
	ssize_t inputbyte;
	int key_flags=fcntl(STDIN_FILENO,F_GETFL,0);
	if (key_flags == -1) {
		printf ("ERROR : fcntl failed.\n");
		exit(EXIT_FAILURE);
	}
	fcntl(STDIN_FILENO,F_SETFL, key_flags | O_NONBLOCK);
	ret=ir8062_hwinit();

	while(1) 
	{
		fflush(0);
		if (state_change == 1) 
		{
			led_send_msg(MSG_HEART_BIT);
			//printf("===== spi read start\n");
			//system("./i2cdump -f -y 1 0x40");
			state_change = 0;
			for(spi_count = 0; spi_count<100; spi_count++)			//101 -> 80*62*2 / 99
			{
				transfer(fd_spi, tx, rx, size);
				// ----------------------Build RAW array----------------------			
				for (i = 0; i < size; i++)
				{
					if(remove_front_count >= 160){
						//printf("Remove header, start record tempature\n");
						thermeal_image_1B_raw[index] = rx[i]; 
						index = index +1 ;
					}
					else{
						thermal_header_parse(i,rx);
						//printf("thermal data[%d]=0x%x\n", i, rx[i]);
					}
					count_break++;		// count for image
					remove_front_count++; 	// count for remove image header
				}
				//printf("thermal data[%d]=0x%x\n", index, thermeal_image_1B_raw[index]);
//				if (remove_front_count<=160) {
//					show_thermal_header(rx);
//					//printf("remove count = %d\n",remove_front_count);
//				}
				if ( count_break == 9920 + 160 )  // [80x62x2 = 9920(1pixel 2byte)]  [9920x2 (1pixel 2byte [SPI 16bits])]  [+160 because header ]
				{
					//printf("Get 9920 + 160 byte\n");
					count_break = 0; 
					remove_front_count = 0;
					index = 0;
#if 0
					// Conver Thermal raw data to string
					memset(thermal_data_string,'\0',strlen(thermal_data_string));
					sprintf(thermal_data_string , "%02X", thermeal_image_1B_raw[0]);
					for (i=1; i < 9920; i++) {
						if ( (i*3)>= THERMAL_STRING_LEN ) {
							//printf("ERROR : over buffer length (%d)\n",i*3);
							break; // over buffer length
						}
						//printf("%d : ,%02X\n",i, thermeal_image_1B_raw[i]);
						sprintf(thermal_data_string + (i * 3)-1, ",%02X", thermeal_image_1B_raw[i]);
					}
					//printf("Output string : %s\n",thermal_data_string);
					// end Thermal raw data string

					curl_fun(thermal_data_string);
#else				
					//printf("%s :DATA = %s, len=%ld\n",__FUNCTION__,thermeal_image_1B_raw,strlen(thermeal_image_1B_raw));
					ir8062_send_data(thermeal_image_1B_raw);
#endif
					//printf("OK : size=%ld Byte\n ",sizeof(thermeal_image_1B_raw) );
					//show_thermal_raw(thermeal_image_1B_raw);
					break;
				}
			} // end spi_count 
			//printf("spi_count=%d\n",spi_count);
			//printf("spi read stop =================\n");
			//system("./i2cdump -f -y 1 0x40");
			//system("./i2cget -f -y 1 0x40 0xB6");
			memory_print();
		}
		inputbyte = read(STDIN_FILENO,&key_val,1);
		if (inputbyte > 0) {
			printf("INPUT Value = 0x%x, bytes=%d\n", key_val, inputbyte);
			if (key_val == 0xa) {// enter
				sleep(1);
				printf ("exit ir8062 process\n");
				//system("./i2cget -f -y 1 0x40 0xB6");
				//system("./i2cset -f -y 1 0x40 0xB1 0");
				break;
			}
		}
//		else 
//			printf("No keyboard input. %d\n", inputbyte);
	}
	printf("Exit SPIRW\n");
#if 0	
	curl_global_cleanup();
#else
	ir8062_cloud_service_release();
#endif


	free(rx);
	free(tx);
	close(fd_spi);
	close(fd_capture);
	return ret;
}
