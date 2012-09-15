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

#include "qcommon.h"

/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

static const entity_state_t	nullEntityState;
static const player_state_t	nullPlayerState;
static const usercmd_t		nullUserCmd;

//
// writing functions
//

void MSG_WriteChar (sizebuf_t *sb, int c)
{
	byte	*buf;
	
#ifdef PARANOID
	if (c < -128 || c > 127)
		Com_Error (ERR_FATAL, "MSG_WriteChar: range error");
#endif

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteByte (sizebuf_t *sb, int c)
{
	byte	*buf;
	
#ifdef PARANOID
	if (c < 0 || c > 255)
		Com_Error (ERR_FATAL, "MSG_WriteByte: range error");
#endif

	buf = SZ_GetSpace(sb, 1);
	buf[0] = c;
}

void MSG_WriteShort (sizebuf_t *sb, int c)
{
	byte	*buf;
	
#ifdef PARANOID
	if (c < ((short)0x8000) || c > (short)0x7fff)
		Com_Error (ERR_FATAL, "MSG_WriteShort: range error");
#endif

	buf = SZ_GetSpace (sb, 2);
	buf[0] = c&0xff;
	buf[1] = c>>8;
}

void MSG_WriteLong (sizebuf_t *sb, int c)
{
	byte	*buf;
	
	buf = SZ_GetSpace (sb, 4);
	buf[0] = c&0xff;
	buf[1] = (c>>8)&0xff;
	buf[2] = (c>>16)&0xff;
	buf[3] = c>>24;
}

void MSG_WriteFloat (sizebuf_t *sb, float f)
{
	union
	{
		float f1;
		int	l;
	} dat;
	
	
	dat.f1 = f;
	dat.l = LittleLong (dat.l);
	
	SZ_Write (sb, &dat.l, 4);
}

void MSG_WriteString (sizebuf_t *sb, const char *s)
{
	if (!s)
		SZ_Write (sb, "", 1);
	else
		SZ_Write (sb, s, strlen(s)+1);
}

void MSG_WriteCoord (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, (int)(f*8));
}

void MSG_WritePos (sizebuf_t *sb, const vec3_t pos)
{
	MSG_WriteShort (sb, (int)(pos[0]*8));
	MSG_WriteShort (sb, (int)(pos[1]*8));
	MSG_WriteShort (sb, (int)(pos[2]*8));
}

void MSG_WriteAngle (sizebuf_t *sb, float f)
{
	MSG_WriteByte (sb, (int)(f*256/360) & 255);
}

void MSG_WriteAngle16 (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, ANGLE2SHORT(f));
}


void MSG_WriteDeltaUsercmd (sizebuf_t *buf, const usercmd_t *from, const usercmd_t *cmd, int protocol)
{
	int		bits = 0, buttons = 0;

	if( !from ) {
		from = &nullUserCmd;
	}

	// send the movement message
	if (cmd->angles[0] != from->angles[0])
		bits |= CM_ANGLE1;
	if (cmd->angles[1] != from->angles[1])
		bits |= CM_ANGLE2;
	if (cmd->angles[2] != from->angles[2])
		bits |= CM_ANGLE3;
	if (cmd->forwardmove != from->forwardmove)
		bits |= CM_FORWARD;
	if (cmd->sidemove != from->sidemove)
		bits |= CM_SIDE;
	if (cmd->upmove != from->upmove)
		bits |= CM_UP;
	if (cmd->buttons != from->buttons) {
		bits |= CM_BUTTONS;
		buttons = cmd->buttons;
	}
	if (cmd->impulse != from->impulse)
		bits |= CM_IMPULSE;

    MSG_WriteByte (buf, bits);

	if (protocol >= PROTOCOL_VERSION_R1Q2_UCMD)
	{
		if (bits & CM_BUTTONS)
		{
			if ((bits & CM_FORWARD) && !(cmd->forwardmove % 5))
				buttons |= BUTTON_UCMD_DBLFORWARD;
			if ((bits && CM_SIDE) && !(cmd->sidemove % 5))
				buttons |= BUTTON_UCMD_DBLSIDE;
			if ((bits && CM_UP) && !(cmd->upmove % 5))
				buttons |= BUTTON_UCMD_DBLUP;

			if ((bits & CM_ANGLE1) && !(cmd->angles[0] % 64) && (abs(cmd->angles[0] / 64)) < 128)
				buttons |= BUTTON_UCMD_DBL_ANGLE1;
			if ((bits & CM_ANGLE2) && !(cmd->angles[1] % 256))
				buttons |= BUTTON_UCMD_DBL_ANGLE2;

			MSG_WriteByte (buf, buttons);
		}
	}

	if (bits & CM_ANGLE1)
	{
		if (buttons & BUTTON_UCMD_DBL_ANGLE1)
			MSG_WriteChar (buf, cmd->angles[0] / 64);
		else
			MSG_WriteShort (buf, cmd->angles[0]);
	}

	if (bits & CM_ANGLE2)
	{
		if (buttons & BUTTON_UCMD_DBL_ANGLE2)
			MSG_WriteChar (buf, cmd->angles[1] / 256);
		else
			MSG_WriteShort (buf, cmd->angles[1]);
	}

	if (bits & CM_ANGLE3)
		MSG_WriteShort (buf, cmd->angles[2]);
	
	if (bits & CM_FORWARD)
	{
		if (buttons & BUTTON_UCMD_DBLFORWARD)
			MSG_WriteChar (buf, cmd->forwardmove / 5);
		else
			MSG_WriteShort (buf, cmd->forwardmove);
	}

	if (bits & CM_SIDE)
	{
		if (buttons & BUTTON_UCMD_DBLSIDE)
			MSG_WriteChar (buf, cmd->sidemove / 5);
		else
			MSG_WriteShort (buf, cmd->sidemove);
	}

	if (bits & CM_UP)
	{
		if (buttons & BUTTON_UCMD_DBLUP)
			MSG_WriteChar (buf, cmd->upmove / 5);
		else
			MSG_WriteShort (buf, cmd->upmove);
	}

	if (protocol < PROTOCOL_VERSION_R1Q2_UCMD)
	{
 		if (bits & CM_BUTTONS)
	  		MSG_WriteByte (buf, buttons);
	}

 	if (bits & CM_IMPULSE)
	    MSG_WriteByte (buf, cmd->impulse);

    MSG_WriteByte (buf, cmd->msec);
	MSG_WriteByte (buf, cmd->lightlevel);
}


void MSG_WriteDir (sizebuf_t *sb, const vec3_t dir)
{
	MSG_WriteByte (sb, DirToByte(dir));
}

void MSG_ReadDir (sizebuf_t *sb, vec3_t dir)
{
	ByteToDir (MSG_ReadByte (sb), dir);
}


/* values transmitted over network are discrete, so
 * we use special macros to check for delta conditions
 */
#define Delta_Angle( a, b ) \
  ( ((int)((a)*256/360) & 255) != ((int)((b)*256/360) & 255) )

#define Delta_Coord( a, b ) \
  ( (int)((b)*8) != (int)((a)*8) )

#define Delta_Pos( a, b ) \
  ( (int)((b)[0]*8) != (int)((a)[0]*8) || \
    (int)((b)[1]*8) != (int)((a)[1]*8) || \
    (int)((b)[2]*8) != (int)((a)[2]*8) )

#define Delta_VecChar( a, b ) \
  ( (int)((b)[0]*4) != (int)((a)[0]*4) || \
	(int)((b)[1]*4) != (int)((a)[1]*4) || \
	(int)((b)[2]*4) != (int)((a)[2]*4) )

#define Delta_Blend( a, b ) \
  ( (int)((b)[0]*255) != (int)((a)[0]*255) || \
    (int)((b)[1]*255) != (int)((a)[1]*255) || \
    (int)((b)[2]*255) != (int)((a)[2]*255) || \
    (int)((b)[3]*255) != (int)((a)[3]*255) )

#define Delta_Angle16( a, b ) \
	( ANGLE2SHORT(b) != ANGLE2SHORT(a) )

#define Delta_VecAngle16( a, b ) \
  ( ANGLE2SHORT((b)[0]) != ANGLE2SHORT((a)[0]) || \
    ANGLE2SHORT((b)[1]) != ANGLE2SHORT((a)[1]) || \
    ANGLE2SHORT((b)[2]) != ANGLE2SHORT((a)[2]) )

#define Delta_Fov( a, b ) \
	( (int)(b) != (int)(a) )


/*
==================
MSG_WriteDeltaEntity

Writes part of a packetentities message.
Can delta from either a baseline or a previous packet_entity
==================
*/
void MSG_WriteDeltaEntity (const entity_state_t *from, const entity_state_t *to, sizebuf_t *msg, qboolean force, qboolean newentity)
{
	int		bits;

	if (!to) {
		if (!from) {
			Com_Error( ERR_DROP, "MSG_WriteDeltaEntity: NULL" );
		}
		bits = U_REMOVE;
		if( from->number >= 256 ) {
			bits |= U_NUMBER16 | U_MOREBITS1;
		}

		MSG_WriteByte( msg, bits & 255 );
		if( bits & 0x0000ff00 )
			MSG_WriteByte( msg, ( bits >> 8 ) & 255 );

		if( bits & U_NUMBER16 )
			MSG_WriteShort( msg, from->number );
		else
			MSG_WriteByte( msg, from->number );

		return; // remove entity
	}

	if( to->number < 1 || to->number >= MAX_EDICTS ) {
		Com_Error( ERR_DROP, "MSG_WriteDeltaEntity: bad entity number %i", to->number );
	}

	if( !from ) {
		from = &nullEntityState;
	}

	// send an update
	bits = 0;

	if (Delta_Coord( to->origin[0], from->origin[0] ))
		bits |= U_ORIGIN1;
	if (Delta_Coord( to->origin[1], from->origin[1] ))
		bits |= U_ORIGIN2;
	if (Delta_Coord( to->origin[2], from->origin[2] ))
		bits |= U_ORIGIN3;

	if (Delta_Angle( to->angles[0], from->angles[0] ))
		bits |= U_ANGLE1;		
	if (Delta_Angle( to->angles[1], from->angles[1] ))
		bits |= U_ANGLE2;
	if (Delta_Angle( to->angles[2], from->angles[2] ))
		bits |= U_ANGLE3;
		
	if ( to->skinnum != from->skinnum )
	{
		if ((unsigned)to->skinnum < 256)
			bits |= U_SKIN8;
		else if ((unsigned)to->skinnum < 0x10000)
			bits |= U_SKIN16;
		else
			bits |= (U_SKIN8|U_SKIN16);
	}
		
	if ( to->frame != from->frame ) {
		if (to->frame < 256)
			bits |= U_FRAME8;
		else
			bits |= U_FRAME16;
	}

	if ( to->effects != from->effects ) {
		if (to->effects < 256)
			bits |= U_EFFECTS8;
		else if (to->effects < 0x8000)
			bits |= U_EFFECTS16;
		else
			bits |= U_EFFECTS8|U_EFFECTS16;
	}
	
	if ( to->renderfx != from->renderfx ) {
		if (to->renderfx < 256)
			bits |= U_RENDERFX8;
		else if (to->renderfx < 0x8000)
			bits |= U_RENDERFX16;
		else
			bits |= U_RENDERFX8|U_RENDERFX16;
	}
	
	if ( to->solid != from->solid )
		bits |= U_SOLID;

	// event is not delta compressed, just 0 compressed
	if ( to->event  )
		bits |= U_EVENT;
	
	if ( to->modelindex != from->modelindex )
		bits |= U_MODEL;
	if ( to->modelindex2 != from->modelindex2 )
		bits |= U_MODEL2;
	if ( to->modelindex3 != from->modelindex3 )
		bits |= U_MODEL3;
	if ( to->modelindex4 != from->modelindex4 )
		bits |= U_MODEL4;

	if ( to->sound != from->sound )
		bits |= U_SOUND;

	if ((to->renderfx & RF_BEAM) ||
		(newentity && Delta_Pos(to->old_origin, from->old_origin)) )
	{
		bits |= U_OLDORIGIN;
	}

	// write the message
	if (!bits && !force)
		return;		// nothing to send!

	if (to->number >= 256)
		bits |= U_NUMBER16;		// number8 is implicit otherwise

	if (bits & 0xff000000)
		bits |= U_MOREBITS3 | U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x00ff0000)
		bits |= U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x0000ff00)
		bits |= U_MOREBITS1;

	MSG_WriteByte (msg,	bits&255 );

	if (bits & 0xff000000) {
		MSG_WriteByte (msg,	(bits>>8)&255 );
		MSG_WriteByte (msg,	(bits>>16)&255 );
		MSG_WriteByte (msg,	(bits>>24)&255 );
	}
	else if (bits & 0x00ff0000) {
		MSG_WriteByte (msg,	(bits>>8)&255 );
		MSG_WriteByte (msg,	(bits>>16)&255 );
	}
	else if (bits & 0x0000ff00) {
		MSG_WriteByte (msg,	(bits>>8)&255 );
	}

	//----------

	if (bits & U_NUMBER16)
		MSG_WriteShort (msg, to->number);
	else
		MSG_WriteByte (msg,	to->number);

	if (bits & U_MODEL)
		MSG_WriteByte (msg,	to->modelindex);
	if (bits & U_MODEL2)
		MSG_WriteByte (msg,	to->modelindex2);
	if (bits & U_MODEL3)
		MSG_WriteByte (msg,	to->modelindex3);
	if (bits & U_MODEL4)
		MSG_WriteByte (msg,	to->modelindex4);

	if (bits & U_FRAME8)
		MSG_WriteByte (msg, to->frame);
	if (bits & U_FRAME16)
		MSG_WriteShort (msg, to->frame);

	if ( (bits & (U_SKIN8|U_SKIN16)) == (U_SKIN8|U_SKIN16) )		//used for laser colors
		MSG_WriteLong (msg, to->skinnum);
	else if (bits & U_SKIN8)
		MSG_WriteByte (msg, to->skinnum);
	else if (bits & U_SKIN16)
		MSG_WriteShort (msg, to->skinnum);


	if ( (bits & (U_EFFECTS8|U_EFFECTS16)) == (U_EFFECTS8|U_EFFECTS16) )
		MSG_WriteLong (msg, to->effects);
	else if (bits & U_EFFECTS8)
		MSG_WriteByte (msg, to->effects);
	else if (bits & U_EFFECTS16)
		MSG_WriteShort (msg, to->effects);

	if ( (bits & (U_RENDERFX8|U_RENDERFX16)) == (U_RENDERFX8|U_RENDERFX16) )
		MSG_WriteLong (msg, to->renderfx);
	else if (bits & U_RENDERFX8)
		MSG_WriteByte (msg, to->renderfx);
	else if (bits & U_RENDERFX16)
		MSG_WriteShort (msg, to->renderfx);

	if (bits & U_ORIGIN1)
		MSG_WriteCoord (msg, to->origin[0]);		
	if (bits & U_ORIGIN2)
		MSG_WriteCoord (msg, to->origin[1]);
	if (bits & U_ORIGIN3)
		MSG_WriteCoord (msg, to->origin[2]);

	if (bits & U_ANGLE1)
		MSG_WriteAngle(msg, to->angles[0]);
	if (bits & U_ANGLE2)
		MSG_WriteAngle(msg, to->angles[1]);
	if (bits & U_ANGLE3)
		MSG_WriteAngle(msg, to->angles[2]);

	if (bits & U_OLDORIGIN) {
		MSG_WriteCoord (msg, to->old_origin[0]);
		MSG_WriteCoord (msg, to->old_origin[1]);
		MSG_WriteCoord (msg, to->old_origin[2]);
	}

	if (bits & U_SOUND)
		MSG_WriteByte (msg, to->sound);
	if (bits & U_EVENT)
		MSG_WriteByte (msg, to->event);
	if (bits & U_SOLID)
		MSG_WriteShort (msg, to->solid);
}

void MSG_WriteDeltaPlayerstate_Default (const player_state_t *from, const player_state_t *to, sizebuf_t *msg)
{
	int	i, pflags, statbits;

	if( !to ) {
		Com_Error( ERR_DROP, "MSG_WriteDeltaPlayerstate_Default: NULL" );
	}

	if( !from ) {
		from = &nullPlayerState;
	}

	//
	// determine what needs to be sent
	//
	pflags = 0;

	if( to->pmove.pm_type != from->pmove.pm_type )
		pflags |= PS_M_TYPE;

	if( to->pmove.origin[0] != from->pmove.origin[0] ||
		to->pmove.origin[1] != from->pmove.origin[1] ||
		to->pmove.origin[2] != from->pmove.origin[2] )
	{
		pflags |= PS_M_ORIGIN;
	}

	if( to->pmove.velocity[0] != from->pmove.velocity[0] ||
		to->pmove.velocity[1] != from->pmove.velocity[1] ||
		to->pmove.velocity[2] != from->pmove.velocity[2] )
	{
		pflags |= PS_M_VELOCITY;
	}

	if( to->pmove.pm_time != from->pmove.pm_time )
		pflags |= PS_M_TIME;

	if( to->pmove.pm_flags != from->pmove.pm_flags )
		pflags |= PS_M_FLAGS;

	if( to->pmove.gravity != from->pmove.gravity )
		pflags |= PS_M_GRAVITY;

	if( to->pmove.delta_angles[0] != from->pmove.delta_angles[0] ||
		to->pmove.delta_angles[1] != from->pmove.delta_angles[1] ||
		to->pmove.delta_angles[2] != from->pmove.delta_angles[2] )
	{
		pflags |= PS_M_DELTA_ANGLES;
	}

	if( Delta_VecChar( to->viewoffset, from->viewoffset ) ) {
		pflags |= PS_VIEWOFFSET;
	}

	if( Delta_VecAngle16( to->viewangles, from->viewangles ) ) {
		pflags |= PS_VIEWANGLES;
	}

	if( Delta_VecChar( to->kick_angles, from->kick_angles ) ) {
		pflags |= PS_KICKANGLES;
	}

	if( Delta_Blend( to->blend, from->blend ) ) {
		pflags |= PS_BLEND;
	}

	if( Delta_Fov( to->fov, from->fov ) )
		pflags |= PS_FOV;

	if( to->rdflags != from->rdflags )
		pflags |= PS_RDFLAGS;

	if( to->gunframe != from->gunframe ||
		Delta_VecChar( to->gunoffset, from->gunoffset ) ||
		Delta_VecChar( to->gunangles, from->gunangles ) )
	{
		pflags |= PS_WEAPONFRAME;
	}

	if( to->gunindex != from->gunindex )
		pflags |= PS_WEAPONINDEX;

	//
	// write it
	//
	MSG_WriteShort (msg, pflags);

	//
	// write the pmove_state_t
	//
	if (pflags & PS_M_TYPE)
		MSG_WriteByte (msg, to->pmove.pm_type);

	if (pflags & PS_M_ORIGIN) {
		MSG_WriteShort(msg, to->pmove.origin[0]);
		MSG_WriteShort(msg, to->pmove.origin[1]);
		MSG_WriteShort(msg, to->pmove.origin[2]);
	}

	if (pflags & PS_M_VELOCITY) {
		MSG_WriteShort(msg, to->pmove.velocity[0]);
		MSG_WriteShort(msg, to->pmove.velocity[1]);
		MSG_WriteShort(msg, to->pmove.velocity[2]);
	}

	if (pflags & PS_M_TIME)
		MSG_WriteByte(msg, to->pmove.pm_time);

	if (pflags & PS_M_FLAGS)
		MSG_WriteByte(msg, to->pmove.pm_flags);

	if (pflags & PS_M_GRAVITY)
		MSG_WriteShort(msg, to->pmove.gravity);

	if (pflags & PS_M_DELTA_ANGLES) {
		MSG_WriteShort(msg, to->pmove.delta_angles[0]);
		MSG_WriteShort(msg, to->pmove.delta_angles[1]);
		MSG_WriteShort(msg, to->pmove.delta_angles[2]);
	}

	//
	// write the rest of the player_state_t
	//
	if (pflags & PS_VIEWOFFSET) {
		MSG_WriteChar(msg, (int)(to->viewoffset[0]*4));
		MSG_WriteChar(msg, (int)(to->viewoffset[1]*4));
		MSG_WriteChar(msg, (int)(to->viewoffset[2]*4));
	}

	if (pflags & PS_VIEWANGLES) {
		MSG_WriteAngle16(msg, to->viewangles[0]);
		MSG_WriteAngle16(msg, to->viewangles[1]);
		MSG_WriteAngle16(msg, to->viewangles[2]);
	}

	if (pflags & PS_KICKANGLES) {
		MSG_WriteChar(msg, (int)(to->kick_angles[0]*4));
		MSG_WriteChar(msg, (int)(to->kick_angles[1]*4));
		MSG_WriteChar(msg, (int)(to->kick_angles[2]*4));
	}

	if (pflags & PS_WEAPONINDEX) {
		MSG_WriteByte(msg, to->gunindex);
	}

	if (pflags & PS_WEAPONFRAME) {
		MSG_WriteByte(msg, to->gunframe);
		MSG_WriteChar(msg, (int)(to->gunoffset[0]*4));
		MSG_WriteChar(msg, (int)(to->gunoffset[1]*4));
		MSG_WriteChar(msg, (int)(to->gunoffset[2]*4));
		MSG_WriteChar(msg, (int)(to->gunangles[0]*4));
		MSG_WriteChar(msg, (int)(to->gunangles[1]*4));
		MSG_WriteChar(msg, (int)(to->gunangles[2]*4));
	}

	if (pflags & PS_BLEND) {
		MSG_WriteByte(msg, (int)(to->blend[0]*255));
		MSG_WriteByte(msg, (int)(to->blend[1]*255));
		MSG_WriteByte(msg, (int)(to->blend[2]*255));
		MSG_WriteByte(msg, (int)(to->blend[3]*255));
	}
	if (pflags & PS_FOV)
		MSG_WriteByte (msg, (int)(to->fov));

	if (pflags & PS_RDFLAGS)
		MSG_WriteByte (msg, to->rdflags);

	// send stats
	statbits = 0;
	for (i = 0; i < MAX_STATS; i++) {
		if (to->stats[i] != from->stats[i])
			statbits |= 1<<i;
	}

	MSG_WriteLong (msg, statbits);
	for (i = 0; i < MAX_STATS; i++) {
		if (statbits & (1<<i) )
			MSG_WriteShort (msg, to->stats[i]);
	}
}

//============================================================

// reading functions

void MSG_BeginReading (sizebuf_t *msg)
{
	msg->readcount = 0;
}

// returns -1 if no more characters are available
int MSG_ReadChar (sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount+1 > msg_read->cursize)
		c = -1;
	else
		c = (signed char)msg_read->data[msg_read->readcount];
	msg_read->readcount++;
	
	return c;
}

int MSG_ReadByte (sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount+1 > msg_read->cursize)
		c = -1;
	else
		c = (unsigned char)msg_read->data[msg_read->readcount];
	msg_read->readcount++;
	
	return c;
}

int MSG_ReadShort (sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount+2 > msg_read->cursize)
		c = -1;
	else		
		c = (int16)(msg_read->data[msg_read->readcount]
		+ (msg_read->data[msg_read->readcount+1]<<8));
	
	msg_read->readcount += 2;
	
	return c;
}

int MSG_ReadLong (sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount+4 > msg_read->cursize)
		c = -1;
	else
		c = msg_read->data[msg_read->readcount]
		+ (msg_read->data[msg_read->readcount+1]<<8)
		+ (msg_read->data[msg_read->readcount+2]<<16)
		+ (msg_read->data[msg_read->readcount+3]<<24);
	
	msg_read->readcount += 4;
	
	return c;
}

float MSG_ReadFloat (sizebuf_t *msg_read)
{
	union
	{
		byte	b[4];
		float	f;
		int	l;
	} dat;
	
	if (msg_read->readcount+4 > msg_read->cursize)
		dat.f = -1;
	else
	{
		dat.b[0] =	msg_read->data[msg_read->readcount];
		dat.b[1] =	msg_read->data[msg_read->readcount+1];
		dat.b[2] =	msg_read->data[msg_read->readcount+2];
		dat.b[3] =	msg_read->data[msg_read->readcount+3];
	}
	msg_read->readcount += 4;
	
	dat.l = LittleLong (dat.l);

	return dat.f;	
}

char *MSG_ReadString (sizebuf_t *msg_read)
{
	static char	string[2048];
	int		l = 0, c;
	
	do
	{
		c = MSG_ReadByte (msg_read);
		if (c == -1 || c == 0)
			break;
		if (c == 0xFF)
			c = '.';
		string[l++] = c;
	} while (l < sizeof(string)-1);
	
	string[l] = 0;
	
	return string;
}

char *MSG_ReadStringLine (sizebuf_t *msg_read)
{
	static char	string[2048];
	int		l = 0, c;
	
	do
	{
		c = MSG_ReadByte (msg_read);
		if (c == -1 || c == 0 || c == '\n')
			break;
		if (c == 0xFF)
			c = '.';
		string[l++] = c;
	} while (l < sizeof(string)-1);
	
	string[l] = 0;
	
	return string;
}

float MSG_ReadCoord (sizebuf_t *msg_read)
{
	return MSG_ReadShort(msg_read) * 0.125f;
}

void MSG_ReadPos (sizebuf_t *msg_read, vec3_t pos)
{
	pos[0] = MSG_ReadShort(msg_read) * 0.125f;
	pos[1] = MSG_ReadShort(msg_read) * 0.125f;
	pos[2] = MSG_ReadShort(msg_read) * 0.125f;
}

float MSG_ReadAngle (sizebuf_t *msg_read)
{
	return MSG_ReadChar(msg_read) * 1.40625f;
}

float MSG_ReadAngle16 (sizebuf_t *msg_read)
{
	return SHORT2ANGLE(MSG_ReadShort(msg_read));
}

void MSG_ReadDeltaUsercmd (sizebuf_t *msg_read, const usercmd_t *from, usercmd_t *move)
{
	int bits;

	if( from ) {
		memcpy( move, from, sizeof( *move ) );
	} else {
		memset( move, 0, sizeof( *move ) );
	}

	bits = MSG_ReadByte(msg_read);
		
// read current angles
	if (bits & CM_ANGLE1)
		move->angles[0] = MSG_ReadShort(msg_read);
	if (bits & CM_ANGLE2)
		move->angles[1] = MSG_ReadShort(msg_read);
	if (bits & CM_ANGLE3)
		move->angles[2] = MSG_ReadShort(msg_read);
		
// read movement
	if (bits & CM_FORWARD)
		move->forwardmove = MSG_ReadShort(msg_read);
	if (bits & CM_SIDE)
		move->sidemove = MSG_ReadShort(msg_read);
	if (bits & CM_UP)
		move->upmove = MSG_ReadShort(msg_read);
	
// read buttons
	if (bits & CM_BUTTONS)
		move->buttons = MSG_ReadByte(msg_read);

	if (bits & CM_IMPULSE)
		move->impulse = MSG_ReadByte(msg_read);

// read time to run command
	move->msec = MSG_ReadByte(msg_read);

// read the light level
	move->lightlevel = MSG_ReadByte(msg_read);
}


void MSG_ReadData (sizebuf_t *msg_read, void *data, int len)
{
	int		i;

	for (i = 0; i < len; i++)
		((byte *)data)[i] = MSG_ReadByte (msg_read);
}

/*
==================
MSG_ParseDeltaEntity

Can go from either a baseline or a previous packet_entity
==================
*/
void MSG_ParseDeltaEntity ( sizebuf_t *msg, const entity_state_t *from, entity_state_t *to, int number, int bits, int protocol )
{
	if( !to ) {
		Com_Error( ERR_DROP, "MSG_ParseDeltaEntity: NULL" );
	}

	// set everything to the state we are delta'ing from
	if( from ) {
		memcpy( to, from, sizeof( *to ) );
		if (protocol < PROTOCOL_VERSION_R1Q2_MINIMUM)
			VectorCopy (from->origin, to->old_origin);
		else if (!(bits & U_OLDORIGIN) && !(from->renderfx & RF_BEAM))
			VectorCopy (from->origin, to->old_origin);
	} else {
		memset( to, 0, sizeof( *to ) );
		from = &nullEntityState;
	}
	
	to->number = number;
	to->event = 0;

	if ( !bits )
		return;

	if (bits & U_MODEL)
		to->modelindex = MSG_ReadByte(msg);
	if (bits & U_MODEL2)
		to->modelindex2 = MSG_ReadByte(msg);
	if (bits & U_MODEL3)
		to->modelindex3 = MSG_ReadByte(msg);
	if (bits & U_MODEL4)
		to->modelindex4 = MSG_ReadByte(msg);
		
	if (bits & U_FRAME8)
		to->frame = MSG_ReadByte(msg);
	if (bits & U_FRAME16)
		to->frame = MSG_ReadShort(msg);

	if ( (bits & (U_SKIN8|U_SKIN16)) == (U_SKIN8|U_SKIN16) )		//used for laser colors
		to->skinnum = MSG_ReadLong(msg);
	else if (bits & U_SKIN8)
		to->skinnum = MSG_ReadByte(msg);
	else if (bits & U_SKIN16)
		to->skinnum = MSG_ReadShort(msg);

	if ( (bits & (U_EFFECTS8|U_EFFECTS16)) == (U_EFFECTS8|U_EFFECTS16) )
		to->effects = MSG_ReadLong(msg);
	else if (bits & U_EFFECTS8)
		to->effects = MSG_ReadByte(msg);
	else if (bits & U_EFFECTS16)
		to->effects = MSG_ReadShort(msg);

	if ( (bits & (U_RENDERFX8|U_RENDERFX16)) == (U_RENDERFX8|U_RENDERFX16) )
		to->renderfx = MSG_ReadLong(msg);
	else if (bits & U_RENDERFX8)
		to->renderfx = MSG_ReadByte(msg);
	else if (bits & U_RENDERFX16)
		to->renderfx = MSG_ReadShort(msg);

	if (bits & U_ORIGIN1)
		to->origin[0] = MSG_ReadCoord(msg);
	if (bits & U_ORIGIN2)
		to->origin[1] = MSG_ReadCoord(msg);
	if (bits & U_ORIGIN3)
		to->origin[2] = MSG_ReadCoord(msg);
		
	if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle(msg);
	if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle(msg);
	if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle(msg);

	if (bits & U_OLDORIGIN)
		MSG_ReadPos(msg, to->old_origin);

	if (bits & U_SOUND)
		to->sound = MSG_ReadByte(msg);

	if (bits & U_EVENT)
		to->event = MSG_ReadByte(msg);

	if (bits & U_SOLID){
		if(protocol >= PROTOCOL_VERSION_R1Q2_SOLID)
			to->solid = MSG_ReadLong(msg);
		else
			to->solid = MSG_ReadShort(msg);
	}
}

/*
===================
MSG_ParseDeltaPlayerstate_Default
===================
*/
void MSG_ParseDeltaPlayerstate_Default ( sizebuf_t *msg, const player_state_t *from, player_state_t *to, int flags )
{
	int			i, statbits;

	if( !to ) {
		Com_Error( ERR_DROP, "MSG_ParseDeltaPlayerstate_Default: NULL" );
	}

	// clear to old value before delta parsing
	if( from ) {
		memcpy( to, from, sizeof( *to ) );
	} else {
		memset( to, 0, sizeof( *to ) );
	}

	//
	// parse the pmove_state_t
	//
	if( flags & PS_M_TYPE )
		to->pmove.pm_type = MSG_ReadByte(msg);

	if( flags & PS_M_ORIGIN ) {
		to->pmove.origin[0] = MSG_ReadShort(msg);
		to->pmove.origin[1] = MSG_ReadShort(msg);
		to->pmove.origin[2] = MSG_ReadShort(msg);
	}

	if( flags & PS_M_VELOCITY ) {
		to->pmove.velocity[0] = MSG_ReadShort(msg);
		to->pmove.velocity[1] = MSG_ReadShort(msg);
		to->pmove.velocity[2] = MSG_ReadShort(msg);
	}

	if( flags & PS_M_TIME )
		to->pmove.pm_time = MSG_ReadByte(msg);

	if( flags & PS_M_FLAGS )
		to->pmove.pm_flags = MSG_ReadByte(msg);

	if( flags & PS_M_GRAVITY )
		to->pmove.gravity = MSG_ReadShort(msg);

	if( flags & PS_M_DELTA_ANGLES ) {
		to->pmove.delta_angles[0] = MSG_ReadShort(msg);
		to->pmove.delta_angles[1] = MSG_ReadShort(msg);
		to->pmove.delta_angles[2] = MSG_ReadShort(msg);
	}

	//
	// parse the rest of the player_state_t
	//
	if( flags & PS_VIEWOFFSET ) {
		to->viewoffset[0] = MSG_ReadChar(msg) * 0.25f;
		to->viewoffset[1] = MSG_ReadChar(msg) * 0.25f;
		to->viewoffset[2] = MSG_ReadChar(msg) * 0.25f;
	}

	if( flags & PS_VIEWANGLES ) {
		to->viewangles[0] = MSG_ReadAngle16(msg);
		to->viewangles[1] = MSG_ReadAngle16(msg);
		to->viewangles[2] = MSG_ReadAngle16(msg);
	}

	if( flags & PS_KICKANGLES ) {
		to->kick_angles[0] = MSG_ReadChar(msg) * 0.25f;
		to->kick_angles[1] = MSG_ReadChar(msg) * 0.25f;
		to->kick_angles[2] = MSG_ReadChar(msg) * 0.25f;
	}

	if( flags & PS_WEAPONINDEX ) {
		to->gunindex = MSG_ReadByte(msg);
	}

	if( flags & PS_WEAPONFRAME ) {
		to->gunframe = MSG_ReadByte(msg);
		to->gunoffset[0] = MSG_ReadChar(msg) * 0.25f;
		to->gunoffset[1] = MSG_ReadChar(msg) * 0.25f;
		to->gunoffset[2] = MSG_ReadChar(msg) * 0.25f;
		to->gunangles[0] = MSG_ReadChar(msg) * 0.25f;
		to->gunangles[1] = MSG_ReadChar(msg) * 0.25f;
		to->gunangles[2] = MSG_ReadChar(msg) * 0.25f;
	}

	if( flags & PS_BLEND ) {
		to->blend[0] = MSG_ReadByte(msg) / 255.0f;
		to->blend[1] = MSG_ReadByte(msg) / 255.0f;
		to->blend[2] = MSG_ReadByte(msg) / 255.0f;
		to->blend[3] = MSG_ReadByte(msg) / 255.0f;
	}

	if( flags & PS_FOV )
		to->fov = (float)MSG_ReadByte(msg);

	if( flags & PS_RDFLAGS )
		to->rdflags = MSG_ReadByte(msg);

	// parse stats
	statbits = MSG_ReadLong(msg);
	if (statbits) {
		for( i = 0; i < MAX_STATS; i++ ) {
			if (statbits & ( 1 << i ))
				to->stats[i] = MSG_ReadShort(msg);
		}
	}
}


/*
===================
MSG_ParseDeltaPlayerstate_Default
===================
*/
void MSG_ParseDeltaPlayerstate_Enhanced( sizebuf_t *msg, const player_state_t *from, player_state_t *to, int flags, int extraflags )
{
	int			i, statbits;

	if( !to ) {
		Com_Error( ERR_DROP, "MSG_ParseDeltaPlayerstate_Enhanced: NULL" );
	}

	// clear to old value before delta parsing
	if( from ) {
		memcpy( to, from, sizeof( *to ) );
	} else {
		memset( to, 0, sizeof( *to ) );
	}

	//
	// parse the pmove_state_t
	//
	if( flags & PS_M_TYPE )
		to->pmove.pm_type = MSG_ReadByte(msg);

	if( flags & PS_M_ORIGIN ) {
		to->pmove.origin[0] = MSG_ReadShort(msg);
		to->pmove.origin[1] = MSG_ReadShort(msg);
	}

	if( extraflags & EPS_ORIGIN2 ) {
		to->pmove.origin[2] = MSG_ReadShort(msg);
	}

	if( flags & PS_M_VELOCITY ) {
		to->pmove.velocity[0] = MSG_ReadShort(msg);
		to->pmove.velocity[1] = MSG_ReadShort(msg);
	}

	if( extraflags & EPS_VELOCITY2 ) {
		to->pmove.velocity[2] = MSG_ReadShort(msg);
	}

	if( flags & PS_M_TIME )
		to->pmove.pm_time = MSG_ReadByte(msg);

	if( flags & PS_M_FLAGS )
		to->pmove.pm_flags = MSG_ReadByte(msg);

	if( flags & PS_M_GRAVITY )
		to->pmove.gravity = MSG_ReadShort(msg);

	if( flags & PS_M_DELTA_ANGLES ) {
		to->pmove.delta_angles[0] = MSG_ReadShort(msg);
		to->pmove.delta_angles[1] = MSG_ReadShort(msg);
		to->pmove.delta_angles[2] = MSG_ReadShort(msg);
	}

	//
	// parse the rest of the player_state_t
	//
	if( flags & PS_VIEWOFFSET ) {
		to->viewoffset[0] = MSG_ReadChar(msg) * 0.25f;
		to->viewoffset[1] = MSG_ReadChar(msg) * 0.25f;
		to->viewoffset[2] = MSG_ReadChar(msg) * 0.25f;
	}

	if( flags & PS_VIEWANGLES ) {
		to->viewangles[0] = MSG_ReadAngle16(msg);
		to->viewangles[1] = MSG_ReadAngle16(msg);
	}

	if( extraflags & EPS_VIEWANGLE2 ) {
		to->viewangles[2] = MSG_ReadAngle16(msg);
	}

	if( flags & PS_KICKANGLES ) {
		to->kick_angles[0] = MSG_ReadChar(msg) * 0.25f;
		to->kick_angles[1] = MSG_ReadChar(msg) * 0.25f;
		to->kick_angles[2] = MSG_ReadChar(msg) * 0.25f;
	}

	if( flags & PS_WEAPONINDEX ) {
		to->gunindex = MSG_ReadByte(msg);
	}

	if( flags & PS_WEAPONFRAME ) {
		to->gunframe = MSG_ReadByte(msg);
	}

	if( extraflags & EPS_GUNOFFSET ) {
		to->gunoffset[0] = MSG_ReadChar(msg) * 0.25f;
		to->gunoffset[1] = MSG_ReadChar(msg) * 0.25f;
		to->gunoffset[2] = MSG_ReadChar(msg) * 0.25f;
	}

	if( extraflags & EPS_GUNANGLES ) {
		to->gunangles[0] = MSG_ReadChar(msg) * 0.25f;
		to->gunangles[1] = MSG_ReadChar(msg) * 0.25f;
		to->gunangles[2] = MSG_ReadChar(msg) * 0.25f;
	}

	if( flags & PS_BLEND ) {
		to->blend[0] = MSG_ReadByte(msg) / 255.0f;
		to->blend[1] = MSG_ReadByte(msg) / 255.0f;
		to->blend[2] = MSG_ReadByte(msg) / 255.0f;
		to->blend[3] = MSG_ReadByte(msg) / 255.0f;
	}

	if( flags & PS_FOV )
		to->fov = (float)MSG_ReadByte(msg);

	if( flags & PS_RDFLAGS )
		to->rdflags = MSG_ReadByte(msg);

	if (flags & PS_BBOX)
		i = MSG_ReadShort(msg);

	// parse stats
	if( extraflags & EPS_STATS ) {
		statbits = MSG_ReadLong(msg);
		if (statbits) {
			for( i = 0; i < MAX_STATS; i++ ) {
				if (statbits & ( 1 << i ))
					to->stats[i] = MSG_ReadShort(msg);
			}
		}
	}
	
}

//===========================================================================

void SZ_Init (sizebuf_t *buf, byte *data, int length)
{
	memset (buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
}

void SZ_Clear (sizebuf_t *buf)
{
	buf->cursize = 0;
	buf->readcount = 0;
	buf->overflowed = false;
}

void *SZ_GetSpace (sizebuf_t *buf, int length)
{
	void	*data;
	
	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
			Com_Error (ERR_FATAL, "SZ_GetSpace: overflow without allowoverflow set (%d+%d > %d)", buf->cursize, length, buf->maxsize);
			
		if (length > buf->maxsize)
			Com_Error (ERR_FATAL, "SZ_GetSpace: %i is > full buffer size", length);

		Com_DPrintf ("SZ_GetSpace: overflowed maxsize\n");
		SZ_Clear (buf);
		buf->overflowed = true;
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;
	
	return data;
}

void SZ_Write (sizebuf_t *buf, const void *data, int length)
{
	memcpy (SZ_GetSpace(buf,length),data,length);		
}

void SZ_Print (sizebuf_t *buf, const char *data)
{
	int		len;
	
	len = strlen(data)+1;

	if (buf->cursize)
	{
		if (buf->data[buf->cursize-1])
			memcpy ((byte *)SZ_GetSpace(buf, len),data,len); // no trailing 0
		else
			memcpy ((byte *)SZ_GetSpace(buf, len-1)-1,data,len); // write over trailing 0
	}
	else
		memcpy ((byte *)SZ_GetSpace(buf, len),data,len);
}
