PROJECT=level_designer
SOURCES=$(shell ls *cpp)
INCPATHS=
LIBPATHS=
LDFLAGS=-lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio -lboost_program_options
#CFLAGS=-std=c++14 -c -Wall -Wextra -Werror -Wpedantic
CFLAGS=-std=c++20 -c
CC=clang++

# Automatic generation of some important lists
OBJECTS=$(SOURCES:.cpp=.o)
	INCFLAGS=$(foreach TMP,$(INCPATHS),-I$(TMP))
	LIBFLAGS=$(foreach TMP,$(LIBPATHS),-L$(TMP))

	BINARY=$(PROJECT)

all: CFLAGS += -O3
all: $(SOURCES) $(BINARY)

debug: CFLAGS += -O0 -g -D_DEBUG
debug: $(BINARY)

$(BINARY): $(OBJECTS)
	$(CC) $(LIBFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

.cpp.o:
	$(CC) $(INCFLAGS) $(CFLAGS) $< -o $@

distclean: clean
	rm -f $(BINARY)

clean:
	rm -f $(OBJECTS)

