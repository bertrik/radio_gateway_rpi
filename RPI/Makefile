VERSION=0.1

DEBUG=-g # -D_DEBUG=1

CXXFLAGS+=-Wall -O3 -DVERSION=\"$(VERSION)\" $(DEBUG)
LDFLAGS+=$(DEBUG)

OBJSb=rgr_lib.o rgr_beacon.o error.o
OBJSl=rgr_lib.o rgr_listen.o error.o

all: rgr_beacon rgr_listen

rgr_beacon: $(OBJSb)
	$(CXX) -Wall -W $(OBJSb) $(LDFLAGS) -o rgr_beacon

rgr_listen: $(OBJSl)
	$(CXX) -Wall -W $(OBJSl) $(LDFLAGS) -o rgr_listen

install: rgr_beacon rgr_listen
	cp rgr_beacon $ rgr_listen(PREFIX)/bin

clean:
	rm -f $(OBJS) rgr_beacon rgr_listen

package: clean
	mkdir radio_gateway_rpi-$(VERSION)
	cp *.c* Makefile readme.txt license.txt radio_gateway_rpi-$(VERSION)
	tar czf radio_gateway_rpi-$(VERSION).tgz radio_gateway_rpi-$(VERSION)
	rm -rf radio_gateway_rpi-$(VERSION)

check:
	cppcheck -v --enable=all --std=c++11 --inconclusive -I. . 2> err.txt
