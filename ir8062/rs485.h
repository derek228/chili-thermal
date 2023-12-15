#ifndef __RS485__
#define __RS485__

#define TX_BUFFER	128
#define RX_BUFFER	128

#define RS485_DEV "/dev/ttyS8"
#define RS485_BAUDRATE 9600
typedef struct {
	int dev;
	unsigned int baudrate;
	char tx_buf[TX_BUFFER];
	char rx_buf[RX_BUFFER];
} rs485_t;

int rs485_init();
void rs485_send(char *tx, size_t size);
int rs485_read(char *rx, size_t size);
void rs485_ack();
#endif