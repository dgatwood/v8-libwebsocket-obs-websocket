
CFLAGS=-I$(shell ./find_ssl_include_dir.sh)

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

bin/gettally: gettally.c
	make makebin;
	cc ${CFLAGS} ${LDFLAGS} gettally.c -o bin/gettally -lwebsockets
