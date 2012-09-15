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
#include "../client/client.h"

#ifdef WITH_EVDEV 
 #include <fcntl.h> 
 #include <linux/input.h>
 #include <unistd.h>
 void EVDEV_IN_Commands (void);
 qboolean evdev_masked = false;
 qboolean mevdev_avail = false;
 int mevdev_fd = -1;
 cvar_t *mevdev;
#endif 

 int vid_minimized = 0;

//void HandleEvents(void);
extern int mx, my;
qboolean	mouse_avail;
int old_mouse_x, old_mouse_y;

static qboolean	mlooking;

cvar_t	*m_filter;
cvar_t	*in_mouse;
cvar_t	*in_dgamouse;
cvar_t	*m_autosens;
cvar_t	*m_accel;

void IN_MLookDown (void) 
{ 
	mlooking = true;
}

void IN_MLookUp (void) 
{
	mlooking = false;
	IN_CenterView ();
}

void IN_Init(void)
{
#ifdef WITH_EVDEV
	mevdev_avail = false;
	
	mevdev = Cvar_Get("mevdev", "/dev/input/event2", CVAR_ARCHIVE);
	if ((mevdev_fd = open(mevdev->string, O_RDONLY)) != -1) { 
		Com_Printf ( "evdev mouse enabled: %s\n", mevdev->string); 
		mevdev_avail = true; 
	} else { 
		Com_Printf ( "evdev mouse disabled: unable to open %s\n", mevdev->string);
	}
#endif

	// mouse variables
	m_filter = Cvar_Get ("m_filter", "0", 0);
	in_mouse = Cvar_Get ("in_mouse", "1", CVAR_ARCHIVE);
	in_dgamouse = Cvar_Get ("in_dgamouse", "1", CVAR_ARCHIVE);
	m_autosens				= Cvar_Get ("m_autosens",				"0",		0);
	m_accel					= Cvar_Get ("m_accel",					"0",		0);

	Cmd_AddCommand ("+mlook", IN_MLookDown);
	Cmd_AddCommand ("-mlook", IN_MLookUp);

	old_mouse_x = old_mouse_y = 0;
	mx = my = 0;  
	mouse_avail = true;	
}

void IN_Shutdown(void)
{
	if (!mouse_avail)
		return;

#ifdef WITH_EVDEV
	if (mevdev_avail) { 
		close(mevdev_fd);
		mevdev_avail = false;
	}
#endif

	IN_Activate(false);

	mouse_avail = false;

	Cmd_RemoveCommand ("+mlook");
	Cmd_RemoveCommand ("-mlook");
}


/*
===========
IN_Commands
===========
*/
void IN_Commands (void)
{
#ifdef WITH_EVDEV
	if (mevdev_avail) {
		EVDEV_IN_Commands ();
	}
#endif
}

/*
===========
IN_Move
===========
*/
void IN_Move (usercmd_t *cmd)
{
	if (!mouse_avail)
		return;

	if( cls.key_dest == key_menu ) {
		M_MouseMove( mx, my );
		mx = my = 0;
		return;
	}

	if (m_filter->integer)
	{
		mx = (mx + old_mouse_x) * 0.5f;
		my = (my + old_mouse_y) * 0.5f;
	}

	old_mouse_x = mx;
	old_mouse_y = my;

	if (m_accel->value) {
		float speed = (float)sqrt(mx * mx + my * my);
		speed = sensitivity->value + speed * m_accel->value;
		mx *= speed;
		my *= speed;
	} else {
		mx *= sensitivity->value;
		my *= sensitivity->value;
	}

	if (m_autosens->integer)
	{
		mx *= cl.refdef.fov_x/90.0;
		my *= cl.refdef.fov_y/90.0;
	}

	// add mouse X/Y movement to cmd
	if ((in_strafe.state & 1) || (lookstrafe->integer && mlooking))
		cmd->sidemove += m_side->value * mx;
	else
		cl.viewangles[YAW] -= m_yaw->value * mx;

	if ((mlooking || freelook->integer) && !(in_strafe.state & 1))
		cl.viewangles[PITCH] += m_pitch->value * my;
	else
		cmd->forwardmove -= m_forward->value * my;

	mx = my = 0;

}

void IN_Frame (void)
{
	if (!mouse_avail || vid_minimized)
		return;

	if ( !cl.refresh_prepped || cls.key_dest == key_console || cls.key_dest == key_menu)
	{
		// temporarily deactivate if in fullscreen
		if (Cvar_VariableIntValue ("vid_fullscreen") == 0)
		{
			IN_Activate(false);
			return;
		}
	}

	IN_Activate(true);

//	HandleEvents();
}

#ifdef WITH_EVDEV
void EVDEV_IN_Commands (void)
{
   static struct input_event ev[64]; 
    
   fd_set set; 
   struct timeval timeout;
   size_t bytes;
   int i;

   while (mevdev_avail) {
      //poll instead of waiting 
      timeout.tv_sec = 0; 
      timeout.tv_usec = 0; 
 
      FD_ZERO (&set); 
      FD_SET (mevdev_fd, &set); 

      if(select(FD_SETSIZE, &set, NULL, NULL, &timeout) < 1) { 
         break; 
      } 

      bytes = read(mevdev_fd, ev, sizeof(struct input_event) * 64); 
 
      if (bytes < (int) sizeof(struct input_event)) { 
         Com_Printf ( "mevdev: %i < %i\n", (int) bytes, (int) sizeof(struct input_event)); 
         break; 
      } 

		if(evdev_masked) {
			break;
		}
		for (i = 0; i < (int) (bytes / sizeof(struct input_event)); i++) { 
			switch (ev[i].type)
			{ 
            case EV_KEY: 
				switch (ev[i].code)
				{
				case BTN_MOUSE:
					Key_Event (K_MOUSE1, (int)ev[i].value != 0, Sys_Milliseconds());
					break;
				case BTN_MOUSE+1:
					Key_Event (K_MOUSE2, (int)ev[i].value != 0, Sys_Milliseconds());
					break;
				case BTN_MOUSE+2:
					Key_Event (K_MOUSE3, (int)ev[i].value != 0, Sys_Milliseconds());
					break;
				case BTN_MOUSE+3:
					Key_Event (K_MOUSE4, (int)ev[i].value != 0, Sys_Milliseconds());
					break;
				case BTN_MOUSE+4:
					Key_Event (K_MOUSE5, (int)ev[i].value != 0, Sys_Milliseconds());
					break;
				case BTN_MOUSE+5:
					Key_Event (K_MOUSE6, (int)ev[i].value != 0, Sys_Milliseconds());
					break;
				case BTN_MOUSE+6:
					Key_Event (K_MOUSE7, (int)ev[i].value != 0, Sys_Milliseconds());
					break;
			} 
			break;
            case EV_REL: 
				switch (ev[i].code) { 
				case REL_X: 
					mx += (int)ev[i].value; 
					break; 
				case REL_Y: 
					my += (int)ev[i].value; 
					break; 
				case REL_WHEEL: 
					if((int)ev[i].value == 1) { 
						Key_Event (K_MWHEELUP, true, Sys_Milliseconds());
						Key_Event (K_MWHEELUP, false, Sys_Milliseconds());
					} else if((int)ev[i].value == -1) {
						Key_Event (K_MWHEELDOWN, true, Sys_Milliseconds());
						Key_Event (K_MWHEELDOWN, false, Sys_Milliseconds());
					} 
					break;
				}
			break;
         }
      }
    }
}
#endif
