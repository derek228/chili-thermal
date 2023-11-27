.SUFFIXES : .x .o .c .s

CC := arm-linux-gcc
#STRIP := arm-linux-strip
TARGET = ir8062

#SRCS := ini-parser.c main.c
#OBJS= main.o cJSON.o curl_fun.o sensor.o cJSON.o
OBJS= main.o cJSON.o curl_fun.o sensor.o ini-parse.o
LIBS = -lcurl -lm #-lssl -lcrypto #-ljson-c -lnsl 

$(TARGET) : $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS) $(CFLAGS) $(LIBS) 
clean:
	rm -r *.o

