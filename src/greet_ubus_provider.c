#include <stdio.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <libubox/uloop.h>
#include "log.h"

static struct ubus_context *context;

enum {
    WELCOME_NAME,
    __WELCOME_MAX,
};

// Handler declaration
static int welcome_handler(struct ubus_context *ctx, struct ubus_object *obj,
                          struct ubus_request_data *req, const char *method,
                          struct blob_attr *msg);

static const struct blobmsg_policy welcome_policy[] = {
    [0] = {.name = "name", .type = BLOBMSG_TYPE_STRING}
};

// method array definition
static const struct ubus_method greet_methods[] = {
    UBUS_METHOD("welcome", welcome_handler, welcome_policy),
};

// Add type of greet
static struct ubus_object_type greet_type = {
    .name = "greet",
    .methods = greet_methods,
    .n_methods = 1,
};

// Instance of ubus_object
static struct ubus_object greet_object = {
    .name = "greet",
    .type = &greet_type,
    .methods = greet_methods,
    .n_methods = 1,
};

static int welcome_handler(struct ubus_context *ctx, struct ubus_object *obj,
                          struct ubus_request_data *req, const char *method,
                          struct blob_attr *msg)
{
    (void)obj;
    (void)method;
    
    struct blob_attr *tb[__WELCOME_MAX];

    // parse binary blobmsg
    blobmsg_parse(welcome_policy, __WELCOME_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[WELCOME_NAME])
    {
        log_warn("Missing or invalid 'name' parameter");
        return UBUS_STATUS_INVALID_ARGUMENT;
    }

    // Extract string value
    char *name = blobmsg_get_string(tb[WELCOME_NAME]);
    log_debug("Received welcome request for name='%s'", name);

    // Reply
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    char msg_buf[256];

    // Build response
    snprintf(msg_buf, sizeof(msg_buf), "Hello %s, Welcome to XYZ Company", name);

    // Create binary blobmsg reply
    blobmsg_add_string(&b, "message", msg_buf);

    // Send reply to ubusd
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);

    log_debug("Sent reply: %s", msg_buf);
    return UBUS_STATUS_OK;
}

int main(void)
{
    log_info("Starting greet-ubus-provider...");
    
    uloop_init();
    log_debug("Event loop initialized");
    
    // Connect to the UBUS daemon
    context = ubus_connect(NULL);
    
    if (!context)
    {
        log_error("Failed to connect to ubus daemon");
        return 1;
    }
    log_info("Connected to ubus daemon");

    // Registers the ubus socket FD
    ubus_add_uloop(context);
    log_debug("Registered ubus socket with event loop");

    // Register and add ubus object
    if (ubus_add_object(context, &greet_object) != 0)
    {
        log_error("Failed to register greet object");
        ubus_free(context);
        return 1;
    }
    log_info("Registered ubus object 'greet' with method 'welcome'");

    log_info("Starting main loop (PID=%d)", getpid());

    uloop_run();
    
    log_info("Shutting down...");
    ubus_free(context);
    uloop_done();

    return 0;   
}
