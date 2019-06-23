#ifndef UWIFIMON_h_
#define UWIFIMON_h_

int uwifimon_cap_start(char *file);
int uwifimon_cap_stop(void);
int uwifimon_net_start(void);
int uwifimon_net_stop(void);
int uwifimon_mod_state(void);
int uwifimon_mod_stats(struct wifimon_stats_t *s, int reset);

#endif
