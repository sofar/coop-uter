
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

#include <modbus.h>

long int get_num(char *s)
{
	if ((strlen(s) > 2) && (s[0] == '0') && (s[1] == 'x')) {
		return strtol(s, NULL, 16);
	} else if ((strlen(s) > 1) && (s[0] == '0')) {
		return strtol(s, NULL, 8);
	} else {
		return strtol(s, NULL, 10);
	}
}

int main(int argc, char *argv[]) {
	modbus_t *ctx;

	// parse args
	if (argc != 3) {
		fprintf(stderr, "Usage: modbus_write <offset> <value>\n");
		exit(EXIT_FAILURE);
	}

	long int addr = get_num(argv[1]);
	long int val = get_num(argv[2]);

	fprintf(stderr, "Writing %li:%li\n", addr, val);

	// setup modbus
	ctx = modbus_new_rtu("/dev/ttyS1", 9600, 'N', 8, 1);
	if (!ctx) {
		perror("Unable to create the libmodbus context\n");
		exit(EXIT_FAILURE);
	}

	modbus_set_slave(ctx, 1);

	// do the actual write
	if (modbus_connect(ctx) == -1) {
		fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		exit(EXIT_FAILURE);
	}

	if (modbus_write_register(ctx, addr, val) < 0)
		fprintf(stderr, "Error writing register: %s\n", strerror(errno));

	modbus_close(ctx);
	modbus_free(ctx);
}
