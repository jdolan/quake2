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
// cl_ents.c -- entity parsing and management

#include "client.h"

extern	struct model_s	*cl_mod_powerscreen;

/*
=========================================================================

FRAME PARSING

=========================================================================
*/

/*
=================
CL_ParseEntityBits

Returns the entity number and the header bits
=================
*/
//int	bitcounts[32];	/// just for protocol profiling
int CL_ParseEntityBits (sizebuf_t *msg, unsigned int *bits)
{
	unsigned int b, total, number;

	total = MSG_ReadByte (msg);
	if (total & U_MOREBITS1)
	{
		b = MSG_ReadByte (msg);
		total |= b<<8;
	}
	if (total & U_MOREBITS2)
	{
		b = MSG_ReadByte (msg);
		total |= b<<16;
	}
	if (total & U_MOREBITS3)
	{
		b = MSG_ReadByte (msg);
		total |= b<<24;
	}

	// count the bits for net profiling
	/*for (i=0 ; i<32 ; i++)
		if (total&(1<<i))
			bitcounts[i]++;*/

	if (total & U_NUMBER16)
	{
		number = MSG_ReadShort (msg);
		if (number >= MAX_EDICTS)
			Com_Error (ERR_DROP, "CL_ParseEntityBits: Bad entity number %u", number);
	}
	else
	{
		number = MSG_ReadByte (msg);
	}

	*bits = total;

	return (int)number;
}

/*
==================
CL_DeltaEntity

Parses deltas from the given base and adds the resulting entity
to the current frame
==================
*/
static void CL_DeltaEntity (sizebuf_t *msg, frame_t *frame, int newnum, const entity_state_t *old, int bits)
{
	centity_t	*ent;
	entity_state_t	*state;

	ent = &cl_entities[newnum];

	state = &cl_parse_entities[cl.parse_entities & PARSE_ENTITIES_MASK];
	cl.parse_entities++;
	frame->num_entities++;

	MSG_ParseDeltaEntity(msg, old, state, newnum, bits, cls.protocolVersion);

	// some data changes will force no lerping
	if (state->modelindex != ent->current.modelindex
		|| state->modelindex2 != ent->current.modelindex2
		|| state->modelindex3 != ent->current.modelindex3
		|| state->modelindex4 != ent->current.modelindex4
		|| fabs(state->origin[0] - ent->current.origin[0]) > 512.0
		|| fabs(state->origin[1] - ent->current.origin[1]) > 512.0
		|| fabs(state->origin[2] - ent->current.origin[2]) > 512.0
		|| state->event == EV_PLAYER_TELEPORT
		|| state->event == EV_OTHER_TELEPORT
		)
	{
		ent->serverframe = -99;
	}

	if (ent->serverframe != cl.frame.serverframe - 1)
	{	// wasn't in last update, so initialize some things
		ent->trailcount = 1024;		// for diminishing rocket / grenade trails
		// duplicate the current state so lerping doesn't hurt anything
		ent->prev = *state;
		if (state->event == EV_OTHER_TELEPORT)
		{
			VectorCopy (state->origin, ent->prev.origin);
			VectorCopy (state->origin, ent->lerp_origin);
		}
		else
		{
			VectorCopy (state->old_origin, ent->prev.origin);
			VectorCopy (state->old_origin, ent->lerp_origin);
		}
	}
	else
	{	// shuffle the last state to previous
		ent->prev = ent->current;
	}

	ent->serverframe = cl.frame.serverframe;
	ent->current = *state;
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
static void CL_ParsePacketEntities (sizebuf_t *msg, const frame_t *oldframe, frame_t *newframe)
{
	int			newnum;
	unsigned int	bits;
	entity_state_t	*oldstate  = NULL;
	int			oldindex = 0, oldnum;

	newframe->parse_entities = cl.parse_entities;
	newframe->num_entities = 0;

	// delta from the entities present in oldframe
	if (!oldframe)
		oldnum = 99999;
	else
	{
		if (oldindex >= oldframe->num_entities)
			oldnum = 99999;
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & PARSE_ENTITIES_MASK];
			oldnum = oldstate->number;
		}
	}

	while (1)
	{
		newnum = CL_ParseEntityBits(msg, &bits);

		if (msg->readcount > msg->cursize)
			Com_Error (ERR_DROP,"CL_ParsePacketEntities: end of message");

		if (!newnum)
			break;

		while (oldnum < newnum)
		{	// one or more entities from the old packet are unchanged
			if (cl_shownet->integer == 3)
				Com_Printf ("   unchanged: %i\n", oldnum);
			CL_DeltaEntity(msg, newframe, oldnum, oldstate, 0);
			
			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & PARSE_ENTITIES_MASK];
				oldnum = oldstate->number;
			}
		}

		if (bits & U_REMOVE)
		{	// the entity present in oldframe is not in the current frame
			if (cl_shownet->integer == 3)
				Com_Printf ("   remove: %i\n", newnum);
			if (oldnum != newnum)
				Com_DPrintf ("U_REMOVE: oldnum != newnum\n");

			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & PARSE_ENTITIES_MASK];
				oldnum = oldstate->number;
			}
			continue;
		}

		if (oldnum == newnum)
		{	// delta from previous state
			if (cl_shownet->integer == 3)
				Com_Printf ("   delta: %i\n", newnum);
			CL_DeltaEntity(msg, newframe, newnum, oldstate, bits);

			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & PARSE_ENTITIES_MASK];
				oldnum = oldstate->number;
			}
			continue;
		}

		if (oldnum > newnum)
		{	// delta from baseline
			if (cl_shownet->integer == 3)
				Com_Printf ("   baseline: %i\n", newnum);
			CL_DeltaEntity(msg, newframe, newnum, &cl_entities[newnum].baseline, bits);
			continue;
		}

	}

	// any remaining entities in the old frame are copied over
	while (oldnum != 99999)
	{	// one or more entities from the old packet are unchanged
		if (cl_shownet->integer == 3)
			Com_Printf ("   unchanged: %i\n", oldnum);
		CL_DeltaEntity(msg, newframe, oldnum, oldstate, 0);
		
		oldindex++;

		if (oldindex >= oldframe->num_entities)
			oldnum = 99999;
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & PARSE_ENTITIES_MASK];
			oldnum = oldstate->number;
		}
	}
}



//r1: this fakes a protocol 34 packetentites write from the clients state instead
//of the server. used to write demo stream regardless of c/s protocol in use.
static void CL_DemoPacketEntities (sizebuf_t *buf, const frame_t /*@null@*/*from, const frame_t *to)
{
	const entity_state_t	*oldent = NULL;
	const entity_state_t	*newent = NULL;

	int				oldindex = 0, newindex = 0;
	int				oldnum, newnum;
	int				from_num_entities;

	//r1: pointless waste of byte since this is already inside an svc_frame
	MSG_WriteByte(buf, svc_packetentities);

	if (!from)
		from_num_entities = 0;
	else
		from_num_entities = from->num_entities;

	while (newindex < to->num_entities || oldindex < from_num_entities)
	{
		if (newindex >= to->num_entities)
			newnum = 9999;
		else
		{
			newent = &cl_parse_entities[(to->parse_entities +newindex) & PARSE_ENTITIES_MASK];
			newnum = newent->number;
		}

		if (oldindex >= from_num_entities)
			oldnum = 9999;
		else
		{
			//Com_Printf ("server: its in old entities!\n");
			oldent = &cl_parse_entities[(from->parse_entities+oldindex) & PARSE_ENTITIES_MASK];
			oldnum = oldent->number;
		}

		if (newnum == oldnum)
		{	// delta update from old position
			// because the force parm is false, this will not result
			// in any bytes being emited if the entity has not changed at all
			// note that players are always 'newentities', this updates their oldorigin always
			// and prevents warping

			MSG_WriteDeltaEntity(oldent, newent, buf, false, newent->number <= cl.maxclients);

			oldindex++;
			newindex++;
			continue;
		}
	
		if (newnum < oldnum)
		{	// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity(&cl_entities[newnum].baseline, newent, buf, true, true);
			newindex++;
			continue;
		}

		if (newnum > oldnum)
		{	// the old entity isn't present in the new message
			MSG_WriteDeltaEntity(oldent, NULL, buf, true, false);
			oldindex++;
			continue;
		}
	}

	MSG_WriteShort (buf, 0);	// end of packetentities
}


/*
==================
CL_FireEntityEvents

==================
*/
void CL_FireEntityEvents (const frame_t *frame)
{
	entity_state_t		*s1;
	int					pnum, num;

	for (pnum = 0 ; pnum<frame->num_entities ; pnum++)
	{
		num = (frame->parse_entities + pnum) & PARSE_ENTITIES_MASK;
		s1 = &cl_parse_entities[num];
		if (s1->event)
			CL_EntityEvent (s1);

		// EF_TELEPORTER acts like an event, but is not cleared each frame
		if (s1->effects & EF_TELEPORTER)
			CL_TeleporterParticles (s1);
	}
}


/*
================
CL_ParseFrame
================
*/
void CL_ParseFrame (sizebuf_t *msg, int extrabits)
{
	int			cmd, len;
	uint32		bits, extraflags = 0;
	frame_t		*oldframe;
	player_state_t	*from;

	memset (&cl.frame, 0, sizeof(cl.frame));

	if (cls.serverProtocol > PROTOCOL_VERSION_DEFAULT)
	{	
		bits = MSG_ReadLong(msg);

		cl.frame.serverframe = bits & FRAMENUM_MASK;
		bits >>= FRAMENUM_BITS;
		
		if (bits == 31)
			cl.frame.deltaframe = -1;
		else
			cl.frame.deltaframe = cl.frame.serverframe - bits;

		bits = MSG_ReadByte(msg);

		cl.surpressCount = bits & SURPRESSCOUNT_MASK;
		extraflags = ( extrabits << 4 ) | ( bits >> SURPRESSCOUNT_BITS );
	}
	else
	{
		cl.frame.serverframe = MSG_ReadLong(msg);
		cl.frame.deltaframe = MSG_ReadLong(msg);

		/* BIG HACK to let old demos continue to work */
		if (cls.serverProtocol != 26)
			cl.surpressCount = MSG_ReadByte(msg);
	}

	cl.serverTime = cl.frame.serverframe*100;

	if (cl_shownet->integer == 3)
		Com_Printf ("   frame:%i  delta:%i\n", cl.frame.serverframe, cl.frame.deltaframe);

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message 
	if (cl.frame.deltaframe > 0)
	{
		oldframe = &cl.frames[cl.frame.deltaframe & UPDATE_MASK];
		from = &oldframe->playerstate;
		if( cl.frame.serverframe == cl.frame.deltaframe ) {
            /* old buggy q2 servers still cause this on map change */
			Com_DPrintf( "Delta from current frame (should not happen).\n" );
		}
		else if (!oldframe->valid)
		{	// should never happen
			Com_Printf ("Delta from invalid frame (not supposed to happen!).\n");
		}
		if (oldframe->serverframe != cl.frame.deltaframe)
		{	// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			Com_DPrintf ("Delta frame too old.\n");
		}
		else if (cl.parse_entities - oldframe->parse_entities > MAX_PARSE_ENTITIES-128)
		{
			Com_DPrintf ("Delta parse_entities too old.\n");
		}
		else
			cl.frame.valid = true;	// valid delta parse
	}
	else
	{
		cl.frame.valid = true;		// uncompressed frame
		oldframe = NULL;
		cls.demowaiting = false;	// we can start recording now
		from = NULL;
	}

	// clamp time 
	if (cl.time > cl.serverTime)
		cl.time = cl.serverTime;
	else if (cl.time < cl.serverTime - 100)
		cl.time = cl.serverTime - 100;

	// read areabits
	len = MSG_ReadByte(msg);
	if (len) {
		if (len > sizeof(cl.frame.areabits)) {
			Com_Error(ERR_DROP, "CL_ParseFrame: invalid areabits length");
		}
		if (msg->readcount + len > msg->cursize) {
			Com_Error(ERR_DROP, "CL_ParseFrame: read past end of message");
		}
		MSG_ReadData(msg, cl.frame.areabits, len);
	}

	if( cls.serverProtocol > PROTOCOL_VERSION_DEFAULT ) {
		/* parse playerstate */
		bits = MSG_ReadShort(msg);
		MSG_ParseDeltaPlayerstate_Enhanced( msg, from, &cl.frame.playerstate, bits, extraflags );

		CL_ParsePacketEntities(msg, oldframe, &cl.frame);

		//r1: now write protocol 34 compatible delta from our localstate for demo.
		if (cls.demorecording)
		{
			sizebuf_t	fakeMsg;
			byte		fakeDemoFrame[1300];
			player_state_t		*oldstate;

			//do it
			SZ_Init (&fakeMsg, fakeDemoFrame, sizeof(fakeDemoFrame));
			fakeMsg.allowoverflow = true;

			//svc_frame header shit
			MSG_WriteByte(&fakeMsg, svc_frame);
			MSG_WriteLong(&fakeMsg, cl.frame.serverframe);
			MSG_WriteLong(&fakeMsg, cl.frame.deltaframe);
			MSG_WriteByte(&fakeMsg, cl.surpressCount);

			//areabits
			MSG_WriteByte (&fakeMsg, len);
			SZ_Write(&fakeMsg, &cl.frame.areabits, len);

			//delta ps
			MSG_WriteByte(&fakeMsg, svc_playerinfo);
			if( oldframe ) {
				oldstate = &oldframe->playerstate;	
			} else {
				oldstate = NULL;
			}
			MSG_WriteDeltaPlayerstate_Default(oldstate, &cl.frame.playerstate, &fakeMsg);

			//delta pe
			CL_DemoPacketEntities(&fakeMsg, oldframe, &cl.frame);

			//copy to demobuff
			if (!fakeMsg.overflowed)
			{
				if (fakeMsg.cursize + cl.demoBuff.cursize > cl.demoBuff.maxsize)
					Com_DPrintf ("Discarded a demoframe of %d bytes.\n", fakeMsg.cursize);
				else
					SZ_Write (&cl.demoBuff, fakeDemoFrame, fakeMsg.cursize);
			}
		}
	} else {

		// read playerinfo
		cmd = MSG_ReadByte(msg);
		if (cmd != svc_playerinfo)
			Com_Error (ERR_DROP, "CL_ParseFrame: 0x%.2x not playerinfo", cmd);

		SHOWNET(msg, svc_strings[cmd]);

		/* parse playerstate */
		bits = MSG_ReadShort(msg);
		MSG_ParseDeltaPlayerstate_Default( msg, from, &cl.frame.playerstate, bits );

		// read packet entities
		cmd = MSG_ReadByte(msg);
		if (cmd != svc_packetentities)
			Com_Error (ERR_DROP, "CL_ParseFrame: 0x%.2x not packetentities", cmd);
		SHOWNET(msg, svc_strings[cmd]);

		CL_ParsePacketEntities(msg, oldframe, &cl.frame);
	}

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.serverframe & UPDATE_MASK] = cl.frame;

	if (!cl.frame.valid) {
		return;
	}

	// getting a valid frame message ends the connection process
	if (cls.state != ca_active)
	{
		cls.state = ca_active;

		//cl.time = cl.serverTime;
		//cl.initial_server_frame = cl.frame.serverframe;
		//cl.frame.servertime = (cl.frame.serverframe - cl.initial_server_frame) * 100;

		cl.force_refdef = true;
		VectorScale(cl.frame.playerstate.pmove.origin, 0.125f, cl.predicted_origin);
		VectorCopy (cl.frame.playerstate.viewangles, cl.predicted_angles);

		SCR_ClearLagometer();
		SCR_ClearChatHUD_f();

		if (cls.disable_servercount != cl.servercount && cl.refresh_prepped)
			SCR_EndLoadingPlaque ();	// get rid of loading plaque

		if(!cl.attractloop) {
			Cmd_ExecTrigger( "#cl_enterlevel" );
			CL_StartAutoRecord();
		}

		cl.sound_prepped = true;	// can start mixing ambient sounds
	}

	// fire entity events
	CL_FireEntityEvents (&cl.frame);
	CL_CheckPredictionError ();
}

/*
==========================================================================

INTERPOLATE BETWEEN FRAMES TO GET RENDERING PARMS

==========================================================================
*/
extern int Developer_searchpath( void );
/*
===============
CL_AddPacketEntities

===============
*/
void CL_AddPacketEntities (const frame_t *frame)
{
	entity_t			ent = {0};
	const entity_state_t		*s1;
	float				autorotate;
	int					i;
	int					pnum;
	centity_t			*cent;
	int					autoanim;
	const clientinfo_t		*ci;
	unsigned int		effects, renderfx;

	// bonus items rotate at a fixed rate
	autorotate = anglemod(cl.time*0.1f);

	// brush models can auto animate their frames
	autoanim = cl.time/500;

	//memset (&ent, 0, sizeof(ent));

	for (pnum = 0 ; pnum<frame->num_entities ; pnum++)
	{
		s1 = &cl_parse_entities[(frame->parse_entities+pnum) & PARSE_ENTITIES_MASK];

		cent = &cl_entities[s1->number];

		effects = s1->effects;
		renderfx = s1->renderfx;

			// set frame
		if (effects & EF_ANIM01)
			ent.frame = autoanim & 1;
		else if (effects & EF_ANIM23)
			ent.frame = 2 + (autoanim & 1);
		else if (effects & EF_ANIM_ALL)
			ent.frame = autoanim;
		else if (effects & EF_ANIM_ALLFAST)
			ent.frame = cl.time / 100;
		else
			ent.frame = s1->frame;

		// quad and pent can do different things on client
		if (effects & EF_PENT)
		{
			effects &= ~EF_PENT;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_RED;
		}

		if (effects & EF_QUAD)
		{
			effects &= ~EF_QUAD;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_BLUE;
		}
//======
// PMM
		if (effects & EF_DOUBLE)
		{
			effects &= ~EF_DOUBLE;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_DOUBLE;
		}

		if (effects & EF_HALF_DAMAGE)
		{
			effects &= ~EF_HALF_DAMAGE;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_HALF_DAM;
		}
// pmm
//======
		ent.oldframe = cent->prev.frame;
		ent.backlerp = 1.0f - cl.lerpfrac;

		if (renderfx & (RF_FRAMELERP|RF_BEAM))
		{	// step origin discretely, because the frames
			// do the animation properly
			VectorCopy (cent->current.origin, ent.origin);
			VectorCopy (cent->current.old_origin, ent.oldorigin);
		}
/*		else if (renderfx & RF_BEAM)
		{	// interpolate start and end points for beams
			ent.oldorigin[0] = cent->prev.old_origin[0] + cl.lerpfrac * (cent->current.old_origin[0] - cent->prev.old_origin[0]);
			ent.oldorigin[1] = cent->prev.old_origin[1] + cl.lerpfrac * (cent->current.old_origin[1] - cent->prev.old_origin[1]);
			ent.oldorigin[2] = cent->prev.old_origin[2] + cl.lerpfrac * (cent->current.old_origin[2] - cent->prev.old_origin[2]);

			ent.origin[0] = cent->prev.origin[0] + cl.lerpfrac * (cent->current.origin[0] - cent->prev.origin[0]);
			ent.origin[1] = cent->prev.origin[1] + cl.lerpfrac * (cent->current.origin[1] - cent->prev.origin[1]);
			ent.origin[2] = cent->prev.origin[2] + cl.lerpfrac * (cent->current.origin[2] - cent->prev.origin[2]);
		}*/
		else
		{	// interpolate origin
			ent.origin[0] = ent.oldorigin[0] = cent->prev.origin[0] + cl.lerpfrac * (cent->current.origin[0] - cent->prev.origin[0]);
			ent.origin[1] = ent.oldorigin[1] = cent->prev.origin[1] + cl.lerpfrac * (cent->current.origin[1] - cent->prev.origin[1]);
			ent.origin[2] = ent.oldorigin[2] = cent->prev.origin[2] + cl.lerpfrac * (cent->current.origin[2] - cent->prev.origin[2]);
		}

		// create a new entity
	
		// tweak the color of beams
		if ( renderfx & RF_BEAM )
		{	// the four beam colors are encoded in 32 bits of skinnum (hack)
			ent.alpha = 0.30f;
			ent.skinnum = (s1->skinnum >> ((rand() % 4)*8)) & 0xff;
			ent.model = NULL;
		}
		else
		{
			// set skin
			if (s1->modelindex == 255)
			{	// use custom player skin
				ent.skinnum = 0;
				ci = &cl.clientinfo[s1->skinnum & 0xff];
				ent.skin = ci->skin;
				ent.model = ci->model;
				if (!ent.skin || !ent.model)
				{
					ent.skin = cl.baseclientinfo.skin;
					ent.model = cl.baseclientinfo.model;
				}

//============
//PGM
				if (renderfx & RF_USE_DISGUISE)
				{
					if(!strncmp((char *)ent.skin, "players/male", 12))
					{
						ent.skin = R_RegisterSkin ("players/male/disguise.pcx");
						ent.model = R_RegisterModel ("players/male/tris.md2");
					}
					else if(!strncmp((char *)ent.skin, "players/female", 14))
					{
						ent.skin = R_RegisterSkin ("players/female/disguise.pcx");
						ent.model = R_RegisterModel ("players/female/tris.md2");
					}
					else if(!strncmp((char *)ent.skin, "players/cyborg", 14))
					{
						ent.skin = R_RegisterSkin ("players/cyborg/disguise.pcx");
						ent.model = R_RegisterModel ("players/cyborg/tris.md2");
					}
				}
//PGM
//============
			}
			else
			{
				ent.skinnum = s1->skinnum;
				ent.skin = NULL;
				ent.model = cl.model_draw[s1->modelindex];
			}
		}

		// only used for black hole model right now, FIXME: do better
		if (renderfx & RF_TRANSLUCENT)
			ent.alpha = 0.70f;

		// render effects (fullbright, translucent, etc)
		if ((effects & EF_COLOR_SHELL))
			ent.flags = 0;	// renderfx go on color shell entity
		else
			ent.flags = renderfx;

		// calculate angles
		if (effects & EF_ROTATE) { // some bonus items auto-rotate
			VectorSet(ent.angles, 0, autorotate, 0);
		}
		// RAFAEL
		else if (effects & EF_SPINNINGLIGHTS)
		{
			vec3_t forward;
			vec3_t start;
			ent.angles[0] = 0;
			ent.angles[1] = anglemod(cl.time*0.5f) + s1->angles[1];
			ent.angles[2] = 180;

			AngleVectors (ent.angles, forward, NULL, NULL);
			VectorMA (ent.origin, 64, forward, start);
			V_AddLight (start, 100, 1, 0, 0);
		}
		else
		{	// interpolate angles
			float	a1, a2;

			for (i=0 ; i<3 ; i++)
			{
				a1 = cent->current.angles[i];
				a2 = cent->prev.angles[i];
				ent.angles[i] = LerpAngle (a2, a1, cl.lerpfrac);
			}
		}

		if (s1->number == cl.playernum+1)
		{
			ent.flags |= RF_VIEWERMODEL;	// only draw from mirrors
			// FIXME: still pass to refresh

			if (effects & EF_FLAG1)
				V_AddLight (ent.origin, 225, 1.0f, 0.1f, 0.1f);
			else if (effects & EF_FLAG2)
				V_AddLight (ent.origin, 225, 0.1f, 0.1f, 1.0f);
			else if (effects & EF_TAGTRAIL)						//PGM
				V_AddLight (ent.origin, 225, 1.0f, 1.0f, 0.0f);	//PGM
			else if (effects & EF_TRACKERTRAIL)					//PGM
				V_AddLight (ent.origin, 225, -1.0f, -1.0f, -1.0f);	//PGM

			continue;
		}

		// if set to invisible, skip
		if (!s1->modelindex)
			continue;

		if (effects & EF_BFG)
		{
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 0.30f;
		}

		// RAFAEL
		if (effects & EF_PLASMA)
		{
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 0.6f;
		}

		if (effects & EF_SPHERETRANS)
		{
			ent.flags |= RF_TRANSLUCENT;
			// PMM - *sigh*  yet more EF overloading
			if (effects & EF_TRACKERTRAIL)
				ent.alpha = 0.6f;
			else
				ent.alpha = 0.3f;
		}
//pmm

		// add to refresh list
#ifdef GL_QUAKE
		AnglesToAxis(ent.angles, ent.axis);
#endif
		V_AddEntity (&ent);


		// color shells generate a seperate entity for the main model
		if (effects & EF_COLOR_SHELL)
		{
			// PMM - at this point, all of the shells have been handled
			// if we're in the rogue pack, set up the custom mixing, otherwise just
			// keep going
			if(Developer_searchpath() == 2)
			{
				// all of the solo colors are fine.  we need to catch any of the combinations that look bad
				// (double & half) and turn them into the appropriate color, and make double/quad something special
				if (renderfx & RF_SHELL_HALF_DAM)
				{
					// ditch the half damage shell if any of red, blue, or double are on
					if (renderfx & (RF_SHELL_RED|RF_SHELL_BLUE|RF_SHELL_DOUBLE))
						renderfx &= ~RF_SHELL_HALF_DAM;
				}

				if (renderfx & RF_SHELL_DOUBLE)
				{
					// lose the yellow shell if we have a red, blue, or green shell
					if (renderfx & (RF_SHELL_RED|RF_SHELL_BLUE|RF_SHELL_GREEN))
						renderfx &= ~RF_SHELL_DOUBLE;
					// if we have a red shell, turn it to purple by adding blue
					if (renderfx & RF_SHELL_RED)
						renderfx |= RF_SHELL_BLUE;
					// if we have a blue shell (and not a red shell), turn it to cyan by adding green
					else if (renderfx & RF_SHELL_BLUE) {
						// go to green if it's on already, otherwise do cyan (flash green)
						if (renderfx & RF_SHELL_GREEN)
							renderfx &= ~RF_SHELL_BLUE;
						else
							renderfx |= RF_SHELL_GREEN;
					}
				}
			}
			ent.flags = renderfx | RF_TRANSLUCENT;
			ent.alpha = 0.30f;
			V_AddEntity (&ent);

#ifdef GL_QUAKE
			// duplicate for linked models
			if (s1->modelindex2)
			{
				if (s1->modelindex2 == 255)
				{// custom weapon
					ci = &cl.clientinfo[s1->skinnum & 0xff];
					i = (s1->skinnum >> 8); // 0 is default weapon model
					if (!cl_vwep->value || i > MAX_CLIENTWEAPONMODELS - 1)
						i = 0;
					ent.model = ci->weaponmodel[i];
					if (!ent.model) {
						if (i != 0)
							ent.model = ci->weaponmodel[0];
						if (!ent.model)
							ent.model = cl.baseclientinfo.weaponmodel[0];
					}
				}
				else
					ent.model = cl.model_draw[s1->modelindex2];

				V_AddEntity (&ent);
			}
			if (s1->modelindex3)
			{
				ent.model = cl.model_draw[s1->modelindex3];
				V_AddEntity (&ent);
			}
			if (s1->modelindex4)
			{
				ent.model = cl.model_draw[s1->modelindex4];
				V_AddEntity (&ent);
			}
#endif
		}

		ent.skin = NULL;		// never use a custom skin on others
		ent.skinnum = ent.flags = 0;
		ent.alpha = 0;

		// duplicate for linked models
		if (s1->modelindex2)
		{
			if (s1->modelindex2 == 255)
			{	// custom weapon
				ci = &cl.clientinfo[s1->skinnum & 0xff];
				i = (s1->skinnum >> 8); // 0 is default weapon model
				if (!cl_vwep->integer || i > MAX_CLIENTWEAPONMODELS - 1)
					i = 0;
				ent.model = ci->weaponmodel[i];
				if (!ent.model) {
					if (i != 0)
						ent.model = ci->weaponmodel[0];
					if (!ent.model)
						ent.model = cl.baseclientinfo.weaponmodel[0];
				}
			}
			else
				ent.model = cl.model_draw[s1->modelindex2];

			// PMM - check for the defender sphere shell .. make it translucent
			// replaces the previous version which used the high bit on modelindex2 to determine transparency
			if (!Q_stricmp (cl.configstrings[CS_MODELS+(s1->modelindex2)], "models/items/shell/tris.md2"))
			{
				ent.alpha = 0.32f;
				ent.flags = RF_TRANSLUCENT;
			}
			// pmm
			V_AddEntity (&ent);

			//PGM - make sure these get reset.
			ent.flags = 0;
			ent.alpha = 0;
			//PGM
		}
		if (s1->modelindex3)
		{
			ent.model = cl.model_draw[s1->modelindex3];
			V_AddEntity (&ent);
		}
		if (s1->modelindex4)
		{
			ent.model = cl.model_draw[s1->modelindex4];
			V_AddEntity (&ent);
		}

		if ( effects & EF_POWERSCREEN )
		{
			ent.model = cl_mod_powerscreen;
			ent.oldframe = 0;
			ent.frame = 0;
			ent.flags |= (RF_TRANSLUCENT | RF_SHELL_GREEN);
			ent.alpha = 0.30f;
			V_AddEntity (&ent);
		}

		// add automatic particle trails
		if ( (effects&~EF_ROTATE) )
		{
			if (effects & EF_ROCKET)
			{
				CL_RocketTrail (cent->lerp_origin, ent.origin, cent);
				V_AddLight (ent.origin, 200, 1, 0.3, 0);
			}
			// PGM - Do not reorder EF_BLASTER and EF_HYPERBLASTER. 
			// EF_BLASTER | EF_TRACKER is a special case for EF_BLASTER2... Cheese!
			else if (effects & EF_BLASTER)
			{
//				CL_BlasterTrail (cent->lerp_origin, ent.origin);
//PGM
				if (effects & EF_TRACKER)	// lame... problematic?
				{
					CL_BlasterTrail2 (cent->lerp_origin, ent.origin);
					V_AddLight (ent.origin, 200, 0, 1, 0);		
				}
				else
				{
					CL_BlasterTrail (cent->lerp_origin, ent.origin);
					V_AddLight (ent.origin, 200, 1, 0.5, 0);
				}
//PGM
			}
			else if (effects & EF_HYPERBLASTER)
			{
				if (effects & EF_TRACKER)						// PGM	overloaded for blaster2.
					V_AddLight (ent.origin, 200, 0, 1, 0);		// PGM
				else											// PGM
					V_AddLight (ent.origin, 200, 1, 0.5, 0);
			}
			else if (effects & EF_GIB)
			{
				CL_DiminishingTrail (cent->lerp_origin, ent.origin, cent, effects);
			}
			else if (effects & EF_GRENADE)
			{
				CL_DiminishingTrail (cent->lerp_origin, ent.origin, cent, effects);
				//V_AddLight (ent.origin, 100, 1, 1, 0);
			}
			else if (effects & EF_FLIES)
			{
				CL_FlyEffect (cent, ent.origin);
			}
			else if (effects & EF_BFG)
			{
				static const int bfg_lightramp[6] = {300, 400, 450, 300, 150, 75};

				if (effects & EF_ANIM_ALLFAST)
				{
					CL_BfgParticles (&ent);
					i = 300;
				}
				else
				{
					i = bfg_lightramp[s1->frame];
				}
				V_AddLight (ent.origin, i, 0, 1, 0);
			}
			// RAFAEL
			else if (effects & EF_TRAP)
			{
				ent.origin[2] += 32;
				CL_TrapParticles (&ent);
				i = (rand()%100) + 100;
				V_AddLight (ent.origin, i, 1.0f, 0.8f, 0.1f);
			}
			else if (effects & EF_FLAG1)
			{
				CL_FlagTrail (cent->lerp_origin, ent.origin, 242);
				V_AddLight (ent.origin, 225, 1.0f, 0.1f, 0.1f);
			}
			else if (effects & EF_FLAG2)
			{
				CL_FlagTrail (cent->lerp_origin, ent.origin, 115);
				V_AddLight (ent.origin, 225, 0.1f, 0.1f, 1.0f);
			}
//======
//ROGUE
			else if (effects & EF_TAGTRAIL)
			{
				CL_TagTrail (cent->lerp_origin, ent.origin, 220);
				V_AddLight (ent.origin, 225, 1.0f, 1.0f, 0.0f);
			}
			else if (effects & EF_TRACKERTRAIL)
			{
				if (effects & EF_TRACKER)
				{
					float intensity;

					intensity = 50 + (500 * ((float)sin(cl.time/500.0f) + 1.0f));
					// FIXME - check out this effect in rendition
#ifdef GL_QUAKE
					V_AddLight (ent.origin, intensity, -1.0f, -1.0f, -1.0f);
#else
					V_AddLight (ent.origin, -1.0f * intensity, 1.0f, 1.0f, 1.0f);
#endif
				}
				else
				{
					CL_Tracker_Shell (cent->lerp_origin);
					V_AddLight (ent.origin, 155, -1.0f, -1.0f, -1.0f);
				}
			}
			else if (effects & EF_TRACKER)
			{
				CL_TrackerTrail (cent->lerp_origin, ent.origin, 0);
				// FIXME - check out this effect in rendition
#ifdef GL_QUAKE
				V_AddLight (ent.origin, 200, -1, -1, -1);
#else
				V_AddLight (ent.origin, -200, 1, 1, 1);
#endif
			}
//ROGUE
//======
			// RAFAEL
			else if (effects & EF_GREENGIB)
			{
				CL_DiminishingTrail (cent->lerp_origin, ent.origin, cent, effects);				
			}
			// RAFAEL
			else if (effects & EF_IONRIPPER)
			{
				CL_IonripperTrail (cent->lerp_origin, ent.origin);
				V_AddLight (ent.origin, 100, 1, 0.5f, 0.5f);
			}
			// RAFAEL
			else if (effects & EF_BLUEHYPERBLASTER)
			{
				V_AddLight (ent.origin, 200, 0, 0, 1);
			}
			// RAFAEL
			else if (effects & EF_PLASMA)
			{
				if (effects & EF_ANIM_ALLFAST)
					CL_BlasterTrail (cent->lerp_origin, ent.origin);

				V_AddLight (ent.origin, 130, 1, 0.5f, 0.5f);
			}
		}

		VectorCopy (ent.origin, cent->lerp_origin);
	}
}



/*
==============
CL_AddViewWeapon
==============
*/
extern cvar_t *cl_gunalpha;
extern cvar_t *info_hand;
extern cvar_t *cl_gun_x, *cl_gun_y, *cl_gun_z;

static void CL_AddViewWeapon (const player_state_t *ps, const player_state_t *ops)
{
	entity_t	gun = {0};		// view model
	int			i;
	float		fovOffset;
#ifdef GL_QUAKE
	int pnum;
	entity_state_t *s1;
#endif

	// allow the gun to be completely removed
	if (!cl_gun->integer)
		return;

	// don't draw gun if in wide angle view
	if (ps->fov > 90) {
		if (cl_gun->integer < 2)
			return;

		fovOffset = -0.2f * ( ps->fov - 90.0f );
	} else {
		fovOffset = 0.0f;
	}

	//memset (&gun, 0, sizeof(gun));

	gun.model = cl.model_draw[ps->gunindex];
	if (!gun.model)
		return;

	// set up gun position
	for (i=0 ; i<3 ; i++) {
		gun.origin[i] = cl.refdef.vieworg[i] + ops->gunoffset[i] + cl.lerpfrac * (ps->gunoffset[i] - ops->gunoffset[i]);
		gun.angles[i] = cl.refdef.viewangles[i] + LerpAngle (ops->gunangles[i], ps->gunangles[i], cl.lerpfrac);
	}

	VectorMA( gun.origin, cl_gun_y->value, cl.v_forward, gun.origin );
	VectorMA( gun.origin, cl_gun_x->value, cl.v_right, gun.origin );
	VectorMA( gun.origin, (cl_gun_z->value+fovOffset), cl.v_up, gun.origin );

	gun.frame = ps->gunframe;
	if (gun.frame == 0)
		gun.oldframe = 0;	// just changed weapons, don't lerp from old
	else
		gun.oldframe = ops->gunframe;

	gun.flags = RF_MINLIGHT | RF_DEPTHHACK | RF_WEAPONMODEL;
	if(cl_gunalpha->value < 1.0f) {
		gun.flags |= RF_TRANSLUCENT;
		gun.alpha = cl_gunalpha->value;
	}

#ifdef GL_QUAKE
	AnglesToAxis(gun.angles, gun.axis);
	if (info_hand->integer == 1) {
		gun.flags |= RF_CULLHACK;
		VectorInverse (gun.axis[1]);
	}
#endif

	gun.backlerp = 1.0f - cl.lerpfrac;
	VectorCopy (gun.origin, gun.oldorigin);	// don't lerp at all
	V_AddEntity (&gun);

#ifdef GL_QUAKE
	for (pnum = 0 ; pnum<cl.frame.num_entities ; pnum++)
	{
		s1 = &cl_parse_entities[(cl.frame.parse_entities+pnum) & PARSE_ENTITIES_MASK];
		if (s1->number != cl.playernum + 1)
			continue;

		if (s1->effects & (EF_COLOR_SHELL|EF_QUAD|EF_PENT|EF_DOUBLE|EF_HALF_DAMAGE))
		{
			gun.flags |= (RF_TRANSLUCENT|s1->renderfx);
			if (s1->effects & EF_PENT)
				gun.flags |= RF_SHELL_RED;
			if (s1->effects & EF_QUAD)
				gun.flags |= RF_SHELL_BLUE;
			if (s1->effects & EF_DOUBLE)
				gun.flags |= RF_SHELL_DOUBLE;
			if (s1->effects & EF_HALF_DAMAGE)
				gun.flags |= RF_SHELL_HALF_DAM;
			gun.alpha = 0.1f;
			V_AddEntity(&gun);
		}
		break;
	}
#endif
}


/*
===============
CL_CalcViewValues

Sets cl.refdef view values
===============
*/
static void CL_CalcViewValues (void)
{
	int			i;
	float		lerp, backlerp;
	const frame_t		*oldframe;
	const player_state_t	*ps, *ops;
	qboolean demoplayback;

	// find the previous frame to interpolate from
	ps = &cl.frame.playerstate;
	i = (cl.frame.serverframe - 1) & UPDATE_MASK;
	oldframe = &cl.frames[i];
	if (oldframe->serverframe != cl.frame.serverframe-1 || !oldframe->valid)
		oldframe = &cl.frame;		// previous frame was dropped or involid
	ops = &oldframe->playerstate;

	// see if the player entity was teleported this frame
	if ( abs(ops->pmove.origin[0] - ps->pmove.origin[0]) > 2048
		|| abs(ops->pmove.origin[1] - ps->pmove.origin[1]) > 2048
		|| abs(ops->pmove.origin[2] - ps->pmove.origin[2]) > 2048)
		ops = ps;		// don't interpolate

	//ent = &cl_entities[cl.playernum+1];
	lerp = cl.lerpfrac;

	demoplayback = (Com_ServerState() == ss_demo || cls.demoplaying);
	// calculate the origin
	if (!demoplayback && cl_predict->integer && !(cl.frame.playerstate.pmove.pm_flags & PMF_NO_PREDICTION))
	{	// use predicted values
		unsigned	delta;

		backlerp = 1.0f - lerp;
		for (i=0 ; i<3 ; i++)
		{
			cl.refdef.vieworg[i] = cl.predicted_origin[i] + ops->viewoffset[i]
				+ lerp * (ps->viewoffset[i] - ops->viewoffset[i])
				- backlerp * cl.prediction_error[i];
		}

		// smooth out stair climbing
		delta = cls.realtime - cl.predicted_step_time;
		if (delta < 100)
			cl.refdef.vieworg[2] -= cl.predicted_step * (100 - delta) * 0.01f;
	}
	else
	{	// just use interpolated values
		for (i=0 ; i<3 ; i++)
			cl.refdef.vieworg[i] = ops->pmove.origin[i]*0.125f + ops->viewoffset[i] 
				+ lerp * (ps->pmove.origin[i]*0.125f + ps->viewoffset[i] 
				- (ops->pmove.origin[i]*0.125f + ops->viewoffset[i]) );
	}

	// if not running a demo or on a locked frame, add the local angle movement
	if (demoplayback)
	{
		if( Key_IsDown( K_SHIFT ) ) {
			VectorCopy(cl.predicted_angles, cl.refdef.viewangles);
		} else {
			cl.refdef.viewangles[0] = LerpAngle (ops->viewangles[0], ps->viewangles[0], lerp);
			cl.refdef.viewangles[1] = LerpAngle (ops->viewangles[1], ps->viewangles[1], lerp);
			cl.refdef.viewangles[2] = LerpAngle (ops->viewangles[2], ps->viewangles[2], lerp);
		}
	}
	else if (ps->pmove.pm_type < PM_DEAD)
	{	// use predicted values
		VectorCopy (cl.predicted_angles, cl.refdef.viewangles);
	}
	else if (ops->pmove.pm_type < PM_DEAD && cls.serverProtocol > PROTOCOL_VERSION_DEFAULT)
	{	//r1: fix for server no longer sending viewangles every frame.
		cl.refdef.viewangles[0] = LerpAngle (cl.predicted_angles[0], ps->viewangles[0], lerp);
		cl.refdef.viewangles[1] = LerpAngle (cl.predicted_angles[1], ps->viewangles[1], lerp);
		cl.refdef.viewangles[2] = LerpAngle (cl.predicted_angles[2], ps->viewangles[2], lerp);
	}
	else
	{	// just use interpolated values
		cl.refdef.viewangles[0] = LerpAngle (ops->viewangles[0], ps->viewangles[0], lerp);
		cl.refdef.viewangles[1] = LerpAngle (ops->viewangles[1], ps->viewangles[1], lerp);
		cl.refdef.viewangles[2] = LerpAngle (ops->viewangles[2], ps->viewangles[2], lerp);
	}

	cl.refdef.viewangles[0] += LerpAngle (ops->kick_angles[0], ps->kick_angles[0], lerp);
	cl.refdef.viewangles[1] += LerpAngle (ops->kick_angles[1], ps->kick_angles[1], lerp);
	cl.refdef.viewangles[2] += LerpAngle (ops->kick_angles[2], ps->kick_angles[2], lerp);

	AngleVectors (cl.refdef.viewangles, cl.v_forward, cl.v_right, cl.v_up);

	// interpolate field of view
	cl.refdef.fov_x = ops->fov + lerp * (ps->fov - ops->fov);

	// don't interpolate blend color
	Vector4Copy (ps->blend, cl.refdef.blend);

	// add the weapon
	CL_AddViewWeapon (ps, ops);
}

/*
===============
CL_AddEntities

Emits all entities, particles, and lights to the refresh
===============
*/
void CL_AddEntities (void)
{

	if (cl.time > cl.serverTime)
	{
		if (cl_showclamp->integer)
			Com_Printf ("high clamp %i\n", cl.time - cl.serverTime);
		cl.time = cl.serverTime;
		cl.lerpfrac = 1.0f;
	}
	else if (cl.time < cl.serverTime - 100)
	{
		if (cl_showclamp->integer)
			Com_Printf ("low clamp %i\n", cl.serverTime-100 - cl.time);
		cl.time = cl.serverTime - 100;
		cl.lerpfrac = 0;
	}
	else
		cl.lerpfrac = 1.0f - (cl.serverTime - cl.time) * 0.01f;

	if (cl_timedemo->integer)
		cl.lerpfrac = 1.0f;

	CL_CalcViewValues ();
	// PMM - moved this here so the heat beam has the right values for the vieworg, and can lock the beam to the gun
	CL_AddPacketEntities (&cl.frame);

	CL_AddViewLocs();

	CL_AddTEnts ();
	CL_AddParticles ();
	CL_AddDLights ();
	CL_AddLightStyles ();
}


/*
===============
CL_GetEntitySoundOrigin

Called to get the sound spatialization origin
===============
*/
void CL_GetEntitySoundOrigin (int ent, vec3_t org)
{
	centity_t	*cent;

	if ((unsigned)ent >= MAX_EDICTS)
		Com_Error(ERR_DROP, "CL_GetEntityOrigin: ent = %i", ent);

	// Player entity
	if (ent == cl.playernum + 1)
	{
		VectorCopy (cl.refdef.vieworg, org);
		return;
	}

	cent = &cl_entities[ent];
	//VectorCopy(cent->lerp_origin, org);

	if (cent->current.renderfx & (RF_FRAMELERP|RF_BEAM))
	{
		// Calculate origin
		org[0] = cent->current.old_origin[0] + (cent->current.origin[0] - cent->current.old_origin[0]) * cl.lerpfrac;
		org[1] = cent->current.old_origin[1] + (cent->current.origin[1] - cent->current.old_origin[1]) * cl.lerpfrac;
		org[2] = cent->current.old_origin[2] + (cent->current.origin[2] - cent->current.old_origin[2]) * cl.lerpfrac;
	}
	else
	{
		// Calculate origin
		org[0] = cent->prev.origin[0] + (cent->current.origin[0] - cent->prev.origin[0]) * cl.lerpfrac;
		org[1] = cent->prev.origin[1] + (cent->current.origin[1] - cent->prev.origin[1]) * cl.lerpfrac;
		org[2] = cent->prev.origin[2] + (cent->current.origin[2] - cent->prev.origin[2]) * cl.lerpfrac;
	}

	// If a brush model, offset the origin
	if (cent->current.solid == 31)
	{
		vec3_t		midPoint;
		cmodel_t	*cmodel = cl.model_clip[cent->current.modelindex];

		if (!cmodel)
			return;

		VectorAvg(cmodel->mins, cmodel->maxs, midPoint);
		VectorAdd(org, midPoint, org);
	}
}

