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
// in_win.c -- windows 95 mouse and joystick code
// 02/21/97 JCB Added extended DirectInput code to support external controllers.

#include "../client/client.h"
#include "winquake.h"

#define DIRECTINPUT_VERSION	0x0700
#include <dinput.h>

extern	unsigned	sys_msg_time;

static qboolean	input_active = false;

cvar_t	*in_mouse;

#ifdef JOYSTICK
// joystick defines and variables
// where should defines be moved?
#define JOY_ABSOLUTE_AXIS	0x00000000		// control like a joystick
#define JOY_RELATIVE_AXIS	0x00000010		// control like a mouse, spinner, trackball
#define	JOY_MAX_AXES		6				// X, Y, Z, R, U, V
#define JOY_AXIS_X			0
#define JOY_AXIS_Y			1
#define JOY_AXIS_Z			2
#define JOY_AXIS_R			3
#define JOY_AXIS_U			4
#define JOY_AXIS_V			5

enum _ControlList
{
	AxisNada = 0, AxisForward, AxisLook, AxisSide, AxisTurn, AxisUp
};

DWORD	dwAxisFlags[JOY_MAX_AXES] =
{
	JOY_RETURNX, JOY_RETURNY, JOY_RETURNZ, JOY_RETURNR, JOY_RETURNU, JOY_RETURNV
};

DWORD	dwAxisMap[JOY_MAX_AXES];
DWORD	dwControlMap[JOY_MAX_AXES];
PDWORD	pdwRawValue[JOY_MAX_AXES];

cvar_t	*in_joystick;


// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage
// or when changing from one controller to another.  this way at least something
// works.
cvar_t	*joy_name;
cvar_t	*joy_advanced;
cvar_t	*joy_advaxisx;
cvar_t	*joy_advaxisy;
cvar_t	*joy_advaxisz;
cvar_t	*joy_advaxisr;
cvar_t	*joy_advaxisu;
cvar_t	*joy_advaxisv;
cvar_t	*joy_forwardthreshold;
cvar_t	*joy_sidethreshold;
cvar_t	*joy_pitchthreshold;
cvar_t	*joy_yawthreshold;
cvar_t	*joy_forwardsensitivity;
cvar_t	*joy_sidesensitivity;
cvar_t	*joy_pitchsensitivity;
cvar_t	*joy_yawsensitivity;
cvar_t	*joy_upthreshold;
cvar_t	*joy_upsensitivity;

qboolean	joy_avail, joy_advancedinit, joy_haspov;
DWORD		joy_oldbuttonstate, joy_oldpovstate;

int			joy_id;
DWORD		joy_flags;
DWORD		joy_numbuttons;

static JOYINFOEX	ji;

// forward-referenced functions
void IN_StartupJoystick (void);
void Joy_AdvancedUpdate_f (void);
void IN_JoyMove (usercmd_t *cmd);
#endif

static qboolean	in_appactive;


/*
============================================================

  MOUSE CONTROL

============================================================
*/

// mouse variables
cvar_t	*m_filter;
cvar_t	*m_xpfix;
cvar_t	*m_autosens;
cvar_t	*m_accel;

static qboolean	mlooking;

static void IN_MLookDown (void)
{
	mlooking = true;
}

static void IN_MLookUp (void)
{
	mlooking = false;
	if (!freelook->integer && lookspring->integer)
		IN_CenterView ();
}

static int		mouse_oldbuttonstate;
static POINT	current_pos;
static int		mouse_x = 0, mouse_y = 0, old_mouse_x = 0, old_mouse_y = 0;

static qboolean	mouseactive = false;	// false when not focus app

static qboolean	restore_spi = false;
static qboolean	mouseinitialized = false;
static int originalmouseparms[3], newmouseparms[3] = {0, 0, 1}, xpmouseparms[3] = {0, 0, 0};
static qboolean	mouseparmsvalid = false;

static int	window_center_x, window_center_y;
static RECT	window_rect;



/*
=====================================================================

						DIRECT INPUT

=====================================================================
*/

#undef DEFINE_GUID

#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        EXTERN_C const GUID name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

DEFINE_GUID(GUID_SysMouse,   0x6F1D2B60,0xD5A0,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_XAxis,   0xA36D02E0,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_YAxis,   0xA36D02E1,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_ZAxis,   0xA36D02E2,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);

#define	DX_MOUSE_BUFFER_SIZE	16
#define DX_MOUSE_BUTTONS		8

#define DINPUT_BUFFERSIZE           16

//HRESULT (WINAPI *pDirectInputCreate)(HINSTANCE hinst, DWORD dwVersion,
//    REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter);
#define iDirectInputCreate(a,b,c,d)	pDirectInputCreate(a,b,c,d)
HRESULT (WINAPI *pDirectInputCreate)(HINSTANCE hinst, DWORD dwVersion,
	LPDIRECTINPUT * lplpDirectInput, LPUNKNOWN punkOuter);

cvar_t	*m_directinput;

static HINSTANCE			hInstDI  = NULL;
static LPDIRECTINPUT		g_pdi    = NULL;
static LPDIRECTINPUTDEVICE	g_pMouse = NULL;
static qboolean	dinput_acquired = false;
qboolean DIMouse = false;

typedef struct MYDATA {
	LONG  lX;                   // X axis goes here
	LONG  lY;                   // Y axis goes here
	LONG  lZ;                   // Z axis goes here
	BYTE  bButtonA;             // One button goes here
	BYTE  bButtonB;             // Another button goes here
	BYTE  bButtonC;             // Another button goes here
	BYTE  bButtonD;             // Another button goes here
	BYTE  bButtonE;             // Another button goes here
	BYTE  bButtonF;             // Another button goes here
	BYTE  bButtonG;             // Another button goes here
	BYTE  bButtonH;             // Another button goes here
} MYDATA;

static DIOBJECTDATAFORMAT rgodf[] = {
  { &GUID_XAxis,    FIELD_OFFSET(MYDATA, lX),       DIDFT_AXIS | DIDFT_ANYINSTANCE,   0,},
  { &GUID_YAxis,    FIELD_OFFSET(MYDATA, lY),       DIDFT_AXIS | DIDFT_ANYINSTANCE,   0,},
  { &GUID_ZAxis,    FIELD_OFFSET(MYDATA, lZ),       0x80000000 | DIDFT_AXIS | DIDFT_ANYINSTANCE,   0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonA), DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonB), DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonC), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonD), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonE), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonF), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonG), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonH), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
};

#define NUM_OBJECTS (sizeof(rgodf) / sizeof(rgodf[0]))

static DIDATAFORMAT	df = {
	sizeof(DIDATAFORMAT),       // this structure
	sizeof(DIOBJECTDATAFORMAT), // size of object data format
	DIDF_RELAXIS,               // absolute axis coordinates
	sizeof(MYDATA),             // device data size
	NUM_OBJECTS,                // number of objects
	rgodf,                      // and here they are
};

/*
========================
IN_InitDIMouse
========================
*/
static qboolean IN_InitDIMouse( void ) {
    HRESULT		hr;
	DIPROPDWORD	dipdw = {
		{
			sizeof(DIPROPDWORD),        // diph.dwSize
			sizeof(DIPROPHEADER),       // diph.dwHeaderSize
			0,                          // diph.dwObj
			DIPH_DEVICE,                // diph.dwHow
		},
		DX_MOUSE_BUFFER_SIZE,           // dwData
	};


	Com_Printf ("...initializing DirectInput: ");
	
	if (!hInstDI) {
		hInstDI = LoadLibrary("dinput.dll");

		if (hInstDI == NULL) {
			Com_Printf ("Couldn't load dinput.dll\n");
			return false;
		}
		pDirectInputCreate = NULL;
	}

	if (!pDirectInputCreate) {
		pDirectInputCreate = (void *)GetProcAddress(hInstDI, "DirectInputCreateA");
		if (!pDirectInputCreate) {
			Com_Printf ("Couldn't get DI proc addr\n");
			return false;
		}
	}

	// register with DirectInput and get an IDirectInput to play with
	hr = iDirectInputCreate(global_hInstance, DIRECTINPUT_VERSION, &g_pdi, NULL);
	if (FAILED(hr))
	{
		Com_Printf ("DirectInputCreate failed\n");
		return false;
	}
	
    // Obtain an interface to the system mouse device.
	hr = IDirectInput_CreateDevice(g_pdi, &GUID_SysMouse, &g_pMouse, NULL);
    if(FAILED(hr)) {
		Com_Printf ("Couldn't open DI device\n");
        return false;
    }
    
	// Set the data format to "mouse format".
	hr = IDirectInputDevice_SetDataFormat(g_pMouse, &df);
	if(FAILED(hr)) {
		Com_Printf ("Couldn't set DI mouse format\n");
        return false;
    }

    // Set the cooperativity level
    hr = IDirectInputDevice_SetCooperativeLevel(g_pMouse, cl_hwnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
	if (FAILED(hr))	{
		Com_Printf ("Couldn't set DI coop level\n");
        return false;
    }

	// Set the buffer size to DINPUT_BUFFERSIZE elements
	// The buffer size is a DWORD property associated with the device
	hr = IDirectInputDevice_SetProperty(g_pMouse, DIPROP_BUFFERSIZE, &dipdw.diph);
    if(FAILED(hr)) {
		Com_Printf ("Couldn't set buffersize\n");
        return false;
	}

	Com_Printf ("ok\n");
	DIMouse = true;
	return true;
}

static void IN_FreeDirectInput (void)
{
	if (g_pMouse) {
		IDirectInputDevice_SetCooperativeLevel( g_pMouse, cl_hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND );
		IDirectInputDevice_Release (g_pMouse);
		g_pMouse = NULL;
	}

	if(g_pdi) {
		IDirectInput_Release (g_pdi);
		g_pdi = NULL;
	}

	if( hInstDI ) {
		FreeLibrary( hInstDI );
		hInstDI = NULL;
	}

	pDirectInputCreate = NULL;
	DIMouse = false;
}

static void IN_ReadBufferedData( int *mx, int *my )
{
    DIDEVICEOBJECTDATA	od[ DX_MOUSE_BUFFER_SIZE ];  // Receives buffered data 
	DIMOUSESTATE2		state;
    DWORD				dwElements;
    DWORD				i;
    HRESULT				hr;
	int					value;
	//DWORD				nx = 0, ny = 0;

	dwElements = DX_MOUSE_BUFFER_SIZE;
	hr = IDirectInputDevice_GetDeviceData (g_pMouse,
		sizeof(DIDEVICEOBJECTDATA), od, &dwElements, 0);

	if ((hr == DIERR_INPUTLOST) || (hr == DIERR_NOTACQUIRED)) {
		IDirectInputDevice_Acquire(g_pMouse);
		return;
	}

    if( FAILED(hr) )  
        return;

    for( i = 0; i < dwElements; i++ ) 
    {
        switch (od[i].dwOfs) {
/*		case DIMOFS_X:
			nx += od[i].dwData;
			break;

		case DIMOFS_Y:
			ny += od[i].dwData;
			break;*/
        case DIMOFS_BUTTON0:
        case DIMOFS_BUTTON1:
        case DIMOFS_BUTTON2:
        case DIMOFS_BUTTON3:
        case DIMOFS_BUTTON4:
        case DIMOFS_BUTTON5:
        case DIMOFS_BUTTON6:
        case DIMOFS_BUTTON7:
			Key_Event (K_MOUSE1 + od[i].dwOfs-DIMOFS_BUTTON0, od[i].dwData & 0x80, od[i].dwTimeStamp);
			break;

        case DIMOFS_Z:
			value = (int)od[i].dwData;
			if (value > 0)
			{
				Key_Event (K_MWHEELUP, true, od[i].dwTimeStamp);
				Key_Event (K_MWHEELUP, false, od[i].dwTimeStamp);
			}
			else if (value < 0)
			{
				Key_Event (K_MWHEELDOWN, true, od[i].dwTimeStamp);
				Key_Event (K_MWHEELDOWN, false, od[i].dwTimeStamp);
			}
            break;
        }
    }

/*	if(m_directinput->integer == 2) {
		*mx = (int)nx;
		*my = (int)ny;
		return;
	}*/
	// read the raw delta counter and ignore
	// the individual sample time / values
	hr = IDirectInputDevice_GetDeviceState(g_pMouse,
			sizeof(DIMOUSESTATE2), &state);
	if ( FAILED(hr) ) {
		return;
	}
	*mx = (int)state.lX;
	*my = (int)state.lY;
}

//==============================================================================================
/*
===========
IN_ActivateMouse

Called when the window gains focus or changes in some way
===========
*/
static void IN_ActivateMouse (void)
{
	int		width, height;

	if (!mouseinitialized)
		return;
	if (!in_mouse->integer)
	{
		mouseactive = false;
		return;
	}
	if (mouseactive)
		return;

	mouseactive = true;
	if (g_pMouse)
	{
		if (FAILED(IDirectInputDevice_Acquire(g_pMouse))) {
			if ( !IN_InitDIMouse() ) {
				IN_FreeDirectInput();
				Com_Printf ("Falling back to Win32 mouse support...\n");
			}
		}
	}

	if (mouseparmsvalid)
	{
		if(m_xpfix->integer)
			restore_spi = SystemParametersInfo (SPI_SETMOUSE, 0, xpmouseparms, 0);
		else
			restore_spi = SystemParametersInfo (SPI_SETMOUSE, 0, newmouseparms, 0);
	}

	width = GetSystemMetrics (SM_CXSCREEN);
	height = GetSystemMetrics (SM_CYSCREEN);

	GetWindowRect ( cl_hwnd, &window_rect);
	if (window_rect.left < 0)
		window_rect.left = 0;
	if (window_rect.top < 0)
		window_rect.top = 0;
	if (window_rect.right >= width)
		window_rect.right = width-1;
	if (window_rect.bottom >= height-1)
		window_rect.bottom = height-1;

	window_center_x = (window_rect.right + window_rect.left)/2;
	window_center_y = (window_rect.top + window_rect.bottom)/2;

	SetCursorPos (window_center_x, window_center_y);

	SetCapture ( cl_hwnd );
	ClipCursor (&window_rect);

	while (ShowCursor (FALSE) >= 0)
		;
}


/*
===========
IN_DeactivateMouse

Called when the window loses focus
===========
*/
static void IN_DeactivateMouse (void)
{
	if (!mouseinitialized)
		return;
	if (!mouseactive)
		return;

	mouseactive = false;

	if (g_pMouse)
	{
		IDirectInputDevice_Unacquire(g_pMouse);
	}

	if (restore_spi)
		SystemParametersInfo (SPI_SETMOUSE, 0, originalmouseparms, 0);

	ClipCursor (NULL);
	ReleaseCapture ();

	while (ShowCursor (TRUE) < 0)
		;
}

/*
===========
IN_StartupMouse
===========
*/
static void IN_StartupMouse (void)
{

	if ( !in_mouse->integer ) 
		return; 

	if (m_directinput->integer)
	{
		if (!IN_InitDIMouse())
		{
			IN_FreeDirectInput();
			Com_Printf ("Falling back to Win32 mouse support...\n");
		}
	}

	mouseparmsvalid = SystemParametersInfo (SPI_GETMOUSE, 0, originalmouseparms, 0);
	mouseinitialized = true;
}

static void IN_Restart_f(void)
{
	if (!input_active)
		return;

	IN_Shutdown ();

	IN_StartupMouse ();
}

/*
===========
IN_MouseEvent
===========
*/
void IN_MouseEvent (int mstate, int buttons)
{
	int		i;

	if (!mouseinitialized)
		return;

// perform button actions
	for (i=0 ; i<buttons ; i++)
	{
		if ( (mstate & (1<<i)) && !(mouse_oldbuttonstate & (1<<i)) )
			Key_Event (K_MOUSE1 + i, true, sys_msg_time);

		if ( !(mstate & (1<<i)) && (mouse_oldbuttonstate & (1<<i)) )
				Key_Event (K_MOUSE1 + i, false, sys_msg_time);
	}	
		
	mouse_oldbuttonstate = mstate;
}


/*
===========
IN_MouseMove
===========
*/
void IN_MouseMove (usercmd_t *cmd)
{
	int mx, my;

	if (!mouseactive)
		return;

	if (g_pMouse) {
		mx = my = 0;
		IN_ReadBufferedData (&mx, &my);
		//if (!mx && !my)
		//	return;
	}
	else
	{
		// find mouse movement
		if (!GetCursorPos (&current_pos))
			return;

		mx = current_pos.x - window_center_x;
		my = current_pos.y - window_center_y;

		//if (!mx && !my)
		//	return;

		// force the mouse to the center, so there's room to move
		SetCursorPos (window_center_x, window_center_y);
	}

	if( cls.key_dest == key_menu ) {
		M_MouseMove( mx, my );
		return;
	}

	if (m_filter->integer)
	{
		mouse_x = (mx + old_mouse_x) * 0.5f;
		mouse_y = (my + old_mouse_y) * 0.5f;
	}
	else
	{
		mouse_x = mx;
		mouse_y = my;
	}

	old_mouse_x = mx;
	old_mouse_y = my;

	if (m_accel->value) {
		float speed = (float)sqrt(mx * mx + my * my);
		speed = sensitivity->value + speed * m_accel->value;
		mouse_x *= speed;
		mouse_y *= speed;
	} else {
		mouse_x *= sensitivity->value;
		mouse_y *= sensitivity->value;
	}

	if (m_autosens->integer)
	{
		mouse_x *= cl.refdef.fov_x/90.0f;
		mouse_y *= cl.refdef.fov_y/90.0f;
	}

// add mouse X/Y movement to cmd
	if ( (in_strafe.state & 1) || (lookstrafe->integer && mlooking ))
		cmd->sidemove += (int)(m_side->value * mouse_x);
	else
		cl.viewangles[YAW] -= m_yaw->value * mouse_x;

	if ( (mlooking || freelook->integer) && !(in_strafe.state & 1))
		cl.viewangles[PITCH] += m_pitch->value * mouse_y;
	else
		cmd->forwardmove -= (int)(m_forward->value * mouse_y);

}


/*
=========================================================================

VIEW CENTERING

=========================================================================
*/

cvar_t	*v_centermove;
cvar_t	*v_centerspeed;

static void OnChange_IN_Restart (cvar_t *self, const char *oldValue)
{
	if(!atoi(oldValue) || !self->integer)
		IN_Restart_f();
}

static void OnChange_IN_MAccel (cvar_t *self, const char *oldValue)
{
	if(self->value < 0)
		Cvar_Set(self->name, "0");
	else if(self->value > 1)
		Cvar_Set(self->name, "1" );
}

/*
===========
IN_Init
===========
*/
void IN_Init (void)
{
	// mouse variables
	m_filter				= Cvar_Get ("m_filter",					"0",		0);
    in_mouse				= Cvar_Get ("in_mouse",					"1",		0);

	m_xpfix					= Cvar_Get ("m_xpfix",					"0",		CVAR_ARCHIVE);
	m_autosens				= Cvar_Get ("m_autosens",				"0",		0);
	m_accel					= Cvar_Get ("m_accel",					"0",		0);

	m_directinput			= Cvar_Get ("m_directinput",			"0",		0);

	m_xpfix->OnChange = OnChange_IN_Restart;
	m_directinput->OnChange = OnChange_IN_Restart;
	m_accel->OnChange = OnChange_IN_MAccel;
	OnChange_IN_MAccel(m_accel, m_accel->resetString);

#ifdef JOYSTICK
	// joystick variables
	in_joystick				= Cvar_Get ("in_joystick",				"0",		CVAR_ARCHIVE);
	joy_name				= Cvar_Get ("joy_name",					"joystick",	0);
	joy_advanced			= Cvar_Get ("joy_advanced",				"0",		0);
	joy_advaxisx			= Cvar_Get ("joy_advaxisx",				"0",		0);
	joy_advaxisy			= Cvar_Get ("joy_advaxisy",				"0",		0);
	joy_advaxisz			= Cvar_Get ("joy_advaxisz",				"0",		0);
	joy_advaxisr			= Cvar_Get ("joy_advaxisr",				"0",		0);
	joy_advaxisu			= Cvar_Get ("joy_advaxisu",				"0",		0);
	joy_advaxisv			= Cvar_Get ("joy_advaxisv",				"0",		0);
	joy_forwardthreshold	= Cvar_Get ("joy_forwardthreshold",		"0.15",		0);
	joy_sidethreshold		= Cvar_Get ("joy_sidethreshold",		"0.15",		0);
	joy_upthreshold  		= Cvar_Get ("joy_upthreshold",			"0.15",		0);
	joy_pitchthreshold		= Cvar_Get ("joy_pitchthreshold",		"0.15",		0);
	joy_yawthreshold		= Cvar_Get ("joy_yawthreshold",			"0.15",		0);
	joy_forwardsensitivity	= Cvar_Get ("joy_forwardsensitivity",	"-1",		0);
	joy_sidesensitivity		= Cvar_Get ("joy_sidesensitivity",		"-1",		0);
	joy_upsensitivity		= Cvar_Get ("joy_upsensitivity",		"-1",		0);
	joy_pitchsensitivity	= Cvar_Get ("joy_pitchsensitivity",		"1",		0);
	joy_yawsensitivity		= Cvar_Get ("joy_yawsensitivity",		"-1",		0);
#endif

	// centering
	v_centermove			= Cvar_Get ("v_centermove",				"0.15",		0);
	v_centerspeed			= Cvar_Get ("v_centerspeed",			"500",		0);

	Cmd_AddCommand ("+mlook", IN_MLookDown);
	Cmd_AddCommand ("-mlook", IN_MLookUp);

	Cmd_AddCommand ("m_restart", IN_Restart_f);

#ifdef JOYSTICK
	Cmd_AddCommand ("joy_advancedupdate", Joy_AdvancedUpdate_f);
	IN_StartupJoystick ();
#endif

	IN_StartupMouse ();

	input_active = true;
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown (void)
{
	IN_DeactivateMouse ();
	IN_FreeDirectInput ();
	mouseinitialized = false;
}


/*
===========
IN_Activate

Called when the main window gains or loses focus.
The window may have been destroyed and recreated
between a deactivate and an activate.
===========
*/
void IN_Activate (qboolean active)
{
	in_appactive = active;
	//mouseactive = !active;		// force a new window check or turn off
	if (!active)
		IN_DeactivateMouse ();
}


/*
==================
IN_Frame

Called every frame, even if not generating commands
==================
*/
void IN_Frame (void)
{
	if (!mouseinitialized)
		return;

	if (!in_mouse || !in_appactive)
	{
		IN_DeactivateMouse ();
		return;
	}

	if ( !cl.refresh_prepped || cls.key_dest == key_console || cls.key_dest == key_menu)
	{
		// temporarily deactivate if in fullscreen
		if (Cvar_VariableIntValue ("vid_fullscreen") == 0)
		{
			IN_DeactivateMouse ();
			return;
		}
	}

	IN_ActivateMouse ();
}

/*
===========
IN_Move
===========
*/
void IN_Move (usercmd_t *cmd)
{
	IN_MouseMove (cmd);

#ifdef JOYSTICK
	if (ActiveApp)
		IN_JoyMove (cmd);
#endif
}

/*
=========================================================================

JOYSTICK

=========================================================================
*/

/* 
=============== 
IN_StartupJoystick 
=============== 
*/
#ifdef JOYSTICK
void IN_StartupJoystick (void) 
{ 
	int			numdevs;
	JOYCAPS		jc;
	MMRESULT	mmr = 0;
	cvar_t		*cv;

 	// assume no joystick
	joy_avail = false; 

	// abort startup if user requests no joystick
	cv = Cvar_Get ("in_initjoy", "1", CVAR_NOSET);
	if ( !cv->integer ) 
		return; 
 
	// verify joystick driver is present
	if ((numdevs = joyGetNumDevs ()) == 0)
	{
//		Com_Printf ("\njoystick not found -- driver not present\n\n");
		return;
	}

	// cycle through the joystick ids for the first valid one
	for (joy_id=0 ; joy_id<numdevs ; joy_id++)
	{
		memset (&ji, 0, sizeof(ji));
		ji.dwSize = sizeof(ji);
		ji.dwFlags = JOY_RETURNCENTERED;

		if ((mmr = joyGetPosEx (joy_id, &ji)) == JOYERR_NOERROR)
			break;
	} 

	// abort startup if we didn't find a valid joystick
	if (mmr != JOYERR_NOERROR)
	{
		Com_Printf ("\njoystick not found -- no valid joysticks (%x)\n\n", mmr);
		return;
	}

	// get the capabilities of the selected joystick
	// abort startup if command fails
	memset (&jc, 0, sizeof(jc));
	if ((mmr = joyGetDevCaps (joy_id, &jc, sizeof(jc))) != JOYERR_NOERROR)
	{
		Com_Printf ("\njoystick not found -- invalid joystick capabilities (%x)\n\n", mmr); 
		return;
	}

	// save the joystick's number of buttons and POV status
	joy_numbuttons = jc.wNumButtons;
	joy_haspov = jc.wCaps & JOYCAPS_HASPOV;

	// old button and POV states default to no buttons pressed
	joy_oldbuttonstate = joy_oldpovstate = 0;

	// mark the joystick as available and advanced initialization not completed
	// this is needed as cvars are not available during initialization

	joy_avail = true; 
	joy_advancedinit = false;

	Com_Printf ("\njoystick detected\n\n"); 
}


/*
===========
RawValuePointer
===========
*/
PDWORD RawValuePointer (int axis)
{
	switch (axis)
	{
	case JOY_AXIS_X:
		return &ji.dwXpos;
	case JOY_AXIS_Y:
		return &ji.dwYpos;
	case JOY_AXIS_Z:
		return &ji.dwZpos;
	case JOY_AXIS_R:
		return &ji.dwRpos;
	case JOY_AXIS_U:
		return &ji.dwUpos;
	case JOY_AXIS_V:
		return &ji.dwVpos;
	}

	return NULL;
}


/*
===========
Joy_AdvancedUpdate_f
===========
*/
void Joy_AdvancedUpdate_f (void)
{

	// called once by IN_ReadJoystick and by user whenever an update is needed
	// cvars are now available
	int	i;
	DWORD dwTemp;

	// initialize all the maps
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		dwAxisMap[i] = AxisNada;
		dwControlMap[i] = JOY_ABSOLUTE_AXIS;
		pdwRawValue[i] = RawValuePointer(i);
	}

	if( joy_advanced->value == 0.0)
	{
		// default joystick initialization
		// 2 axes only with joystick control
		dwAxisMap[JOY_AXIS_X] = AxisTurn;
		// dwControlMap[JOY_AXIS_X] = JOY_ABSOLUTE_AXIS;
		dwAxisMap[JOY_AXIS_Y] = AxisForward;
		// dwControlMap[JOY_AXIS_Y] = JOY_ABSOLUTE_AXIS;
	}
	else
	{
		if (strcmp (joy_name->string, "joystick") != 0)
		{
			// notify user of advanced controller
			Com_Printf ("\n%s configured\n\n", joy_name->string);
		}

		// advanced initialization here
		// data supplied by user via joy_axisn cvars
		dwTemp = (DWORD) joy_advaxisx->value;
		dwAxisMap[JOY_AXIS_X] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_X] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisy->value;
		dwAxisMap[JOY_AXIS_Y] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Y] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisz->value;
		dwAxisMap[JOY_AXIS_Z] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Z] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisr->value;
		dwAxisMap[JOY_AXIS_R] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_R] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisu->value;
		dwAxisMap[JOY_AXIS_U] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_U] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisv->value;
		dwAxisMap[JOY_AXIS_V] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_V] = dwTemp & JOY_RELATIVE_AXIS;
	}

	// compute the axes to collect from DirectInput
	joy_flags = JOY_RETURNCENTERED | JOY_RETURNBUTTONS | JOY_RETURNPOV;
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		if (dwAxisMap[i] != AxisNada)
		{
			joy_flags |= dwAxisFlags[i];
		}
	}
}

#endif
/*
===========
IN_Commands
===========
*/
void IN_Commands (void)
{
#ifdef JOYSTICK
	int		i, key_index;
	DWORD	buttonstate, povstate;

	if (!joy_avail)
	{
		return;
	}

	
	// loop through the joystick buttons
	// key a joystick event or auxillary event for higher number buttons for each state change
	buttonstate = ji.dwButtons;
	for (i=0 ; i < joy_numbuttons ; i++)
	{
		if ( (buttonstate & (1<<i)) && !(joy_oldbuttonstate & (1<<i)) )
		{
			key_index = (i < 4) ? K_JOY1 : K_AUX1;
			Key_Event (key_index + i, true, 0);
		}

		if ( !(buttonstate & (1<<i)) && (joy_oldbuttonstate & (1<<i)) )
		{
			key_index = (i < 4) ? K_JOY1 : K_AUX1;
			Key_Event (key_index + i, false, 0);
		}
	}
	joy_oldbuttonstate = buttonstate;

	if (joy_haspov)
	{
		// convert POV information into 4 bits of state information
		// this avoids any potential problems related to moving from one
		// direction to another without going through the center position
		povstate = 0;
		if(ji.dwPOV != JOY_POVCENTERED)
		{
			if (ji.dwPOV == JOY_POVFORWARD)
				povstate |= 0x01;
			if (ji.dwPOV == JOY_POVRIGHT)
				povstate |= 0x02;
			if (ji.dwPOV == JOY_POVBACKWARD)
				povstate |= 0x04;
			if (ji.dwPOV == JOY_POVLEFT)
				povstate |= 0x08;
		}
		// determine which bits have changed and key an auxillary event for each change
		for (i=0 ; i < 4 ; i++)
		{
			if ( (povstate & (1<<i)) && !(joy_oldpovstate & (1<<i)) )
			{
				Key_Event (K_AUX29 + i, true, 0);
			}

			if ( !(povstate & (1<<i)) && (joy_oldpovstate & (1<<i)) )
			{
				Key_Event (K_AUX29 + i, false, 0);
			}
		}
		joy_oldpovstate = povstate;
	}
#endif
}

#ifdef JOYSTICK
/* 
=============== 
IN_ReadJoystick
=============== 
*/  
qboolean IN_ReadJoystick (void)
{

	memset (&ji, 0, sizeof(ji));
	ji.dwSize = sizeof(ji);
	ji.dwFlags = joy_flags;

	if (joyGetPosEx (joy_id, &ji) == JOYERR_NOERROR)
	{
		return true;
	}
	else
	{
		// read error occurred
		// turning off the joystick seems too harsh for 1 read error,\
		// but what should be done?
		// Com_Printf ("IN_ReadJoystick: no response\n");
		// joy_avail = false;
		return false;
	}
}


/*
===========
IN_JoyMove
===========
*/
void IN_JoyMove (usercmd_t *cmd)
{
	float	speed, aspeed;
	float	fAxisValue;
	int		i;

	// complete initialization if first time in
	// this is needed as cvars are not available at initialization time
	if( joy_advancedinit != true )
	{
		Joy_AdvancedUpdate_f();
		joy_advancedinit = true;
	}

	// verify joystick is available and that the user wants to use it
	if (!joy_avail || !in_joystick->integer)
	{
		return; 
	}
 
	// collect the joystick data, if possible
	if (IN_ReadJoystick () != true)
	{
		return;
	}

	if ( (in_speed.state & 1) ^ cl_run->integer)
		speed = 2;
	else
		speed = 1;
	aspeed = speed * cls.frametime;

	// loop through the axes
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		// get the floating point zero-centered, potentially-inverted data for the current axis
		fAxisValue = (float) *pdwRawValue[i];
		// move centerpoint to zero
		fAxisValue -= 32768.0;

		// convert range from -32768..32767 to -1..1 
		fAxisValue /= 32768.0;

		switch (dwAxisMap[i])
		{
		case AxisForward:
			if ((joy_advanced->value == 0.0) && mlooking)
			{
				// user wants forward control to become look control
				if ((float)fabs(fAxisValue) > joy_pitchthreshold->value)
				{		
					// if mouse invert is on, invert the joystick pitch value
					// only absolute control support here (joy_advanced is false)
					if (m_pitch->value < 0.0)
					{
						cl.viewangles[PITCH] -= (fAxisValue * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
					}
					else
					{
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
					}
				}
			}
			else
			{
				// user wants forward control to be forward control
				if ((float)fabs(fAxisValue) > joy_forwardthreshold->value)
				{
					cmd->forwardmove += (fAxisValue * joy_forwardsensitivity->value) * speed * cl_forwardspeed->value;
				}
			}
			break;

		case AxisSide:
			if ((float)fabs(fAxisValue) > joy_sidethreshold->value)
			{
				cmd->sidemove += (fAxisValue * joy_sidesensitivity->value) * speed * cl_sidespeed->value;
			}
			break;

		case AxisUp:
			if ((float)fabs(fAxisValue) > joy_upthreshold->value)
			{
				cmd->upmove += (fAxisValue * joy_upsensitivity->value) * speed * cl_upspeed->value;
			}
			break;

		case AxisTurn:
			if ((in_strafe.state & 1) || (lookstrafe->value && mlooking))
			{
				// user wants turn control to become side control
				if ((float)fabs(fAxisValue) > joy_sidethreshold->value)
				{
					cmd->sidemove -= (fAxisValue * joy_sidesensitivity->value) * speed * cl_sidespeed->value;
				}
			}
			else
			{
				// user wants turn control to be turn control
				if ((float)fabs(fAxisValue) > joy_yawthreshold->value)
				{
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity->value) * aspeed * cl_yawspeed->value;
					}
					else
					{
						cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity->value) * speed * 180.0;
					}

				}
			}
			break;

		case AxisLook:
			if (mlooking)
			{
				if ((float)fabs(fAxisValue) > joy_pitchthreshold->value)
				{
					// pitch movement detected and pitch movement desired by user
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
					}
					else
					{
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value) * speed * 180.0;
					}
				}
			}
			break;

		default:
			break;
		}
	}
}
#endif
