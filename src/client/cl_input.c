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
#include "cl_weapon_offsets.h"

static cvar_t *cl_nodelta;
static cvar_t *cl_maxpackets;
extern cvar_t *cl_maxfps;
extern cvar_t *cl_gun_x, *cl_gun_y, *cl_gun_z;
extern cvar_t *cl_gun_pitch, *cl_gun_yaw, *cl_gun_roll;
extern cvar_t *info_fov;


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

// Forward declarations for auto-aim functions
static void CL_UpdateAutoAim(void);
static void CL_SmoothAimToward(vec3_t target_angles, float max_adjust, int entity_num, int model_index);

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
	
	// Update auto-aim tracking if active
	CL_UpdateAutoAim();
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

// Stored default gun positions and angles
static float default_gun_x = 0;
static float default_gun_y = 0;
static float default_gun_z = 0;
static float default_gun_pitch = 0;
static float default_gun_yaw = 0;
static float default_gun_roll = 0;
static float default_fov = 0;
static qboolean gun_centered = false;
static int aim_target_entity = -1; // Currently tracked entity

// Calculate angle difference between two angles (in degrees)
static float AngleDiff(float a1, float a2)
{
	float diff = a1 - a2;
	while (diff > 180) diff -= 360;
	while (diff < -180) diff += 360;
	return diff;
}

// Update auto-aim tracking each frame
static void CL_UpdateAutoAim(void)
{
	// Auto-aim is now a one-time snap when entering aim mode
	// No continuous tracking needed
	return;
}

// Smoothly adjust view angles toward target angles
static void CL_SmoothAimToward(vec3_t target_angles, float max_adjust, int entity_num, int model_index)
{
	// Normalize target angles to [-180, 180] range to match cl.viewangles
	float target_pitch = target_angles[PITCH];
	float target_yaw = target_angles[YAW];
	const char *model_name = "";
	
	if (model_index > 0 && model_index < MAX_MODELS) {
		model_name = cl.configstrings[CS_MODELS + model_index];
	}
	
	while (target_yaw > 180) target_yaw -= 360;
	while (target_yaw < -180) target_yaw += 360;
	while (target_pitch > 180) target_pitch -= 360;
	while (target_pitch < -180) target_pitch += 360;
	
	// Use predicted_angles which includes delta_angles offset
	float pitch_diff = AngleDiff(target_pitch, cl.predicted_angles[PITCH]);
	float yaw_diff = AngleDiff(target_yaw, cl.predicted_angles[YAW]);
	float total_angle = sqrt(pitch_diff * pitch_diff + yaw_diff * yaw_diff);
	
	// Deadzone: stop adjusting when already very close to avoid shaking
	const float deadzone = 0.5f; // degrees
	if (total_angle < deadzone) {
		// Already locked on, no adjustment needed
		return;
	}
	
	Com_Printf("Tracking entity %d (model: %s): target=(%.1f, %.1f) current=(%.1f, %.1f) diff=(%.1f, %.1f) total=%.1f\n",
			entity_num, model_name,
			target_pitch, target_yaw,
			cl.predicted_angles[PITCH], cl.predicted_angles[YAW],
			pitch_diff, yaw_diff, total_angle);
	
	// Clamp adjustments to max_adjust degrees per frame
	if (pitch_diff > max_adjust) pitch_diff = max_adjust;
	else if (pitch_diff < -max_adjust) pitch_diff = -max_adjust;
	
	if (yaw_diff > max_adjust) yaw_diff = max_adjust;
	else if (yaw_diff < -max_adjust) yaw_diff = -max_adjust;
	
	// Apply smooth adjustment to viewangles (the delta will be applied by the engine)
	cl.viewangles[PITCH] += pitch_diff;
	cl.viewangles[YAW] += yaw_diff;
}

// Find the closest enemy to the crosshair and return the angles to aim at it
// Returns true if an enemy was found within the auto-aim radius
static qboolean CL_FindAutoAimTarget(vec3_t out_angles)
{
	int i;
	float best_dist = 999999.0f;
	float auto_aim_radius = 45.0f; // degrees from crosshair center
	int best_entity = -1;
	vec3_t target_angles;
	vec3_t dir;
	float dist, angle_diff;
	
	if (!cl.frame.valid)
		return false;
	
	Com_Printf("=== Auto-aim scan: %d entities ===\n", cl.frame.num_entities);
	Com_Printf("Player view angles: pitch=%.1f yaw=%.1f\n", cl.viewangles[PITCH], cl.viewangles[YAW]);
	Com_Printf("Player position: x=%.1f y=%.1f z=%.1f\n", cl.refdef.vieworg[0], cl.refdef.vieworg[1], cl.refdef.vieworg[2]);
	
	// Iterate through all entities in the current frame
	for (i = 0; i < cl.frame.num_entities; i++)
	{
		entity_state_t *ent = &cl_parse_entities[(cl.frame.parse_entities + i) & PARSE_ENTITIES_MASK];
		float distance;
		const char *model_name;
		
		// Skip the player entity
		if (ent->number == cl.playernum + 1) {
			Com_Printf("  Entity %d: SKIP (player)\n", ent->number);
			continue;
		}
		
		// Skip non-solid entities (effects, projectiles, etc)
		if (!ent->solid) {
			Com_Printf("  Entity %d: SKIP (non-solid)\n", ent->number);
			continue;
		}
		
		// Skip if no model (invisible entities)
		if (!ent->modelindex) {
			Com_Printf("  Entity %d: SKIP (no model)\n", ent->number);
			continue;
		}
		
		// Skip brush models (map geometry like doors, platforms, etc)
		// These have model names starting with '*'
		model_name = cl.configstrings[CS_MODELS + ent->modelindex];
		if (model_name && model_name[0] == '*') {
			Com_Printf("  Entity %d: SKIP (brush model: %s)\n", ent->number, model_name);
			continue;
		}
		
		// Only target monsters - skip items, barrels, gibs, etc.
		// Monster models are in "models/monsters/" directory
		if (!model_name || !strstr(model_name, "models/monsters/")) {
			Com_Printf("  Entity %d: SKIP (not a monster: %s)\n", ent->number, model_name ? model_name : "(null)");
			continue;
		}
		
		// Skip entities that are too far (beyond reasonable combat range)
		distance = Distance(ent->origin, cl.refdef.vieworg);
		if (distance > 1000.0f) {
			Com_Printf("  Entity %d (model: %s): SKIP (too far: %.1f)\n", ent->number, model_name, distance);
			continue;
		}
		
		// Calculate direction to entity
		// Add vertical offset to aim at center of entity (not feet)
		vec3_t target_pos;
		VectorCopy(ent->origin, target_pos);
		target_pos[2] += 24; // Aim at roughly chest/head height
		
		VectorSubtract(target_pos, cl.refdef.vieworg, dir);
		
		// Calculate angles to entity
		VecToAngles(dir, target_angles);
		
		Com_Printf("  Entity %d position: x=%.1f y=%.1f z=%.1f\n", ent->number, target_pos[0], target_pos[1], target_pos[2]);
		Com_Printf("  Entity %d: VecToAngles result: pitch=%.1f yaw=%.1f\n", ent->number, target_angles[PITCH], target_angles[YAW]);
		
		// Invert pitch for Quake's coordinate system
		target_angles[PITCH] = -target_angles[PITCH];
		
		Com_Printf("  Entity %d: After pitch inversion: pitch=%.1f yaw=%.1f\n", ent->number, target_angles[PITCH], target_angles[YAW]);
		Com_Printf("  Entity %d: Current view angles (raw): pitch=%.1f yaw=%.1f\n", ent->number, cl.viewangles[PITCH], cl.viewangles[YAW]);
		Com_Printf("  Entity %d: Current view angles (predicted): pitch=%.1f yaw=%.1f\n", ent->number, cl.predicted_angles[PITCH], cl.predicted_angles[YAW]);
		
		// Calculate angular distance from current crosshair position
		// Use predicted_angles which includes delta_angles offset
		float yaw_diff = fabs(AngleDiff(target_angles[YAW], cl.predicted_angles[YAW]));
		float pitch_diff = fabs(AngleDiff(target_angles[PITCH], cl.predicted_angles[PITCH]));
		angle_diff = sqrt(yaw_diff * yaw_diff + pitch_diff * pitch_diff);
		
		Com_Printf("  Entity %d (model: %s): dist=%.1f units, angle_off=%.1f deg (yaw_diff=%.1f pitch_diff=%.1f)\n", 
				ent->number, model_name, distance, angle_diff, yaw_diff, pitch_diff);
		
		// Check if within auto-aim radius and closer than previous best
		if (angle_diff < auto_aim_radius && angle_diff < best_dist)
		{
			Com_Printf("    -> NEW BEST TARGET\n");
			best_dist = angle_diff;
			best_entity = ent->number;
			VectorCopy(target_angles, out_angles);
		} else if (angle_diff >= auto_aim_radius) {
			Com_Printf("    -> outside aim radius (%.1f >= %.1f)\n", angle_diff, auto_aim_radius);
		}
	}
	
	Com_Printf("\n=== AUTO-AIM SUMMARY ===\n");
	
	if (best_entity != -1)
	{
		const char *target_model = "";
		entity_state_t *target_ent = NULL;
		float target_distance = 0;
		
		// Find the target entity to get its info
		for (i = 0; i < cl.frame.num_entities; i++)
		{
			entity_state_t *ent = &cl_parse_entities[(cl.frame.parse_entities + i) & PARSE_ENTITIES_MASK];
			if (ent->number == best_entity)
			{
				target_ent = ent;
				if (ent->modelindex > 0 && ent->modelindex < MAX_MODELS)
					target_model = cl.configstrings[CS_MODELS + ent->modelindex];
				target_distance = Distance(ent->origin, cl.refdef.vieworg);
				break;
			}
		}
		
		aim_target_entity = best_entity;
		
		Com_Printf("✓ AUTO-AIM ACTIVE\n");
		Com_Printf("  Target: Entity %d\n", best_entity);
		Com_Printf("  Model: %s\n", target_model);
		Com_Printf("  Distance: %.1f units\n", target_distance);
		Com_Printf("  Angular offset: %.1f degrees\n", best_dist);
		Com_Printf("  Within FOV: YES (< %.1f degrees)\n", auto_aim_radius);
		Com_Printf("  Status: Auto-aim will track this target\n");
		
		return true;
	}
	else
	{
		Com_Printf("✗ AUTO-AIM INACTIVE\n");
		Com_Printf("  Reason: No valid targets found within %.1f degree radius\n", auto_aim_radius);
		Com_Printf("  Common causes:\n");
		Com_Printf("    - No enemies within FOV cone\n");
		Com_Printf("    - All nearby entities are brush models (map geometry)\n");
		Com_Printf("    - Enemies too far away (> 1000 units)\n");
		Com_Printf("    - Enemies are non-solid or have no model\n");
	}
	
	aim_target_entity = -1;
	return false;
}

// Get weapon index from current weapon model path
static int CL_GetCurrentWeaponIndex(void)
{
	int gunindex;
	const char *model_path;
	
	if (!cl.frame.valid)
		return 0;
	
	// Get the current gun model index from player state
	gunindex = cl.frame.playerstate.gunindex;
	if (gunindex <= 0 || gunindex >= MAX_MODELS)
		return 0;
	
	// Get the model path from config strings
	model_path = cl.configstrings[CS_MODELS + gunindex];
	if (!model_path || !model_path[0])
		return 0;
	
	// Match model path to weapon index
	// The view models are stored like "models/weapons/v_blast/tris.md2"
	if (strstr(model_path, "v_blast"))
		return WEAP_BLASTER;
	else if (strstr(model_path, "v_shotg2"))
		return WEAP_SUPERSHOTGUN;
	else if (strstr(model_path, "v_shotg"))
		return WEAP_SHOTGUN;
	else if (strstr(model_path, "v_machn"))
		return WEAP_MACHINEGUN;
	else if (strstr(model_path, "v_chain"))
		return WEAP_CHAINGUN;
	else if (strstr(model_path, "v_handgr"))
		return WEAP_GRENADES;
	else if (strstr(model_path, "v_launch"))
		return WEAP_GRENADELAUNCHER;
	else if (strstr(model_path, "v_rocket"))
		return WEAP_ROCKETLAUNCHER;
	else if (strstr(model_path, "v_hyperb"))
		return WEAP_HYPERBLASTER;
	else if (strstr(model_path, "v_rail"))
		return WEAP_RAILGUN;
	else if (strstr(model_path, "v_bfg"))
		return WEAP_BFG;
	
	return 0;
}

void IN_CenterWeaponDown (void)
{
	int weapon_index;
	float aim_x, aim_y;
	float aim_fov;
	vec3_t auto_aim_angles;
	
	// Store current positions and angles
	default_gun_x = cl_gun_x->value;
	default_gun_y = cl_gun_y->value;
	default_gun_z = cl_gun_z->value;
	default_gun_pitch = cl_gun_pitch->value;
	default_gun_yaw = cl_gun_yaw->value;
	default_gun_roll = cl_gun_roll->value;
	default_fov = info_fov->value;
	
	// Find auto-aim target and snap to it (one-time adjustment)
	if (CL_FindAutoAimTarget(auto_aim_angles))
	{
		// Snap viewangles directly to target (instant lock)
		// Normalize target angles to match viewangles range
		float target_pitch = auto_aim_angles[PITCH];
		float target_yaw = auto_aim_angles[YAW];
		
		while (target_pitch > 180) target_pitch -= 360;
		while (target_pitch < -180) target_pitch += 360;
		while (target_yaw > 180) target_yaw -= 360;
		while (target_yaw < -180) target_yaw += 360;
		
		// Calculate the adjustment needed in viewangles space
		// predicted_angles = viewangles + delta_angles
		// So: viewangles = predicted_angles - delta_angles
		float delta_pitch = SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[PITCH]);
		float delta_yaw = SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[YAW]);
		
		// Set viewangles to achieve the target predicted_angles
		cl.viewangles[PITCH] = target_pitch - delta_pitch;
		cl.viewangles[YAW] = target_yaw - delta_yaw;
		
		Com_Printf("Auto-aim: Snapped to target entity %d\n", aim_target_entity);
	}
	
	// Clear the target so we don't track continuously
	aim_target_entity = -1;
	
	// Get weapon-specific aim offsets
	weapon_index = CL_GetCurrentWeaponIndex();
	if (weapon_index > 0 && weapon_index < MAX_WEAPON_OFFSETS)
	{
		aim_x = weapon_aim_offsets[weapon_index].x;
		aim_y = weapon_aim_offsets[weapon_index].y;
	}
	else
	{
		// Default offsets if weapon not found
		aim_x = -7.0f;
		aim_y = 0.0f;
	}
	
	// Calculate aim FOV (reduce by 15% for zoom effect)
	aim_fov = default_fov * 0.85f;
	
	// Center and straighten the weapon for aiming
	Cvar_SetValue("cl_gun_x", aim_x);
	Cvar_SetValue("cl_gun_y", aim_y);
	Cvar_SetValue("cl_gun_z", 0);
	Cvar_SetValue("cl_gun_pitch", 0);
	Cvar_SetValue("cl_gun_yaw", 0);
	Cvar_SetValue("cl_gun_roll", 0);
	Cvar_SetValue("fov", aim_fov);
	gun_centered = true;
}

void IN_CenterWeaponUp (void)
{
	// Restore original positions and angles
	if (gun_centered) {
		Cvar_SetValue("cl_gun_x", default_gun_x);
		Cvar_SetValue("cl_gun_y", default_gun_y);
		Cvar_SetValue("cl_gun_z", default_gun_z);
		Cvar_SetValue("cl_gun_pitch", default_gun_pitch);
		Cvar_SetValue("cl_gun_yaw", default_gun_yaw);
		Cvar_SetValue("cl_gun_roll", default_gun_roll);
		Cvar_SetValue("fov", default_fov);
		gun_centered = false;
		aim_target_entity = -1; // Stop tracking
	}
}

/*
============
CL_InitInput
============
*/
void CL_InitInput (void)
{
	Cmd_AddCommand ("centerview",IN_CenterView);
	Cmd_AddCommand ("+centerweapon",IN_CenterWeaponDown);
	Cmd_AddCommand ("-centerweapon",IN_CenterWeaponUp);

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

	if (cl.cinematictime > 0) {
		// DEV MOD: Do not play cinematics
		SCR_FinishCinematic ();
	}

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

