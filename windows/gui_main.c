/***************************************************************************
 SimpleMail - Copyright (C) 2000 Hynek Schlawack and Sebastian Bauer

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
***************************************************************************/

/*
** gui_main.c
*/

#include <windows.h>

#include "mainwnd.h"

#include "simplemail.h"

/* Declare Windows procedure */
LRESULT CALLBACK WindowProcedure(HWND, UINT, WPARAM, LPARAM);
/* Make the class name into a global variable */
char szClassName[ ] = "SimpleMail";
int WINAPI WinMain(HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nFunsterStil)

{
    HWND hwnd;               /* This is the handle for our window */
    MSG messages;            /* Here messages to the application are saved */
    WNDCLASSEX wincl;        /* Data structure for the windowclass */

    /* The Window structure */
    wincl.hInstance = hThisInstance;
    wincl.lpszClassName = szClassName;
    wincl.lpfnWndProc = WindowProcedure;      /* This function is called by windows */
    wincl.style = CS_DBLCLKS;                 /* Catch double-clicks */
    wincl.cbSize = sizeof(WNDCLASSEX);

    /* Use default icon and mouse-pointer */
    wincl.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wincl.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    wincl.hCursor = LoadCursor(NULL, IDC_ARROW);
    wincl.lpszMenuName = NULL; /* No menu */
    wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
    wincl.cbWndExtra = 0;                      /* structure or the window instance */
    /* Use light-gray as the background of the window */
    wincl.hbrBackground = (HBRUSH) GetStockObject(LTGRAY_BRUSH);

    /* Register the window class, if fail quit the program */
    if(!RegisterClassEx(&wincl)) return 0;

    simplemail_main();

    /* The class is registered, let's create the program*/
    hwnd = CreateWindowEx(
           0,                   /* Extended possibilites for variation */
           szClassName,         /* Classname */
           "SimpleMail",        /* Title Text */
           WS_OVERLAPPEDWINDOW, /* default window */
           CW_USEDEFAULT,       /* Windows decides the position */
           CW_USEDEFAULT,       /* where the window ends up on the screen */
           544,                 /* The programs width */
           375,                 /* and height in pixels */
           HWND_DESKTOP,        /* The window is a child-window to desktop */
           NULL,                /* No menu */
           hThisInstance,       /* Program Instance handler */
           NULL                 /* No Window Creation data */
           );

    /* Make the window visible on the screen */
    ShowWindow(hwnd, nFunsterStil);
    /* Run the message loop. It will run until GetMessage( ) returns 0 */
    while(GetMessage(&messages, NULL, 0, 0))
    {
           /* Translate virtual-key messages into character messages */
           TranslateMessage(&messages);
           /* Send message to WindowProcedure */
           DispatchMessage(&messages);
    }

    /* The program return-value is 0 - The value that PostQuitMessage( ) gave */
    return messages.wParam;
}

/* This function is called by the Windows function DispatchMessage( ) */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)                  /* handle the messages */
    {
           case WM_DESTROY:
           PostQuitMessage(0);        /* send a WM_QUIT to the message queue */
           break;
           default:                   /* for messages that we don't deal with */
           return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}


int gui_parseargs(int argc, char *argv[])
{
	return 1;
}

void gui_loop(void)
{
}

int gui_main(int argc, char *argv[])
{
	main_window_init();
	main_refresh_folders();
	main_window_open();
	callback_change_folder_attrs();
        gui_loop();
	return 1;
}

