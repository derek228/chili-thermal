#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "../leds/pwm_message.h"
#include "led_control.h"

int msgid;
char buffer[16];
int led_send_msg(unsigned int msg) {
    struct message msg_data;
	int ret=0;	
    msg_data.msg_type = 1;
	msg_data.msg_text[0]=msg;
	if (msgsnd(msgid, &msg_data, sizeof(msg_data.msg_text), 0) == -1) {
        perror("msgsnd");
        ret = 1;
    }
    //printf("Data sent: %s", msg_data.msg_text);
    return ret;
}

int led_msg_init() {
	msgid = msgget(MSG_KEY, 0666);
	if (msgid == -1) {
		perror("msgget");
		return 1;
		//exit(EXIT_FAILURE);
	}
	led_send_msg(MSG_LEDW_BREATHING);

	led_send_msg(MSG_LEDB_DISABLE);
	return 0;
}