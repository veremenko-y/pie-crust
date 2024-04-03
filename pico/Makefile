# Set one of these depending.
#export PICO_SDK_FETCH_FROM_GIT = yes
export PICO_SDK_PATH = /home/yaroslav/doclocal/pico-sdk/

.PHONY: all

all: build/piecrust.uf2

build/piecrust.uf2: build/Makefile
	$(MAKE) -C build -j$(shell nproc)

build/Makefile: CMakeLists.txt $(wildcard *.[chS]) $(wildcard *.cxx)
	mkdir -p build
	(cd build && cmake ..)

clean:
	make -C build clean
	rm -rf build