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
// cl_view.c -- player rendering positioning

#include "client.h"

static cvar_t		*cl_testparticles;
static cvar_t		*cl_testentities;
static cvar_t		*cl_testlights;
static cvar_t		*cl_testblend;

static cvar_t		*cl_stats;
static cvar_t		*cl_wsfov;

static int			r_numdlights;
static dlight_t	r_dlights[MAX_DLIGHTS];

static int			r_numentities;
static entity_t	r_entities[MAX_ENTITIES];

static int			r_numparticles;
static particle_t	r_particles[MAX_PARTICLES];

static lightstyle_t	r_lightstyles[MAX_LIGHTSTYLES];

char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
int num_cl_weaponmodels;

/*
====================
V_ClearScene

Specifies the model that will be used as the world
====================
*/
static void V_ClearScene (void)
{
	r_numdlights = r_numentities = r_numparticles = 0;
}


/*
=====================
V_AddEntity

=====================
*/
void V_AddEntity (const entity_t *ent)
{
	if (r_numentities < MAX_ENTITIES)
		r_entities[r_numentities++] = *ent;
}

/*
=====================
V_AddParticle

=====================
*/
void V_AddParticle (const particle_t *p)
{
	if (r_numparticles < MAX_PARTICLES)
		r_particles[r_numparticles++] = *p;
}

/*
=====================
V_AddLight

=====================
*/
void V_AddLight (const vec3_t org, float intensity, float r, float g, float b)
{
	dlight_t	*dl;

	if (r_numdlights >= MAX_DLIGHTS)
		return;
	dl = &r_dlights[r_numdlights++];
	VectorCopy (org, dl->origin);
	dl->intensity = intensity;
	VectorSet(dl->color, r, g, b);
}


/*
=====================
V_AddLightStyle

=====================
*/
void V_AddLightStyle (int style, const vec3_t value)
{
	lightstyle_t	*ls;

	if ((unsigned)style >= MAX_LIGHTSTYLES)
		Com_Error (ERR_DROP, "Bad light style %i", style);

	ls = &r_lightstyles[style];
	ls->white = value[0]+value[1]+value[2];
	VectorCopy(value, ls->rgb);
}

/*
================
V_TestParticles

If cl_testparticles is set, create 4096 particles in the view
================
*/
static void V_TestParticles (void)
{
	particle_t	*p;
	int			i, j;
	float		d, r, u;

	r_numparticles = MAX_PARTICLES;
	for (i=0 ; i<r_numparticles ; i++)
	{
		d = i*0.25f;
		r = 4*((i&7)-3.5f);
		u = 4*(((i>>3)&7)-3.5f);
		p = &r_particles[i];

		for (j=0 ; j<3 ; j++)
			p->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*d +
			cl.v_right[j]*r + cl.v_up[j]*u;

		p->color = 8;
		p->alpha = cl_testparticles->value;
	}
}

/*
================
V_TestEntities

If cl_testentities is set, create 32 player models
================
*/
static void V_TestEntities (void)
{
	int			i, j;
	float		f, r;
	entity_t	*ent;

	r_numentities = 32;
	memset (r_entities, 0, sizeof(r_entities));

	for (i=0 ; i<r_numentities ; i++)
	{
		ent = &r_entities[i];

		r = 64 * ( (i%4) - 1.5f );
		f = 64 * (i*0.25f) + 128;

		for (j=0 ; j<3 ; j++)
			ent->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*f +
			cl.v_right[j]*r;

		ent->model = cl.baseclientinfo.model;
		ent->skin = cl.baseclientinfo.skin;
#ifdef GL_QUAKE
		AxisClear(ent->axis);
#endif
	}
}

/*
================
V_TestLights

If cl_testlights is set, create 32 lights models
================
*/
static void V_TestLights (void)
{
	int			i, j;
	float		f, r;
	dlight_t	*dl;

	r_numdlights = 32;
	memset (r_dlights, 0, sizeof(r_dlights));

	for (i=0 ; i<r_numdlights ; i++)
	{
		dl = &r_dlights[i];

		r = 64 * ( (i%4) - 1.5f );
		f = 64 * (i*0.25f) + 128;

		for (j=0 ; j<3 ; j++)
			dl->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*f +
			cl.v_right[j]*r;
		dl->color[0] = ((i%6)+1) & 1;
		dl->color[1] = (((i%6)+1) & 2)>>1;
		dl->color[2] = (((i%6)+1) & 4)>>2;
		dl->intensity = 200;
	}
}

//===================================================================
extern char currentSky[64];
/*
=================
CL_PrepRefresh

Call before entering a new level, or after changing dlls
=================
*/
void CL_PrepRefresh (void)
{
	int			i;
	char		name[MAX_QPATH];
	float		rotate;
	vec3_t		axis;

	if (!cl.configstrings[CS_MODELS+1][0])
		return;		// no map loaded

	SCR_AddDirtyPoint (0, 0);
	SCR_AddDirtyPoint (viddef.width-1, viddef.height-1);

	// let the render dll load the map

	// register models, pics, and skins
	Com_Printf ("Map: %s\r", cls.mapname); 
	SCR_UpdateScreen ();
	R_BeginRegistration (cls.mapname);
	Com_Printf ("                                     \r");

	// precache status bar pics
	Com_Printf ("pics\r"); 
	SCR_UpdateScreen ();
	SCR_TouchPics ();
	Com_Printf ("                                     \r");

	CL_RegisterTEntModels ();

	num_cl_weaponmodels = 1;
	strcpy(cl_weaponmodels[0], "weapon.md2");

	for (i=1 ; i<MAX_MODELS && cl.configstrings[CS_MODELS+i][0] ; i++)
	{
		Q_strncpyz (name, cl.configstrings[CS_MODELS+i], sizeof(name));
		name[37] = 0;	// never go beyond one line
		if (name[0] != '*')
			Com_Printf ("%s\r", name); 
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();	// pump message loop
		if (name[0] == '#')
		{
			// special player weapon model
			if (num_cl_weaponmodels < MAX_CLIENTWEAPONMODELS)
			{
				Q_strncpyz(cl_weaponmodels[num_cl_weaponmodels], cl.configstrings[CS_MODELS+i]+1,
					sizeof(cl_weaponmodels[num_cl_weaponmodels]));
				num_cl_weaponmodels++;
			}
		} 
		else
		{
			cl.model_draw[i] = R_RegisterModel (cl.configstrings[CS_MODELS+i]);
			if (name[0] == '*')
				cl.model_clip[i] = CM_InlineModel (cl.configstrings[CS_MODELS+i]);
			else
				cl.model_clip[i] = NULL;
		}
		if (name[0] != '*')
			Com_Printf ("                                     \r");
	}

	Com_Printf ("images\r"); 
	SCR_UpdateScreen ();
	for (i=1 ; i<MAX_IMAGES && cl.configstrings[CS_IMAGES+i][0] ; i++)
	{
		cl.image_precache[i] = Draw_FindPic (cl.configstrings[CS_IMAGES+i]);
		Sys_SendKeyEvents ();	// pump message loop
	}
	
	Com_Printf ("                                     \r");
	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		if (!cl.configstrings[CS_PLAYERSKINS+i][0])
			continue;
		Com_Printf ("client %i\r", i); 
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();	// pump message loop
		CL_ParseClientinfo (i);
		Com_Printf ("                                     \r");
	}

	CL_LoadClientinfo (&cl.baseclientinfo, "unnamed\\male/grunt");

	// set sky textures and speed
	Com_Printf ("sky\r"); 
	SCR_UpdateScreen ();
	rotate = (float)atof(cl.configstrings[CS_SKYROTATE]);
	sscanf (cl.configstrings[CS_SKYAXIS], "%f %f %f", &axis[0], &axis[1], &axis[2]);
	R_SetSky (cl.configstrings[CS_SKY], rotate, axis);
	Q_strncpyz(currentSky, cl.configstrings[CS_SKY], sizeof(currentSky));
	Com_Printf ("                                     \r");

	// the renderer can now free unneeded stuff
	R_EndRegistration ();

	// clear any lines of console text
	Con_ClearNotify ();

	SCR_UpdateScreen ();
	cl.refresh_prepped = true;
	cl.force_refdef = true;	// make sure we have a valid refdef

	// start the cd track
#ifdef CD_AUDIO
	CDAudio_Play (atoi(cl.configstrings[CS_CDTRACK]), true);
#endif

	cls.lastSpamTime = 0;
	cls.roundtime = 0;
}

//============================================================================

static int entitycmpfnc( const entity_t *a, const entity_t *b )
{
	// all other models are sorted by model then skin
	if ( a->model == b->model )
		return ( (long int) a->skin - (long int) b->skin );

	return ( (long int) a->model - (long int) b->model );
}

/*
==================
V_RenderView

==================
*/
static const float standardRatio = 4.0f/3.0f;

void V_RenderView( float stereo_separation )
{
	float currentRatio;

	if (!cl.refresh_prepped)
		return;			// still loading

	if (cl_timedemo->integer)
	{
		if (!cl.timedemo_start)
			cl.timedemo_start = Sys_Milliseconds ();
		cl.timedemo_frames++;
	}

	// an invalid frame will just use the exact previous refdef
	// we can't use the old frame if the video mode has changed, though...
	if ( cl.frame.valid && (cl.force_refdef || !cl_paused->integer) )
	{
		cl.force_refdef = false;

		V_ClearScene ();

		// build a refresh entity list and calc cl.sim*
		// this also calls CL_CalcViewValues which loads
		// v_forward, etc.
		CL_AddEntities ();

		if (cl_testparticles->integer)
			V_TestParticles ();
		if (cl_testentities->integer)
			V_TestEntities ();
		if (cl_testlights->integer)
			V_TestLights ();
		if (cl_testblend->integer)
			Vector4Set(cl.refdef.blend, 1, 0.5f, 0.25f, 0.5f);

		// offset vieworg appropriately if we're doing stereo separation
		if ( stereo_separation != 0.0f )
		{
			vec3_t tmp;

			VectorScale( cl.v_right, stereo_separation, tmp );
			VectorAdd( cl.refdef.vieworg, tmp, cl.refdef.vieworg );
		}

		// never let it sit exactly on a node line, because a water plane can
		// dissapear when viewed with the eye exactly on it.
		// the server protocol only specifies to 1/8 pixel, so add 1/16 in each axis
		cl.refdef.vieworg[0] += 0.0625f;
		cl.refdef.vieworg[1] += 0.0625f;
		cl.refdef.vieworg[2] += 0.0625f;

		cl.refdef.x = scr_vrect.x;
		cl.refdef.y = scr_vrect.y;
		cl.refdef.width = scr_vrect.width;
		cl.refdef.height = scr_vrect.height;

		// adjust fov for wide aspect ratio
		currentRatio = (float)cl.refdef.width/(float)cl.refdef.height;
		if (cl_wsfov->integer && currentRatio > standardRatio) {
			cl.refdef.fov_y = CalcFov(cl.refdef.fov_x, cl.refdef.width * (standardRatio / currentRatio), cl.refdef.height);
			cl.refdef.fov_x = CalcFov(cl.refdef.fov_y, cl.refdef.height, cl.refdef.width);
			//cl.refdef.fov_x *= (currentRatio / standardRatio);
		} else {
			cl.refdef.fov_y = CalcFov(cl.refdef.fov_x, cl.refdef.width, cl.refdef.height);
		}

		cl.refdef.time = cl.time*0.001f;

		cl.refdef.areabits = cl.frame.areabits;

		if (!cl_add_entities->integer)
			r_numentities = 0;
		if (!cl_add_particles->integer)
			r_numparticles = 0;
		if (!cl_add_lights->integer)
			r_numdlights = 0;
		if (!cl_add_blend->integer)
			VectorClear (cl.refdef.blend);

		cl.refdef.num_entities = r_numentities;
		cl.refdef.entities = r_entities;
		cl.refdef.num_particles = r_numparticles;
		cl.refdef.particles = r_particles;
		cl.refdef.num_dlights = r_numdlights;
		cl.refdef.dlights = r_dlights;
		cl.refdef.lightstyles = r_lightstyles;

		cl.refdef.rdflags = cl.frame.playerstate.rdflags;

		// sort entities for better cache locality
        qsort( cl.refdef.entities, cl.refdef.num_entities, sizeof( cl.refdef.entities[0] ), (int (*)(const void *, const void *))entitycmpfnc );
	}

	R_RenderFrame (&cl.refdef);
	if (cl_stats->integer)
		Com_Printf ("ent:%i  lt:%i  part:%i\n", r_numentities, r_numdlights, r_numparticles);

	SCR_AddDirtyPoint (scr_vrect.x, scr_vrect.y);
	SCR_AddDirtyPoint (scr_vrect.x+scr_vrect.width-1,
		scr_vrect.y+scr_vrect.height-1);
}


/*
=============
V_Viewpos_f
=============
*/
void V_Viewpos_f (void)
{
	Com_Printf ("(%i %i %i) : %i\n", (int)cl.refdef.vieworg[0],
		(int)cl.refdef.vieworg[1], (int)cl.refdef.vieworg[2], 
		(int)cl.refdef.viewangles[YAW]);
}

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	Cmd_AddCommand ("viewpos", V_Viewpos_f);

	cl_testblend = Cvar_Get ("cl_testblend", "0", 0);
	cl_testparticles = Cvar_Get ("cl_testparticles", "0", 0);
	cl_testentities = Cvar_Get ("cl_testentities", "0", 0);
	cl_testlights = Cvar_Get ("cl_testlights", "0", CVAR_CHEAT);

	cl_wsfov = Cvar_Get("cl_wsfov", "1", 0);
	cl_stats = Cvar_Get ("cl_stats", "0", 0);
}
