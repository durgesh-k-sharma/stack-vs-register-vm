CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic

.PHONY: all clean

all: vmstudy

vmstudy: src/vmstudy.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f vmstudy
