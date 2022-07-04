
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
	cc ${CFLAGS} ${LDFLAGS} gettally.c -o bin/gettally -L./deps/
