CFLAGS = -Wall -O2 -std=gnu99
CC = gcc

TARGET = ipcam_search_pc ipcam_search_device

.PHONY: all clean

all: $(TARGET)

ipcam_search_pc: ipcam_search_pc.o
	$(CC) -o $@ $^ -lpthread

ipcam_search_pc.o: ipcam_search_pc.c ipcam_message.h
	$(CC) -c -o $@ $< $(CFLAGS)

ipcam_search_device: ipcam_search_device.o
	$(CC) -o $@ $^ -lpthread

ipcam_search_device.o: ipcam_search_device.c ipcam_message.h
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	-rm -f $(TARGET) *.o
