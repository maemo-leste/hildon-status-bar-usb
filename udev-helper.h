#ifndef __UDEV_HELPER_H__
#define __UDEV_HELPER_H__
typedef void (*UhCallback)(gboolean, gpointer);


int uh_init();
int uh_destroy();
void uh_set_callback(UhCallback cb, gpointer data);
gboolean uh_query_state();

/* TODO */
char *uh_get_device_name();



#endif /* __UDEV_HELPER_H__ */
