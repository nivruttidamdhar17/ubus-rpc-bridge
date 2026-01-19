# BUILD_AND_RUN.md

## Prerequisites Installation

Ref: https://zilogic.com/blog/running-ubus-in-ubuntu.html

### Setting Up Docker Environment on Ubuntu
```bash
sudo apt update
sudo apt install apt-transport-https ca-certificates curl software-properties-common
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt update
sudo apt install docker-ce
sudo usermod -aG docker ${USER}
su - ${USER}
```
### Download Docker image for ubuntu and run it
```bash
docker run -dit --name ubus  ubuntu:24.04 /bin/bash
```

### Start and Attach docker container
```bash
sudo docker start ubus
sudo docker attach ubus
```
---
---

### Install Dependencies inside container
```bash
apt update
apt install -y \
    libjson-c-dev \
    liblua5.1-0-dev \
    liblua5.1-dev \
    lua5.1 \
    git \
    cmake \
    build-essential \
    socat
```

### Fix Lua Headers Path
```bash
ln -sf /usr/include/lua5.1/* /usr/include/
```

## Install libubox
```bash
cd ~
git clone https://git.openwrt.org/project/libubox.git
cd libubox
cmake .
make
make install
ldconfig
```

## Install ubus
```bash
cd ~
git clone https://git.openwrt.org/project/ubus.git
cd ubus
cmake CMakeLists.txt
make
make install
ldconfig
```

## Build Project
```bash
cd /path/to/ubus-rpc-bridge-assignment
make clean
make
```
<br>
<br>
<br>

---
# Run (Manual - 5 Terminals)

### Terminal 1: Start ubusd
```bash
ubusd
```

### Terminal 2: Start greet_ubus_provider
```bash
cd /path/to/ubus-rpc-bridge-assignment
./greet_ubus_provider
```

### Terminal 3: Start rpc_server
```bash
cd /path/to/ubus-rpc-bridge-assignment
./rpc_server
```

### Terminal 4: Start ubus_rpc_bridge
```bash
cd /path/to/ubus-rpc-bridge-assignment
./ubus_rpc_bridge
```

### Terminal 5: Test
```bash
# Test Direction B (ubus -> RPC)
ubus call rpc_greet welcome '{"name":"Shripad"}'

# Expected Output:
# {
#     "message": "Hello From RPC!"
# }

# Test Direction A (RPC -> ubus)
echo '{"id":1,"method":"greet.welcome","params":{"name":"Shripad"}}' | \
    socat - UNIX-CONNECT:/tmp/bridge_rpc.sock

# Expected Output:
# {"id":1,"result":{"message":"Hello Shripad, Welcome to XYZ Company"},"error":null}

# List registered objects
ubus list

# Direct test greet provider
ubus call greet welcome '{"name":"Direct"}'
```

## Cleanup
```bash
# Stop all processes
pkill greet_ubus_provider
pkill rpc_server
pkill ubus_rpc_bridge
pkill ubusd

# Remove socket files
rm -f /tmp/greet_rpc.sock /tmp/bridge_rpc.sock
```

<br>

## Troubleshooting

### Library not found error
```bash
ldconfig
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### ubusd already running
```bash
pkill ubusd
ubusd
```

### Connection refused
Check if all 4 processes are running:
```bash
ps aux | grep -E "ubusd|greet_ubus_provider|rpc_server|ubus_rpc_bridge"
```