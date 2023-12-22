#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#define FILENAME "/mnt/mtdblock1/ir8062.ini"
#define BUFFER_SIZE 1024
#define END_MARKER "END_OF_FILE" // 自定義的結束標記
#define START_MARKER "IR8062"
#define SERIALDEV "/dev/ttyS4"
#define ETHERNET_DEVICE_NAME "eth0"

char device_macno[18]={0};
char ipaddr[16]={0};
#define CMD_START "IR8062_CONNECTED\n"
enum {
	CONNECTION_INIT = 0,
	BROADCASTING,
	SYSTEM_CONFIG_START,
	SYSTEM_CONFIG_FINISH,
	RESTART,
};
int serial_port;


int get_ip() {
    int sockfd;
    struct ifreq ifr;
    struct sockaddr_in *sin;
//	if (ipaddr[0]) {
//		printf("Found IP : %s\n",ipaddr);
//		return 0;
//	}
    // 創建 socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation error");
        return EXIT_FAILURE;
    }

    // 設置介面名稱
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, ETHERNET_DEVICE_NAME, IFNAMSIZ - 1);

    if (ioctl(sockfd, SIOCGIFADDR, &ifr) < 0) {
        perror("IOCTL error");
        close(sockfd);
        return EXIT_FAILURE;
    }

    sin = (struct sockaddr_in *)&ifr.ifr_addr;
    sprintf(ipaddr,"%s\n",inet_ntoa(sin->sin_addr));
    //printf("IP Address: %s\n", ipaddr);// inet_ntoa(sin->sin_addr));

    close(sockfd);

    return 0;
}

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
	//printf("Onboard MAC Address : %s\n", device_macno);
}

static int serial_init(int blocking)
{
    struct termios tty;
	if (blocking) {
		serial_port = open(SERIALDEV, O_RDWR | O_NOCTTY | O_NONBLOCK); // 以读写方式打开串口设备
	}
	else 
		serial_port = open(SERIALDEV, O_RDWR);
    if (serial_port < 0) {
        perror("Error opening serial port");
        return 1;
    }

    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(serial_port, &tty) != 0) {
        perror("Error getting serial port attributes");
        close(serial_port);
        return 1;
    }

    tty.c_cflag &= ~CSTOPB; // 1 停止位
    tty.c_cflag |= CS8; // 8 位数据
    tty.c_cflag |= CREAD | CLOCAL; // 启用接收和本地模式

    cfsetospeed(&tty, B115200); // 设置波特率为 9600
    cfsetispeed(&tty, B115200);

    tty.c_cc[VMIN] = 0; // 非阻塞模式，不等待任何字符
    tty.c_cc[VTIME] = 0; // 非阻塞模式，读取操作立即返回

    tcsetattr(serial_port, TCSANOW, &tty);
	return 0;
}

static void sendcmd(char *cmd, int newline){
	if (newline)
		sprintf(cmd,"%s\n",cmd);
	write(serial_port,cmd,strlen(cmd));
}

static int readdev(char *rx) {
	FILE *log;
	ssize_t rx_size=0;
	rx_size = read(serial_port, rx, BUFFER_SIZE);
	if (rx_size>0) {
		log = fopen("/log", "ab"); 
		fputs(rx,log);
		fclose(log);
		printf("ACK rx=%s\n",rx);
		return 1;
	}
	else 
		usleep(200000);
	return 0;
}


int main() {
	FILE *file;
	char buffer[BUFFER_SIZE];
	char rx[BUFFER_SIZE];
	static int cnt=0,retry=0,err=0;
	ssize_t rx_size=0;
	static int state=CONNECTION_INIT;

	get_macno();
	get_ip();

    // 将要发送的数据
	while (1) { // send mac
		get_ip();
		switch (state) {
		case CONNECTION_INIT:
			err = serial_init(1);
			state=BROADCASTING;
			break;
		case BROADCASTING:
			sprintf(buffer,"IR8062_BROADCASTINGMAC%sIP%s\n",device_macno,ipaddr);
			sendcmd(buffer,0);
			if ((cnt++ %10)==0)
				printf("Broadcasting : %s",buffer);
			usleep(100000);
			if (readdev(rx)) {
				if (strstr(rx,device_macno)!=NULL) {
					state=SYSTEM_CONFIG_START;
					cnt = 0;
				}
				else {
					printf("ACK ERROR : %s\n",rx);
				}
			}
			sleep(1);
			break;
		case SYSTEM_CONFIG_START:
			file = fopen(FILENAME, "wb+"); // 以二進制追加模式打開文件
			usleep(1);
			sendcmd(CMD_START,0);
			printf("Start system config\n");
			state=SYSTEM_CONFIG_FINISH;
			//sleep(1);
			break;			
		case SYSTEM_CONFIG_FINISH:
			while (1) {
				memset(rx,0,BUFFER_SIZE);
				if (readdev(rx)) {
					if (strstr(rx,END_MARKER) == NULL) {
						fputs(rx,file);
						printf("Write INI: %s",rx);
					}
					else
						break;
				}
			}
			fclose(file);
			printf("Systam ini file updated\n");
			state = RESTART;
			sleep(1);
			break;
		case RESTART:
			printf("System restart\n");
			close(serial_port);
			sleep(1);
			state=CONNECTION_INIT;
			break;
		default:
			printf("Unknow state %d\n",state);
			break;
		}
		if (err)
			break;
	}
    close(serial_port);
	printf("EXIT uart\n");

    return 0;
}


