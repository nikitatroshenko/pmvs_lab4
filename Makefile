PROJECT = nikfs
INC_DIR = -I/usr/include/fuse
LIBS = -pthread -lfuse -lrt -ldl
SRC_LIST = nikfs.c
CFLAGS = -D_FILE_OFFSET_BITS=64 -Wall
TARGET = $(PROJECT).out

all: $(SRC_LIST)
	gcc $(CFLAGS) $(SRC_LIST) $(INC_DIR) $(LIBS) -o $(TARGET)

clean:
	rm -f $(TARGET)
