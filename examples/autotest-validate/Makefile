SRC := autotest-validate.c autotest-validate-main.c
TARGET = autotest-validate
OBJS := $(SRC:.c=.o)

all: $(TARGET)
.DEFAULT_GOAL := $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map
