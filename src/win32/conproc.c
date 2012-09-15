/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// conproc.c -- support for qhost
#define WIN32_LEAN_AND_MEAN
#include "../client/client.h"
//#include "win_local.h"
#include <windows.h>
#include "resource.h"
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include "conproc.h"

/*#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <windows.h>
#include "conproc.h"*/

#define COPY_ID			1
#define QUIT_ID			2
#define CLEAR_ID		3

#define ERRORBOX_ID		10
#define ERRORTEXT_ID	11

#define EDIT_ID			100
#define INPUT_ID		101

typedef struct
{
	HWND		hWnd;
	HWND		hwndBuffer;

	HWND		hwndButtonClear;
	HWND		hwndButtonCopy;
	HWND		hwndButtonQuit;

	HWND		hwndErrorBox;
	HWND		hwndErrorText;

	HBITMAP		hbmLogo;
	HBITMAP		hbmClearBitmap;

	HBRUSH		hbrEditBackground;
	HBRUSH		hbrErrorBackground;

	HFONT		hfBufferFont;
	HFONT		hfButtonFont;

	HWND		hwndInputLine;

	char		errorString[80];
	qboolean	timerActive;

	char		consoleText[512], returnedText[512];
	int			visLevel;
	qboolean	quitOnClose;
	int			windowWidth, windowHeight;
	
	WNDPROC		SysInputLineWndProc;

} WinConData;

static WinConData s_wcd;

static LONG WINAPI ConWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
//	char *cmdString;
	static qboolean s_timePolarity;

	switch (uMsg)
	{
	case WM_ACTIVATE:
		if ( LOWORD( wParam ) != WA_INACTIVE )
		{
			SetFocus( s_wcd.hwndInputLine );
		}
		break;
	case WM_CLOSE:
		Cbuf_AddText("quit\n");
		return 0;
	case WM_CTLCOLORSTATIC:
		if ( ( HWND ) lParam == s_wcd.hwndBuffer )
		{
			SetBkColor( ( HDC ) wParam, RGB( 0x00, 0x00, 0xB0 ) );
			SetTextColor( ( HDC ) wParam, RGB( 0xff, 0xff, 0x00 ) );

			return ( long ) s_wcd.hbrEditBackground;
		}
		else if ( ( HWND ) lParam == s_wcd.hwndErrorBox )
		{
			if ( s_timePolarity & 1 )
			{
				SetBkColor( ( HDC ) wParam, RGB( 0x80, 0x80, 0x80 ) );
				SetTextColor( ( HDC ) wParam, RGB( 0xff, 0x0, 0x00 ) );
			}
			else
			{
				SetBkColor( ( HDC ) wParam, RGB( 0x80, 0x80, 0x80 ) );
				SetTextColor( ( HDC ) wParam, RGB( 0x00, 0x0, 0x00 ) );
			}
			return ( long ) s_wcd.hbrErrorBackground;
		}
		break;

	case WM_COMMAND:
		if ( wParam == COPY_ID )
		{
			SendMessage( s_wcd.hwndBuffer, EM_SETSEL, 0, -1 );
			SendMessage( s_wcd.hwndBuffer, WM_COPY, 0, 0 );
		}
		else if ( wParam == QUIT_ID )
		{
			if ( s_wcd.quitOnClose )
			{
				PostQuitMessage( 0 );
			}
			else
			{
				//cmdString = CopyString( "quit" );
				
				//Sys_QueEvent( 0, SE_CONSOLE, 0, 0, strlen( cmdString ) + 1, cmdString );
				Cbuf_AddText("quit\n");
			}
		}
		else if ( wParam == CLEAR_ID )
		{
			SendMessage( s_wcd.hwndBuffer, EM_SETSEL, 0, -1 );
			SendMessage( s_wcd.hwndBuffer, EM_REPLACESEL, FALSE, ( LPARAM ) "" );
			UpdateWindow( s_wcd.hwndBuffer );
		}
		break;
	case WM_TIMER:
		if ( wParam == 1 )
		{
			s_timePolarity = !s_timePolarity;
			if ( s_wcd.hwndErrorBox )
			{
				InvalidateRect( s_wcd.hwndErrorBox, NULL, FALSE );
			}
		}
		break;
    }

    return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

LONG WINAPI InputLineWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char inputBuffer[1024];

	switch ( uMsg )
	{
	case WM_KILLFOCUS:
		if ( ( HWND ) wParam == s_wcd.hWnd ||
			 ( HWND ) wParam == s_wcd.hwndErrorBox )
		{
			SetFocus( hWnd );
			return 0;
		}
		break;

	case WM_CHAR:
		if ( wParam == 13 )
		{
			GetWindowText( s_wcd.hwndInputLine, inputBuffer, sizeof( inputBuffer ) );
			strncat( s_wcd.consoleText, inputBuffer, sizeof( s_wcd.consoleText ) - strlen( s_wcd.consoleText ) - 5 );
			strcat( s_wcd.consoleText, "\n" );
			SetWindowText( s_wcd.hwndInputLine, "" );

			Conbuf_AppendText( va( "]%s\n", inputBuffer ) );

			return 0;
		}
	}

	return CallWindowProc( s_wcd.SysInputLineWndProc, hWnd, uMsg, wParam, lParam );
}

/*
** Sys_CreateConsole
*/
extern HINSTANCE	global_hInstance;

void Sys_CreateConsole( void )
{
	HDC hDC;
	WNDCLASS wc;
	RECT rect;
	const char *DEDCLASS = APPLICATION" WinConsole";
	int nHeight;
	int swidth, sheight;
	int DEDSTYLE = WS_POPUPWINDOW | WS_CAPTION | WS_MINIMIZEBOX;

	memset( &wc, 0, sizeof( wc ) );

	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC) ConWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = global_hInstance;
	wc.hIcon         = LoadIcon( global_hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = (void *)COLOR_WINDOW;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = DEDCLASS;

	if ( !RegisterClass (&wc) )
		return;

	rect.left = 0;
	rect.right = 540;
	rect.top = 0;
	rect.bottom = 450;
	AdjustWindowRect( &rect, DEDSTYLE, FALSE );

	hDC = GetDC( GetDesktopWindow() );
	swidth = GetDeviceCaps( hDC, HORZRES );
	sheight = GetDeviceCaps( hDC, VERTRES );
	ReleaseDC( GetDesktopWindow(), hDC );

	s_wcd.windowWidth = rect.right - rect.left + 1;
	s_wcd.windowHeight = rect.bottom - rect.top + 1;

	s_wcd.hWnd = CreateWindowEx( 0,
							   DEDCLASS,
							   APPLICATION" Console",
							   DEDSTYLE,
							   ( swidth - 600 ) / 2, ( sheight - 450 ) / 2 , rect.right - rect.left + 1, rect.bottom - rect.top + 1,
							   NULL,
							   NULL,
							   global_hInstance,
							   NULL );

	if ( s_wcd.hWnd == NULL )
	{
		return;
	}

	//
	// create fonts
	//
	hDC = GetDC( s_wcd.hWnd );
	nHeight = -MulDiv( 8, GetDeviceCaps( hDC, LOGPIXELSY), 72);

	s_wcd.hfBufferFont = CreateFont( nHeight,
									  0,
									  0,
									  0,
									  FW_LIGHT,
									  0,
									  0,
									  0,
									  DEFAULT_CHARSET,
									  OUT_DEFAULT_PRECIS,
									  CLIP_DEFAULT_PRECIS,
									  DEFAULT_QUALITY,
									  FF_MODERN | FIXED_PITCH,
									  "Courier New" );

	ReleaseDC( s_wcd.hWnd, hDC );

	//
	// create the input line
	//
	s_wcd.hwndInputLine = CreateWindow( "edit", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | 
												ES_LEFT | ES_AUTOHSCROLL,
												6, 400, 528, 20,
												s_wcd.hWnd, 
												( HMENU ) INPUT_ID,	// child window ID
												global_hInstance, NULL );

	//
	// create the buttons
	//
	s_wcd.hwndButtonCopy = CreateWindow( "button", NULL, BS_PUSHBUTTON | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
												5, 425, 72, 24,
												s_wcd.hWnd, 
												( HMENU ) COPY_ID,	// child window ID
												global_hInstance, NULL );
	SendMessage( s_wcd.hwndButtonCopy, WM_SETTEXT, 0, ( LPARAM ) "copy" );

	s_wcd.hwndButtonClear = CreateWindow( "button", NULL, BS_PUSHBUTTON | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
												82, 425, 72, 24,
												s_wcd.hWnd, 
												( HMENU ) CLEAR_ID,	// child window ID
												global_hInstance, NULL );
	SendMessage( s_wcd.hwndButtonClear, WM_SETTEXT, 0, ( LPARAM ) "clear" );

	s_wcd.hwndButtonQuit = CreateWindow( "button", NULL, BS_PUSHBUTTON | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
												462, 425, 72, 24,
												s_wcd.hWnd, 
												( HMENU ) QUIT_ID,	// child window ID
												global_hInstance, NULL );
	SendMessage( s_wcd.hwndButtonQuit, WM_SETTEXT, 0, ( LPARAM ) "quit" );


	//
	// create the scrollbuffer
	//
	s_wcd.hwndBuffer = CreateWindow( "edit", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | 
												ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
												6, 40, 526, 354,
												s_wcd.hWnd, 
												( HMENU ) EDIT_ID,	// child window ID
												global_hInstance, NULL );
	SendMessage( s_wcd.hwndBuffer, WM_SETFONT, ( WPARAM ) s_wcd.hfBufferFont, 0 );

	s_wcd.SysInputLineWndProc = ( WNDPROC ) SetWindowLong( s_wcd.hwndInputLine, GWL_WNDPROC, ( long ) InputLineWndProc );
	SendMessage( s_wcd.hwndInputLine, WM_SETFONT, ( WPARAM ) s_wcd.hfBufferFont, 0 );


	s_wcd.hbrEditBackground = CreateSolidBrush( RGB( 0x00, 0x00, 0xB0 ) );
	s_wcd.hbrErrorBackground = CreateSolidBrush( RGB( 0x80, 0x80, 0x80 ) );

	ShowWindow( s_wcd.hWnd, SW_HIDE);
	UpdateWindow( s_wcd.hWnd );
	//SetForegroundWindow( s_wcd.hWnd );
	//SetFocus( s_wcd.hwndInputLine );
	//ShowWindow( s_wcd.hWnd, SW_HIDE );

	s_wcd.visLevel = 0;
}

/*
** Sys_DestroyConsole
*/
void Sys_DestroyConsole( void ) {

	if(!s_wcd.timerActive)
		KillTimer(s_wcd.hWnd, 1);

	if(s_wcd.hfBufferFont)
		DeleteObject(s_wcd.hfBufferFont);

	if(s_wcd.hbrEditBackground)
		DeleteObject(s_wcd.hbrEditBackground);

	if(s_wcd.hbrErrorBackground)
		DeleteObject(s_wcd.hbrErrorBackground);

	if ( s_wcd.hWnd ) {
		if(s_wcd.visLevel != 0)
			ShowWindow( s_wcd.hWnd, SW_HIDE );

		//CloseWindow( s_wcd.hWnd );
		DestroyWindow( s_wcd.hWnd );
		s_wcd.hWnd = 0;
	}
}

/*
** Sys_ShowConsole
*/
void Sys_ShowConsole( int visLevel, qboolean quitOnClose )
{

	if ( !s_wcd.hWnd )
		return;

	s_wcd.quitOnClose = quitOnClose;

	if ( visLevel == s_wcd.visLevel )
		return;

	s_wcd.visLevel = visLevel;


	switch ( visLevel )
	{
	case 0:
		ShowWindow( s_wcd.hWnd, SW_HIDE );
		break;
	case 1:
		ShowWindow( s_wcd.hWnd, SW_SHOWNORMAL );
		SendMessage( s_wcd.hwndBuffer, EM_LINESCROLL, 0, 0xffff );
		break;
	case 2:
		ShowWindow( s_wcd.hWnd, SW_MINIMIZE );
		break;
	default:
		Sys_Error( "Invalid visLevel %d sent to Sys_ShowConsole\n", visLevel );
		break;
	}
}

/*
** Sys_ConsoleInput
*/
char *Sys_ConsoleInput( void )
{
	if (!s_wcd.consoleText[0])
		return NULL;
		
	strcpy( s_wcd.returnedText, s_wcd.consoleText );
	s_wcd.consoleText[0] = 0;
	
	return s_wcd.returnedText;
}

/*
** Conbuf_AppendText
*/
void Conbuf_AppendText( const char *pMsg )
{
	char buffer[MAXPRINTMSG];
	int bufLen = 0;
	static unsigned long s_totalChars;


	//
	// copy into an intermediate buffer
	//

	while (*pMsg)
	{
		if (*pMsg == '\n')
		{
			buffer[bufLen++] = '\r';
			buffer[bufLen++] = '\n';
			pMsg++;
			if (*pMsg == '\r')
				pMsg++;
		}
		else if (*pMsg == '\r' )
		{
			buffer[bufLen++] = '\r';
			buffer[bufLen++] = '\n';
			pMsg++;
		}
		else
		{
			buffer[bufLen++] = *pMsg++;
		}
	}
	buffer[bufLen] = 0;

	s_totalChars += bufLen;

	//
	// replace selection instead of appending if we're overflowing
	//
	if ( s_totalChars > 0x7fff )
	{
		SendMessage( s_wcd.hwndBuffer, EM_SETSEL, 0, -1 );
		s_totalChars = bufLen;
	}

	//
	// put this text into the windows console
	//
	SendMessage( s_wcd.hwndBuffer, EM_LINESCROLL, 0, 0xffff );
	SendMessage( s_wcd.hwndBuffer, EM_SCROLLCARET, 0, 0 );
	SendMessage( s_wcd.hwndBuffer, EM_REPLACESEL, 0, (LPARAM) buffer );
}

/*
** Sys_SetErrorText
*/
void Sys_SetErrorText( const char *buf )
{
	Q_strncpyz( s_wcd.errorString, buf, sizeof( s_wcd.errorString ) );

	if(!s_wcd.timerActive) {
		SetTimer( s_wcd.hWnd, 1, 1000, NULL );
		s_wcd.timerActive = true;
	}

	if ( !s_wcd.hwndErrorBox )
	{
		s_wcd.hwndErrorBox = CreateWindow( "static", NULL, WS_CHILD | WS_VISIBLE | SS_SUNKEN,
													6, 5, 526, 30,
													s_wcd.hWnd, 
													( HMENU ) ERRORBOX_ID,	// child window ID
													global_hInstance, NULL );
		SendMessage( s_wcd.hwndErrorBox, WM_SETFONT, ( WPARAM ) s_wcd.hfBufferFont, 0 );
		SetWindowText( s_wcd.hwndErrorBox, s_wcd.errorString );

		DestroyWindow( s_wcd.hwndInputLine );
		s_wcd.hwndInputLine = NULL;
	}
}

     
