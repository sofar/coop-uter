
AM_CFLAGS = -g $(modbus_CFLAGS) $(mosquitto_CFLAGS) $(gpiod_CFLAGS) $(config_CFLAGS) \
	 -Wall -Wno-uninitialized -W -D_FORTIFY_SOURCE=2 -L/usr/local/lib64

bin_PROGRAMS = panel-dump panel-pub mqtt-system-control mqtt-door-control modbus-write
panel_dump_SOURCES = dump.c
panel_pub_SOURCES = publish.c
mqtt_system_control_SOURCES = system.c
mqtt_door_control_SOURCES = door.c
modbus_write_SOURCES = write.c

panel_dump_LDADD = \
	$(modbus_LIBS)

panel_pub_LDADD = \
	$(modbus_LIBS) \
	$(mosquitto_LIBS) \
	$(config_LIBS)

mqtt_system_control_LDADD = \
	$(mosquitto_LIBS) \
	$(config_LIBS)

mqtt_door_control_LDADD = \
	$(mosquitto_LIBS) \
	$(gpiod_LIBS) \
	$(config_LIBS)

modbus_write_LDADD = \
	$(modbus_LIBS)
