/*
** upwnd.h
*/

#ifndef UPWND_H
#define UPWND_H

int up_window_init(void);
int up_window_open(void);
void up_window_close(void);
void set_up_title(char *);
void set_up_status(char *);
void init_up_gauge_mail(int);
void set_up_gauge_mail(int);
void init_up_gauge_byte(int);
void set_up_gauge_byte(int);

#endif