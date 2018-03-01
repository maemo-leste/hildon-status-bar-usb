#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <gudev/gudev.h>

#include "udev-helper.h"


/*
 *
 * TODO:
 * - No hardcoded driver names (whitelist?) - see find_phy and
 *   find_otg_controller
 * - Implement uh_get_device_name (add to cache?)
 * - Rework callbacks to just expect a struct with usb data (cable connected,
 *   usb_mode, ...)
 * - Rework usb_plugin.c to use libusbgx [1]
 * - Test on device with proper otg detection (droid4, lime2) and add/implement
 *   USB_MODE_*
 * - Remove usleep (and unistd.h) and replace with g_bla_callback
 * - Figure out how to free `client`
 * - Run through valgrind
 *
 * [1] https://github.com/libusbgx/libusbgx
 */

#define N900_PHY_DRIVER "twl4030_usb"
#define N900_USB_DRIVER "musb-hdrc"

typedef struct {
    gboolean connected;
    gint usb_mode;
} PrivData;


static gpointer user_data = NULL;
static UhCallback user_callback = NULL;

static GUdevClient* client = NULL;
static GUdevDevice* phy = NULL;
static GUdevDevice* otg = NULL;

static PrivData cache = { .connected = 0,
                          .usb_mode = 0 };


static gint read_usb_mode(void) {
    const gchar* otg_sysfs_path = g_udev_device_get_sysfs_path(otg);
    gchar* path;
    gchar *strmode = NULL;
    int mode;

    mode = USB_MODE_UNKNOWN;

    path = g_strconcat(otg_sysfs_path, "/mode", NULL);
    gboolean ok = g_file_get_contents(path, &strmode, NULL, NULL);
    if (ok) {
        if (strcmp(strmode, "b_idle\n") == 0) {
            mode = USB_MODE_B_IDLE;
        } else if (strcmp(strmode, "b_peripheral\n") == 0) {
            mode = USB_MODE_B_PERIPHERAL;
        }

        free(strmode);
    }
    free(path);

    return mode;
}

static gint read_phy_vbus(void) {
    const gchar* phy_sysfs_path = g_udev_device_get_sysfs_path(phy);
    gchar* path;
    gchar *strmode = NULL;
    int vbus;

    vbus = 0;

    path = g_strconcat(phy_sysfs_path, "/vbus", NULL);
    gboolean ok = g_file_get_contents(path, &strmode, NULL, NULL);
    if (ok) {
        if (strcmp(strmode, "on\n") == 0) {
            vbus = 1;
        } else if (strcmp(strmode, "off\n") == 0) {
            vbus = 0;
        }

        free(strmode);
    }
    free(path);

    return vbus;
}

static GUdevDevice* find_device(const gchar* subsystem_match, const gchar* driver_match) {
    GList *l, *e;
    GUdevDevice *dev, *res = NULL;

    l = g_udev_client_query_by_subsystem(client, subsystem_match);
    for (e = l; e; e = e->next) {
        const gchar* driver;

        dev = (GUdevDevice*)e->data;
        driver = g_udev_device_get_driver(dev);

        if (driver && (strcmp(driver, driver_match) == 0)) {
            res = dev;
            goto DONE;
        }
    }

DONE:
    g_list_free(l);

    return res;
}

/* TODO: Improve with not just single hardcoded value */
static GUdevDevice *find_phy(void) {
    GUdevDevice *dev = find_device("platform", N900_PHY_DRIVER);
    return dev;
}

static GUdevDevice *find_otg_controller(void) {
    GUdevDevice *dev = find_device("platform", N900_USB_DRIVER);
    return dev;
}

static int setup_udev(void) {
    const gchar *subsystems[] = { NULL};
    client = g_udev_client_new(subsystems);
    if (!client)
        return 1;

    return 0;
}

static int find_devices(void) {
    phy = find_phy();
    if (!phy)
        return 1;

    otg = find_otg_controller();
    if (!otg)
        return 1;

    cache.connected = read_phy_vbus();
    cache.usb_mode = read_usb_mode();

    return 0;
}

static void on_uevent(GUdevClient *client, const char *action, GUdevDevice *device) {
    (void)client;

    const gchar* sysfs_path = g_udev_device_get_sysfs_path(device);
    const gchar* phy_sysfs_path;

    phy_sysfs_path = g_udev_device_get_sysfs_path(phy);

    if (strcmp(phy_sysfs_path, sysfs_path) == 0) {
        cache.connected = (strcmp(action, "online") == 0);

        /* FIXME: Schedule a callback to read usb mode and then call
         * user_callback (instead of usleep(2) */
        usleep(1000 * 100);

        cache.usb_mode = read_usb_mode();
        fprintf(stderr, "Connected: %d, usb_mode: %d\n", cache.connected, cache.usb_mode);


        if ((user_callback)) {
            user_callback(cache.connected, cache.usb_mode, user_data);
        }
    }
    return;
}

int uh_init() {
    int ret = 1;
    client = NULL;
    user_data = NULL;
    user_callback = NULL;

    ret = setup_udev();
    if (ret) {
        fprintf(stderr, "setup_udev failed\n");
        return ret;
    }
    ret = find_devices();
    if (ret) {
        fprintf(stderr, "find_devices failed\n");
        return ret;
    }

    g_signal_connect(client, "uevent", G_CALLBACK(on_uevent), NULL);

    return ret;
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

void uh_query_state(gboolean* cable_connected, gint* usb_mode) {
    *cable_connected = cache.connected;
    *usb_mode = cache.usb_mode;
}

/* TODO */
char *uh_get_device_name() {
    return NULL;
}

#if 0
/* TODO: g_udev_device_has_sysfs_attr can be used to see if something has attrs
 * like vbus and mode */

static void test_callback(gboolean cable_connected, gpointer data) {
    fprintf(stderr, "test_callback: %d - %p.\n", cable_connected, data);
}

static int main_loop(void) {
    static GMainLoop *loop = NULL;

    uh_init();
    uh_set_callback((UhCallback)test_callback, (void*)42);

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    uh_destroy();

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
