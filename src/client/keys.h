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

//
// these are the key numbers that should be passed to Key_Event
//
#define	K_TAB			9
#define	K_ENTER			13
#define	K_ESCAPE		27
#define	K_SPACE			32

// normal keys should be passed as lowercased ascii

#define	K_BACKSPACE		127
#define	K_UPARROW		128
#define	K_DOWNARROW		129
#define	K_LEFTARROW		130
#define	K_RIGHTARROW	131

#define	K_ALT			132
#define	K_CTRL			133
#define	K_SHIFT			134
#define	K_F1			135
#define	K_F2			136
#define	K_F3			137
#define	K_F4			138
#define	K_F5			139
#define	K_F6			140
#define	K_F7			141
#define	K_F8			142
#define	K_F9			143
#define	K_F10			144
#define	K_F11			145
#define	K_F12			146
#define	K_INS			147
#define	K_DEL			148
#define	K_PGDN			149
#define	K_PGUP			150
#define	K_HOME			151
#define	K_END			152

#define K_KP_HOME		160
#define K_KP_UPARROW	161
#define K_KP_PGUP		162
#define	K_KP_LEFTARROW	163
#define K_KP_5			164
#define K_KP_RIGHTARROW	165
#define K_KP_END		166
#define K_KP_DOWNARROW	167
#define K_KP_PGDN		168
#define	K_KP_ENTER		169
#define K_KP_INS   		170
#define	K_KP_DEL		171
#define K_KP_SLASH		172
#define K_KP_MINUS		173
#define K_KP_PLUS		174

#define	K_CAPSLOCK		175

#define K_PAUSE			255

//
// mouse buttons generate virtual keys
//
#define	K_MOUSE1		200
#define	K_MOUSE2		201
#define	K_MOUSE3		202
#define	K_MOUSE4		203
#define	K_MOUSE5		204
#define	K_MOUSE6		205
#define	K_MOUSE7		206
#define	K_MOUSE8		207

//
// joystick buttons
//
#define	K_JOY1			208
#define	K_JOY2			209
#define	K_JOY3			210
#define	K_JOY4			211

//
// aux keys are for multi-buttoned joysticks to generate so they can use
// the normal binding process
//
#define	K_AUX1			212
#define	K_AUX2			213
#define	K_AUX3			214
#define	K_AUX4			215
#define	K_AUX5			216
#define	K_AUX6			217
#define	K_AUX7			218
#define	K_AUX8			219
#define	K_AUX9			220
#define	K_AUX10			221
#define	K_AUX11			222
#define	K_AUX12			223
#define	K_AUX13			224
#define	K_AUX14			225
#define	K_AUX15			226
#define	K_AUX16			227
#define	K_AUX17			228
#define	K_AUX18			229
#define	K_AUX19			230
#define	K_AUX20			231
#define	K_AUX21			232
#define	K_AUX22			233
#define	K_AUX23			234
#define	K_AUX24			235
#define	K_AUX25			236
#define	K_AUX26			237
#define	K_AUX27			238
#define	K_AUX28			239
#define	K_AUX29			240
#define	K_AUX30			241
#define	K_AUX31			242
#define	K_AUX32			243

#define K_MWHEELDOWN	244
#define K_MWHEELUP		245

#define	MAX_KEYS		256

typedef struct {
	qboolean	down;
	int			repeats;		// if > 1, it is autorepeating
	char		*binding;
	qboolean	consolekey;		// if true, can't be rebound while in console
	qboolean	menubound;		// if true, can't be rebound while in menu
	int			keyshift;		// key to map to if shift held down in console
} qkey_t;

extern	qkey_t		keys[MAX_KEYS];

extern	int	anykeydown;

void Key_Event (int key, qboolean down, unsigned time);
void Key_Init (void);
void Key_WriteBindings (FILE *f);
void Key_SetBinding (int keynum, const char *binding);
void Key_ClearStates (void);

void Key_Message( int key );
void Key_Console( int key );
qboolean Key_IsDown( int key );
qboolean Key_GetOverstrikeMode( void );
void Key_SetOverstrikeMode( qboolean state );
