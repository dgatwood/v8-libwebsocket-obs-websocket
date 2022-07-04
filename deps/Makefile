
all: libwebsockets build build/lib/libwebsockets.dylib

clean:
	rm -rf build

libwebsockets:
	git clone https://libwebsockets.org/repo/libwebsockets

build:
	mkdir -p build

build/lib/libwebsockets.dylib:
	make build;
	make libwebsockets;
	cd build && cmake ../libwebsockets -DOPENSSL_ROOT_DIR=$(shell ./get_ssl_dir.sh) && make && sudo make install

