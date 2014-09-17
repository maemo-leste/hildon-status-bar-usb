#include <stdlib.h>
#include <libhildondesktop/libhildondesktop.h>
#include <hildon/hildon-button.h>
#include <hildon/hildon-helper.h>

#include "hal-helper.h"

#define USB_TYPE_STATUS_MENU_ITEM (usb_status_menu_item_get_type ())

#define USB_STATUS_MENU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
            USB_TYPE_STATUS_MENU_ITEM, UsbStatusMenuItem))

#define USB_STATUS_MENU_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            USB_TYPE_STATUS_MENU_ITEM, UsbStatusMenuItemClass))

#define USB_IS_STATUS_MENU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            USB_TYPE_STATUS_MENU_ITEM))

#define USB_IS_STATUS_MENU_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
            USB_TYPE_STATUS_MENU_ITEM))

#define USB_STATUS_MENU_ITEM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
            USB_TYPE_STATUS_MENU_ITEM, UsbStatusMenuItemClass))

#define USB_STATUS_MENU_ITEM_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
            USB_TYPE_STATUS_MENU_ITEM, UsbStatusMenuItemPrivate))

#if 1
#undef g_debug
#define g_debug g_warning
#endif

typedef struct _UsbStatusMenuItem UsbStatusMenuItem;
typedef struct _UsbStatusMenuItemClass UsbStatusMenuItemClass;
typedef struct _UsbStatusMenuItemPrivate UsbStatusMenuItemPrivate;
typedef struct _UsbStatusMenuItemCbData UsbStatusMenuItemCbData;
struct _UsbStatusMenuItem {
  HDStatusMenuItem parent;
  UsbStatusMenuItemPrivate *priv;
};

struct _UsbStatusMenuItemClass {
  HDStatusMenuItemClass parent_class;
};

struct _UsbStatusMenuItemPrivate {
  GtkWidget *dialog;
  GtkWidget *label;
  GObject *status_menu_button;
  GtkWidget *hbox;
  GtkWidget *button_usb_image;
  GtkWidget *box_usb_image;
  GdkPixbuf *status_area_icon;
  GdkPixbuf *status_menu_icon;
  int current_mode;
  DBusPendingCall *pending;
  int expected_replies;
  int tries_count;
  guint enable_usb_mode_timeout;
  int ke_recv_alive;
  int has_hal_reply;
};

struct _UsbStatusMenuItemCbData {
  UsbStatusMenuItem *plugin;
  int mode;
};

HD_DEFINE_PLUGIN_MODULE (UsbStatusMenuItem, usb_status_menu_item, HD_TYPE_STATUS_MENU_ITEM);

static const char *usb_modes[] = {
  "NO_CONNECTION",
  "CHARGING_ONLY",
  "MASS_STORAGE",
  "PC_SUITE"
};

static void usb_status_menu_show_dialog(UsbStatusMenuItem *plugin, int mode);
static guint usb_status_menu_enable_usb_mode_cb(UsbStatusMenuItemCbData *data);
static void usb_status_menu_create_dialog(UsbStatusMenuItem *plugin);
static void usb_status_menu_disable_usb_button(UsbStatusMenuItem *plugin);

static void usb_status_menu_item_class_finalize(UsbStatusMenuItemClass *klass)
{
}

static void usb_status_menu_show(UsbStatusMenuItem *plugin,
                                 gboolean show_in_menu,
                                 gboolean show_in_status_area)
{
  UsbStatusMenuItemPrivate *priv = plugin->priv;

  if (show_in_status_area) {
    if (!priv->status_area_icon)
      priv->status_area_icon =
          gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                                   "statusarea_usb_active",
                                   18, GTK_ICON_LOOKUP_NO_SVG,
                                   0);

    hd_status_plugin_item_set_status_area_icon(HD_STATUS_PLUGIN_ITEM(plugin),
                                               priv->status_area_icon);
  } else
    hd_status_plugin_item_set_status_area_icon(HD_STATUS_PLUGIN_ITEM(plugin),
                                               NULL);

  if (show_in_menu && !priv->status_menu_icon) {
    priv->status_menu_icon =
        gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                                 "statusarea_usb_active",
                                 48,
                                 GTK_ICON_LOOKUP_NO_SVG,
                                 0);
    gtk_image_set_from_pixbuf(GTK_IMAGE(priv->button_usb_image),
                              priv->status_menu_icon);
    gtk_image_set_from_pixbuf(GTK_IMAGE(priv->box_usb_image),
                              priv->status_menu_icon);
  }

  g_object_set(G_OBJECT(plugin), "visible", show_in_menu, NULL);
}

static void usb_status_menu_item_finalize(GObject *object)
{
  UsbStatusMenuItem *plugin = USB_STATUS_MENU_ITEM(object);
  UsbStatusMenuItemPrivate *priv;

  priv = plugin->priv;
  hh_destroy();
  usb_status_menu_show(plugin, FALSE, FALSE);

  if (priv) {
    g_object_unref(priv->hbox);
    g_object_unref(priv->status_menu_button);
    if (priv->status_area_icon)
      g_object_unref(priv->status_area_icon);
    if (priv->status_menu_icon)
      g_object_unref(priv->status_menu_icon);
  }
}

static void usb_status_menu_item_class_init(UsbStatusMenuItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->finalize = (GObjectFinalizeFunc)usb_status_menu_item_finalize;

  g_type_class_add_private (klass, sizeof(UsbStatusMenuItemPrivate));
}

static gboolean osso_usb_mass_storage_is_used()
{
  return system("/usr/sbin/osso-usb-mass-storage-is-used.sh") > 0;
}

static gboolean is_pcsuite_enabled()
{
  return system("/bin/ls /dev/ttyGS*") == 0;
}

static void stop_enable_usb_mode_timeout(UsbStatusMenuItem *plugin)
{
  UsbStatusMenuItemPrivate *priv = plugin->priv;

  if (priv->enable_usb_mode_timeout) {
    g_source_remove(priv->enable_usb_mode_timeout);
    priv->enable_usb_mode_timeout = 0;
  }

  priv->tries_count = 0;
  priv->expected_replies = 0;
}

static gboolean module_is_loaded(const char *module)
{
  gchar *s;
  int res;

  s = g_strdup_printf("/bin/grep /proc/modules -e %s", module);
  res = system(s);
  g_free(s);

  return res == 0;
}

static void usb_status_menu_enable_button(UsbStatusMenuItem *plugin)
{
  UsbStatusMenuItemPrivate *priv = plugin->priv;
  GtkWidget *child = gtk_bin_get_child(GTK_BIN(&plugin->parent));

  if (child == priv->hbox) {
    g_object_ref(G_OBJECT(child));
    gtk_container_remove(GTK_CONTAINER(&plugin->parent), priv->hbox);
    gtk_container_add(GTK_CONTAINER(&plugin->parent),
                      GTK_WIDGET(priv->status_menu_button));
    g_object_set(plugin, "border-width", 0, NULL);
  }
}

static void usb_status_menu_enable_mode(UsbStatusMenuItem *plugin,
                                        const char *mode,
                                        DBusPendingCallNotifyFunction cb)
{
  UsbStatusMenuItemPrivate *priv = plugin->priv;

  if (priv->tries_count <= 29) {
    if (hh_query_state() == 1) {
      DBusMessage *message;
      DBusConnection *connection;
      DBusPendingCall *pending = NULL;
      gchar *s;

      connection = dbus_bus_get(DBUS_BUS_SYSTEM, 0);
      if (!connection)
        g_warning("usb-plugin::Couldn't obtain dbus_bus_system");
      s = g_strdup_printf("/com/nokia/ke_recv/%s", mode);
      message = dbus_message_new_method_call("com.nokia.ke_recv", s,
                                             "com.nokia.ke_recv",
                                             "dummymethodname");
      g_free(s);
      if (message) {
        priv->expected_replies = 0;

        if ((!g_strcmp0(mode, "enable_pcsuite") &&
             !module_is_loaded("g_nokia")) ||
            (!g_strcmp0(mode, "enable_mass_storage") &&
             !module_is_loaded("g_file_storage")) )
          priv->expected_replies = 2;

        if (!dbus_connection_send_with_reply(
              connection, message, &pending, -1)) {
          g_warning("usb-plugin::dbus send error (%s)!", mode);
          priv->expected_replies = 0;
        }

        dbus_message_unref(message);

        if (pending) {
          priv->pending = pending;
          if (!dbus_pending_call_set_notify(pending, cb, plugin, 0)) {
            g_warning("usb-plugin::dbus pending call set notify failing!");
            priv->expected_replies = 0;
          }
        }
      }
      dbus_connection_unref(connection);
    }
  } else {
    usb_status_menu_enable_button(plugin);
    g_warning(
          "usb-plugin::ke-recv isn't running, you cannot change the usb-mode");
    priv->has_hal_reply = 0;
    priv->ke_recv_alive = 1;
    stop_enable_usb_mode_timeout(plugin);
  }
}



static gboolean is_cable_detached(UsbStatusMenuItem *plugin)
{
  gboolean rv;

  rv = !hh_query_state();
  if (rv) {
    g_warning("usb-plugin::warning Cable detached before reply from ke-recv");
    stop_enable_usb_mode_timeout(plugin);
    usb_status_menu_show_dialog(plugin, FALSE);
  }

  return rv;
}

static gboolean reply_is_error(DBusPendingCall *pending)
{
  DBusMessage *message;
  int type;

  g_return_val_if_fail(pending != NULL, TRUE);

  message = dbus_pending_call_steal_reply(pending);
  type = dbus_message_get_type(message);
  dbus_message_unref(message);
  return type == DBUS_MESSAGE_TYPE_ERROR;
}

static void usb_status_menu_enable_mass_storage_cb(DBusPendingCall *pending,
                                                   UsbStatusMenuItem *plugin)
{
  UsbStatusMenuItemPrivate *priv = plugin->priv;

  if (reply_is_error(pending) && !is_cable_detached(plugin)) {
    UsbStatusMenuItemCbData *cbdata =
        (UsbStatusMenuItemCbData *)g_malloc0(sizeof(UsbStatusMenuItemCbData));
    cbdata->plugin = plugin;
    cbdata->mode = 2;
    priv->enable_usb_mode_timeout =
        g_timeout_add_seconds(1,
                              (GSourceFunc)usb_status_menu_enable_usb_mode_cb,
                              cbdata);
    g_debug("usb-plugin::ke-recv isn't running, postponing message");
  } else {
    priv->ke_recv_alive = TRUE;
    usb_status_menu_enable_button(plugin);
    priv->tries_count = 0;
    g_debug(
          "usb-plugin::got dbus reply: /com/nokia/ke_recv/enable_mass_storage");
    if (osso_usb_mass_storage_is_used())
      usb_status_menu_show_dialog(plugin, 2);
    else if (!is_cable_detached(plugin)) {
      if (priv->has_hal_reply) {
        priv->has_hal_reply = 0;
        priv->current_mode = 1;
        usb_status_menu_create_dialog(plugin);
      }
    }
  }

  dbus_pending_call_unref(pending);
  priv->expected_replies = 0;
  priv->pending = NULL;
}

static void usb_status_menu_enable_pcsuite_cb(DBusPendingCall *pending,
                                               UsbStatusMenuItem *plugin)
{
  UsbStatusMenuItemPrivate *priv = plugin->priv;

  if (reply_is_error(pending) && !is_cable_detached(plugin)) {
    UsbStatusMenuItemCbData *cbdata =
        (UsbStatusMenuItemCbData *)g_malloc0(sizeof(UsbStatusMenuItemCbData));
    cbdata->plugin = plugin;
    cbdata->mode = 3;
    g_timeout_add_seconds(1, (GSourceFunc)usb_status_menu_enable_usb_mode_cb,
                          cbdata);
    g_debug("usb-plugin::ke-recv isn't running, postponing message");
  } else {
    usb_status_menu_enable_button(plugin);
    priv->tries_count = 0;
    g_debug("usb-plugin::got dbus reply: /com/nokia/ke_recv/enable_pcsuite");
    if (is_pcsuite_enabled() && !is_cable_detached(plugin))
      usb_status_menu_show_dialog(plugin, 3);
  }
  dbus_pending_call_unref(pending);
  priv->expected_replies = 0;
  priv->pending = NULL;
}

static void usb_status_menu_enable_charging_cb(DBusPendingCall *pending,
                                               UsbStatusMenuItem *plugin)
{
  UsbStatusMenuItemPrivate *priv = plugin->priv;

  usb_status_menu_enable_button(plugin);

  priv->expected_replies = 0;
  priv->pending = 0;

  if (!is_cable_detached(plugin)) {
    if (reply_is_error(pending) && !is_cable_detached(plugin))
    {
      UsbStatusMenuItemCbData *cbdata =
          (UsbStatusMenuItemCbData *)g_malloc0(sizeof(UsbStatusMenuItemCbData));
      cbdata->plugin = plugin;
      cbdata->mode = 1;
      g_timeout_add_seconds(1, (GSourceFunc)usb_status_menu_enable_usb_mode_cb,
                            cbdata);
      g_debug("usb-plugin::ke-recv isn't running, postponing message");
    } else {
      priv->tries_count = 0;
      g_debug("usb-plugin::got dbus reply: /com/nokia/ke_recv/enable_charging");
    }
    dbus_pending_call_unref(pending);
  }
}

static guint usb_status_menu_enable_usb_mode_cb(UsbStatusMenuItemCbData *data)
{
  UsbStatusMenuItem *plugin = data->plugin;

  plugin->priv->enable_usb_mode_timeout = 0;
  plugin->priv->tries_count ++;

  if (data->mode == 2)
    usb_status_menu_enable_mode(plugin, "enable_mass_storage",
                                (DBusPendingCallNotifyFunction)
                                usb_status_menu_enable_mass_storage_cb);
  else if (data->mode == 3)
    usb_status_menu_enable_mode(plugin, "enable_pcsuite",
                                (DBusPendingCallNotifyFunction)
                                usb_status_menu_enable_pcsuite_cb);
  else
    usb_status_menu_enable_mode(plugin, "enable_charging",
                                (DBusPendingCallNotifyFunction)
                                usb_status_menu_enable_charging_cb);

  g_free(data);

  return 0;
}

static void usb_status_menu_dialog_response_cb(GtkWidget *dialog,
                                               gint response,
                                               UsbStatusMenuItem *plugin)
{
  if (response == GTK_RESPONSE_DELETE_EVENT) {
    if (plugin->priv->dialog) {
      if (GTK_IS_DIALOG(plugin->priv->dialog)) {
        gtk_widget_hide(plugin->priv->dialog);
        gtk_widget_destroy(plugin->priv->dialog);
        plugin->priv->dialog = NULL;
      }
    }
    g_debug("usb-plugin::Tapped outside -> Charging only mode");
    usb_status_menu_enable_mode(plugin, "enable_charging",
             (DBusPendingCallNotifyFunction)usb_status_menu_enable_charging_cb);
  }
}

static void pc_suite_clicked_cb(GtkWidget *widget, UsbStatusMenuItem *plugin)
{
  UsbStatusMenuItemPrivate *priv = plugin->priv;

  g_debug("usb-plugin::PC Suite mode button pressed");

  if (priv->dialog && GTK_IS_DIALOG(priv->dialog)) {
    gtk_widget_hide(priv->dialog);
    gtk_widget_destroy(priv->dialog);
    priv->dialog = NULL;
  }

  usb_status_menu_enable_mode(plugin, "enable_pcsuite",
                              (DBusPendingCallNotifyFunction)
                              usb_status_menu_enable_pcsuite_cb);
  usb_status_menu_disable_usb_button(plugin);
}

static void mass_storage_clicked_cb(GtkWidget *widget,
                                    UsbStatusMenuItem *plugin)
{
  UsbStatusMenuItemPrivate *priv = plugin->priv;

  g_debug("usb-plugin::Mass Storage mode button pressed");

  if (priv->dialog && GTK_IS_DIALOG(priv->dialog)) {
    gtk_widget_hide(priv->dialog);
    gtk_widget_destroy(priv->dialog);
    priv->dialog = NULL;
  }
  priv->ke_recv_alive = 0;
  usb_status_menu_enable_mode(plugin, "enable_mass_storage",
                              (DBusPendingCallNotifyFunction)
                              usb_status_menu_enable_mass_storage_cb);
  usb_status_menu_disable_usb_button(plugin);
}

static void usb_status_menu_create_dialog(UsbStatusMenuItem *plugin)
{
  UsbStatusMenuItemPrivate *priv = plugin->priv;
  char *s;
  gchar *title;
  GtkWidget *label;
  GtkWidget *button_box;
  GtkWidget *button;
  GtkWidget *top;

  if (priv->dialog && GTK_IS_DIALOG(priv->dialog)) {
    gtk_widget_hide(priv->dialog);
    gtk_widget_destroy(priv->dialog);
    priv->dialog = NULL;
  }

  priv->dialog = gtk_dialog_new();
  hildon_gtk_window_set_portrait_flags(GTK_WINDOW(priv->dialog),
                                       HILDON_PORTRAIT_MODE_SUPPORT);

  s = hh_get_device_name();
  if (s && *s)
    title =
        g_strdup_printf(g_dgettext("hildon-status-bar-usb",
                                   "stab_ti_usb_connected_to_%s"), s);
  else
    title = g_strdup_printf(
          g_dgettext("hildon-status-bar-usb", "stab_ti_usb_connected_to_%s"),
          g_dgettext("hildon-status-bar-usb", "stab_me_usb_device_name"));
  gtk_window_set_title(GTK_WINDOW(priv->dialog), title);
  g_free(title);


  label = gtk_label_new(g_dgettext("hildon-status-bar-usb",
                                   "usbh_li_current_state"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(priv->dialog)->vbox), label, 1, 1, 0x16u);

  hildon_helper_set_logical_color(label, GTK_RC_FG, 0, "SecondaryTextColor");
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);

  button_box = gtk_vbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_SPREAD);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(priv->dialog)->vbox), GTK_WIDGET(button_box),
                     0, 0, 0);
  gtk_box_set_spacing(GTK_BOX(button_box), 16);

  button = hildon_button_new_with_text(
        HILDON_SIZE_FINGER_HEIGHT|HILDON_SIZE_HALFSCREEN_WIDTH,
        HILDON_BUTTON_ARRANGEMENT_VERTICAL,
        g_dgettext("hildon-status-bar-usb", "usbh_bd_usb_mode_mass_storage"),
        0);
  gtk_widget_set_name(button, "HildonButton-finger");
  gtk_widget_set_size_request(button, 400, 70);
  g_signal_connect_data(G_OBJECT(button), "clicked",
                        (GCallback)mass_storage_clicked_cb, plugin, NULL, 0);
  gtk_box_pack_start(GTK_BOX(button_box), button, 0, 0, 0);

  button = hildon_button_new_with_text(
        HILDON_SIZE_FINGER_HEIGHT|HILDON_SIZE_HALFSCREEN_WIDTH,
        HILDON_BUTTON_ARRANGEMENT_VERTICAL,
        g_dgettext("hildon-status-bar-usb", "usbh_bd_usb_mode_pc_suite"),
        0);
  gtk_widget_set_name(button, "HildonButton-finger");
  gtk_widget_set_size_request(button, 400, 70);
  g_signal_connect_data(G_OBJECT(button), "clicked",
                        (GCallback)pc_suite_clicked_cb, plugin, NULL, 0);
  gtk_box_pack_start(GTK_BOX(button_box), button, 0, 0, 0);

  top = gtk_widget_get_toplevel(GTK_WIDGET(&plugin->parent));

  if (top && GTK_IS_WINDOW(top) && GTK_WIDGET_TOPLEVEL(top))
    gtk_widget_hide(top);

  gtk_window_set_modal(GTK_WINDOW(priv->dialog), TRUE);
  gtk_widget_show_all(GTK_WIDGET(priv->dialog));
  g_signal_connect_data(G_OBJECT(priv->dialog), "response",
                        G_CALLBACK(usb_status_menu_dialog_response_cb), plugin,
                        NULL, 0);
  gtk_window_present(GTK_WINDOW(priv->dialog));
}

static void usb_status_menu_disable_usb_button(UsbStatusMenuItem *plugin)
{
  UsbStatusMenuItemPrivate *priv;
  GtkWidget *child;

  priv = plugin->priv;
  child = gtk_bin_get_child(GTK_BIN(&plugin->parent));
  if ((GObject*)child == priv->status_menu_button) {
    g_object_ref(G_OBJECT(child));
    gtk_container_remove(GTK_CONTAINER(&plugin->parent),
                         GTK_WIDGET(priv->status_menu_button));
    gtk_container_add(GTK_CONTAINER(&plugin->parent), priv->hbox);
    g_object_set(plugin, "border-width", 8, NULL);
  }
}

static void usb_status_menu_show_dialog(UsbStatusMenuItem *plugin, int mode)
{
  const gchar *s = NULL;
  UsbStatusMenuItemPrivate *priv = plugin->priv;

  priv->current_mode = mode;

  if (mode == 2) {
    g_debug("usb-plugin::current-state: Mass Storage mode");
    s = g_dgettext("hildon-status-bar-usb", "usbh_menu_plugin_va_mass_storage");
  } else if (mode == 3) {
    g_debug("usb-plugin::current-state: PC Suite Mode");
    s = g_dgettext("hildon-status-bar-usb", "usbh_menu_plugin_va_pc_suite");
  } else if (mode == 1) {
    g_debug("usb-plugin::current-state: Peripheral / Charge only");
    s = g_dgettext("hildon-status-bar-usb",
                   "usbh_menu_plugin_va_charging_only");
  } else {
    g_debug("usb-plugin::current-state: No USB connection");
    if (priv->dialog && GTK_IS_DIALOG(priv->dialog)) {
      gtk_widget_hide(priv->dialog);
      gtk_widget_destroy(priv->dialog);
      priv->dialog = 0;
    }
  }

  if (mode) {
    if (s)
      gtk_label_set_label(GTK_LABEL(priv->label), s);

    if ( (unsigned int)(mode - 2) <= 1 ) {
      usb_status_menu_show(plugin, TRUE, TRUE);
      usb_status_menu_disable_usb_button(plugin);
    } else {
      usb_status_menu_show(plugin, TRUE, FALSE);
      usb_status_menu_enable_button(plugin);
    }
  } else
    usb_status_menu_show(plugin, FALSE, FALSE);

  if (!g_file_set_contents(
          "/tmp/.current_usb_mode",
          usb_modes[priv->current_mode],
          -1,
          0))
    g_warning("usb-plugin: state saving failed!");
}

static void usb_status_menu_button_clicked_cb(GtkWidget *widget,
                                              UsbStatusMenuItem *plugin)
{
  if (plugin->priv->current_mode == 1)
    usb_status_menu_create_dialog(plugin);
}

static void usb_status_menu_item_hal_cb(gboolean cable_connected,
                                        UsbStatusMenuItem *plugin)
{
  UsbStatusMenuItemPrivate *priv = plugin->priv;

  if (priv->expected_replies <= 0 ) {
    if (priv->ke_recv_alive) {
      if ( cable_connected == 1 ) {
        g_debug("usb-plugin::usb-event: peripheral");
        if (!priv->current_mode) {
          usb_status_menu_show_dialog(plugin, 1);
          usb_status_menu_create_dialog(plugin);
        }
      } else {
        stop_enable_usb_mode_timeout(plugin);
        g_debug("usb-plugin::usb-event: idle");
        if (priv->dialog && GTK_IS_DIALOG(priv->dialog)) {
            gtk_widget_hide(priv->dialog);
            gtk_widget_destroy(priv->dialog);
            priv->dialog = NULL;
        }
        usb_status_menu_show_dialog(plugin, FALSE);
      }
    } else {
      g_debug("\nusb-plugin::got hal event, but reply isn't received yet from "
              "ke-recv\nusb-plugin::->showing the dialog when ke-recv fails "
              "to change the mode");
      priv->has_hal_reply = TRUE;
    }
  } else
    priv->expected_replies --;
}

static void usb_status_menu_item_init(UsbStatusMenuItem *plugin)
{
  UsbStatusMenuItemPrivate *priv = USB_STATUS_MENU_ITEM_GET_PRIVATE(plugin);
  GtkWidget *vbox1;
  GtkWidget *vbox2;
  GtkWidget *label;
  PangoAttrList *attr;
  unsigned int i;
  gboolean cable_connected;
  gchar *s;

  plugin->priv = priv;
  priv->expected_replies = 0;
  priv->pending = 0;
  priv->tries_count = 0;
  priv->enable_usb_mode_timeout = 0;
  priv->status_area_icon = NULL;
  priv->status_menu_icon = NULL;
  priv->has_hal_reply = 0;
  priv->current_mode = 0;
  priv->ke_recv_alive = 1;

  priv->hbox = gtk_hbox_new(0, 0);
  vbox1 = gtk_vbox_new(0, 0);
  vbox2 = gtk_vbox_new(0, 0);

  label = gtk_label_new(g_dgettext("hildon-status-bar-usb",
                                   "usbh_menu_plugin_title"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  attr = pango_attr_list_new();
  pango_attr_list_insert(attr, pango_attr_size_new_absolute(24 * PANGO_SCALE));
  gtk_label_set_attributes(GTK_LABEL(label), attr);
  pango_attr_list_unref(attr);

  priv->label = gtk_label_new("");
  attr = pango_attr_list_new();
  pango_attr_list_insert(attr, pango_attr_size_new_absolute(18 * PANGO_SCALE));
  gtk_label_set_attributes(GTK_LABEL(priv->label), attr);
  gtk_misc_set_alignment(GTK_MISC(GTK_LABEL(priv->label)), 0.0, 0.5);
  hildon_helper_set_logical_color(priv->label, GTK_RC_FG, 0,
                                  "SecondaryTextColor");
  pango_attr_list_unref(attr);

  priv->box_usb_image = gtk_image_new();
  gtk_widget_set_size_request(priv->box_usb_image, 48, 48);

  gtk_box_pack_start(GTK_BOX(priv->hbox), priv->box_usb_image, 0, 1, 4);
  gtk_box_pack_start(GTK_BOX(priv->hbox), vbox1, 1, 1, 8);
  gtk_box_pack_start(GTK_BOX(vbox1), vbox2, 1, 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox2), label, 0, 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox2), priv->label, 0, 0, 0);
  gtk_widget_show_all(priv->hbox);

  priv->status_menu_button =
      g_object_new(HILDON_TYPE_BUTTON,
                   "title", g_dgettext("hildon-status-bar-usb", "usbh_menu_plugin_title"),
                   "value", g_dgettext("hildon-status-bar-usb", "usbh_menu_plugin_va_charging_only"),
                   "size", 4,
                   "arrangement", 1,
                   "style", 1,
                   NULL);
  gtk_button_set_alignment(GTK_BUTTON(priv->status_menu_button), 0.0, 0.5);
  priv->button_usb_image = gtk_image_new();
  gtk_widget_set_size_request(priv->button_usb_image, 48, 48);
  hildon_button_set_image(HILDON_BUTTON(priv->status_menu_button),
                          priv->button_usb_image);
  hildon_button_set_image_position(HILDON_BUTTON(priv->status_menu_button), 0);
  g_signal_connect_data(G_OBJECT(priv->status_menu_button), "clicked",
                        G_CALLBACK(usb_status_menu_button_clicked_cb), plugin,
                        NULL, G_CONNECT_AFTER);

  gtk_container_add(GTK_CONTAINER(&plugin->parent),
                    GTK_WIDGET(priv->status_menu_button));

  gtk_widget_show_all(GTK_WIDGET(priv->status_menu_button));

  priv->dialog = 0;
  hh_init();
  hh_set_callback((HhCallback)usb_status_menu_item_hal_cb, plugin);

  s = NULL;
  if (g_file_get_contents("/tmp/.current_usb_mode", &s, NULL, NULL))
  {
    for (i = 0; i < sizeof(usb_modes) / sizeof(usb_modes[0]); i++) {
      if (!g_strcmp0(s, usb_modes[i]))
          break;
    }

    if (i == (sizeof(usb_modes) / sizeof(usb_modes[0])))
      i = 0;
  }
  else
    i = 0;

  if (!s)
    s = g_strdup("<no data>");

  cable_connected = hh_query_state();

  if (!cable_connected) {
    g_message("usb-plugin::init [saved_state='%s', usb_conn='%s']", s, "false");
    usb_status_menu_item_hal_cb(FALSE, plugin);
  } else {
    g_message("usb-plugin::init [saved_state='%s', usb_conn='%s']", s, "true");

    if(i == 0) {
      usb_status_menu_item_hal_cb(TRUE, plugin);
    } else if (i == 2) {
      if (osso_usb_mass_storage_is_used())
        usb_status_menu_show_dialog(plugin, 2);
      else
        usb_status_menu_show_dialog(plugin, 1);
    } else if (i != 3 || !is_pcsuite_enabled()) {
      usb_status_menu_show_dialog(plugin, 1);
    } else {
      usb_status_menu_show_dialog(plugin, 3);
    }
  }

  g_free(s);
}
