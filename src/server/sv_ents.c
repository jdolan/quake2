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

#include "server.h"

/*
=============================================================================

Encode a client frame onto the network channel

=============================================================================
*/

/*
=============
SV_EmitPacketEntities

Writes a delta update of an entity_state_t list to the message.
=============
*/
void SV_EmitPacketEntities (client_frame_t *from, client_frame_t *to, sizebuf_t *msg)
{
	entity_state_t	*oldent, *newent;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		from_num_entities;

	MSG_WriteByte (msg, svc_packetentities);

	if (!from)
		from_num_entities = 0;
	else
		from_num_entities = from->num_entities;

	newindex = oldindex = 0;
	newent = NULL;
	oldent = NULL;
	while (newindex < to->num_entities || oldindex < from_num_entities)
	{
		if (newindex >= to->num_entities)
			newnum = 9999;
		else
		{
			newent = &svs.client_entities[(to->first_entity+newindex)%svs.num_client_entities];
			newnum = newent->number;
		}

		if (oldindex >= from_num_entities)
			oldnum = 9999;
		else
		{
			oldent = &svs.client_entities[(from->first_entity+oldindex)%svs.num_client_entities];
			oldnum = oldent->number;
		}

		if (newnum == oldnum)
		{	// delta update from old position
			// because the force parm is false, this will not result
			// in any bytes being emited if the entity has not changed at all
			// note that players are always 'newentities', this updates their oldorigin always
			// and prevents warping
			MSG_WriteDeltaEntity(oldent, newent, msg, false, newent->number <= maxclients->integer);
			oldindex++;
			newindex++;
			continue;
		}

		if (newnum < oldnum)
		{	// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity(&sv.baselines[newnum], newent, msg, true, true);
			newindex++;
			continue;
		}

		if (newnum > oldnum)
		{	// the old entity isn't present in the new message
			MSG_WriteDeltaEntity(oldent, NULL, msg, true, false);
			oldindex++;
			continue;
		}
	}

	MSG_WriteShort (msg, 0);	// end of packetentities

}

/*
==================
SV_WriteFrameToClient
==================
*/
void SV_WriteFrameToClient (client_t *client, sizebuf_t *msg)
{
	client_frame_t		*frame, *oldframe;
	player_state_t		*oldstate;
	int					lastframe;

//Com_Printf ("%i -> %i\n", client->lastframe, sv.framenum);
	// this is the frame we are creating
	frame = &client->frames[sv.framenum & UPDATE_MASK];

	if (client->lastframe <= 0)
	{	// client is asking for a retransmit
		oldframe = NULL;
		lastframe = -1;
	}
	else if (sv.framenum - client->lastframe >= (UPDATE_BACKUP - 3) )
	{	// client hasn't gotten a good message through in a long time
//		Com_Printf ("%s: Delta request from out-of-date packet.\n", client->name);
		oldframe = NULL;
		lastframe = -1;
	}
	else
	{	// we have a valid message to delta from
		oldframe = &client->frames[client->lastframe & UPDATE_MASK];
		lastframe = client->lastframe;
	}

	MSG_WriteByte (msg, svc_frame);
	MSG_WriteLong (msg, sv.framenum);
	MSG_WriteLong (msg, lastframe);	// what we are delta'ing from
	MSG_WriteByte (msg, client->surpressCount);	// rate dropped packets
	client->surpressCount = 0;

	// send over the areabits
	MSG_WriteByte (msg, frame->areabytes);
	SZ_Write (msg, frame->areabits, frame->areabytes);

	// delta encode the playerstate
	MSG_WriteByte(msg, svc_playerinfo);
	if( oldframe ) {
		oldstate = &oldframe->ps;
	} else {
		oldstate = NULL;
	}
	MSG_WriteDeltaPlayerstate_Default( oldstate, &frame->ps, msg );

	// delta encode the entities
	SV_EmitPacketEntities (oldframe, frame, msg);
}


/*
=============================================================================

Build a client frame structure

=============================================================================
*/

byte		fatpvs[65536/8];	// 32767 is MAX_MAP_LEAFS

/*
============
SV_FatPVS

The client will interpolate the view position,
so we can't use a single PVS point
===========
*/
void SV_FatPVS (vec3_t org)
{
	int		leafs[64];
	int		i, j, count;
	int		longs;
	byte	*src;
	vec3_t	mins, maxs;

	for (i=0 ; i<3 ; i++)
	{
		mins[i] = org[i] - 8;
		maxs[i] = org[i] + 8;
	}

	count = CM_BoxLeafnums (mins, maxs, leafs, 64, NULL);
	if (count < 1)
		Com_Error (ERR_FATAL, "SV_FatPVS: count < 1");
	longs = (CM_NumClusters()+31)>>5;

	// convert leafs to clusters
	for (i=0 ; i<count ; i++)
		leafs[i] = CM_LeafCluster(leafs[i]);

	memcpy (fatpvs, CM_ClusterPVS(leafs[0]), longs<<2);
	// or in all the other leaf bits
	for (i=1 ; i<count ; i++)
	{
		for (j=0 ; j<i ; j++)
			if (leafs[i] == leafs[j])
				break;
		if (j != i)
			continue;		// already have the cluster we want
		src = CM_ClusterPVS(leafs[i]);
		for (j=0 ; j<longs ; j++)
			((long *)fatpvs)[j] |= ((long *)src)[j];
	}
}


/*
=============
SV_BuildClientFrame

Decides which entities are going to be visible to the client, and
copies off the playerstat and areabits.
=============
*/
void SV_BuildClientFrame (client_t *client)
{
	int		e, i;
	vec3_t	org;
	edict_t	*ent;
	edict_t	*clent;
	client_frame_t	*frame;
	entity_state_t	*state;
	int		l;
	int		clientarea, clientcluster;
	int		leafnum;
	int		c_fullsend;
	byte	*clientphs;
	byte	*bitvector;

	clent = client->edict;
	if (!clent->client)
		return;		// not in game yet

#if 0
	numprojs = 0; // no projectiles yet
#endif

	// this is the frame we are creating
	frame = &client->frames[sv.framenum & UPDATE_MASK];

	frame->senttime = svs.realtime; // save it for ping calc later

	// find the client's PVS
	for (i=0 ; i<3 ; i++)
		org[i] = clent->client->ps.pmove.origin[i]*0.125f + clent->client->ps.viewoffset[i];

	leafnum = CM_PointLeafnum (org);
	clientarea = CM_LeafArea (leafnum);
	clientcluster = CM_LeafCluster (leafnum);

	// calculate the visible areas
	frame->areabytes = CM_WriteAreaBits (frame->areabits, clientarea);

	// grab the current player_state_t
	frame->ps = clent->client->ps;


	SV_FatPVS (org);
	clientphs = CM_ClusterPHS (clientcluster);

	// build up the list of visible entities
	frame->num_entities = 0;
	frame->first_entity = svs.next_client_entities;

	c_fullsend = 0;

	for (e=1 ; e<ge->num_edicts ; e++)
	{
		ent = EDICT_NUM(e);

		// ignore ents without visible models
		if (ent->svflags & SVF_NOCLIENT)
			continue;

		// ignore ents without visible models unless they have an effect
		if (!ent->s.modelindex && !ent->s.effects && !ent->s.sound
			&& !ent->s.event)
			continue;

		// ignore if not touching a PV leaf
		if (ent != clent)
		{
			// check area
			if (!CM_AreasConnected (clientarea, ent->areanum))
			{	// doors can legally straddle two areas, so
				// we may need to check another one
				if (!ent->areanum2
					|| !CM_AreasConnected (clientarea, ent->areanum2))
					continue;		// blocked by a door
			}

			// beams just check one point for PHS
			if (ent->s.renderfx & RF_BEAM)
			{
				l = ent->clusternums[0];
				if ( !(clientphs[l >> 3] & (1 << (l&7) )) )
					continue;
			}
			else
			{
				// FIXME: if an ent has a model and a sound, but isn't
				// in the PVS, only the PHS, clear the model
				if (ent->s.sound)
				{
					bitvector = fatpvs;	//clientphs;
				}
				else
					bitvector = fatpvs;

				if (ent->num_clusters == -1)
				{	// too many leafs for individual check, go by headnode
					if (!CM_HeadnodeVisible (ent->headnode, bitvector))
						continue;
					c_fullsend++;
				}
				else
				{	// check individual leafs
					for (i=0 ; i < ent->num_clusters ; i++)
					{
						l = ent->clusternums[i];
						if (bitvector[l >> 3] & (1 << (l&7) ))
							break;
					}
					if (i == ent->num_clusters)
						continue;		// not visible
				}

				if (!ent->s.modelindex)
				{	// don't send sounds if they will be attenuated away
					if (Distance(org, ent->s.origin) > 400)
						continue;
				}
			}
		}

#if 0
		if (SV_AddProjectileUpdate(ent))
			continue; // added as a special projectile
#endif

		// add it to the circular client_entities array
		state = &svs.client_entities[svs.next_client_entities%svs.num_client_entities];
		if (ent->s.number != e)
		{
			Com_DPrintf ("FIXING ENT->S.NUMBER!!!\n");
			ent->s.number = e;
		}
		*state = ent->s;

		// don't mark players missiles as solid
		if (ent->owner == client->edict)
			state->solid = 0;

		svs.next_client_entities++;
		frame->num_entities++;
	}
}


/*
==================
SV_RecordDemoMessage

Save everything in the world out without deltas.
Used for recording footage for merged or assembled demos
==================
*/
void SV_RecordDemoMessage (void)
{
	int			e;
	edict_t		*ent;
	sizebuf_t	buf;
	byte		buf_data[32768];
	int			len;
	size_t		size;

	if (!svs.demofile)
		return;

	SZ_Init (&buf, buf_data, sizeof(buf_data));

	// write a frame message that doesn't contain a player_state_t
	MSG_WriteByte (&buf, svc_frame);
	MSG_WriteLong (&buf, sv.framenum);

	MSG_WriteByte (&buf, svc_packetentities);

	e = 1;
	ent = EDICT_NUM(e);
	while (e < ge->num_edicts) 
	{
		// ignore ents without visible models unless they have an effect
		if (ent->inuse &&
			ent->s.number && 
			(ent->s.modelindex || ent->s.effects || ent->s.sound || ent->s.event) && 
			!(ent->svflags & SVF_NOCLIENT))
			MSG_WriteDeltaEntity (NULL, &ent->s, &buf, false, true);

		e++;
		ent = EDICT_NUM(e);
	}

	MSG_WriteShort (&buf, 0);		// end of packetentities

	// now add the accumulated multicast information
	SZ_Write (&buf, svs.demo_multicast.data, svs.demo_multicast.cursize);
	SZ_Clear (&svs.demo_multicast);

	// now write the entire message to the file, prefixed by the length
	len = LittleLong (buf.cursize);
	size = fwrite (&len, 4, 1, svs.demofile);
	size = fwrite (buf.data, buf.cursize, 1, svs.demofile);
}
