# Makefile for gkrellm cpupower plugin

GTK_INCLUDE = `pkg-config gtk+-2.0 --cflags`
GTK_LIB = `pkg-config gtk+-2.0 --libs`

FLAGS = -O2 -Wall -fPIC $(GTK_INCLUDE)
LIBS = $(GTK_LIB)

LFLAGS = -shared -lcpupower

CC = gcc $(CFLAGS) $(FLAGS)

OBJS = cpupower.o

cpupower.so: $(OBJS)
	$(CC) $(OBJS) -o cpupower.so $(LFLAGS) $(LIBS)

install: cpupower.so
	install -D -m 755 -s cpupower.so $(DESTDIR)/usr/lib/gkrellm2/plugins/cpupower.so

install-sudo:
	mkdir -p $(DESTDIR)/etc/sudoers.d
	echo "%trusted ALL = (root) NOPASSWD: /usr/bin/cpupower -c [0-9]* frequency-set -g [a-z]*" >> $(DESTDIR)/etc/sudoers.d/gkrellm2-cpupower
	echo "%trusted ALL = (root) NOPASSWD: /usr/bin/cpupower -c [0-9]* frequency-set -f [0-9]*" >> $(DESTDIR)/etc/sudoers.d/gkrellm2-cpupower

clean:
	rm -f *.o core *.so* *.bak *~

cpupower.o: cpupower.c

