#include <stdio.h>
#include <curl/curl.h>
//#include <x86_64-linux-gnu/curl/curl.h>

#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#define POST_SENSOR_DATA_URL "http://demo.igreent.com/php/insert_ir_arrary_post.php"
#define IGREENT_IR_GATEWAY_URL "http://demo.igreent.com/php/exec_q_ir_gateway.php?macno=25:17:51:47:00:01"
//#define IGREENT_IR_GATEWAY_URL "http://demo.igreent.com/php/exec_q_ir_gateway.php?macno=25:17:51:47:00:02"
#define SENSOR_ID_LEN	20

#define MACNO_PARSER // derek
#ifdef MACNO_PARSER
// derek parser arg for macno
//#define IGREENT_IR_GATEWAY_URL_PREFIX "http://demo.igreent.com/php/exec_q_ir_gateway.php?macno="
static char igreent_ir_gateway_url_prefix[128]="http://demo.igreent.com/php/exec_q_ir_gateway.php?macno="; // {0};
#endif
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

int curl_exec_ir_gateway() {
  CURL *curl;
  CURLcode res;
  curl = curl_easy_init();
  if(curl) {
	printf("URL = %s, len=%d\n", IGREENT_IR_GATEWAY_URL, strlen(IGREENT_IR_GATEWAY_URL));
	printf("url = %s, len=%d\n", igreent_ir_gateway_url_prefix, strlen(igreent_ir_gateway_url_prefix));
#ifdef MACNO_PARSER
    curl_easy_setopt(curl, CURLOPT_URL, igreent_ir_gateway_url_prefix);
#else
    curl_easy_setopt(curl, CURLOPT_URL, IGREENT_IR_GATEWAY_URL);
#endif
//    curl_easy_setopt(curl, CURLOPT_URL, "https://example.com/");

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

static void fps_print() {
	static int fps=0;
	int rate=0;
	static	clock_t start;
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
static int cJSON_create_curl_ir_arrary_post_data(){
	cJSON *cjson_ir_array = NULL;
	cJSON *cjson_sensor_id = NULL;
	cJSON *cjson_modbus_cmd = NULL;
	cJSON *cjson_ir_value = NULL;
	char *out;
	cjson_ir_array = cJSON_CreateObject();
	cJSON_AddStringToObject(cjson_ir_array, "sensor_id", SensorID);
//	cJSON_AddStringToObject(cjson_ir_array, "modbus_cmd","IRCMD");
	cJSON_AddStringToObject(cjson_ir_array, "modbus_cmd","IG8062");
	cJSON_AddStringToObject(cjson_ir_array, "ir_value",sensor_data);
//	cJSON_AddStringToObject(cjson_ir_array, "ir_value",test_value);
//	cJSON_AddRawToObject(cjson_ir_array, "ir_value",test_value);
	out = cJSON_Print(cjson_ir_array);
	//printf("Create ir_array_post data\n %s \n len=%ld   END\n",out,strlen(out));
	

  CURL *curl;
  CURLcode res;
  struct curl_slist *headers = NULL;
  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, POST_SENSOR_DATA_URL);
  headers = curl_slist_append(headers, "Expect:");
  headers = curl_slist_append(headers, "Content-Type: application/json");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);



    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, out);

    /* if we do not provide POSTFIELDSIZE, libcurl will strlen() by
       itself */
//    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(ir_value));
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L);
//    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)sizeof(out));

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  cJSON_Delete(cjson_ir_array);
  free(out);
	fps_print();	
}
#ifdef MACNO_PARSER
void set_gateway_url(char *mac){
//	memset(igreent_ir_gateway_url_prefix,0,sizeof(igreent_ir_gateway_url_prefix));
//	sprintf(igreent_ir_gateway_url_prefix,"%s%s", IGREENT_IR_GATEWAY_URL_PREFIX,mac);
	strcat(igreent_ir_gateway_url_prefix, mac);
	printf("URL = %s\n", igreent_ir_gateway_url_prefix);
}
#else 
void set_gateway_url(char *mac){
	return;
}

#endif

#if 0
int main(void)
{
  curl_global_init(CURL_GLOBAL_DEFAULT);
	if (curl_exec_ir_gateway() < 0) {
		printf("ERROR : curl exec_q_ir_gateway fail\n");
	}
	cJSON_create_curl_ir_arrary_post_data();
  curl_global_cleanup();
 
  return 0;
}
#else
int curl_fun(char *data)
{
	//printf("DATA = %s, len=%ld\n",data,strlen(data));
	if (data!=NULL)
		sensor_data=data;
	else 
		printf("ERROR : there are no sensor data\n");
//  curl_global_init(CURL_GLOBAL_DEFAULT);
/*	if (curl_exec_ir_gateway() < 0) {
		printf("ERROR : curl exec_q_ir_gateway fail\n");
		return -1;
	}*/
	cJSON_create_curl_ir_arrary_post_data();
//  curl_global_cleanup();
 
  return 0;
}
#endif

