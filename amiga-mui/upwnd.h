/*
** upwnd.h
*/

#ifndef UPWND_H
#define UPWND_H

int up_window_init(void);
int up_window_open(void);
void up_window_close(void);
void up_set_title(char *);
void up_set_status(char *);
void up_init_gauge_mail(int);
void up_set_gauge_mail(int);
void up_init_gauge_byte(int);
void up_set_gauge_byte(int);

#endif