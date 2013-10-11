
export LD_LIBRARY_PATH=/usr/local/lib

CFLAGS=-g

LDFLAGS=-L/usr/local/lib/

LIBS = -lpthread
LIBS += -levent 

TARGET=libevent_server_multi

CC=gcc

SRC=libevent_main.c libevent_multi.c libevent_socket.c log.c

$(TARGET):$(SRC)
	$(CC) -o $(TARGET) $(SRC) $(LDFLAGS) $(LIBS)

clean:
	rm $(TARGET)
