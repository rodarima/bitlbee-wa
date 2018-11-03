LIBS = bitlbee
TARGET_LIB = wa.so

CFLAGS = $(shell pkg-config --cflags $(LIBS)) -fPIC -Wall -Wextra -Werror
LIB_CFLAGS=$(CFLAGS) -shared
LDLIBS = $(shell pkg-config --libs $(LIBS)) -lwa

.PHONY: all
all: wa.so

wa.so: bee.o
	$(CC) $(LIB_CFLAGS) $^ -o $@ $(LDLIBS)

install: wa.so
	cp wa.so /usr/lib/bitlbee/

.PHONY: clean
clean:
	-rm -rf wa.so *.o
