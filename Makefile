CC = gcc
CFLAGS = -Wall -g

TARGET = message_app

# Default rule
all: $(TARGET)

# Compile
$(TARGET): message.c message.h
	$(CC) $(CFLAGS) -o $(TARGET) message.c

# Clean up files
clean:
	rm -f $(TARGET)