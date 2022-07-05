
CFLAGS=-I$(shell ./find_ssl_include_dir.sh)
CXXFLAGS=-std=c++14

# On Mac, at least with Homebrew, the cmake command builds x86_64 binaries
# even on arm, so force our binaries to also use that architecture.  Ugh.
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
    CFLAGS+=-arch x86_64
endif

all: alldeps bin/gettally

alldeps:
	cd deps && make all

allclean:
	make clean && cd deps && make clean

clean:
	rm -rf bin

makebin:
	mkdir -p bin

bin/gettally.o: gettally.c
	cc ${CFLAGS} gettally.c -o bin/gettally.o

bin/v8_setup.o: v8_setup.cpp
	c++ ${CXXFLAGS} ${CFLAGS} v8_setup.cpp -o bin/v8_setup.o

bin/gettally: bin/gettally.o bin/v8_setup.o
	make makebin;
	cc ${LDFLAGS} bin/*.o -o bin/gettally -lwebsockets
