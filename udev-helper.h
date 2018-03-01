#ifndef __UDEV_HELPER_H__
#define __UDEV_HELPER_H__
typedef void (*UhCallback)(gboolean, gint, gpointer);


int uh_init();
int uh_destroy();
void uh_set_callback(UhCallback cb, gpointer data);
void uh_query_state(gboolean*, gint*);

/* TODO: Implement this */
char *uh_get_device_name();

enum {
    USB_MODE_UNKNOWN = 0,
    USB_MODE_B_IDLE = 1,
    USB_MODE_B_PERIPHERAL = 2,
    /*
    TODO: Check on droid4/lime2
    USB_MODE_A_IDLE,
    USB_MODE_A_PERIPHERAL,
    */
};

#endif /* __UDEV_HELPER_H__ */
