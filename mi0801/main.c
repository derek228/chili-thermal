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

#define CAP_SIG_ID          0x0a 
#define CMD_SIG_TASK_REG    _IOW(CAP_SIG_ID, 0, int32_t*)

#define CAP_MAX_LINE                32
#define CAP_INFO_LEN_UNIT_BYTE      12
#define CAP_MAX_LEN                 (CAP_MAX_LINE * CAP_INFO_LEN_UNIT_BYTE)
#define GPIO_INFO_SIZE_INT          3
#define CAP_MAX_LEN_INT             (CAP_MAX_LEN/4)
#define ETHERNET_DEVICE_NAME		"eth0"
#define RAW_DATA_LEN	9920 // 80x62x2
#define THERMAL_STRING_LEN	29760  // 80x62x2x3 ( 3 char per 1 byte)

char thermeal_image_1B_raw[ RAW_DATA_LEN ]; //80*62 pixel , 1 pixel = 2byte , spi odd=dummy even=real data
char thermal_data_string[THERMAL_STRING_LEN]={0};

//gpio
int state_change = 0;
char device_macno[17]={0};//"00:00:00:00:00:00";

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
	printf("MAC NO : %s\n", device_macno);
    
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
				macno = optarg+1;
				memcpy(device_macno, macno, strlen(macno));
				printf("SPECIFICT MAC NO = %s, len=%d\n",macno,strlen(macno));
				printf("Specific mac number=%s\n", device_macno);
				break;
//			case 'R':
//				mode |= SPI_READY;
//				break;
			default:
				//print_usage(argv[0]);
				printf("Unknow args, use original MAC NO=%s\n", device_macno);
				break;
		}
	}
}


void sig_event_handler(int sig_id, siginfo_t *sig_info, void *unused)
{
	//printf("SIG ENENT HANDLE\n");
	if ( sig_id == CAP_SIG_ID ) {
	//printf("SIG ENENT CHANGE\n");
		state_change = 1;
	}
}


#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
static void pabort(const char *s)
{
	perror(s);
	abort();
}
static const char *device = "/dev/spidev1.0";
static uint32_t mode;
static uint8_t bits = 8;
static uint32_t speed = 4000000; //500000    4000000
static uint16_t delay;
static int verbose;
char *input_tx;

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
    printf("Free memory: %llu bytes\n", free_memory);
    return 0;
}
int main(int argc, char *argv[])
{
	// GPIO
	int fd_capture;
	struct sigaction act;
	int read_len;
	int data_idx;
	int line_idx;
	int ii;
	int i;
	int j;
	sigemptyset(&act.sa_mask);
	act.sa_flags = (SA_SIGINFO | SA_RESTART);
	act.sa_sigaction = sig_event_handler;
	sigaction(CAP_SIG_ID, &act, NULL);

	printf("signal handler= %d\n", CAP_SIG_ID);

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
#ifdef SAVE_RAW_FILE
	// Save image
	int image_index = 0;
	char image_name[] 	  = "./programs";
	char image_index_number[] = ".raw";	
	int len = strlen(image_name) + strlen(image_index_number) + 3;
	char concated[len];
	memset(concated, '\0', len);
	FILE *fptr;
	char text[23];
#endif
	//-------remove front 80 byte--------
	static uint16_t remove_front_count = 0;
	static uint8_t remove_flag = 1;
	static uint16_t remove_number = 160;
	

	//SPI
	int ret = 0;
	int fd;
	uint8_t *tx;
	uint8_t *rx;
	int size;
//	parse_opts(argc, argv);
	fd = open(device, O_RDWR);
	if (fd < 0)
		pabort("can't open device");
	/*
	 * spi mode
	 */
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("can't set spi mode");
	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");
	/*
	 * bits per word
	 */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");
	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");
	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");
	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");

	printf("spi mode: 0x%x\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

	int spi_count = 0;
	int count_break = 0;
	unsigned int index = 0;
	int testcount = 0;
	
	unsigned int indexi = 0;
	unsigned int indexj = 0;


	input_tx = "./spidev_test -D /dev/spidev1.0 -p 11111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111";  //98 byte
	size = 160;
	tx = malloc(size);
	rx = malloc(size);
	for (i=0; i<size; i++)
	{
		tx[i] = i;
		//printf("%x ", tx[i] );
	}


	system("./i2cset -f -y 1 0x40 0xB4 0x03");
	system("./i2cset -f -y 1 0x40 0xD0 0x02");
	system("./i2cset -f -y 1 0x40 0xD0 0x03");

	system("./i2cset -f -y 1 0x40 0xB1 0x03");
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


#if 1 // derek
  curl_global_init(CURL_GLOBAL_DEFAULT);

	get_macno();
	parse_opts(argc, argv);
	set_gateway_url(device_macno);

  if (curl_exec_ir_gateway()	<0) {
  	printf("ERROR : get sensor_id fail\n");
  	return ret;
  }
#endif  	
	while(1) 
	{
		fflush(0);
		if (state_change == 1) 
		{
			state_change = 0;

				for(spi_count = 0; spi_count<100; spi_count++)			//101 -> 80*62*2 / 99
				{


						transfer(fd, tx, rx, size);

						// ----------------------Build RAW array----------------------			
						for (i = 0; i < size; i++)
						{
							if(remove_front_count >= 160){
								//printf("Remove header, start record tempature\n");
								thermeal_image_1B_raw[index] = rx[i]; 
								index = index +1 ;
							}
							//else
								//printf("thermal data[%d]=0x%x\n", index, thermeal_image_1B_raw[i]);
							count_break++;		// count for image
							remove_front_count++; 	// count for remove image header
						}
						//printf("thermal data[%d]=0x%x\n", index, thermeal_image_1B_raw[index]);

						if ( count_break == 9920 + 160 )  // [80x62x2 = 9920(1pixel 2byte)]  [9920x2 (1pixel 2byte [SPI 16bits])]  [+160 because header ]
						{

//							printf("Get 9920 + 160 byte\n");
							count_break = 0; 
							remove_front_count = 0;
							index = 0;
//							char test_data[]="1,2,3,4,5,6";
//							curl_fun(test_data);
							
							
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
							
							
							
							
							curl_fun(thermal_data_string);
#ifdef SAVE_RAW_FILE														
							strcat(concated, image_name);
							sprintf(text, "%d", image_index);   
							strcat(concated, text);
							strcat(concated, image_index_number);
							printf("%s \n",concated);
							image_index = image_index + 1;

							if ((fptr = fopen(concated,"wb")) == NULL){
								printf("Error! opening file\n ");
								exit(1);
							}

							fwrite(&thermeal_image_1B_raw, sizeof(thermeal_image_1B_raw), 1, fptr); 
							memset(concated, '\0', len);
							
							fclose(fptr); 
#endif
							printf("OK : size=%ld Byte\n ",sizeof(thermeal_image_1B_raw) );
							break;
						}
				} // end spi_count 
				//printf("spi_count=%d\n",spi_count);
				memory_print();
		}
	}
	printf("Exit SPIRW\n");
  curl_global_cleanup();



	free(rx);
	free(tx);
	close(fd);
	close(fd_capture);
	return ret;
}
