
all: alldeps bin/gettally

alldeps:
	cd deps && make all

makebin:
	mkdir -p bin

bin/gettally: gettally.c
	make makebin;
	cc ${CFLAGS} ${LDFLAGS} gettally.c -o bin/gettally
