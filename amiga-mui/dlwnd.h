#ifndef DLWND_H
#define DLWND_H

int dl_window_init(void);
int dl_window_open(void);
void dl_window_close(void);
void set_dl_title(char *);
void set_dl_status(char *);
void init_dl_gauge_mail(int);
void set_dl_gauge_mail(int);
void init_dl_gauge_byte(int);
void set_dl_gauge_byte(int);

#endif