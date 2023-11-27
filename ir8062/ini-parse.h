#ifndef __INI_PARSE__
#define __INI_PARSE__

#define FRAME_NUMBER 10
#define CONNECTION_UNDEFINE	0
#define CONNECTION_RJ45		1
#define CONNECTION_RS485	2
#define XMAX	80
#define YMAX	62
#define CONNECTIVITY_TITLE "CONNECTIVITY"
#define FRAME_DISABLE		0
#define FRAME_ENABLE		1
#define FRAME_PARAMS_ERROR	2
#define INI_PARSE_DEBUG		0

typedef struct {
int status;
int x;
int y;
int w;
int h;
int alarm_status;
float over_temperature;
float under_temperature;
}tFrame_t;

tFrame_t ir8062_get_frameinfo(int id);
void ir8062_params_print();
int ir8062_get_connectivity();
int parse_ini_file(const char* filename);
#endif