#ifndef DLWND_H
#define DLWND_H

int dl_window_init(void);
int dl_window_open(void);
void dl_window_close(void);
void dl_set_title(char *);
void dl_set_status(char *);
void dl_init_gauge_mail(int);
void dl_set_gauge_mail(int);
void dl_init_gauge_byte(int);
void dl_set_gauge_byte(int);

#endif