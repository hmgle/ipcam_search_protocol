debug =
PLATFORM = $(shell uname -s)
EXESUF =
CFLAGS = -Wall -fno-strict-aliasing
LIBS = -lpthread

ifeq ($(PLATFORM), MINGW32_NT-5.1)
	WINLIBS = -lws2_32 -liphlpapi
	EXESUF = .exe
else ifeq ($(PLATFORM), Linux)
	WINLIBS =
	CFLAGS += -D_LINUX_=1
else
$(error only for Linux or mingw PLATFORM.)
endif
ifeq ($(debug), 1)
	CFLAGS += -Wextra -O0 -DDEBUG=1 -g
else
	CFLAGS += -O2
endif

LIBS += $(WINLIBS)
CROSS_COMPILE = 
CC = gcc
CC := $(CROSS_COMPILE)$(CC)
STRIP ?= strip
STRIP := $(CROSS_COMPILE)$(STRIP)

TARGET = ipcam_search_pc ipcam_search_device ipcam_search_pcv2 \
	 ipcam_search_devicev2

.PHONY: all clean

all: $(TARGET)

strip: all
	$(STRIP) $(patsubst %,%$(EXESUF),$(TARGET))

ipcam_search_pc: ipcam_search_pc.o ipcam_list.o ipcam_message.o \
				 socket_wrap.o
	$(CC) -o $@ $^ $(LIBS)

ipcam_search_pc.o: ipcam_search_pc.c ipcam_message.h debug_print.h \
		   para_parse.o
	$(CC) -c -o $@ $< $(CFLAGS)

ipcam_search_pcv2: ipcam_search_pcv2.o ipcam_list.o ipcam_message.o \
			 socket_wrap.o para_parse.o already_running.o
	$(CC) -o $@ $^

ipcam_search_pcv2.o: ipcam_search_pcv2.c ipcam_message.h debug_print.h
	$(CC) -c -o $@ $< $(CFLAGS)

ipcam_list.o: ipcam_list.c ipcam_list.h debug_print.h
	$(CC) -c -o $@ $< $(CFLAGS)

ipcam_search_device: ipcam_search_device.o get_mac.o ipcam_message.o \
			socket_wrap.o config_ipcam_info.o para_parse.o
	$(CC) -o $@ $^ $(LIBS)

ipcam_search_devicev2: ipcam_search_devicev2.o get_mac.o ipcam_message.o \
			socket_wrap.o config_ipcam_info.o para_parse.o
	$(CC) -o $@ $^

para_parse.o: para_parse.c para_parse.h
	$(CC) -c -o $@ $< $(CFLAGS)

config_ipcam_info.o: config_ipcam_info.c config_ipcam_info.h
	$(CC) -c -o $@ $< $(CFLAGS)

socket_wrap.o: socket_wrap.c socket_wrap.h
	$(CC) -c -o $@ $< $(CFLAGS)

get_mac.o: get_mac.c get_mac.h
	$(CC) -c -o $@ $< $(CFLAGS)

ipcam_message.o: ipcam_message.c ipcam_message.h
	$(CC) -c -o $@ $< $(CFLAGS)

already_running.o: already_running.c already_running.h
	$(CC) -c -o $@ $< $(CFLAGS)

get_mac.h: debug_print.h

ipcam_search_device.o: ipcam_search_device.c ipcam_message.h \
					   debug_print.h get_mac.h
	$(CC) -c -o $@ $< $(CFLAGS)

ipcam_search_devicev2.o: ipcam_search_devicev2.c ipcam_message.h \
					   debug_print.h get_mac.h
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	-rm -f $(TARGET:%=%$(EXESUF)) *.o
