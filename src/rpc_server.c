#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>
#include <json-c/json.h>
#include "log.h"

#define SOCKET_PATH "/tmp/greet_rpc.sock"

int main()
{
    int server_fd;
    int client_fd;
    struct sockaddr_un addr = {0};

    log_info("Starting RPC server...");

    // Create socket
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        log_error("socket() failed: %s", strerror(errno));
        return 1;
    }
    log_debug("Socket created (fd=%d)", server_fd);

    // Bind to path
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    unlink(SOCKET_PATH);
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        log_error("bind() failed: %s", strerror(errno));
        close(server_fd);
        return 1;
    }
    log_debug("Socket bound to %s", SOCKET_PATH);

    // listen for connections on a socket
    if (listen(server_fd, 5) < 0)
    {
        log_error("listen() failed: %s", strerror(errno));
        close(server_fd);
        return 1;
    }

    log_info("RPC server listening on %s (one request per connection)", SOCKET_PATH);

    // Loop
    while (1)
    {
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0)
        {
            log_error("accept() failed: %s", strerror(errno));
            continue;
        }

        log_debug("Client connected (fd=%d)", client_fd);
        
        char buf[2048];
        ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
        
        if (n <= 0)
        {
            log_warn("Client closed without sending data");
            close(client_fd);
            continue;
        }

        buf[n] = '\0';
        log_debug("Received %zd bytes from client", n);

        // JSON Parsing 
        json_object *root = json_tokener_parse(buf);
        if (!root)
        {
            log_error("Invalid JSON received");
            close(client_fd);
            continue;
        }

        // Extract fields 
        json_object *id_obj = NULL;
        json_object *method_obj = NULL;
        json_object *params_obj = NULL;

        json_object_object_get_ex(root, "id", &id_obj);
        json_object_object_get_ex(root, "method", &method_obj);
        json_object_object_get_ex(root, "params", &params_obj);

        int id = 0;
        const char *method = NULL;
        json_object *name_obj = NULL;

        // Validates RPC structure
        if (id_obj && json_object_get_type(id_obj) == json_type_int)
        {
            id = json_object_get_int(id_obj);
        }
            
        if (method_obj && json_object_get_type(method_obj) == json_type_string)
        {
            method = json_object_get_string(method_obj);
        }

        if (params_obj && json_object_object_get_ex(params_obj, "name", &name_obj))
        {
            const char *name = json_object_get_string(name_obj);
            log_info("RPC request: id=%d method='%s' name='%s'", id, method, name);

            // Build reply
            json_object *reply = json_object_new_object();
            json_object_object_add(reply, "id", json_object_new_int(id));
            json_object *result = json_object_new_object();
            json_object_object_add(result, "message", 
                json_object_new_string("Hello From RPC!"));
            json_object_object_add(reply, "result", result);
            json_object_object_add(reply, "error", NULL);

            // Serializes back to string 
            const char *reply_str = json_object_to_json_string(reply);
            write(client_fd, reply_str, strlen(reply_str));
            write(client_fd, "\n", 1);

            log_debug("Sent RPC response: %s", reply_str);
            json_object_put(reply);
        }
        else
        {
            log_error("Invalid RPC request format - missing 'name' parameter");
            
            // Send error response
            char error_buf[256];
            snprintf(error_buf, sizeof(error_buf),
                "{\"id\":%d,\"result\":null,\"error\":{\"code\":400,\"message\":\"Invalid request format\"}}\n",
                id);
            write(client_fd, error_buf, strlen(error_buf));
        }

        json_object_put(root);
        close(client_fd);
    }

    log_info("Shutting down RPC server...");
    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}
