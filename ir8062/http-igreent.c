#include <stdio.h>
#include <curl/curl.h>
//#include <x86_64-linux-gnu/curl/curl.h>

#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "cloud-service.h"
#include "http-igreent.h"
#include "ini-parse.h"
#include "sensor.h"

#define IGREENT_POST_FRAME_URL "http://demo.igreent.com/php/insert_ir_arrary_post.php"
#define IGREENT_IR_GATEWAY_URL "http://demo.igreent.com/php/exec_q_ir_gateway.php?macno=25:17:51:47:00:01"
#define POST_SENSOR_DATA_URL "http://demo.igreent.com/php/insert_ir_arrary_post.php"
//#define IGREENT_IR_GATEWAY_URL "http://demo.igreent.com/php/exec_q_ir_gateway.php?macno=25:17:51:47:00:01"
//#define IGREENT_IR_GATEWAY_URL "http://demo.igreent.com/php/exec_q_ir_gateway.php?macno=25:17:51:47:00:02"
#define SENSOR_ID_LEN	20
#define THERMAL_STRING_LEN  29760  // 80x62x2x3 ( 3 char per 1 byte)

char thermal_data_string[THERMAL_STRING_LEN]={0};

// derek parser arg for macno
static char igreent_ir_gateway_url_prefix[128]="http://demo.igreent.com/php/exec_q_ir_gateway.php?macno="; // {0};
static char SensorID[SENSOR_ID_LEN]={0};

struct memory {
  char *response;
  size_t size;
};
struct memory chunk = {0};
char *sensor_data;

static int cJSONParserID(const char * const response) {
  cJSON *res = NULL;
  cJSON *sensor_id = NULL;
  char *out;    
  long unsigned int len=0;
  int status = 0;
  out=strchr(response,0x7b); // search json string start address of char "{" 
  printf("JSON res = %s\n",out);
  res = cJSON_Parse(out);
  if (res == NULL)
  {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL)
    {
      fprintf(stderr, "Error before: %s\n", error_ptr);
    }
    status = 0;
    return -1;
  }
	sensor_id = cJSON_GetObjectItem(res,"sensor_id");
  if ((sensor_id == NULL))
  {
    printf("ERROR : can't find JSON key of sensor_id\n");
    return -1;
	}
  else if (cJSON_IsString(sensor_id)) {
  	len = strlen(sensor_id->valuestring);
  	printf("Get JSON key of sensor_id, value=%s, len=%ld\n",sensor_id->valuestring, len);
  	memset(SensorID,0, sizeof(SensorID)) ;
  	memcpy(SensorID, sensor_id->valuestring,  len);
  	printf("SensorID is string=%s, len=%ld\n",SensorID,strlen(SensorID));
  }
  else {
  	printf("ERROR : return value not string format of JSON key sensor_id\n");
  	return -1;
  }
  //cJSON_Delete(sensor_id);
  cJSON_Delete(res);
  return 0;
}

static size_t exec_q_ir_gateway_cb(void *data, size_t size, size_t nmemb, void *clientp)
{
  size_t realsize = size * nmemb;
  struct memory *mem = (struct memory *)clientp;
 
  char *ptr = realloc(mem->response, mem->size + realsize + 1);
  if(ptr == NULL)
    return 0;  /* out of memory! */
 
  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), data, realsize);
  mem->size += realsize;
  mem->response[mem->size] = 0;
	printf("response:\n%s\nEnd\n",mem->response);
	cJSONParserID(mem->response);
  return realsize;
}

static void fps_print() {
  static int fps=0;
  int rate=0;
  static  clock_t start;
  clock_t now=clock();
  if (fps==0) {
    start=now;
  }
  fps++;
  if ( (now-start) > 3000000 ) {
    rate = fps/3;
    printf("fps = %d, rate=%d, start = %ld, now=%ld, diff=%d\n",fps, rate, start,now, now-start);
    fps = 0;
    printf("CLOCK_PER_SEC = %ld\n",CLOCKS_PER_SEC);
  }
}

void set_gateway_url(char *mac){
  size_t len;
  strcat(igreent_ir_gateway_url_prefix, mac);
  len=strlen(igreent_ir_gateway_url_prefix);
  igreent_ir_gateway_url_prefix[len]='\0';
  printf("URL = %s len=%d\n", igreent_ir_gateway_url_prefix,len);
}

int curl_exec_ir_gateway() {
  CURL *curl;
  CURLcode res;
  curl = curl_easy_init();
  if(curl) {
  	printf("URL TEST = %s, len=%d\n", IGREENT_IR_GATEWAY_URL, strlen(IGREENT_IR_GATEWAY_URL));
  	printf("url REAL = %s, len=%d\n", igreent_ir_gateway_url_prefix, strlen(igreent_ir_gateway_url_prefix));
    curl_easy_setopt(curl, CURLOPT_URL, igreent_ir_gateway_url_prefix);

    /* send all data to this function  */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, exec_q_ir_gateway_cb);
 
    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
 
    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
      return -1;
    }
 
    /* remember to free the buffer */
	  free(chunk.response);
    /* always cleanup */
    curl_easy_cleanup(curl);
    return 0;
  }
}


static void convert_raw_to_string()
{
  int i;
  memset(thermal_data_string,'\0',strlen(sensor_data));
  sprintf(thermal_data_string , "%02X", sensor_data);
  for (i=1; i < 9920; i++) {
    if ( (i*3)>= THERMAL_STRING_LEN ) {
      //printf("ERROR : over buffer length (%d)\n",i*3);
      break; // over buffer length
    }
    //printf("%d : ,%02X\n",i, sensor_data[i]);
    sprintf(thermal_data_string + (i * 3)-1, ",%02X", sensor_data[i]);
  }
//  printf("Output string : %s\n",thermal_data_string);
  // end Thermal raw data string
  // Conver Thermal raw data to string
}
static int full_data_post(){
	cJSON *cjson_ir_array = NULL;
	cJSON *cjson_sensor_id = NULL;
	cJSON *cjson_modbus_cmd = NULL;
	cJSON *cjson_ir_value = NULL;
	char *out;
  CURL *curl;
  CURLcode res;
  struct curl_slist *headers = NULL;
  convert_raw_to_string();
	cjson_ir_array = cJSON_CreateObject();
	cJSON_AddStringToObject(cjson_ir_array, "sensor_id", SensorID);
	cJSON_AddStringToObject(cjson_ir_array, "modbus_cmd","IG8062_80X62");
	cJSON_AddStringToObject(cjson_ir_array, "ir_value",thermal_data_string);
	out = cJSON_Print(cjson_ir_array);
	//printf("Create ir_array_post data\n %s \n len=%ld   END\n",out,strlen(out));

  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, POST_SENSOR_DATA_URL);
    headers = curl_slist_append(headers, "Expect:");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, out);
    /* if we do not provide POSTFIELDSIZE, libcurl will strlen() by itself */
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L);

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  cJSON_Delete(cjson_ir_array);
  free(out);
	fps_print();	
}

static int simple_data_post() {
  cJSON *cjson_ir_array = NULL;
  cJSON *cjson_sensor_id = NULL;
  cJSON *cjson_modbus_cmd = NULL;
  cJSON *cjson_ir_value = NULL;
  char *out;
  CURL *curl;
  CURLcode res;
  struct curl_slist *headers = NULL;
  char max_temp_str[5]={0};
  char *max_temp=get_full_frame_max_temperature();
  unsigned int temp=(max_temp[0]<<8) | max_temp[1];
  printf("Max temperature = %d\n",temp);
  if (get_eth_over_temperature() > temp) {
    printf("Normal temperature, return\n");
    return 0;
  }
  sprintf(max_temp_str, "%02X,%02X", max_temp[0],max_temp[1]);
  printf("MAX Temperature = %s\n", max_temp_str);
  cjson_ir_array = cJSON_CreateObject();
  cJSON_AddStringToObject(cjson_ir_array, "sensor_id", SensorID);
  cJSON_AddStringToObject(cjson_ir_array, "modbus_cmd","IG8062_01X01");
  cJSON_AddStringToObject(cjson_ir_array, "ir_value",max_temp_str);
  out = cJSON_Print(cjson_ir_array);
  printf("Create ir_array_post data\n %s \n len=%ld   END\n",out,strlen(out));

  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, POST_SENSOR_DATA_URL);
    headers = curl_slist_append(headers, "Expect:");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, out);
    /* if we do not provide POSTFIELDSIZE, libcurl will strlen() by itself */
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L);

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  cJSON_Delete(cjson_ir_array);
  free(out);
  fps_print();  
  return 0;
}

int curl_fun(char *data)
{
  //printf("DATA = %s, len=%ld\n",data,strlen(data));
  if (data!=NULL)
    sensor_data=data;
  else 
    printf("ERROR : there are no sensor data\n");
  full_data_post();
  return 0;
}

static int igreent_register(){
  size_t len;
  strcat(igreent_ir_gateway_url_prefix, ir8062_get_mac_address());
  len=strlen(igreent_ir_gateway_url_prefix);
  igreent_ir_gateway_url_prefix[len]='\0';
  printf("URL = %s len=%d\n", igreent_ir_gateway_url_prefix,len);
  return 0;
}
int http_igreent_post(char *data) {
  //int data_format=ir8062_get_post_data_format();
  int data_format = get_eth_data_format();
  if (SensorID[0]==0) {
    printf("Registering MAC address to igreent cloud\n");
    igreent_register();
    curl_exec_ir_gateway();
    return 1;
  }
  printf("POST sensor data\n");
  sensor_data = data;
  switch (data_format) {
    case HTTP_POST_FULL_DATA:    
      printf("Full data\n");
      full_data_post(); 
      break;
    case HTTP_POST_SIMPLE_DATA:
      printf("Simple data\n");
      simple_data_post();
      break;
    default:
      simple_data_post();
      //full_data_post(); 
      break;
  }
  return 0;
}