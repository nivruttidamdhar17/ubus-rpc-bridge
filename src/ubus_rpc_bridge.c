#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <json-c/json.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <libubox/uloop.h>
#include "log.h"

// SOCKET PATHS DEFINES
#define RPC_SOCK_PATH "/tmp/greet_rpc.sock"
#define BRIDGE_SOCK_PATH "/tmp/bridge_rpc.sock"

static struct ubus_context *ubus_ctx;
static int bridge_listener_fd = -1;

// ============== RPC CLIENT ==============
// it sends JSON-RPC request to rpc_server (Connect to /tmp/greet_rpc.sock, send JSON-RPC, receive response)
int rpc_call(const char *method, const char *name, int id, char **reply_str)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        log_error("rpc_call: socket() failed");
        return -1;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, RPC_SOCK_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        log_error("rpc_call: connect() failed - RPC server unreachable");
        close(fd);
        return -1;
    }

    log_debug("Connected to RPC server at %s", RPC_SOCK_PATH);

    // Build JSON Req to send : '{"id":1,"method":"greet.welcome","params":{"name":"Shripad"}}'

    // Build JSON-RPC request
    json_object *req = json_object_new_object();   // Create empty JSON object
    json_object_object_add(req, "id", json_object_new_int(id)); // Create JSON integer and Add key-value pair to JSON
    json_object_object_add(req, "method", json_object_new_string(method));  // // Create JSON string and Add key-value pair to JSON
    json_object *params = json_object_new_object();
    json_object_object_add(params, "name", json_object_new_string(name));
    json_object_object_add(req, "params", params);

    const char *req_json = json_object_to_json_string(req); // Serialize JSON to string
    ssize_t n_written = write(fd, req_json, strlen(req_json));  // Send data to socket
    write(fd, "\n", 1);

    log_debug("Sent RPC request: %s", req_json);
    json_object_put(req);   // Free JSON object (refcount--)

    if (n_written < 0)
    {
        log_error("rpc_call: write() failed");
        close(fd);
        return -1;
    }

    // Read response
    char buf[2048];
    ssize_t n = read(fd, buf, sizeof(buf) - 1); // Receive data from socket
    close(fd);

    if (n <= 0)
    {
        log_error("rpc_call: read() failed or empty response");
        return -1;
    }

    buf[n] = '\0';
    *reply_str = strdup(buf);   // Duplicate string (malloc)
    log_debug("Received RPC response (%zd bytes)", n);
    return 0;
}

// ============== DIRECTION B: ubus -> RPC ==============
enum {
    RPC_GREET_NAME,     // Index 0 for "name" parameter
    __RPC_GREET_MAX,    // Array size
};

// set string type policy
static const struct blobmsg_policy rpc_greet_policy[] =
{
    [RPC_GREET_NAME] = {.name = "name", .type = BLOBMSG_TYPE_STRING}
};

/*
 * Called when "ubus call rpc_greet welcome" is invoked
*/
static int rpc_greet_handler(struct ubus_context *ctx, struct ubus_object *obj,
                             struct ubus_request_data *req, const char *method,
                             struct blob_attr *msg)
{
    (void)obj;      // Suppress warning of unused
    (void)method;
    
    struct blob_attr *tb[__RPC_GREET_MAX];
    blobmsg_parse(rpc_greet_policy, __RPC_GREET_MAX, tb, blob_data(msg), blob_len(msg));    // Parse binary blob into array

    if (!tb[RPC_GREET_NAME])
    {
        log_warn("Direction B: Missing 'name' parameter in rpc_greet.welcome");
        return UBUS_STATUS_INVALID_ARGUMENT;
    }

    char *name = blobmsg_get_string(tb[RPC_GREET_NAME]);    // Extract string from blob
    log_info("Direction B: ubus call rpc_greet.welcome received, name='%s'", name);

    // Call RPC server via rpc_call()
    char *rpc_reply = NULL;
    if (rpc_call("greet.welcome", name, 1, &rpc_reply) < 0)     // rpc_call() : Send RPC request
    {
        log_error("Direction B: RPC server unreachable or call failed");
        return UBUS_STATUS_UNKNOWN_ERROR;
    }

    log_debug("Direction B: RPC reply JSON: %s", rpc_reply);

    // Parse RPC JSON response
    json_object *reply_obj = json_tokener_parse(rpc_reply);
    free(rpc_reply);
    
    if (!reply_obj)
    {
        log_error("Direction B: Failed to parse RPC reply JSON");
        return UBUS_STATUS_UNKNOWN_ERROR;
    }

    json_object *result_obj = NULL;
    json_object *message_obj = NULL;
    const char *message = NULL;
    
    // extract result.message
    if (json_object_object_get_ex(reply_obj, "result", &result_obj))
    {
        if (result_obj && !json_object_is_type(result_obj, json_type_null))
        {
            if (json_object_object_get_ex(result_obj, "message", &message_obj))
            {
                if (message_obj && json_object_get_type(message_obj) == json_type_string)
                {
                    message = json_object_get_string(message_obj);
                }
            }
        }
    }

    // Build ubus reply
    struct blob_buf b = {};
    blob_buf_init(&b, 0);   // Initialize blob buffer

    if (message)
    {
        blobmsg_add_string(&b, "message", message); // Add string to blob
        log_info("Direction B: Sending ubus reply: %s", message);
    } 
    else
    {
        blobmsg_add_string(&b, "message", "Error: Invalid RPC response");
        log_error("Direction B: RPC response missing result.message");
    }

    ubus_send_reply(ctx, req, b.head);  // Send ubus reply
    blob_buf_free(&b);
    json_object_put(reply_obj);

    return UBUS_STATUS_OK;
}

// ubus Object Registration Structures
static const struct ubus_method rpc_greet_methods[] = {
    UBUS_METHOD("welcome", rpc_greet_handler, rpc_greet_policy),
};

static struct ubus_object_type rpc_greet_type = {
    .name = "rpc_greet",
    .methods = rpc_greet_methods,
    .n_methods = 1,
};

static struct ubus_object rpc_greet_object = {  // Define ubus object metadata for registration
    .name = "rpc_greet",
    .type = &rpc_greet_type,
    .methods = rpc_greet_methods,
    .n_methods = 1,
};

// ============== DIRECTION A: RPC -> ubus ==============
static void handle_rpc_to_ubus_cb(struct ubus_request *ureq, int type, struct blob_attr *msg)
{
    (void)ureq; // Suppress warning of unused
    (void)type;
    
    if (!msg) {
        log_error("Direction A: No reply from ubus greet.welcome");
        return;
    }

    log_debug("Direction A: Received ubus callback");
}

static void handle_bridge_request(int fd)
{
    char buf[2048];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        log_warn("Direction A: Bridge read failed or client closed connection");
        close(fd);
        return;
    }

    buf[n] = '\0';
    log_info("Direction A: Bridge received RPC request (%zd bytes)", n);
    log_debug("Direction A: Request JSON: %s", buf);

    json_object *req = json_tokener_parse(buf);
    if (!req) {
        log_error("Direction A: Failed to parse RPC request JSON");
        close(fd);
        return;
    }

    json_object *id_obj = NULL, *method_obj = NULL, *params_obj = NULL, *name_obj = NULL;
    int id = 0;
    const char *method = NULL;

    json_object_object_get_ex(req, "id", &id_obj);
    json_object_object_get_ex(req, "method", &method_obj);
    json_object_object_get_ex(req, "params", &params_obj);

    if (id_obj && json_object_get_type(id_obj) == json_type_int) {
        id = json_object_get_int(id_obj);
    }

    if (method_obj && json_object_get_type(method_obj) == json_type_string) {
        method = json_object_get_string(method_obj);
    }

    if (!params_obj || !json_object_object_get_ex(params_obj, "name", &name_obj)) {
        log_error("Direction A: Missing 'name' parameter in RPC request");
        
        const char *error_reply = "{\"id\":0,\"result\":null,\"error\":{\"code\":400,\"message\":\"Missing name parameter\"}}";
        write(fd, error_reply, strlen(error_reply));
        write(fd, "\n", 1);
        close(fd);
        json_object_put(req);
        return;
    }

    const char *name = json_object_get_string(name_obj);
    log_info("Direction A: RPC->ubus forwarding method='%s' name='%s'", method, name);

    uint32_t greet_id;
    if (ubus_lookup_id(ubus_ctx, "greet", &greet_id) < 0) {
        log_error("Direction A: 'greet' object not found on ubus");
        
        char error_buf[256];
        snprintf(error_buf, sizeof(error_buf), 
            "{\"id\":%d,\"result\":null,\"error\":{\"code\":500,\"message\":\"ubus greet object not found\"}}\n", id);
        write(fd, error_buf, strlen(error_buf));
        close(fd);
        json_object_put(req);
        return;
    }

    log_debug("Direction A: Found 'greet' object, id=%d", greet_id);

    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "name", name);

    log_debug("Direction A: Invoking ubus greet.welcome");
    int ubus_ret = ubus_invoke(ubus_ctx, greet_id, "welcome", b.head, 
                               handle_rpc_to_ubus_cb, NULL, 3000);
    blob_buf_free(&b);

    if (ubus_ret < 0) {
        log_error("Direction A: ubus_invoke failed with code %d", ubus_ret);
        
        char error_buf[256];
        snprintf(error_buf, sizeof(error_buf), 
            "{\"id\":%d,\"result\":null,\"error\":{\"code\":500,\"message\":\"ubus invoke failed\"}}\n", id);
        write(fd, error_buf, strlen(error_buf));
        close(fd);
        json_object_put(req);
        return;
    }

    log_info("Direction A: ubus call succeeded");

    char greeting[512];
    snprintf(greeting, sizeof(greeting), 
        "{\"id\":%d,\"result\":{\"message\":\"Hello %s, Welcome to XYZ Company\"},\"error\":null}\n", 
        id, name);
    
    write(fd, greeting, strlen(greeting));
    log_info("Direction A: Sent RPC response");
    
    json_object_put(req);
    close(fd);
}

static void bridge_socket_cb(struct uloop_fd *u, unsigned int events)
{
    (void)u;
    
    if (events & ULOOP_READ) {
        int client_fd = accept(bridge_listener_fd, NULL, NULL);
        if (client_fd < 0) {
            log_error("Bridge listener: accept() failed");
            return;
        }
        log_debug("Bridge listener: Client connected (fd=%d)", client_fd);
        handle_bridge_request(client_fd);
    }
}

static struct uloop_fd bridge_fd_listener = {.cb = bridge_socket_cb};

int main(void)
{
    log_info("Starting ubus-rpc-bridge...");

    uloop_init();
    log_debug("Event loop initialized");

    // Connect to the UBUS demon
    ubus_ctx = ubus_connect(NULL);
    if (!ubus_ctx) {
        log_error("Failed to connect to ubus daemon");
        return 1;
    }
    log_info("Connected to ubus daemon");

    // Registers the ubus socket FD.
    ubus_add_uloop(ubus_ctx);
    log_debug("Registered ubus socket with event loop");

    // Register and add ubus object
    if (ubus_add_object(ubus_ctx, &rpc_greet_object) < 0)
    {
        log_error("Failed to register rpc_greet object on ubus");
        ubus_free(ubus_ctx);
        return 1;
    }
    log_info("Registered ubus object 'rpc_greet' with method 'welcome'");

    // Create socket
    bridge_listener_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (bridge_listener_fd < 0) {
        log_error("Failed to create bridge listener socket");
        ubus_free(ubus_ctx);
        return 1;
    }

    // Bind to path
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, BRIDGE_SOCK_PATH, sizeof(addr.sun_path) - 1);

    unlink(BRIDGE_SOCK_PATH);
    if (bind(bridge_listener_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_error("Failed to bind bridge socket to %s", BRIDGE_SOCK_PATH);
        close(bridge_listener_fd);
        ubus_free(ubus_ctx);
        return 1;
    }

    // listen for connections on a socket
    if (listen(bridge_listener_fd, 5) < 0)
    {
        log_error("Failed to listen on bridge socket");
        close(bridge_listener_fd);
        ubus_free(ubus_ctx);
        return 1;
    }
    log_info("Bridge listener socket created at %s", BRIDGE_SOCK_PATH);

    bridge_fd_listener.fd = bridge_listener_fd;
    uloop_fd_add(&bridge_fd_listener, ULOOP_READ);
    log_debug("Bridge socket registered with event loop");

    log_info("ubus-rpc-bridge started successfully (PID=%d)", getpid());
    log_info("Ready to handle:");
    log_info("  - Direction B: ubus calls to rpc_greet.welcome");
    log_info("  - Direction A: RPC requests to %s", BRIDGE_SOCK_PATH);

    uloop_run();

    log_info("Shutting down bridge...");
    ubus_free(ubus_ctx);
    uloop_done();
    close(bridge_listener_fd);
    unlink(BRIDGE_SOCK_PATH);

    return 0;
}
