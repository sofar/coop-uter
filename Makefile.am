
AM_CFLAGS = -g $(modbus_CFLAGS) $(mosquitto_CFLAGS) -Wall -Wno-uninitialized -W -D_FORTIFY_SOURCE=2 -L/usr/local/lib64

bin_PROGRAMS = renogy-dump renogy-pub renogy-sys
renogy_dump_SOURCES = dump.c
renogy_pub_SOURCES = publish.c
renogy_sys_SOURCES = system.c

renogy_dump_LDADD = \
	$(modbus_LIBS)

renogy_pub_LDADD = \
	$(modbus_LIBS) \
	$(mosquitto_LIBS)

renogy_sys_LDADD = \
	$(mosquitto_LIBS)
