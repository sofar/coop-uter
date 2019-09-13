
/**

Copyright 2019 - Auke Kok <sofar@foo-projects.org

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject
to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <modbus.h>
#include <mosquitto.h>
#include <libconfig.h>

#define CONFIG_PATH "/etc/mqtt.conf"

#define PUBLISH_INTERVAL 600

static const char* charging_states[] = {
	"charging deactivated",
	"charging activated",
	"mptt charging",
	"equalizing charging",
	"boost charging",
	"floating charging",
	"current limiting"
};

#define FAULT_BITS_MAX 15
static const char* fault_bits[FAULT_BITS_MAX] = {
	"battery over-discharge",
	"battery over-voltage",
	"battery under-voltage",
	"load short circuit",
	"load overpower or load over-current",
	"controller temperature too high",
	"ambient temperature too high",
	"photovoltaic input overpower",
	"photovoltaic input side short circuit",
	"photovoltaic input side over-voltage",
	"solar panel counter-current",
	"solar panel working point over-voltage",
	"solar panel reversely connected",
	"anti-reverse MOS short",
	"circuit, charge MOS short circuit"
};

static modbus_t *ctx;

static char *topic_control = NULL;
static char *topic_state = NULL;

static int load = -1;

static int stop = 0;

static void sigfunc(int s __attribute__ ((unused)))
{
	stop = 1;
}

static void publish_state(struct mosquitto *mosq)
{
	uint16_t regs[64];
	int ret;

	/* read info block regs */
	memset(regs, 0, sizeof(regs));
	ret = modbus_read_registers(ctx, 0x100, 0x22, regs);
	if (ret < 0) {
		fprintf(stderr, "Failed to read registers: %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		exit(EXIT_FAILURE);
	}

	/* create mqtt publish stream */
	int battery_capacity = regs[0];
	float battery_voltage = regs[1] / 10.;
	float battery_current = regs[2] / 100.;

	int controller_temperature = (int8_t) MODBUS_GET_HIGH_BYTE(regs[3]);

	float load_voltage = regs[4] / 10.;
	float load_current = regs[5] / 100.;
	int load_power = regs[6];

	float panel_voltage = regs[7] / 10.;
	float panel_current = regs[8] / 100.;
	int panel_power = regs[9];

	float battery_voltage_min_day = regs[0xb] / 10.;
	float battery_voltage_max_day = regs[0xc] / 10.;

	float charge_current_max_day = regs[0xd] / 100.;
	float discharge_current_max_day = regs[0xe] / 100.;

	float charge_power_max_day = regs[0xf] / 100.;
	float discharge_power_max_day = regs[0x10] / 100.;

	int charge_amp_hours_day = regs[0x11];
	int discharge_amp_hours_day = regs[0x12];

	float charge_generated_day = regs[0x13] / 10000.;
	float charge_consumed_day = regs[0x14] / 10000.;

	const char *state = charging_states[(int8_t) MODBUS_GET_LOW_BYTE(regs[0x20])];

	int load_enable = MODBUS_GET_HIGH_BYTE(regs[0x20]) >> 7;
	int load_brightness = MODBUS_GET_HIGH_BYTE(regs[0x20]) & 0x7f;

	int errors = regs[0x22];
	char *error_strings = NULL;
	for (int b = 0; b < 15; b++) {
		if (errors & (2 >> b)) {
			if (!error_strings) {
				error_strings = strdup(fault_bits[b]);
			} else {
				char *olderr = strdup(error_strings);
				free(error_strings);
				if (!asprintf(&error_strings, "%s, %s", olderr, fault_bits[b]))
					exit(EXIT_FAILURE);
				free(olderr);
			}
		}
	}
	if (!error_strings) {
		error_strings = strdup("none");
	}

	char *msg = NULL;
	if (asprintf(&msg,
			"{"
			"\"battery_capacity\":\"%d\","
			"\"battery_voltage\":\"%.1f\","
			"\"battery_current\":\"%.2f\","
			"\"controller_temperature\":\"%d\","
			"\"load_voltage\":\"%.1f\","
			"\"load_current\":\"%.2f\","
			"\"load_power\":\"%d\","
			"\"panel_voltage\":\"%.1f\","
			"\"panel_current\":\"%.2f\","
			"\"panel_power\":\"%d\","
			"\"battery_voltage_min_day\":\"%.1f\","
			"\"battery_voltage_max_day\":\"%.1f\","
			"\"charge_current_max_day\":\"%.2f\","
			"\"discharge_current_max_day\":\"%.2f\","
			"\"charge_power_max_day\":\"%.2f\","
			"\"discharge_power_max_day\":\"%.2f\","
			"\"charge_amp_hours_day\":\"%d\","
			"\"discharge_amp_hours_day\":\"%d\","
			"\"charge_generated_day\":\"%.2f\","
			"\"charge_consumed_day\":\"%.2f\","
			"\"charging_state\":\"%s\","
			"\"error_state\":\"%s\","
			"\"load_enable\":\"%d\","
			"\"load_brightness\":\"%d\""
			"}",
			battery_capacity, battery_voltage, battery_current,
			controller_temperature,
			load_voltage, load_current, load_power,
			panel_voltage, panel_current, panel_power,
			battery_voltage_min_day, battery_voltage_max_day,
			charge_current_max_day, discharge_current_max_day,
			charge_power_max_day, discharge_power_max_day,
			charge_amp_hours_day, discharge_amp_hours_day,
			charge_generated_day, charge_consumed_day,
			state, error_strings, load_enable, load_brightness) < 0)
		exit(EXIT_FAILURE);

	if (mosquitto_publish(mosq, NULL, topic_state, strlen(msg), msg, 0, true) != 0)
		exit(EXIT_FAILURE);
	free(msg);
}

static void message_callback(
		struct mosquitto *mosq,
		void *obj __attribute__ ((unused)),
		const struct mosquitto_message *message)
{
	char *tmp = NULL;

	// use strncmp() instead?
	if (!asprintf(&tmp, "%.*s", message->payloadlen, (char *)message->payload))
		exit(EXIT_FAILURE);

	int i = atoi(tmp);
	
	free(tmp);

	if ((i < 0) || (i > 100))
		return;

	if (i == load)
		return;

	if ((load <= 0) && (i > 0)) {
		fprintf(stderr, "Load enabled, %d\n", i);
		// modbus: write enable load
		if (modbus_write_register(ctx, 0x10a, 1) < 0)
			fprintf(stderr, "Error enabling load\n");
		// modbus: write dimmer value
		if (modbus_write_register(ctx, 0xe001, i) < 0)
			fprintf(stderr, "Error setting dimmer value\n");
	} else if ((load > 0) && (i == 0)) {
		fprintf(stderr, "Load disabled\n");
		// modbus: write disable load
		if (modbus_write_register(ctx, 0x10a, 0) < 0)
			fprintf(stderr, "Error disabling load\n");
	} else {
		fprintf(stderr, "Load changed, %d\n", i);
		// modbus: write dimmer value
		if (modbus_write_register(ctx, 0xe001, i) < 0)
			fprintf(stderr, "Error settign dimmer value\n");
	}

	load = i;

	publish_state(mosq);
}

int main(void) {
	static struct mosquitto *mosq = NULL;
	config_t cfg;
	int ret;
	const char *conf_server;
	int conf_port;
	time_t publish_time = time(NULL) - (time_t)PUBLISH_INTERVAL;

	// what to do if terminated
	signal(SIGINT, sigfunc);
	signal(SIGTERM, sigfunc);

	// parse configs
	config_init(&cfg);
	if (!config_read_file(&cfg, CONFIG_PATH)) {
		fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
		exit(EXIT_FAILURE);
	}

	if (!config_lookup_string(&cfg, "server", &conf_server)) {
		fprintf(stderr, "No server defined in " CONFIG_PATH "\n");
		exit(EXIT_FAILURE);
	}
	if (!config_lookup_int(&cfg, "port", &conf_port)) {
		fprintf(stderr, "No port defined in " CONFIG_PATH "\n");
		exit(EXIT_FAILURE);
	}

	fprintf(stderr, "MQTT server: %s:%d\n", conf_server, conf_port);

	// setup modbus
	ctx = modbus_new_rtu("/dev/ttyS1", 9600, 'N', 8, 1);
	if (!ctx) {
		perror("Unable to create the libmodbus context\n");
		exit(EXIT_FAILURE);
	}

	modbus_set_slave(ctx, 1);

	if (modbus_connect(ctx) == -1) {
		fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		exit(EXIT_FAILURE);
	}

	// use system hostname here
	char hostname[HOST_NAME_MAX+1];
	hostname[HOST_NAME_MAX] = 0;
	if (gethostname(hostname, HOST_NAME_MAX) != 0)
		exit(EXIT_FAILURE);

	// setup topics
	if (!asprintf(&topic_state, "/%s/renogy/state", hostname))
		exit(EXIT_FAILURE);
	if (!asprintf(&topic_control, "/%s/renogy/control", hostname))
		exit(EXIT_FAILURE);

	/* setup mqtt */
	mosquitto_lib_init();
	mosq = mosquitto_new(NULL, true, NULL);
	if (!mosq)
		exit(EXIT_FAILURE);

	mosquitto_message_callback_set(mosq, message_callback);

	while (mosquitto_connect(mosq, conf_server, conf_port, 15) != 0) {
		fprintf(stderr, "Waiting for connection to server\n");
		sleep(300);
	}

	ret = mosquitto_subscribe(mosq, NULL, topic_control, 0);
	if (ret != 0) {
		fprintf(stderr, "mosquitto_subscribe: %d: %s\n", ret, strerror(errno));
	}

	fprintf(stderr, "connected, state topic = %s, control topic = %s\n",
		topic_state, topic_control);

	for (;;) {
		ret = mosquitto_loop(mosq, 10000, 1);
		if ((ret == MOSQ_ERR_CONN_LOST) || (ret == MOSQ_ERR_NO_CONN)) {
			sleep(5);
			mosquitto_reconnect(mosq);
		} else if (ret != MOSQ_ERR_SUCCESS) {
			fprintf(stderr, "mosquitto_loop(): %d, %s\n", ret, strerror(errno));
			exit(EXIT_FAILURE);
		}

		if (stop == 1) {
			publish_state(mosq);
			break;
		}

		time_t now = time(NULL);
		if (now - publish_time > (time_t)PUBLISH_INTERVAL) {
			publish_time = now;
			publish_state(mosq);
		}
	}

	mosquitto_disconnect(mosq);
	mosquitto_loop_stop(mosq, false);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	modbus_close(ctx);
	modbus_free(ctx);

	config_destroy(&cfg);
}
