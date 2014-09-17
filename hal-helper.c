#include <string.h>
#include <libhal.h>
#include <dbus/dbus.h>
#include <glib-object.h>

#include "hal-helper.h"

HhCallback callback;
gpointer userdata;
LibHalContext *ctx;
DBusConnection *dbus_connection;
const char *usb_cable_udi;

char *hh_get_device_name()
{
  return NULL;
}

void hh_destroy()
{
  callback = 0;
  userdata = 0;
  if (ctx) {
    libhal_device_remove_property_watch(ctx, usb_cable_udi, NULL);
    libhal_ctx_shutdown(ctx, NULL);
    libhal_ctx_free(ctx);
    dbus_connection_unref(dbus_connection);
  }
}

void hh_set_callback(HhCallback cb, gpointer data)
{
  userdata = data;
  callback = cb;
  if (hh_query_state())
    g_debug("usb-plugin[init]: current-state: usb-connected");
  else
    g_debug("usb-plugin[init]: current-state: usb-disconnected");
}

static void hh_property_modified_cb(LibHalContext *ctx, const char *udi,
                                    const char *key, dbus_bool_t is_removed,
                                    dbus_bool_t is_added)
{
  if (!strcmp("usb_device.mode", key)) {
    char *s = libhal_device_get_property_string(ctx, udi, "usb_device.mode",
                                                NULL);
    if (s)
    {
      if (g_strstr_len(s, 12, "peripheral")){
        g_debug("usb-plugin[hal-event]: usb peripheral connected");
        if (callback)
          callback(TRUE, userdata);
      } else {
        g_debug("usb-plugin[hal-event]: usb disconneced");
        if (callback)
          callback(FALSE, userdata);
      }
      libhal_free_string(s);
    }
  }
}

void hh_init()
{
  if (!ctx) {
    char **s;
    DBusError error;
    int num_devices;

    ctx = libhal_ctx_new();
    dbus_error_init(&error);

    dbus_connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    if (dbus_error_is_set(&error)) {
      g_critical("usb-plugin: dbus_bus_get(DBUS_BUS_SYSTEM) failed");
      return;
    }

    libhal_ctx_set_dbus_connection(ctx, dbus_connection);
    if (!libhal_ctx_init(ctx, NULL)) {
      g_critical("usb-plugin: libhal_ctx_init failed");
      return;
    }

    s = libhal_manager_find_device_string_match(ctx, "button.type", "usb.cable",
                                                &num_devices, NULL);
    if (!s || num_devices != 1) {
      g_warning("usb-plugin: couldn't find USB cable indicator, using default");
      usb_cable_udi =
          "/org/freedesktop/Hal/devices/usb_device_1d6b_2_musb_hdrc";
    } else
        usb_cable_udi = g_strdup(s[0]);

    if (s)
      libhal_free_string_array(s);

    if (libhal_device_add_property_watch(ctx, usb_cable_udi, &error)){
      if (!libhal_ctx_set_device_property_modified(ctx,
                                                   hh_property_modified_cb))
        g_critical("usb-plugin: hal_ctx_set_device_property_modified failed");
    }
    else
    {
      g_critical("usb-plugin: libhal_device_add_property_watch failed: %s",
                 error.message);
      dbus_error_free(&error);
    }
  }
}

/* returns TRUE if peripheral mode */
gboolean hh_query_state()
{
  char *s;
  gboolean rv = FALSE;

  hh_init();

  s = libhal_device_get_property_string(ctx, usb_cable_udi, "usb_device.mode", 0);
  if (s) {
    rv = !!g_strstr_len(s, 12, "peripheral");
    libhal_free_string(s);
  }
  return rv;
}
