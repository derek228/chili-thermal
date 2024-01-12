/*
 * Copyright (c) 2014 Nuvoton technology corporation
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
 
#include     <stdio.h>
#include     <stdlib.h>
#include     <unistd.h> 
#include     <sys/types.h>
#include     <sys/stat.h> 
#include     <fcntl.h> 
#include     <termios.h>  
#include     <errno.h>
#include     <string.h>
#include 	<signal.h>
#include    <pthread.h>
#include <linux/serial.h>
#include "ini-parse.h"
#include "rs485.h"
#define FALSE 0
#define TRUE  1

/* Driver-specific ioctls: ...\linux-3.10.x\include\uapi\asm-generic\ioctls.h */
#define TIOCGRS485      0x542E
#define TIOCSRS485      0x542F

struct my_serial_rs485
{
	unsigned long	flags;			/* RS485 feature flags */
#define SER_RS485_ENABLED		(1 << 0)	/* If enabled */
#define SER_RS485_RTS_ON_SEND		(1 << 1)	/* Logical level for
							   RTS pin when
							   sending */
#define SER_RS485_RTS_AFTER_SEND	(1 << 2)	/* Logical level for
							   RTS pin after sent*/
#define SER_RS485_RX_DURING_TX		(1 << 4)
	unsigned long	delay_rts_before_send;	/* Delay before send (milliseconds) */
	unsigned long	delay_rts_after_send;	/* Delay after send (milliseconds) */
	unsigned long	padding[5];		/* Memory is cheap, new structs
					   are a royal PITA .. */
};

static struct termios newtios,oldtios; /*termianal settings */
static int saved_portfd=-1;            /*serial port fd */



static void reset_tty_atexit(void)
{
	if(saved_portfd != -1)
	{
		tcsetattr(saved_portfd,TCSANOW,&oldtios);
	} 
}

/*cheanup signal handler */
static void reset_tty_handler(int signal)
{
	if(saved_portfd != -1)
	{
		tcsetattr(saved_portfd,TCSANOW,&oldtios);
	}
	_exit(EXIT_FAILURE);
}

static int open_port(const char *portname)
{
	struct sigaction sa;
	int portfd;
#if (__GNUC__ == 4 && __GNUC_MINOR__ == 3)
	struct my_serial_rs485 rs485conf;
	struct my_serial_rs485 rs485conf_bak;
#else
	struct serial_rs485 rs485conf;
	struct serial_rs485 rs485conf_bak;
#endif	
	//printf("opening serial port:%s\n",portname);
	/*open serial port */
	//if ((portfd=open(portname, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0) // 以读写方式打开串口设备
	if((portfd=open(portname,O_RDWR | O_NOCTTY, 0)) < 0 )
	{
   		printf("open serial port %s fail \n ",portname);
   		return portfd;
	}

	printf("opening serial port:%s\n",portname);

	/*get serial port parnms,save away */
	tcgetattr(portfd,&newtios);
	memcpy(&oldtios,&newtios,sizeof newtios);
	/* configure new values */
	cfmakeraw(&newtios); /*see man page */
	newtios.c_iflag |=IGNPAR; /*ignore parity on input */
	newtios.c_oflag &= ~(OPOST | ONLCR | OLCUC | OCRNL | ONOCR | ONLRET | OFILL); 
	newtios.c_cflag = CS8 | CLOCAL | CREAD;
	//newtios.c_cc[VMIN]=1; /* block until 1 char received */
	newtios.c_cc[VMIN]=0; /* block until 1 char received */
	newtios.c_cc[VTIME]=0; /*no inter-character timer */
	/* 115200 bps */
	cfsetospeed(&newtios,B9600);
	cfsetispeed(&newtios,B9600);
	/* register cleanup stuff */
	atexit(reset_tty_atexit);
	memset(&sa,0,sizeof sa);
	sa.sa_handler = reset_tty_handler;
	sigaction(SIGHUP,&sa,NULL);
	sigaction(SIGINT,&sa,NULL);
	sigaction(SIGPIPE,&sa,NULL);
	sigaction(SIGTERM,&sa,NULL);
	/*apply modified termios */
	saved_portfd=portfd;
	tcflush(portfd,TCIFLUSH);
	tcsetattr(portfd,TCSADRAIN,&newtios);
	
		
	if (ioctl (portfd, TIOCGRS485, &rs485conf) < 0) 
	{
		/* Error handling.*/ 
		printf("ioctl TIOCGRS485 error.\n");
	}
	/* Enable RS485 mode: */
	rs485conf.flags |= SER_RS485_ENABLED;

	/* Set logical level for RTS pin equal to 1 when sending: */
	rs485conf.flags |= SER_RS485_RTS_ON_SEND;
	//rs485conf.flags |= SER_RS485_RTS_AFTER_SEND;

	/* set logical level for RTS pin equal to 0 after sending: */ 
	rs485conf.flags &= ~(SER_RS485_RTS_AFTER_SEND);
	//rs485conf.flags &= ~(SER_RS485_RTS_ON_SEND);

	/* Set rts delay after send, if needed: */
	rs485conf.delay_rts_after_send = 0x80;

	if (ioctl (portfd, TIOCSRS485, &rs485conf) < 0)
	{
		/* Error handling.*/ 
		printf("ioctl TIOCSRS485 error.\n");
	}

	if (ioctl (portfd, TIOCGRS485, &rs485conf_bak) < 0)
	{
		/* Error handling.*/ 
		printf("ioctl TIOCGRS485 error.\n");
	}
	else
	{
		printf("rs485conf_bak.flags 0x%x.\n", rs485conf_bak.flags);
		printf("rs485conf_bak.delay_rts_before_send 0x%x.\n", rs485conf_bak.delay_rts_before_send);
		printf("rs485conf_bak.delay_rts_after_send 0x%x.\n", rs485conf_bak.delay_rts_after_send);
	}

	return portfd;
}
int rs485_fd;

void rs485_send(char *tx, size_t size) {
	write(rs485_fd, tx, size);
}

int rs485_read(char *rx, size_t size) {
	return read(rs485_fd,rx,size); // return read length
}

#if 1
int rs485_init() {
	rs485_fd = open_port(RS485_DEV);
	if (rs485_fd < 0) {
		printf("Can't open RS485 dev of %s\n", RS485_DEV);
	}
	return rs485_fd;
}
#else // for loop back test
/**
*@breif 	main()
*/
char buff[256];
int main(int argc, char **argv)
{
	char dev[10]="/dev/ttyS8";
	rs485_fd = open_port(dev);

	int rx_size,i;
	while (1) {
		write(rs485_fd,RS485_DO1_OFF,sizeof(RS485_DO1_OFF));
		rx_size = read(rs485_fd,buff,sizeof(buff));
		if (rx_size) {
			if (memcmp(buff,RS485_DO1_OFF,sizeof(RS485_DO1_OFF)-1) == 0)
				break;
			else {
				printf("RX size=%d...\n", rx_size);
				for ( i=0; i<rx_size; i++) {
					printf("0x%x, ",buff[i]);
				}
				printf("\n");
			}
		}
		else {
			printf("No data input \n");
		}
		sleep(1);
	}
	printf("RS485 test stop\n");

  return 0;
}
#endif