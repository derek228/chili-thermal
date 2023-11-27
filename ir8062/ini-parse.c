#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ini-parse.h"

#define MAX_LINE_LEN 1024

//int parse_ini_file(const char* filename);
static char *frame_title[]={"FRAME1","FRAME2","FRAME3","FRAME4","FRAME5","FRAME6","FRAME7","FRAME8","FRAME9","FRAME10"};

static int ir8062_connectivity=CONNECTION_UNDEFINE;

static tFrame_t tFrame[FRAME_NUMBER]={0};


static void ir8062_set_connectivity(char *mode){
	if (strcmp(mode,"RJ45")==0)
		ir8062_connectivity=CONNECTION_RJ45;
	else if (strcmp(mode,"RS485")==0)
		ir8062_connectivity=CONNECTION_RS485;
	else {
		ir8062_connectivity=CONNECTION_RJ45;
		printf("Error : Unknow connection mode %s. Use RJ45 as default\n", mode);
	}
}
static void ir8062_set_frame_params(int id, char *key, char *value)
{
	int val=0;
	if (tFrame[id].status==FRAME_PARAMS_ERROR)
	{
		printf("ERROR: Wrong Frame%d params, skip...(key=%s, vlaue=%s)\n", id+1, key,value);
		return;
	}
	tFrame[id].status=FRAME_ENABLE;
	if (strcmp(key,"X")==0) {
		val=atoi(value);
		if (val<XMAX)
			tFrame[id].x=val;
		else {
			printf("ERROR: frame%d X=%d value over %d\n",id+1,val,XMAX);
			tFrame[id].status=FRAME_PARAMS_ERROR;
		}
	}
	else if (strcmp(key,"Y")==0) {
		val=atoi(value);
		if (val<YMAX)
			tFrame[id].y=val;
		else {
			printf("ERROR: frame%d Y=%d axis value over %d\n",id+1,val,YMAX);
			tFrame[id].status=FRAME_PARAMS_ERROR;
		}

	}
	else if (strcmp(key,"W")==0) {
		val=atoi(value);
		if ((val+tFrame[id].x)<XMAX)
			tFrame[id].w=val;
		else {
			printf("ERROR: frame%d X end =%d, over %d\n",id+1,val+tFrame[id].x,XMAX);
			tFrame[id].status=FRAME_PARAMS_ERROR;
		}

	}
	else if (strcmp(key,"H")==0) {
		val=atoi(value);
		if ((val+tFrame[id].y)<YMAX)
			tFrame[id].h=val;
		else {
			printf("ERROR: frame%d Y end =%d, over %d\n",id+1,val+tFrame[id].y,YMAX);
			tFrame[id].status=FRAME_PARAMS_ERROR;
		}

	}
	else if (strcmp(key,"ALARM")==0) {
		if (strcmp("on",value)==0)
			tFrame[id].alarm_status=1;
		else 
			tFrame[id].alarm_status=0;
	}
	else if (strcmp(key,"OVER_TEMPERATURE")==0) {
		tFrame[id].over_temperature=atof(value);
	}
	else if (strcmp(key,"UNDER_TEMPERATURE")==0) {
		tFrame[id].under_temperature=atof(value);
	}
}
static int ir8062_ini_params(char *title, char *key, char *value) {
	int i=0;
	if (strcmp(title,CONNECTIVITY_TITLE)==0) {
		ir8062_set_connectivity(value);
	}
	if (ir8062_connectivity==CONNECTION_RJ45) {
		//printf("RJ45 mode not support frame setting function\n");
		return 1;
	}
	else {
		while (i<FRAME_NUMBER) {
			//printf("frame_title=%s, title =%s\n", frame_title[i], title);
			if (strcmp(frame_title[i],title)==0) {
				ir8062_set_frame_params(i,key,value);
			}
			i++;
		}
	}
	return 0;
}

int parse_ini_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return 0;
    }

    char line[MAX_LINE_LEN];
    char current_section[MAX_LINE_LEN] = "";
    while (fgets(line, MAX_LINE_LEN, file)) {
        // Remove leading and trailing whitespace
        char* trimmed_line = line;
        while (*trimmed_line == ' ' || *trimmed_line == '\t' || *trimmed_line == '\r' || *trimmed_line == '\n') {
            ++trimmed_line;
        }
        char* end = trimmed_line + strlen(trimmed_line) - 1;
        while (end > trimmed_line && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
            --end;
        }
        *(end + 1) = '\0';

        // Skip empty lines and comments
        if (*trimmed_line == '\0' || *trimmed_line == '#') {
            continue;
        }

        // Check for section headers
        if (*trimmed_line == '[' && *end == ']') {
            *end = '\0';
            strcpy(current_section, trimmed_line + 1);
        } else {
            // Parse key-value pairs
            char* equals_sign = strchr(trimmed_line, '=');
            if (equals_sign) {
                *equals_sign = '\0';
                char* key = trimmed_line;
                char* value = equals_sign + 1;
		ir8062_ini_params(current_section,key,value);

 //               printf("[%s] %s=%s\n", current_section, key, value);
            }
        }
    }
	ir8062_params_print();
    fclose(file);
    return 1;
}

void ir8062_params_print() {
	int i=0;
	while (i < FRAME_NUMBER) {
		if (tFrame[i].status==FRAME_ENABLE)
			printf("FRAME%d: X=%d, Y=%d, W=%d, H=%d, alarm %d, over=%f, under=%f\n", i+1, tFrame[i].x,tFrame[i].y,tFrame[i].w,tFrame[i].h,tFrame[i].alarm_status,tFrame[i].over_temperature,tFrame[i].under_temperature);
		else if (tFrame[i].status==FRAME_PARAMS_ERROR)
			printf("FRAME%d: Wrong ini file format\n",i+1);
		else
			printf("FRAME%d disable\n",i+1);
		i++;
	}
}

int ir8062_get_connectivity() {
	return ir8062_connectivity;
}

tFrame_t ir8062_get_frameinfo(int id) {
	return tFrame[id];
}

#if INI_PARSE_DEBUG
int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s filename.ini\n", argv[0]);
        return 1;
    }
    if (!parse_ini_file(argv[1])) {
        fprintf(stderr, "Error parsing file: %s\n", argv[1]);
        return 1;
    }
    return 0;
}
#endif
