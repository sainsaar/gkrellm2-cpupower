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
	install -g root -o root -D -m 755 cpufreqnextgovernor $(DESTDIR)/usr/sbin/cpufreqnextgovernor

install-sudo:
	echo "$(USER) ALL = (root) NOPASSWD: /usr/bin/cpupower -c [0-9]* frequency-set -g userspace" >> /etc/sudoers
	echo "$(USER) ALL = (root) NOPASSWD: /usr/bin/cpupower -c [0-9]* frequency-set -f [0-9]*" >> /etc/sudoers
	echo "$(USER) ALL = (root) NOPASSWD: /usr/sbin/cpufreqnextgovernor" >> /etc/sudoers

clean:
	rm -f *.o core *.so* *.bak *~

cpupower.o: cpupower.c

