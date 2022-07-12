
CFLAGS=-DV8_COMPRESS_POINTERS -DV8_31BIT_SMIS_ON_64BIT_ARCH
CXXFLAGS=-std=c++20
LDFLAGS=-lv8 -lv8_libplatform -lv8_libbase -lc++ -lwebsockets

# On Mac, at least with Homebrew, the cmake command builds x86_64 binaries
# even on arm, so force our binaries to also use that architecture.  Ugh.
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
    CFLAGS+=-arch x86_64 -O0 -g -I/usr/local/Cellar/openssl@1.1/1.1.1o/include/
    CXXFLAGS+=-arch x86_64
    LDFLAGS+=-arch x86_64
endif

all: bin/gettally

install:
	cp bin/gettally /usr/local/bin/
	cp bin/libgettally.a /usr/local/lib/
	cp bin/libgettally.so /usr/local/lib/

clean:
	rm -rf bin

makebin:
	mkdir -p bin

bin/translatejstocstring: translatejstocstring.c
	make makebin;
	cc translatejstocstring.c -o bin/translatejstocstring

bin/websocket.h: websocket.js bin/translatejstocstring
	make makebin;
	cat websocket.js | bin/translatejstocstring websocket_js > bin/websocket.h

bin/obs-websocket.h: obs-websocket.js bin/translatejstocstring
	make makebin;
	cat obs-websocket.js | bin/translatejstocstring obs_websocket_js > bin/obs-websocket.h

bin/gettally.h: gettally.js bin/translatejstocstring
	make makebin;
	cat gettally.js | bin/translatejstocstring gettally_js > bin/gettally.h

bin/gettally.o: gettally.c bin/obs-websocket.h bin/gettally.h bin/websocket.h # bin/websocket_all_js.h # bin/nextTick.h bin/buffer.h
	make makebin;
	cc -c ${CFLAGS} gettally.c -o bin/gettally.o

bin/v8_setup.o: v8_setup.cpp
	make makebin;
	c++ -c ${CXXFLAGS} ${CFLAGS} v8_setup.cpp -o bin/v8_setup.o

bin/gettally: libraries main.c
	make makebin;
	cc main.c bin/libgettally.a -o bin/gettally ${LDFLAGS} 

libraries: bin/libgettally.a bin/libgettally.so

bin/libgettally.a: bin/gettally.o bin/v8_setup.o
	make makebin;
	ar rcs bin/libgettally.a bin/*.o

bin/libgettally.so: bin/gettally.o bin/v8_setup.o
	make makebin;
	gcc -shared bin/*.o -o bin/libgettally.so ${LDFLAGS}
