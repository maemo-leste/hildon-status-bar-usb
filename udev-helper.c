#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <gudev/gudev.h>

#include "udev-helper.h"


/*
 * /sys/class/phy/phy-48070000.i2c\:twl\@48\:twl4030-usb.0
 * /sys/class/udc/musb-hdrc.0.auto
 *
 * /sys/class/phy/phy-48070000.i2c\:twl\@48\:twl4030-usb.0/device/vbus
 * /sys/devices/platform/68000000.ocp/480ab000.usb_otg_hs/musb-hdrc.0.auto/vbus
 * /sys/devices/platform/68000000.ocp/480ab000.usb_otg_hs/musb-hdrc.0.auto/mode
 */

typedef struct {
    gboolean connected;

} PrivData;

/* TODO: Better/non-conflicting(?) names? */
static gpointer user_data = NULL;
static UhCallback user_callback;

static GUdevClient* client = NULL;
static PrivData cache = { .connected = 0 };

static void on_uevent(GUdevClient *client, const char *action, GUdevDevice *device) {
    (void)client;

    const gchar* subsys = g_udev_device_get_subsystem(device);
    const gchar* devtype = g_udev_device_get_devtype(device);
    const gchar* name = g_udev_device_get_name(device);

    fprintf(stderr, "subsys: %s; devtype: %s; name: %s; action: %s\n",
            subsys, devtype, name, action);

    if (strcmp(name, "48070000.i2c:twl@48:twl4030-usb") == 0) {
        cache.connected = (strcmp(action, "online") == 0);

        if (user_callback) {
            user_callback(cache.connected, user_data);
        }
        fprintf(stderr, "Match\n");
    }
    return;
}

int setup_udev(void) {
    const gchar *subsystems[] = { NULL};
    client = g_udev_client_new(subsystems);
    if (!client)
        return 1;

    g_signal_connect(client, "uevent", G_CALLBACK(on_uevent), NULL);

    return 0;
}

int uh_init() {
    client = NULL;
    user_data = NULL;
    user_callback = NULL;

    return setup_udev();
}

int uh_destroy() {
    if (client) {
        /* TODO: Figure out how to free - just unref ? */
    }

    return 0;
}

void uh_set_callback(UhCallback cb, gpointer data) {
    user_data = data;
    user_callback = cb;

    return;
}

gboolean uh_query_state() {
    return cache.connected;
}

/* TODO */
char *uh_get_device_name() {
    return NULL;
}

#if 0
static void test_callback(gboolean cable_connected, gpointer data) {
    fprintf(stderr, "test_callback: %d - %p.\n", cable_connected, data);
}

static int main_loop(void) {
    static GMainLoop *loop = NULL;

    setup_udev();
    uh_set_callback((UhCallback)test_callback, (void*)42);

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    return 0;
}

int
main (int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    return main_loop();
}
#endif
