#***************************************************************************
#                                  _   _ ____  _
#  Project                     ___| | | |  _ \| |
#                             / __| | | | |_) | |
#                            | (__| |_| |  _ <| |___
#                             \___|\___/|_| \_\_____|
#
# Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
#
# This software is licensed as described in the file COPYING, which
# you should have received as part of this distribution. The terms
# are also available at https://curl.se/docs/copyright.html.
#
# You may opt to use, copy, modify, merge, publish, distribute and/or sell
# copies of the Software, and permit persons to whom the Software is
# furnished to do so, under the terms of the COPYING file.
#
# This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
# KIND, either express or implied.
#
# SPDX-License-Identifier: curl
#
###########################################################################

# What to call the final executable
TARGET = mi0801

# Which object files that the executable consists of
#OBJS= simple.o
#OBJS= http-post.o
#OBJS= simplepost.o cJSON.o
#OBJS= main.c cJSON.c curl_fun.c
OBJS= main.o cJSON.o curl_fun.o

# What compiler to use
CC = arm-linux-gcc

# Compiler flags, -g for debug, -c to make an object file
#CFLAGS = -I/usr/local/arm_linux_4.8/user/include #-c -g
#CFLAGS = -c -g
# This should point to a directory that holds libcurl, if it isn't
# in the system's standard lib dir
# We also set a -L to include the directory where we have the openssl
# libraries
#LDFLAGS = -L/home/dast/lib -L/usr/local/ssl/lib
#LDFLAGS = -L/usr/local/arm_linux_4.8/usr/lib #-L/usr/local/arm_linux_4.8/usr/arm-nuvoton-linux-uclibcgnueabi/sysroot/usr/lib/
# We need -lcurl for the curl stuff
# We need -lsocket and -lnsl when on Solaris
# We need -lssl and -lcrypto when using libcurl with SSL support
# We need -lpthread for the pthread example
#LIBS = -lcurl -lsocket -lnsl -lssl -lcrypto
LIBS = -lcurl -lm #-lssl -lcrypto #-ljson-c -lnsl 

# Link the target with all objects and libraries
$(TARGET) : $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS) $(CFLAGS) $(LIBS) 
clean:
	rm -r *.o
