LIBS = -lps
LIB_DIR = -L../../lib

.PHONY: clean

all: list_devices

list_devices: list_devices.c
	gcc -O2 -g -Wall -o list_devices list_devices.c $(LIB_DIR) $(LIBS)

clean:
	rm -f list_devices
