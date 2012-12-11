CFLAGS = -Wall -O2 -fno-strict-aliasing
CC = gcc

TARGET = ipcam_search_pc ipcam_search_device

.PHONY: all clean

all: $(TARGET)

ipcam_search_pc: ipcam_search_pc.o ipcam_list.o
	$(CC) -o $@ $^ -lpthread

ipcam_search_pc.o: ipcam_search_pc.c ipcam_message.h debug_print.h
	$(CC) -c -o $@ $< $(CFLAGS)

ipcam_list.o: ipcam_list.c ipcam_list.h debug_print.h
	$(CC) -c -o $@ $< $(CFLAGS)

ipcam_search_device: ipcam_search_device.o get_mac.o
	$(CC) -o $@ $^ -lpthread

get_mac.o: get_mac.c get_mac.h
	$(CC) -c -o $@ $< $(CFLAGS)

get_mac.h: debug_print.h

ipcam_search_device.o: ipcam_search_device.c ipcam_message.h \
					   debug_print.h get_mac.h
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	-rm -f $(TARGET) *.o
