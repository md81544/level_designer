MAKEFLAGS += --silent

.PHONY: all debug clean release

all:
	./m.sh

debug:
	./m.sh debug

clean:
	./m.sh clean

release:
	./m.sh release
