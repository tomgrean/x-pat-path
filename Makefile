CC := gcc
OBJS := xpatpath.o
LIBS := -L. -Llibexpat -lexpat
SLIB := libexpat/libexpat.a
CFLAGS := -I. -D_GNU_SOURCE=1 -g -O0 -Wall -Wextra -Wno-unused-parameter

all: libxpatpath.a test

libxpatpath.a: $(OBJS)
	ar r $@ $(OBJS)

test: test.o libxpatpath.a
	$(CC) $(CFLAGS) -o $@ $< $(LIBS) -lxpatpath

include libexpat/Makefile

clean:
	@rm -vf *.o *.a *.so test

.PHONY: all clean libxpatpath libexpatclean

%.o: %.c
	$(CC) $(CFLAGS) $(EXPAT_CFLAGS) -o $@ -c $^
