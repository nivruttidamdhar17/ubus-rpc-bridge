

/*
 * TEST PURPOSE FILE this code also present in ubus_rpc_bridge.c
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <json-c/json.h>
#include "../include/rpc_protocol.h"

int rpc_call(const char *method, const char *name, int id, char **reply_str) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("rpc_call socket");
        return -1;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, RPC_SOCK_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("rpc_call connect");
        close(fd);
        return -1;
    }

    log_debug("Connected to RPC server at %s", RPC_SOCK_PATH);

    // Build JSON-RPC request
    json_object *req = json_object_new_object();
    json_object_object_add(req, "id", json_object_new_int(id));
    json_object_object_add(req, "method", json_object_new_string(method));
    json_object *params = json_object_new_object();
    json_object_object_add(params, "name", json_object_new_string(name));
    json_object_object_add(req, "params", params);

    const char *req_json = json_object_to_json_string(req);
    ssize_t n_written = write(fd, req_json, strlen(req_json));
    write(fd, "\n", 1);  // Line terminator as in rpc_server
    json_object_put(req);

    if (n_written < 0) {
        perror("rpc_call write");
        close(fd);
        return -1;
    }

    // Read response
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) {
        perror("rpc_call read");
        return -1;
    }
    buf[n] = '\0';

    *reply_str = strdup(buf);
    return 0;
}

#ifdef TEST_RPC_CLIENT
int main() {
    char *reply;
    if (rpc_call("greet.welcome", "Shripad", 42, &reply) == 0) {
        printf("RPC Reply: %s\n", reply);
        free(reply);
        return 0;
    }
    return 1;
}
#endif

