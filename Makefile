# mingw: make os=win
# linux: make

os =
EXESUF =
CFLAGS = -Wall -O2 -fno-strict-aliasing
LIBS = -lpthread
ifeq ($(os), win)
	WINLIBS = -lws2_32 -liphlpapi
	EXESUF = .exe
else
	WINLIBS =
	CFLAGS += -D_LINUX_=1
endif
CROSS_COMPILE = 
CC = gcc
CC := $(CROSS_COMPILE)$(CC)
STRIP ?= strip
STRIP := $(CROSS_COMPILE)$(STRIP)

TARGET = ipcam_search_pc ipcam_search_device

.PHONY: all clean

all: $(TARGET)

ipcam_search_pc: ipcam_search_pc.o ipcam_list.o ipcam_message.o \
				 socket_wrap.o
	$(CC) -o $@ $^ $(LIBS)
	$(STRIP) $@$(EXESUF)

ipcam_search_pc.o: ipcam_search_pc.c ipcam_message.h debug_print.h
	$(CC) -c -o $@ $< $(CFLAGS)

ipcam_list.o: ipcam_list.c ipcam_list.h debug_print.h
	$(CC) -c -o $@ $< $(CFLAGS)

ipcam_search_device: ipcam_search_device.o get_mac.o ipcam_message.o \
					 socket_wrap.o config_ipcam_info.o
	$(CC) -o $@ $^ $(LIBS)
	$(STRIP) $@$(EXESUF)

config_ipcam_info.o: config_ipcam_info.c config_ipcam_info.h
	$(CC) -c -o $@ $< $(CFLAGS)

socket_wrap.o: socket_wrap.c socket_wrap.h
	$(CC) -c -o $@ $< $(CFLAGS)

get_mac.o: get_mac.c get_mac.h
	$(CC) -c -o $@ $< $(CFLAGS)

ipcam_message.o: ipcam_message.c ipcam_message.h
	$(CC) -c -o $@ $< $(CFLAGS)

get_mac.h: debug_print.h

ipcam_search_device.o: ipcam_search_device.c ipcam_message.h \
					   debug_print.h get_mac.h
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	-rm -f $(TARGET:%=%$(EXESUF)) *.o
