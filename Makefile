CC = gcc
CFLAGS-common = -Wall -Wextra -Werror -pedantic -std=gnu18
CFLAGS = $(CFLAGS-common) -g -O2
#CFLAGS-dbg = $(CFLAGS-common) -Og -ggdb  
CFLAGS-dbg = $(CFLAGS-common) -Og -ggdb -fsanitize=address
 
LOGIN = kerai
SUBMITPATH = ~cs537-1/handin/kerai/

TARGET = wsh
SRC = $(TARGET).c

all: $(TARGET) $(TARGET)-dbg

$(TARGET): $(SRC) 
	$(CC) $(CFLAGS) $< -o $@

$(TARGET)-dbg: $(SRC)
	$(CC) $(CFLAGS-dbg) $< -o $@

clean:
	rm -f $(TARGET) $(TARGET)-dbg 

