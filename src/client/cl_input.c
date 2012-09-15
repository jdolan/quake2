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
// cl.input.c  -- builds an intended movement command to send to the server

#include "client.h"

static cvar_t *cl_nodelta;
static cvar_t *cl_maxpackets;
extern cvar_t *cl_maxfps;


cvar_t	*cl_upspeed;
cvar_t	*cl_forwardspeed;
cvar_t	*cl_sidespeed;

cvar_t	*cl_yawspeed;
cvar_t	*cl_pitchspeed;

cvar_t	*cl_run;

cvar_t	*cl_anglespeedkey;

extern cvar_t	*cl_instantpacket;

#ifdef MOUSE_TEST
cvar_t	*m_filter;
cvar_t	*m_autosens;
cvar_t	*m_accel;

static qboolean	mlooking;
#endif

extern	unsigned	sys_frame_time;
static unsigned	frame_msec;
static unsigned	old_sys_frame_time;

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition


Key_Event (int key, qboolean down, unsigned time);

  +mlook src time

===============================================================================
*/


static kbutton_t	in_klook;
static kbutton_t	in_left, in_right, in_forward, in_back;
static kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t	in_strafe, in_speed, in_use, in_attack;
static kbutton_t	in_up, in_down;

static int			in_impulse;


static void KeyDown (kbutton_t *b)
{
	int		k;
	char	*c;
	
	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = -1;		// typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return;		// repeating key
	
	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else
	{
		Com_Printf ("Three keys down for a button!\n");
		return;
	}
	
	if (b->state & 1)
		return;		// still down

	// save timestamp
	c = Cmd_Argv(2);
	b->downtime = atoi(c);
	if (!b->downtime)
		b->downtime = sys_frame_time - 100;

	b->state |= 1 + 2;	// down + impulse down
}

static void KeyUp (kbutton_t *b)
{
	int		k;
	char	*c;
	unsigned	uptime;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
	{ // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4;	// impulse up
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return;		// key up without coresponding down (menu pass through)
	if (b->down[0] || b->down[1])
		return;		// some other key is still holding it down

	if (!(b->state & 1))
		return;		// still up (this should not happen)

	// save timestamp
	c = Cmd_Argv(2);
	uptime = atoi(c);
	if (uptime)
		b->msec += uptime - b->downtime;
	else
		b->msec += 10;

	b->state &= ~1;		// now up
	b->state |= 4; 		// impulse up
}

static void IN_KLookDown (void) {KeyDown(&in_klook);}
static void IN_KLookUp (void) {KeyUp(&in_klook);}
static void IN_UpDown(void) {KeyDown(&in_up);}
static void IN_UpUp(void) {KeyUp(&in_up);}
static void IN_DownDown(void) {KeyDown(&in_down);}
static void IN_DownUp(void) {KeyUp(&in_down);}
static void IN_LeftDown(void) {KeyDown(&in_left);}
static void IN_LeftUp(void) {KeyUp(&in_left);}
static void IN_RightDown(void) {KeyDown(&in_right);}
static void IN_RightUp(void) {KeyUp(&in_right);}
static void IN_ForwardDown(void) {KeyDown(&in_forward);}
static void IN_ForwardUp(void) {KeyUp(&in_forward);}
static void IN_BackDown(void) {KeyDown(&in_back);}
static void IN_BackUp(void) {KeyUp(&in_back);}
static void IN_LookupDown(void) {KeyDown(&in_lookup);}
static void IN_LookupUp(void) {KeyUp(&in_lookup);}
static void IN_LookdownDown(void) {KeyDown(&in_lookdown);}
static void IN_LookdownUp(void) {KeyUp(&in_lookdown);}
static void IN_MoveleftDown(void) {KeyDown(&in_moveleft);}
static void IN_MoveleftUp(void) {KeyUp(&in_moveleft);}
static void IN_MoverightDown(void) {KeyDown(&in_moveright);}
static void IN_MoverightUp(void) {KeyUp(&in_moveright);}

static void IN_SpeedDown(void) {KeyDown(&in_speed);}
static void IN_SpeedUp(void) {KeyUp(&in_speed);}
static void IN_StrafeDown(void) {KeyDown(&in_strafe);}
static void IN_StrafeUp(void) {KeyUp(&in_strafe);}

static void IN_AttackDown(void) {
	KeyDown(&in_attack);
    if( cl_instantpacket->integer ) {
        cl.sendPacketNow = true;
    }
}
static void IN_AttackUp(void) {KeyUp(&in_attack);}

static void IN_UseDown (void) {
	KeyDown(&in_use);
    if( cl_instantpacket->integer ) {
        cl.sendPacketNow = true;
    }
}
static void IN_UseUp (void) {KeyUp(&in_use);}

static void IN_Impulse (void) {
	in_impulse = atoi(Cmd_Argv(1));
}

#ifdef MOUSE_TEST
static void IN_MLookDown (void) {
	mlooking = true;
}

static void IN_MLookUp (void) {
	mlooking = false;
	if (!freelook->integer && lookspring->integer)
		IN_CenterView ();
}
#endif

/*
===============
CL_KeyState

Returns the fraction of the frame that the key was down
===============
*/
static float CL_KeyState (kbutton_t *key)
{
	float		val;
	int			msec;

	key->state &= 1;		// clear impulses

	msec = key->msec;
	key->msec = 0;

	if (key->state)
	{	// still down
		msec += sys_frame_time - key->downtime;
		key->downtime = sys_frame_time;
	}

	if (!frame_msec)
		return 0;

	val = (float)msec / frame_msec;
	clamp(val, 0, 1);

	return val;
}

// FIXME: always discrete?
static float CL_ImmKeyState( kbutton_t *key ) {
	if( key->state & 1 ) {
		return 1;
	}

	return 0;
}


//==========================================================================
#ifdef MOUSE_TEST
static int mouse_x, mouse_y;
static int old_mouse_x, old_mouse_y;

/*
================
CL_MouseEvent
================
*/
void CL_MouseEvent( int dx, int dy ) {
	if( cls.key_dest == key_menu ) {
		M_MouseMove( mx, my );
		return;
	}

	mouse_x += dx;
	mouse_y += dy;
}

/*
================
CL_MouseMove
================
*/
static void CL_MouseMove( usercmd_t *cmd ) {
	float	mx, my;

	if (m_filter->integer) {
		mx = ( mouse_x + old_mouse_x ) * 0.5f;
		my = ( mouse_y + old_mouse_y ) * 0.5f;
	} else {
		mx = mouse_x;
		my = mouse_y;
	}

	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;
	mouse_x = mouse_y = 0;

	if (!mx && !my)
		return;

	if (m_accel->value) {
		float speed = (float)sqrt(mx * mx + my * my);
		speed = sensitivity->value + speed * m_accel->value;
		mx *= speed;
		my *= speed;
	} else {
		mx *= sensitivity->value;
		my *= sensitivity->value;
	}

	if (m_autosens->integer) {
		mx *= cl.refdef.fov_x/90.0f;
		my *= cl.refdef.fov_y/90.0f;
	}

// add mouse X/Y movement to cmd
	if( ( in_strafe.state & 1 ) || ( lookstrafe->value && mlooking ) )
		cmd->sidemove += (int16)(m_side->value * mx);
	else
		cl.viewangles[YAW] -= m_yaw->value * mx;

	if( ( mlooking || freelook->integer ) && !( in_strafe.state & 1 ) )
		cl.viewangles[PITCH] += m_pitch->value * my;
	else
		cmd->sidemove -= (int16)(m_forward->value * my);
}
#endif
/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
/*
static void CL_AdjustAngles (void)
{
	float	speed, tspeed;
	
	if (in_speed.state & 1)
		speed = cls.frametime * cl_anglespeedkey->value;
	else
		speed = cls.frametime;

	if (!(in_strafe.state & 1))
	{
		tspeed = speed*cl_yawspeed->value;
		cl.viewangles[YAW] -= tspeed * CL_KeyState(&in_right);
		cl.viewangles[YAW] += tspeed * CL_KeyState(&in_left);
	}
	tspeed = speed*cl_pitchspeed->value;
	if (in_klook.state & 1)
	{
		cl.viewangles[PITCH] -= tspeed * CL_KeyState(&in_forward);
		cl.viewangles[PITCH] += tspeed * CL_KeyState(&in_back);
	}
	
	cl.viewangles[PITCH] -= tspeed * CL_KeyState(&in_lookup);
	cl.viewangles[PITCH] += tspeed * CL_KeyState(&in_lookdown);
}*/

/*
================
CL_BaseMove

Send the intended movement message to the server
================
*/
static void CL_BaseMove (usercmd_t *cmd)
{
	vec3_t	move = {0,0,0};

	if (in_strafe.state & 1) {
		move[1] += cl_sidespeed->value * CL_KeyState(&in_right);
		move[1] -= cl_sidespeed->value * CL_KeyState(&in_left);
	}

	move[1] += cl_sidespeed->value * CL_KeyState(&in_moveright);
	move[1] -= cl_sidespeed->value * CL_KeyState(&in_moveleft);

	move[2] += cl_upspeed->value * CL_KeyState(&in_up);
	move[2] -= cl_upspeed->value * CL_KeyState(&in_down);

	if ( !(in_klook.state & 1) ) {
		move[0] += cl_forwardspeed->value * CL_KeyState(&in_forward);
		move[0] -= cl_forwardspeed->value * CL_KeyState(&in_back);
	}

	// adjust for speed key / running
	if( ( in_speed.state & 1 ) ^ cl_run->integer ) {
		VectorScale( move, 2, move );
	}

	cmd->forwardmove += (int16)move[0];
	cmd->sidemove += (int16)move[1];
	cmd->upmove += (int16)move[2];
}

/*
================
CL_ImmBaseMove

Builds intended movement message for
local pmove sampling
================
*/
static void CL_ImmBaseMove( void )
{
	float	speed, tspeed;

	if (in_speed.state & 1)
		speed = cls.frametime * cl_anglespeedkey->value;
	else
		speed = cls.frametime;

	VectorClear( cl.move );
	if( in_strafe.state & 1 ) {
		cl.move[1] += cl_sidespeed->value * CL_ImmKeyState( &in_right );
		cl.move[1] -= cl_sidespeed->value * CL_ImmKeyState( &in_left );
	} else {
		tspeed = speed*cl_yawspeed->value;
		cl.viewangles[YAW] -= tspeed * CL_KeyState(&in_right);
		cl.viewangles[YAW] += tspeed * CL_KeyState(&in_left);
	}

	tspeed = speed*cl_pitchspeed->value;
	if (in_klook.state & 1) {
		cl.viewangles[PITCH] -= tspeed * CL_KeyState(&in_forward);
		cl.viewangles[PITCH] += tspeed * CL_KeyState(&in_back);
	} else {	
		cl.move[0] += cl_forwardspeed->value * CL_ImmKeyState(&in_forward);
		cl.move[0] -= cl_forwardspeed->value * CL_ImmKeyState(&in_back);
	}

	cl.viewangles[PITCH] -= tspeed * CL_KeyState(&in_lookup);
	cl.viewangles[PITCH] += tspeed * CL_KeyState(&in_lookdown);

	cl.move[1] += cl_sidespeed->value * CL_ImmKeyState( &in_moveright );
	cl.move[1] -= cl_sidespeed->value * CL_ImmKeyState( &in_moveleft );

	cl.move[2] += cl_upspeed->value * CL_ImmKeyState( &in_up );
	cl.move[2] -= cl_upspeed->value * CL_ImmKeyState( &in_down );

//
// adjust for speed key / running
//
	if( ( in_speed.state & 1 ) ^ cl_run->integer ) {
		VectorScale( cl.move, 2, cl.move );
	}
}

static void CL_ClampPitch (void)
{
	float	pitch;

	pitch = SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[PITCH]);
	if (pitch > 180)
		pitch -= 360;

	if (cl.viewangles[PITCH] + pitch < -360)
		cl.viewangles[PITCH] += 360; // wrapped
	if (cl.viewangles[PITCH] + pitch > 360)
		cl.viewangles[PITCH] -= 360; // wrapped

	if (cl.viewangles[PITCH] + pitch > 89)
		cl.viewangles[PITCH] = 89 - pitch;
	if (cl.viewangles[PITCH] + pitch < -89)
		cl.viewangles[PITCH] = -89 - pitch;
}


// CL_UpdateCmd
void CL_UpdateCmd( int msec )
{
	CL_ImmBaseMove();

	// allow mice or other external controllers to add to the move
	IN_Move(&cl.cmd);

	// update cmd viewangles for CL_PredictMove
	CL_ClampPitch ();

	cl.cmd.angles[0] = ANGLE2SHORT(cl.viewangles[0]);
	cl.cmd.angles[1] = ANGLE2SHORT(cl.viewangles[1]);
	cl.cmd.angles[2] = ANGLE2SHORT(cl.viewangles[2]);

	// update cmd->msec for CL_PredictMove
	cl.cmd.msec += msec;
}

// CL_FinalizeCmd
void CL_FinalizeCmd (void)
{
	frame_msec = sys_frame_time - old_sys_frame_time;
	clamp( frame_msec, 1, 200 );

	//set any button hits that occured since last frame
	if ( in_attack.state & 3 )
		cl.cmd.buttons |= BUTTON_ATTACK;
	in_attack.state &= ~2;

	if (in_use.state & 3)
		cl.cmd.buttons |= BUTTON_USE;
	in_use.state &= ~2;

	if (anykeydown && cls.key_dest == key_game)
		cl.cmd.buttons |= BUTTON_ANY;

	if (cl.cmd.msec > 250)
		cl.cmd.msec = 100;

	CL_BaseMove(&cl.cmd);

	cl.cmd.impulse = in_impulse;
	in_impulse = 0;

	// set the ambient light level at the player's current position
	cl.cmd.lightlevel = (byte)cl_lightlevel->value;

	clamp(cl.cmd.forwardmove, -300, 300);
	clamp(cl.cmd.sidemove, -300, 300);
	clamp(cl.cmd.upmove, -300, 300);

	cl.cmds[cls.netchan.outgoing_sequence & CMD_MASK] = cl.cmd;
	memset( &cl.cmd, 0, sizeof( cl.cmd ) );

	//update counter
	old_sys_frame_time = sys_frame_time;
}

void IN_CenterView (void)
{
	cl.viewangles[PITCH] = -SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[PITCH]);
}

/*
============
CL_InitInput
============
*/
void CL_InitInput (void)
{
	Cmd_AddCommand ("centerview",IN_CenterView);

	Cmd_AddCommand ("+moveup",IN_UpDown);
	Cmd_AddCommand ("-moveup",IN_UpUp);
	Cmd_AddCommand ("+movedown",IN_DownDown);
	Cmd_AddCommand ("-movedown",IN_DownUp);
	Cmd_AddCommand ("+left",IN_LeftDown);
	Cmd_AddCommand ("-left",IN_LeftUp);
	Cmd_AddCommand ("+right",IN_RightDown);
	Cmd_AddCommand ("-right",IN_RightUp);
	Cmd_AddCommand ("+forward",IN_ForwardDown);
	Cmd_AddCommand ("-forward",IN_ForwardUp);
	Cmd_AddCommand ("+back",IN_BackDown);
	Cmd_AddCommand ("-back",IN_BackUp);
	Cmd_AddCommand ("+lookup", IN_LookupDown);
	Cmd_AddCommand ("-lookup", IN_LookupUp);
	Cmd_AddCommand ("+lookdown", IN_LookdownDown);
	Cmd_AddCommand ("-lookdown", IN_LookdownUp);
	Cmd_AddCommand ("+strafe", IN_StrafeDown);
	Cmd_AddCommand ("-strafe", IN_StrafeUp);
	Cmd_AddCommand ("+moveleft", IN_MoveleftDown);
	Cmd_AddCommand ("-moveleft", IN_MoveleftUp);
	Cmd_AddCommand ("+moveright", IN_MoverightDown);
	Cmd_AddCommand ("-moveright", IN_MoverightUp);
	Cmd_AddCommand ("+speed", IN_SpeedDown);
	Cmd_AddCommand ("-speed", IN_SpeedUp);
	Cmd_AddCommand ("+attack", IN_AttackDown);
	Cmd_AddCommand ("-attack", IN_AttackUp);
	Cmd_AddCommand ("+use", IN_UseDown);
	Cmd_AddCommand ("-use", IN_UseUp);
	Cmd_AddCommand ("impulse", IN_Impulse);
	Cmd_AddCommand ("+klook", IN_KLookDown);
	Cmd_AddCommand ("-klook", IN_KLookUp);

	cl_nodelta = Cvar_Get ("cl_nodelta", "0", 0);
	cl_maxpackets = Cvar_Get( "cl_maxpackets", "0", 0 );
}

/*
=================
CL_SendCmd
=================
*/
extern cvar_t *cl_async;

void CL_SendCmd (void)
{
	sizebuf_t	buf;
	byte		data[128];
	int			i;
	usercmd_t	*cmd, *oldcmd;
	int			checksumIndex = 0;
	static unsigned int	prevTime;
	unsigned int			time;

	if (cls.state < ca_connected || cls.demoplaying)
		return;

	if ( cls.state == ca_connected)
	{
		if (cls.netchan.got_reliable || cls.netchan.message.cursize	|| curtime - cls.netchan.last_sent > 100)
			Netchan_Transmit (&cls.netchan, 0, NULL);	
		return;
	}

	// save this command off for prediction
	i = cls.netchan.outgoing_sequence & CMD_MASK;
	cmd = &cl.cmds[i];
	cl.history[i].realtime = cls.realtime;	// for netgraph ping calculation

	CL_FinalizeCmd();

	// send a userinfo update if needed
	if (userinfo_modified) {
		CL_FixUpGender();
		userinfo_modified = false;
		MSG_WriteByte (&cls.netchan.message, clc_userinfo);
		MSG_WriteString (&cls.netchan.message, Cvar_Userinfo() );
	}

	SZ_Init (&buf, data, sizeof(data));

	if (cmd->buttons && cl.cinematictime > 0 && !cl.attractloop 
		&& cls.realtime - cl.cinematictime > 1000)
	{	// skip the rest of the cinematic
		SCR_FinishCinematic ();
	}

	// begin a client move command
	MSG_WriteByte (&buf, clc_move);

	// save the position for a checksum byte
	if (cls.serverProtocol < PROTOCOL_VERSION_R1Q2) {
		checksumIndex = buf.cursize;
		MSG_WriteByte (&buf, 0);
	}

	// let the server know what the last frame we
	// got was, so the next message can be delta compressed
	if (cl_nodelta->integer || !cl.frame.valid || cls.demowaiting)
		MSG_WriteLong (&buf, -1);	// no compression
	else
		MSG_WriteLong (&buf, cl.frame.serverframe);

	// send this and the previous cmds in the message, so
	// if the last packet was dropped, it can be recovered
	cmd = &cl.cmds[(cls.netchan.outgoing_sequence-2) & CMD_MASK];
	MSG_WriteDeltaUsercmd(&buf, NULL, cmd, cls.protocolVersion);
	oldcmd = cmd;

	cmd = &cl.cmds[(cls.netchan.outgoing_sequence-1) & CMD_MASK];
	MSG_WriteDeltaUsercmd(&buf, oldcmd, cmd, cls.protocolVersion);
	oldcmd = cmd;

	cmd = &cl.cmds[cls.netchan.outgoing_sequence & CMD_MASK];
	MSG_WriteDeltaUsercmd(&buf, oldcmd, cmd, cls.protocolVersion);

	// calculate a checksum over the move commands
	if (cls.serverProtocol < PROTOCOL_VERSION_R1Q2) {
		buf.data[checksumIndex] = COM_BlockSequenceCRCByte(
			buf.data + checksumIndex + 1, buf.cursize - checksumIndex - 1,
			cls.netchan.outgoing_sequence);
	}

	cl.sendPacketNow = false;

	if(cl_maxpackets->integer && !cl_async->integer) {
		if( cl_maxpackets->integer < cl_maxfps->integer/3 )
			Cvar_SetValue( "cl_maxpackets", cl_maxfps->integer/3 );
		else if( cl_maxpackets->value > cl_maxfps->integer )
			Cvar_SetValue( "cl_maxpackets", cl_maxfps->integer );

		time = cls.realtime;
		if( prevTime > time )
			prevTime = time;

		if( !cls.netchan.message.cursize && time - prevTime < 1000 / cl_maxpackets->integer ) {
			// drop the packet, saving reliable contents
			cls.netchan.outgoing_sequence++;
			return;
		}
		prevTime = time;
	}

	i = cls.netchan.message.cursize + buf.cursize + 10;
	SCR_AddLagometerOutPacketInfo( i );
	//
	// deliver the message
	//
	Netchan_Transmit (&cls.netchan, buf.cursize, buf.data);

}

