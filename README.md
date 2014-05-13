Using an RFM69HW (as a modem) with a Raspberry Pi.

For this I used a simple Arduino in a TNC-like setup.

Arduino:
The code from Arduino/radio_bridge.ino must be put in an Arduino.
I tested it with a Arduini Mini Pro, 3.3V, 8MHz, AtMega328.

Arduino                 RFM
- digital I/O pin 2     DI0 on the RFM
- 3,3V                  3,3V
- GND                   GND
- digital I/O pin 10    NSS
- digital I/O pin 11    MOSI
- digital I/O pin 12    MISO
- digital I/O pin 13    SCK

Then connect the Arduino to the RPI:
Arduino                 RPI
- 3,3V                  3,3V [1]
- GND                   GND  [6]
- TX                    RX   [10]
- RX                    TX   [8]

The code from RPI/ can be run on the raspberry pi.
Run "make" to build things.

	rgr_beacon /dev/ttyAMA0 "Hello, this is dog." 5

This will send a beacon (just the text given) every 5 seconds.
Any received messages will be shown. If you don't want that,
set "verbose = false" in the top of main().

	rgr_listen /dev/ttyAMA0

This will only listen and show what is received.

If any program gives an error code, then this is what they mean:

210	unspecified
211	time out
212	escape out of range
213	unexpected start byte
214	end without start
215	buffer overflow

They're all related to communication between the Arduino and
the RPI.
If you get them a lot, then check the cabling between the
arduino and the rpi.


Any feedback can go to: mail@vanheusden.com
