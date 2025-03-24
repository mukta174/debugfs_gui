# Compiler
CC = gcc

# Compiler Flags
CFLAGS = -Wall -Wextra -g -D_GNU_SOURCE

# Libraries
LIBS = -lext2fs -lcom_err -le2p

# Source files
SRCS = main.c ext_2.c

# Object files
OBJS = $(SRCS:.c=.o)

TARGET = ext2
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET) ext2.img
