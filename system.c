
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

static char *topic_control = NULL;
static char *topic_state = NULL;

static int performance_mode = 1; // 0 == powersave
static int power_on = 1; // 0 == off

// 15 minute intervals between normal idle publishes
#define PUBLISH_INTERVAL 900

void sigfunc(int s __attribute__ ((unused)))
{
	power_on = 0;
}

void publish_state(struct mosquitto *mosq)
{
	char *msg = NULL;
	int cores = 0;
	int n = 0;
	FILE *loadavg;
	float temp = 0.;
	float load1, load5, load15;

	/* CPU/system health */
	for (;;) {
		char *fname = NULL;
		FILE *f;
		int t;

		if (asprintf(&fname, "/sys/devices/platform/coretemp.0/hwmon/hwmon0/temp%i_input", n++) < 0)
			exit(EXIT_FAILURE);
		f = fopen(fname, "r");
		free(fname);
		if (!f) {
			if (cores == 0)
				continue;
			break;
		}
		if (fscanf(f, "%i", &t) != 1)
			exit(EXIT_FAILURE);
		fclose(f);
		temp += (float)t / 1000.;
		cores++;
	}
	temp = temp / (float)cores;

	// load?
	loadavg = fopen("/proc/loadavg", "r");
	if (!loadavg)
		exit(EXIT_FAILURE);
	if (fscanf(loadavg, "%f %f %f", &load1, &load5, &load15) != 3)
		exit(EXIT_FAILURE);
	fclose(loadavg);
	
	// craft msg
	if (asprintf(&msg, "{ "
			"\"cpu_temperature_average\":\"%.1f\","
			"\"load_1\":\"%.1f\","
			"\"load_5\":\"%.1f\","
			"\"load_15\":\"%.1f\","
			"\"performance_mode\":\"%d\","
			"\"power\":\"%d\""
		 	"}",
			temp,
			load1, load5, load15,
			performance_mode, power_on) < 0)
		exit(EXIT_FAILURE);

	// send it
	if (mosquitto_publish(mosq, NULL, topic_state, strlen(msg), msg, 0, true) != 0)
		exit(EXIT_FAILURE);

	free(msg);

	fprintf(stderr, "published state info\n");
}

static void message_callback(
		struct mosquitto *mosq,
		void *obj __attribute__ ((unused)),
		const struct mosquitto_message *message)
{
	char *tmp = NULL;

	if (!asprintf(&tmp, "%.*s", message->payloadlen, (char *)message->payload))
		exit(EXIT_FAILURE);
	
	if (strcmp(tmp, "poweroff") == 0) {
		fprintf(stderr, "Halting the system\n");
		power_on = 0;
		publish_state(mosq);
		// then do the actual poweroff
	} else if (strcmp(tmp, "powersave") == 0) {
		if (performance_mode == 1) {
			fprintf(stderr, "Switching to powersave mode \n");
			performance_mode = 0;
			publish_state(mosq);
		}
	} else if (strcmp(tmp, "performance") == 0) {
		if (performance_mode == 0) {
			fprintf(stderr, "Switching to performance mode\n");
			performance_mode = 1;
			publish_state(mosq);
		}
	}

	free(tmp);
}

int main(void)
{
	struct mosquitto *mosq = NULL;
	int ret;
	int interval = 0; // publish shortly after connecting to the server

	// what to do if terminated
	signal(SIGINT, sigfunc);
	signal(SIGTERM, sigfunc);

	// use system hostname here
	char hostname[HOST_NAME_MAX+1];
	hostname[HOST_NAME_MAX] = 0;
	if (gethostname(hostname, HOST_NAME_MAX) != 0)
		exit(EXIT_FAILURE);

	// setup topics
	if (!asprintf(&topic_state, "/%s/system/state", hostname))
		exit(EXIT_FAILURE);
	if (!asprintf(&topic_control, "/%s/system/control", hostname))
		exit(EXIT_FAILURE);

	/* setup mqtt */
	mosquitto_lib_init();
	mosq = mosquitto_new(NULL, true, NULL);
	if (!mosq)
		exit(EXIT_FAILURE);

	mosquitto_message_callback_set(mosq, message_callback);

	while (mosquitto_connect(mosq, "192.168.1.52", 1883, 15) != 0) {
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
		ret = mosquitto_loop(mosq, 15000, 1);
		if ((ret == MOSQ_ERR_CONN_LOST) || (ret == MOSQ_ERR_NO_CONN)) {
			sleep(30);
			fprintf(stderr, "Reconnecting to server\n");
			mosquitto_reconnect(mosq);
		} else if (ret != MOSQ_ERR_SUCCESS) {
			fprintf(stderr, "mosquitto_loop(): %d, %s\n", ret, strerror(errno));
			exit(EXIT_FAILURE);
		}
		
		if (interval <= 0) {
			publish_state(mosq);
			interval = PUBLISH_INTERVAL;
		}
		interval -= 15;

		if (power_on == 0) {
			publish_state(mosq);
			break;
		}
	}

	mosquitto_disconnect(mosq);
	mosquitto_loop_stop(mosq, false);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
}

