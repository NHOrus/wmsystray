include ../Rules.make

CC = gcc
CFLAGS = ${X11CFLAGS} -DTRACE_LEVEL=${TRACE_LEVEL} -I../xembed -Wall -g -D_POSIX_C_SOURCE=199309L -D_XOPEN_SOURCE=500
LDFLAGS = ${X11LDFLAGS} -lXpm

OBJS = main.o ui.o systray.o xpms.o
XEMBED_OBJS = ../xembed/xembed.o

wmsystray: ${OBJS} ${XEMBED_OBJS}
	gcc ${OBJS} ${XEMBED_OBJS} -o wmsystray ${LDFLAGS}

clean:
	rm -f wmsystray ${OBJS}
