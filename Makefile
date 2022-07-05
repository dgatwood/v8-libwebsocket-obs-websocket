
CXXFLAGS=-std=c++14
LDFLAGS=-lv8 -lv8_libplatform -lv8_libbase -lc++

# On Mac, at least with Homebrew, the cmake command builds x86_64 binaries
# even on arm, so force our binaries to also use that architecture.  Ugh.
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
    CFLAGS+=-arch x86_64
    CXXFLAGS+=-arch x86_64
    LDFLAGS+=-arch x86_64
endif

all: bin/gettally

clean:
	rm -rf bin

makebin:
	mkdir -p bin

bin/gettally.o: gettally.c
	make makebin;
	cc -c ${CFLAGS} gettally.c -o bin/gettally.o

bin/v8_setup.o: v8_setup.cpp
	make makebin;
	c++ -c ${CXXFLAGS} ${CFLAGS} v8_setup.cpp -o bin/v8_setup.o

bin/gettally: bin/gettally.o bin/v8_setup.o
	make makebin;
	cc bin/*.o -o bin/gettally ${LDFLAGS} 
