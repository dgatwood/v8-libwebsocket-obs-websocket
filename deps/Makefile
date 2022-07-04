
OPENSSL_ROOT_DIR=/usr/local/opt/openssl

all: libwebsockets build build/lib/libwebsockets.dylib

libwebsockets:
	git clone https://libwebsockets.org/repo/libwebsockets

build:
	mkdir -p build

build/lib/libwebsockets.dylib:
	make build;
	make libwebsockets;
	cd build && cmake ../libwebsockets -DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR} && make

