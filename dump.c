
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <modbus.h>


int main(void) {
	modbus_t *ctx;
	int ret;

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

	/* identify the charge controller */
	uint16_t regs[64];
	memset(regs, 0, sizeof(regs));
	ret = modbus_read_registers(ctx, 0x0c, 0x08, regs);
	if (ret < 0) {
		fprintf(stderr, "Failed to read registers: %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		exit(EXIT_FAILURE);
	}

	fprintf(stderr, "Model: \"%s\"\n", (char *)regs);

	/* software/hardware version */
	memset(regs, 0, sizeof(regs));
	ret = modbus_read_registers(ctx, 0x14, 0x02, regs);
	if (ret < 0) {
		fprintf(stderr, "Failed to read registers: %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		exit(EXIT_FAILURE);
	}

	fprintf(stderr, "Software version: V%02d.%02d.%02d\n",
		MODBUS_GET_LOW_BYTE(regs[0]),
		MODBUS_GET_HIGH_BYTE(regs[1]),
		MODBUS_GET_LOW_BYTE(regs[1]));
	fprintf(stderr, "Hardware version: V%02d.%02d.%02d\n",
		MODBUS_GET_LOW_BYTE(regs[2]),
		MODBUS_GET_HIGH_BYTE(regs[3]),
		MODBUS_GET_LOW_BYTE(regs[3]));

	/* serial nr */
	ret = modbus_read_registers(ctx, 0x18, 0x02, regs);
	if (ret < 0) {
		fprintf(stderr, "Failed to read registers: %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		exit(EXIT_FAILURE);
	}

	fprintf(stderr, "Serial number: %08x\n", regs[0] * 65536 + regs[1]);

	/* various levels */
	memset(regs, 0, sizeof(regs));
	ret = modbus_read_registers(ctx, 0x100, 0x22, regs);
	if (ret < 0) {
		fprintf(stderr, "Failed to read registers: %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i <= 0x22; i++) {
		fprintf(stderr, "Reg %04x: %04x\n", i + 0x100, regs[i]);
	}

	modbus_close(ctx);
	modbus_free(ctx);
}

