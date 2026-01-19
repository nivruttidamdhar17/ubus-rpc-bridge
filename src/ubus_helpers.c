
/*
 * TEST PURPOSE code
*/

#include <stdio.h>
#include <string.h>
#include <libubox/uloop.h>
#include <libubox/blobmsg_json.h>
#include <libubus.h>
#include "log.h"

static struct ubus_context *ctx;

// Callback when greet.welcome replies
static void greet_reply_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
    (void)req;
    (void)type;
    
    if (!msg) {
        log_error("No reply received from greet.welcome");
        return;
    }
    
    log_info("Received reply from greet.welcome");
    char *json_str = blobmsg_format_json(msg, true);
    printf("%s\n", json_str);
    free(json_str);
}

int main(void)
{
    uint32_t greet_id;
    struct blob_buf b;

    log_info("Starting ubus helper test client...");

    // 1. Init event loop
    uloop_init();
    log_debug("Event loop initialized");

    // 2. Connect to ubus
    ctx = ubus_connect(NULL);
    if (!ctx) {
        log_error("Failed to connect to ubus daemon");
        return 1;
    }
    log_info("Connected to ubus daemon");

    // 3. Register uloop with ubus
    ubus_add_uloop(ctx);
    log_debug("Registered ubus socket with event loop");

    // 4. Lookup "greet" object ID
    if (ubus_lookup_id(ctx, "greet", &greet_id)) {
        log_error("'greet' object not found on ubus");
        ubus_free(ctx);
        return 1;
    }
    log_info("Found 'greet' object, id=%u", greet_id);

    // 5. Build request JSON: {"name":"Nivrutti"}
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "name", "Nivrutti");

    // 6. Invoke greet.welcome with callback
    log_debug("Invoking greet.welcome with name='Nivrutti'");
    ubus_invoke(ctx, greet_id, "welcome", b.head, greet_reply_cb, NULL, 3000);

    // 7. Run event loop for 1 second (process callback)
    uloop_timeout_set(&(struct uloop_timeout){
        .cb = NULL
    }, 1000);

    uloop_run();

    log_debug("Cleaning up...");
    blob_buf_free(&b);
    ubus_free(ctx);
    uloop_done();
    
    log_info("Test complete");
    return 0;
}
