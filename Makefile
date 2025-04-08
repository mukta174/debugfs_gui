# Makefile for debugfs GUI

CC = gcc
PYTHON = python3
CFLAGS = -Wall -Wextra -fPIC -g -O2
LDFLAGS = -shared
LDLIBS = -lext2fs -lcom_err
TARGET_LIB = libext2_reader.so
HEADERS = 
SOURCES = ext2_reader.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean run test

all: $(TARGET_LIB)

$(TARGET_LIB): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET_LIB)
	$(PYTHON) main.py

clean:
	rm -f $(OBJECTS) $(TARGET_LIB)

test: $(TARGET_LIB)
	$(PYTHON) -c "from ext2_wrapper import Ext2Reader; reader = Ext2Reader('/dev/null'); print('Library loaded successfully!')"
