#ifndef __HAL_HELPER_H__
#define __HAL_HELPER_H__

typedef void (*HhCallback)(gboolean, gpointer);

void hh_init();
void hh_destroy();
void hh_set_callback(HhCallback cb, gpointer data);
gboolean hh_query_state();
char *hh_get_device_name();

#endif /* __HAL_HELPER_H__ */
