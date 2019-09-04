
## libmodbus+MQTT stuff for my solar chicken coop project

This is all licensed MIT and is considered "example" code at best. It
contains hardcoded configuration values that shouldn't be there,
does not do TLS or proper authentication and runs as root with
too many privileges. You should absolutely not deploy this on any
networked system.

DISCLAIMER: This was made as a hobby project in my personal capacity
and not as part of the clearlinux project as part of my employment
at Intel. Intel does not endorse Renogy or DLI, the makers of the
Atomic PI, etc. I paid for all the hardware and wrote this software
in my free time.


## Dependencies

- libmosquitto
- libgpiod
- libmodbus
- libconfig


## What's in it?

- renogy-pub - a program that regularly reads out the libmodbus data
from Renogy solar charge controllers with a serial port. The data is
interpreted and relayed in curated form to the MQTT server.

- renogy-sys - a program that regularly publishes OS/system state info
to an MQTT server and can manipulate basic system state - power off,
powersave/performance modes. These are based on the fact that I might
want to shut down the system or suspend it with some script to make
it use less power overnight when there is no powersource.

- renogy-dump - a program to read out the libmodbus solar charge
controller data and dump it to standard out. Mostly for debugging and
inspection purposes. With it I found out that the Renogy spec has 2
values reversed.

- renogy-door - a program to drive a shutter (door) using 2 GPIO's
and track it's state through 2 more GPIO's connected to door
sensors.


## Config

- `/etc/mqtt.conf` - create this to signal the program what MQTT
server to connect to. There is no default, `server` and `port`
are required values. Example:

```
server = "localhost";
port = 1883;
```


## License

```
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
```

