.PHONY: all clean	
CC = gcc	
CROSS_COMPILE?=	

CFLAGS = -g -Wall -Werror -pthread
SRCS = aesdsocket.c
TARGET = aesdsocket

(TARGET):$(TARGET).o
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -I/ $(TARGET).o -o $(TARGET)	

$(TARGET).o:$(TARGET).c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $(TARGET).c -o $(TARGET).o	


all: $(TARGET)

clean:	
	rm -f *.o	