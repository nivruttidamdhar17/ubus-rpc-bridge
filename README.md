# README.md

# ubus-RPC Bridge

Bidirectional bridge connecting OpenWrt's **ubus** IPC system with **JSON-RPC** over Unix domain sockets.

## Components

- **greet_ubus_provider**: ubus service (`greet.welcome`)
- **rpc_server**: JSON-RPC server on `/tmp/greet_rpc.sock`
- **ubus_rpc_bridge**: Bidirectional translator (ubus <-> RPC)

## Quick Start

### Prerequisites
```bash
sudo apt update
sudo apt install -y libjson-c-dev cmake build-essential socat

# Install libubox
git clone https://git.openwrt.org/project/libubox.git
cd libubox && cmake . && make && sudo make install && sudo ldconfig

# Install ubus
git clone https://git.openwrt.org/project/ubus.git
cd ubus && cmake . && make && sudo make install && sudo ldconfig
```

### Build
```bash
make
```

### Run (4 Terminals)
```bash
# Terminal 1
sudo ubusd

# Terminal 2
./greet_ubus_provider

# Terminal 3
./rpc_server

# Terminal 4
./ubus_rpc_bridge
```

### Test
```bash
# Direction B: ubus → RPC
ubus call rpc_greet welcome '{"name":"Test"}'
# Output: {"message": "Hello From RPC!"}

# Direction A: RPC → ubus
echo '{"id":1,"method":"greet.welcome","params":{"name":"User"}}' | \
    socat - UNIX-CONNECT:/tmp/bridge_rpc.sock
# Output: {"id":1,"result":{"message":"Hello User, Welcome to XYZ Company"},"error":null}
```

## Documentation

- [BUILD_AND_RUN.md](docs/BUILD_AND_RUN.md) 
- [DESIGN.md](docs/DESIGN.md) 

## Project Structure

```
.
├── docs
│   ├── BUILD_AND_RUN.html
│   ├── BUILD_AND_RUN.md
│   ├── DESIGN.html
│   └── DESIGN.md
├── include
│   ├── log.h
│   └── rpc_protocol.h
├── Makefile
├── README.md
├── src
    ├── greet_ubus_provider.c
    ├── rpc_client.c
    ├── rpc_server.c
    ├── ubus_helpers.c
    └── ubus_rpc_bridge.c
```

## Features

Bidirectional IPC (ubus <-> JSON-RPC)  
Unix domain socket communication  
Event-driven architecture (uloop)  
Protocol translation (blobmsg <-> JSON)  
Error handling with logging  

## Technologies

- **C** programming
- **OpenWrt ubus** - IPC bus system
- **libubox** - Event loop (uloop) & binary blob messages
- **json-c** - JSON parsing/serialization
- **Unix domain sockets** - Local IPC

