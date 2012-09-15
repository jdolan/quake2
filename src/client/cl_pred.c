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

#include "client.h"


/*
===================
CL_CheckPredictionError
===================
*/
void CL_CheckPredictionError (void)
{
	int		frame, delta[3], len;

	if (!cl_predict->integer || (cl.frame.playerstate.pmove.pm_flags & PMF_NO_PREDICTION))
		return;

	// calculate the last usercmd_t we sent that the server has processed
	frame = cls.netchan.incoming_acknowledged & CMD_MASK;

	// compare what the server returned with what we had predicted it to be
	VectorSubtract (cl.frame.playerstate.pmove.origin, cl.predicted_origins[frame], delta);

	// save the prediction error for interpolation
	len = (int)(abs(delta[0]) + abs(delta[1]) + abs(delta[2]));
	if (len > (cl.attractloop ? 1280 : 640))	// 80 world units
	{	// a teleport or something
		VectorClear (cl.prediction_error);
	}
	else
	{
		if (cl_showmiss->integer && (delta[0] || delta[1] || delta[2]) )
			Com_Printf ("prediction miss on %i: %i\n", cl.frame.serverframe, 
			delta[0] + delta[1] + delta[2]);

		VectorCopy (cl.frame.playerstate.pmove.origin, cl.predicted_origins[frame]);

		// save for error itnerpolation
		VectorScale(delta, 0.125f, cl.prediction_error);
	}
}


/*
====================
CL_ClipMoveToEntities

====================
*/
static void CL_ClipMoveToEntities ( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, trace_t *tr )
{
	int			i, x, zd, zu, headnode, num;
	trace_t		trace;
	float		*angles;
	entity_state_t	*ent;
	cmodel_t		*cmodel;
	vec3_t		bmins, bmaxs;

	for (i = 0; i < cl.frame.num_entities; i++)
	{
		num = (cl.frame.parse_entities + i) & PARSE_ENTITIES_MASK;
		ent = &cl_parse_entities[num];

		if (!ent->solid)
			continue;

		if (ent->number == cl.playernum+1)
			continue;

		if (ent->solid == 31)
		{	// special value for bmodel
			cmodel = cl.model_clip[ent->modelindex];
			if (!cmodel)
				continue;
			headnode = cmodel->headnode;
			angles = ent->angles;
		}
		else
		{	// encoded bbox
			// encoded bbox
			if (cls.protocolVersion >= PROTOCOL_VERSION_R1Q2_SOLID)
			{
				x = (ent->solid & 255);
				zd = ((ent->solid>>8) & 255);
				zu = ((ent->solid>>16) & 65535) - 32768;
			}
			else
			{
				x = 8*(ent->solid & 31);
				zd = 8*((ent->solid>>5) & 31);
				zu = 8*((ent->solid>>10) & 63) - 32;
			}

			bmins[0] = bmins[1] = -(float)x;
			bmaxs[0] = bmaxs[1] = (float)x;
			bmins[2] = -(float)zd;
			bmaxs[2] = (float)zu;

			headnode = CM_HeadnodeForBox (bmins, bmaxs);
			angles = vec3_origin;	// boxes don't rotate
		}

		if (tr->allsolid)
			return;

		trace = CM_TransformedBoxTrace (start, end,
			mins, maxs, headnode,  MASK_PLAYERSOLID,
			ent->origin, angles);

		if (trace.allsolid || trace.startsolid ||
		trace.fraction < tr->fraction)
		{
			trace.ent = (struct edict_s *)ent;
		 	if (tr->startsolid)
			{
				*tr = trace;
				tr->startsolid = true;
			}
			else
				*tr = trace;
		}
		else if (trace.startsolid)
			tr->startsolid = true;
	}
}


/*
================
CL_PMTrace
================
*/
static trace_t CL_PMTrace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	trace_t	t;

	// check against world
	t = CM_BoxTrace (start, end, mins, maxs, 0, MASK_PLAYERSOLID);
	if (t.fraction < 1.0f)
		t.ent = (struct edict_s *)1;

	// check all other solid models
	CL_ClipMoveToEntities (start, mins, maxs, end, &t);

	return t;
}

static int CL_PMpointcontents (vec3_t point)
{
	int				i, num, contents;
	entity_state_t	*ent;
	cmodel_t		*cmodel;

	contents = CM_PointContents (point, 0);

	for (i = 0; i < cl.frame.num_entities; i++)
	{
		num = (cl.frame.parse_entities + i) & PARSE_ENTITIES_MASK;
		ent = &cl_parse_entities[num];

		if (ent->solid != 31) // special value for bmodel
			continue;

		cmodel = cl.model_clip[ent->modelindex];
		if (!cmodel)
			continue;

		contents |= CM_TransformedPointContents (point, cmodel->headnode, ent->origin, ent->angles);
	}

	return contents;
}


/*
=================
CL_PredictMovement

Sets cl.predicted_origin and cl.predicted_angles
=================
*/
void CL_PredictMovement (void)
{
	int			ack, current, frame, step;
	pmove_t		pm;

	if (cls.state != ca_active || cl_paused->integer)
		return;

	if (!cl_predict->integer || (cl.frame.playerstate.pmove.pm_flags & PMF_NO_PREDICTION))
	{	// just set angles
		cl.predicted_angles[0] = cl.viewangles[0] + SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[0]);
		cl.predicted_angles[1] = cl.viewangles[1] + SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[1]);
		cl.predicted_angles[2] = cl.viewangles[2] + SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[2]);
		return;
	}

	ack = cls.netchan.incoming_acknowledged;
	current = cls.netchan.outgoing_sequence;

	// if we are too far out of date, just freeze
	if (current - ack >= CMD_BACKUP) {
		if (cl_showmiss->integer)
			Com_Printf ("exceeded CMD_BACKUP\n");
		return;	
	}

	// copy current state to pmove
	pm.snapinitial = false;
	pm.trace = CL_PMTrace;
	pm.pointcontents = CL_PMpointcontents;
	pm.s = cl.frame.playerstate.pmove;

//	SCR_DebugGraph (current - ack - 1, 0);

	frame = 0;

	VectorClear(pm.mins);
	VectorClear(pm.maxs);

	// run frames
	while (++ack < current) {
		frame = ack & CMD_MASK;

		pm.cmd = cl.cmds[frame];
		Pmove(&pm, &cl.pmp);

		// save for debug checking
		VectorCopy (pm.s.origin, cl.predicted_origins[frame]);
	}

	// run pending cmd
	if (cl.cmd.msec) {
		frame = current;

		pm.cmd = cl.cmd;
		pm.cmd.forwardmove = cl.move[0];
		pm.cmd.sidemove = cl.move[1];
		pm.cmd.upmove = cl.move[2];
		Pmove(&pm, &cl.pmp);

		// save for debug checking
		VectorCopy (pm.s.origin, cl.predicted_origins[frame & CMD_MASK]);
	} else {
		frame = current - 1;
	}

	step = pm.s.origin[2] - cl.predicted_origins[(frame-1) & CMD_MASK][2];
	if (cl.predicted_step_frame != frame && 
		step > 63 && step < 160 && (pm.s.pm_flags & PMF_ON_GROUND) )
	{
		cl.predicted_step = step * 0.125f;
		cl.predicted_step_time = cls.realtime - (unsigned int)(cls.frametime * 500);
		cl.predicted_step_frame = frame;
	}

	// copy results out for rendering
	VectorScale( pm.s.origin, 0.125f, cl.predicted_origin );
	VectorScale( pm.s.velocity, 0.125f, cl.predicted_velocity );
	VectorCopy( pm.viewangles, cl.predicted_angles );
}
