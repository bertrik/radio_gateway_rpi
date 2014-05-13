VERSION=0.1

DEBUG=-g # -D_DEBUG=1

CXXFLAGS+=-Wall -Wextra -O3 -DVERSION=\"$(VERSION)\" $(DEBUG)
LDFLAGS+=$(DEBUG)

OBJSb=rgr_lib.o rgr_beacon.o error.o
OBJSl=rgr_lib.o rgr_listen.o error.o
OBJSk=rgr_lib.o rgr_kiss.o error.o

all: rgr_beacon rgr_listen rgr_kiss

rgr_beacon: $(OBJSb)
	$(CXX) -Wall -W $(OBJSb) $(LDFLAGS) -o rgr_beacon

rgr_listen: $(OBJSl)
	$(CXX) -Wall -W $(OBJSl) $(LDFLAGS) -o rgr_listen

rgr_kiss: $(OBJSk)
	$(CXX) -Wall -W $(OBJSk) $(LDFLAGS) -lutil -lax25 -o rgr_kiss

install: rgr_beacon rgr_listen rgr_kiss
	cp rgr_beacon rgr_listen rgr_kiss $(PREFIX)/bin

clean:
	rm -f $(OBJS) rgr_beacon rgr_listen rgr_kiss

package: clean
	mkdir radio_gateway_rpi-$(VERSION)
	cp *.c* Makefile readme.txt license.txt radio_gateway_rpi-$(VERSION)
	tar czf radio_gateway_rpi-$(VERSION).tgz radio_gateway_rpi-$(VERSION)
	rm -rf radio_gateway_rpi-$(VERSION)

check:
	cppcheck -v --enable=all --std=c++11 --inconclusive -I. . 2> err.txt
