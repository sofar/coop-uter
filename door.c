
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
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <limits.h>

#include <mosquitto.h>
#include <gpiod.h>
#include <libconfig.h>

#define GPIOD_CONSUMER "renogy-door"
#define CONFIG_PATH "/etc/mqtt.conf"

static const char* door_states[] = {
	"closed", //0
	"opening", //1
	"open", //2
	"closing", //3
	"error", //4
	"initializing" //5
};

static int state = 5; // door_states, -1 means unknown state (initializing).
static int published_state = -1;

static int sensor_closed = 0;
static int sensor_open = 0;

static char *topic_control = NULL;
static char *topic_state = NULL;

static int stop = 0;

static time_t command_time = 0; // time() of last command received
static bool command = false; // command pending

// 2 minute intervals between normal idle publishes
#define CHECK_INTERVAL 150

static void sigfunc(int s __attribute__ ((unused)))
{
	stop = 1;
}

static void get_sensor_data()
{
	int closed = gpiod_ctxless_get_value("3", 22, true, GPIOD_CONSUMER);
	if (closed < 0) {
		fprintf(stderr, "gpiod sensor read 15: %d\n", closed);
	} else {
		sensor_closed = closed;
	}

	int open = gpiod_ctxless_get_value("3", 15, true, GPIOD_CONSUMER);
	if (open < 0) {
		fprintf(stderr, "gpiod sensor read 22: %d\n", open);
	} else {
		sensor_open = open;
	}
}

static void get_state()
{
	get_sensor_data();

	if ((sensor_closed == 1) && (sensor_open == 1)) {
		fprintf(stderr, "Invalid sensor data: both open and closed.\n");
		state = 4;
	} else if (state == 5) {
		// initializing
		if ((sensor_closed == 1) && (sensor_open == 0)) {
			state = 0;
		} else if ((sensor_closed == 0) && (sensor_open == 1)) {
			state = 2;
		}
	} else if (state == 0) {
		// closed
		if ((sensor_closed == 1) && (sensor_open == 0)) {
			return;
		} else if (sensor_closed == 0) {
			fprintf(stderr, "Door no longer is closed\n");
			if (sensor_open == 0) {
				state = 1;
			} else if (sensor_open == 1) {
				state = 2;
			}
		}
	} else if (state == 2) {
		// open
		if ((sensor_closed == 0) && (sensor_open == 1)) {
			return;
		} else if (sensor_open == 0) {
			fprintf(stderr, "Door no longer is open\n");
			if (sensor_closed == 0) {
				state = 3;
			} else if (sensor_closed == 1) {
				state = 0;
			}
		}
	} else {
		if (state == 1) { // opening
			if ((sensor_closed == 0) && (sensor_open == 1)) {
				state = 2;
				command = false;
			}
		} else if (state == 3) { // closing
			if ((sensor_closed == 1) && (sensor_open == 0)) {
				state = 0;
				command = false;
			}
		}

		if (command && ((state == 1) || (state == 3))) {
			time_t t = time(NULL) - command_time;
			if (t > CHECK_INTERVAL) {
				// command should have finished, check it
				fprintf(stderr, "Command %d did not finish within time.\n", state);
				state = 4;
				command = false;
			}
		}

	}
}

static void publish_state(struct mosquitto *mosq)
{
	char *msg = NULL;

	if (published_state == state)
		return;

	// craft msg
	if (asprintf(&msg, "%s", door_states[state]) < 0)
		exit(EXIT_FAILURE);

	// send it
	if (mosquitto_publish(mosq, NULL, topic_state, strlen(msg), msg, 0, true) != 0)
		exit(EXIT_FAILURE);

	free(msg);

	fprintf(stderr, "published state info: %d (%s)\n", state, door_states[state]);
	published_state = state;
}

static void usl(void *data __attribute__ ((unused)))
{
	usleep(25000);
}

static void message_callback(
		struct mosquitto *mosq,
		void *obj __attribute__ ((unused)),
		const struct mosquitto_message *message)
{
	if (message->payloadlen != 1) {
		fprintf(stderr, "Invalid payloadlen: %d\n", message->payloadlen);
		return;
	}

	if (((char *)message->payload)[0] == '0') {
		// close
		if ((state == 0) || (state == 3)) {
			// already closing or closed
			return;
		}

		fprintf(stderr, "Closing door\n");

		command = true;
		command_time = time(NULL);
		state = 3;
		publish_state(mosq);
		// perform the change
		gpiod_ctxless_set_value("3", 24, 1, true, GPIOD_CONSUMER, usl, NULL);
		gpiod_ctxless_set_value("3", 24, 0, true, GPIOD_CONSUMER, usl, NULL);
	} else if (((char *)message->payload)[0] == '1') {
		// open
		if ((state == 2) || (state == 1)) {
			// already opening or open
			return;
		}

		fprintf(stderr, "Opening door\n");

		command = true;
		command_time = time(NULL);
		state = 1;
		publish_state(mosq);
		// perform the change
		gpiod_ctxless_set_value("3", 18, 1, true, GPIOD_CONSUMER, usl, NULL);
		gpiod_ctxless_set_value("3", 18, 0, true, GPIOD_CONSUMER, usl, NULL);
	} else if (((char *)message->payload)[0] == 'q') {
		// cancel commands, reset errors, read state

		fprintf(stderr, "Cancelling command and error state\n");

		command = false;
		state = 5;
		publish_state(mosq);
	} else {
		fprintf(stderr, "Invalid command received: %c\n", ((char *)message->payload)[0]);
	}
}

int main(void)
{
	struct mosquitto *mosq = NULL;
	int ret;
	config_t cfg;
	const char *conf_server;
	int conf_port;

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

	// what to do if terminated
	signal(SIGINT, sigfunc);
	signal(SIGTERM, sigfunc);

	// use system hostname here
	char hostname[HOST_NAME_MAX+1];
	hostname[HOST_NAME_MAX] = 0;
	if (gethostname(hostname, HOST_NAME_MAX) != 0)
		exit(EXIT_FAILURE);

	// setup topics
	if (!asprintf(&topic_state, "/%s/door/state", hostname))
		exit(EXIT_FAILURE);
	if (!asprintf(&topic_control, "/%s/door/control", hostname))
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
	if (ret != 0)
		fprintf(stderr, "mosquitto_subscribe: %d: %s\n", ret, strerror(errno));

	fprintf(stderr, "connected, state topic = %s, control topic = %s\n",
		topic_state, topic_control);

	for (;;) {
		ret = mosquitto_loop(mosq, 15000, 1);
		if ((ret == MOSQ_ERR_CONN_LOST) || (ret == MOSQ_ERR_NO_CONN)) {
			sleep(30);
			fprintf(stderr, "Reconnecting to server\n");
			mosquitto_reconnect(mosq);
		} else if (ret != MOSQ_ERR_SUCCESS) {
			fprintf(stderr, "mosquitto_loop(): %d, %s\n", ret, strerror(errno));
			exit(EXIT_FAILURE);
		}
		
		get_state();
		publish_state(mosq);

		if (stop == 1)
			break;
	}

	mosquitto_disconnect(mosq);
	mosquitto_loop_stop(mosq, false);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	config_destroy(&cfg);
}

