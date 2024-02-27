.SUFFIXES : .x .o .c .s

#CC := gcc #arm-linux-gcc
TARGET = jpegenc

CC := arm-linux-gcc
#STRIP := arm-linux-strip
#TARGET = ini-parse
OBJS= jpegenc.o
LIBS= -ljpeg

$(TARGET) : $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS) $(CFLAGS) $(LIBS) 
clean:
	rm -f *.o 
	rm -f *.x 
	rm -f *.flat
	rm -f *.map
	rm -f temp
	rm -f *.img
	rm -f $(TARGET)	
	rm -f *.gdb
	rm -f *.bak
