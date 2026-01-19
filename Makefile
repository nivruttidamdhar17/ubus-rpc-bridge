CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -Iinclude -Wno-unused-parameter
UBUS_INC = -I/usr/local/include -I/usr/include
UBUS_LIB = -L/usr/local/lib -L/usr/lib
LDFLAGS = $(UBUS_LIB) -lubus -lubox -lblobmsg_json -ljson-c

all: greet_ubus_provider rpc_server ubus_rpc_bridge # ubus_helpers

greet_ubus_provider: src/greet_ubus_provider.c
	$(CC) $(CFLAGS) $(UBUS_INC) -o $@ $^ $(LDFLAGS)

rpc_server: src/rpc_server.c
	$(CC) $(CFLAGS) -o $@ $^ -ljson-c

ubus_rpc_bridge: src/ubus_rpc_bridge.c
	$(CC) $(CFLAGS) $(UBUS_INC) -o $@ $^ $(LDFLAGS)

#ubus_helpers: src/ubus_helpers.c
#	$(CC) $(CFLAGS) $(UBUS_INC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f greet_ubus_provider rpc_server ubus_rpc_bridge

.PHONY: all clean
