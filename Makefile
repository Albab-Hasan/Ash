CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lreadline
TARGET = ash

all: $(TARGET)

$(TARGET): shell.c vars.c
	$(CC) $(CFLAGS) -o $(TARGET) shell.c vars.c $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
