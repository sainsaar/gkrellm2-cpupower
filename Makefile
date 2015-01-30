# Makefile for gkrellm cpufreq plugin

GTK_INCLUDE = `pkg-config gtk+-2.0 --cflags`
GTK_LIB = `pkg-config gtk+-2.0 --libs`

FLAGS = -O2 -Wall -fPIC $(GTK_INCLUDE)
LIBS = $(GTK_LIB)

LFLAGS = -shared -lcpufreq

CC = gcc $(CFLAGS) $(FLAGS)

OBJS = cpufreq.o

cpufreq.so: $(OBJS)
	$(CC) $(OBJS) -o cpufreq.so $(LFLAGS) $(LIBS)

install: cpufreq.so
	install -D -m 755 -s cpufreq.so $(DESTDIR)/usr/lib/gkrellm2/plugins/cpufreq.so
	install -g root -o root -D -m 755 cpufreqnextgovernor $(DESTDIR)/usr/sbin/cpufreqnextgovernor

install-sudo:
	echo "$(USER) ALL = (root) NOPASSWD: /usr/bin/cpufreq-set -c [0-9]* -g userspace" >> /etc/sudoers
	echo "$(USER) ALL = (root) NOPASSWD: /usr/bin/cpufreq-set -c [0-9]* -f [0-9]*" >> /etc/sudoers
	echo "$(USER) ALL = (root) NOPASSWD: /usr/sbin/cpufreqnextgovernor" >> /etc/sudoers

clean:
	rm -f *.o core *.so* *.bak *~

cpufreq.o: cpufreq.c

