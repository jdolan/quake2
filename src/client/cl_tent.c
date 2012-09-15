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
// cl_tent.c -- client side temporary entities

#include "client.h"

typedef enum
{
	ex_free, ex_explosion, ex_misc, ex_flash, ex_mflash, ex_poly, ex_poly2
} exptype_t;

typedef struct explosion_s
{
	struct explosion_s *prev, *next;

	exptype_t	type;
	entity_t	ent;

	int			frames;
	float		light;
	vec3_t		lightcolor;
	float		start;
	int			baseframe;
} explosion_t;



#define	MAX_EXPLOSIONS	64
static explosion_t	cl_explosions[MAX_EXPLOSIONS];
static explosion_t	cl_explosions_headnode, *cl_free_expl;


#define	MAX_BEAMS	32
typedef struct
{
	int		entity;
	int		dest_entity;
	struct model_s	*model;
	int		endtime;
	vec3_t	offset;
	vec3_t	start, end;
} beam_t;
static beam_t		cl_beams[MAX_BEAMS];
//PMM - added this for player-linked beams.  Currently only used by the plasma beam
static beam_t		cl_playerbeams[MAX_BEAMS];


#define	MAX_LASERS	32
typedef struct
{
	entity_t	ent;
	int			endtime;
} laser_t;
static laser_t		cl_lasers[MAX_LASERS];

//ROGUE
static cl_sustain_t	cl_sustains[MAX_SUSTAINS];
//ROGUE

//PGM
extern void CL_TeleportParticles (const vec3_t org);
//PGM

void CL_BlasterParticles (const vec3_t org, const vec3_t dir);
void CL_ExplosionParticles (const vec3_t org);
void CL_BFGExplosionParticles (const vec3_t org);
// RAFAEL
//void CL_BlueBlasterParticles (vec3_t org, vec3_t dir);

struct sfx_s	*cl_sfx_ric1;
struct sfx_s	*cl_sfx_ric2;
struct sfx_s	*cl_sfx_ric3;
struct sfx_s	*cl_sfx_lashit;
struct sfx_s	*cl_sfx_spark5;
struct sfx_s	*cl_sfx_spark6;
struct sfx_s	*cl_sfx_spark7;
struct sfx_s	*cl_sfx_railg;
struct sfx_s	*cl_sfx_rockexp;
struct sfx_s	*cl_sfx_grenexp;
struct sfx_s	*cl_sfx_watrexp;
// RAFAEL
struct sfx_s	*cl_sfx_plasexp;
struct sfx_s	*cl_sfx_footsteps[4];

struct model_s	*cl_mod_explode;
struct model_s	*cl_mod_smoke;
struct model_s	*cl_mod_flash;
struct model_s	*cl_mod_parasite_segment;
struct model_s	*cl_mod_grapple_cable;
struct model_s	*cl_mod_parasite_tip;
struct model_s	*cl_mod_explo4;
struct model_s	*cl_mod_bfg_explo;
struct model_s	*cl_mod_powerscreen;
// RAFAEL
struct model_s	*cl_mod_plasmaexplo;

//ROGUE
struct sfx_s	*cl_sfx_lightning;
struct sfx_s	*cl_sfx_disrexp;
struct model_s	*cl_mod_lightning;
struct model_s	*cl_mod_heatbeam;
struct model_s	*cl_mod_monster_heatbeam;
struct model_s	*cl_mod_explo4_big;

//ROGUE
/*
=================
CL_RegisterTEntSounds
=================
*/
void CL_RegisterTEntSounds (void)
{
	int		i;
	char	name[MAX_QPATH];

	cl_sfx_ric1 = S_RegisterSound ("world/ric1.wav");
	cl_sfx_ric2 = S_RegisterSound ("world/ric2.wav");
	cl_sfx_ric3 = S_RegisterSound ("world/ric3.wav");
	cl_sfx_lashit = S_RegisterSound("weapons/lashit.wav");
	cl_sfx_spark5 = S_RegisterSound ("world/spark5.wav");
	cl_sfx_spark6 = S_RegisterSound ("world/spark6.wav");
	cl_sfx_spark7 = S_RegisterSound ("world/spark7.wav");
	cl_sfx_railg = S_RegisterSound ("weapons/railgf1a.wav");
	cl_sfx_rockexp = S_RegisterSound ("weapons/rocklx1a.wav");
	cl_sfx_grenexp = S_RegisterSound ("weapons/grenlx1a.wav");
	cl_sfx_watrexp = S_RegisterSound ("weapons/xpld_wat.wav");
	S_RegisterSound ("player/land1.wav");

	S_RegisterSound ("player/fall2.wav");
	S_RegisterSound ("player/fall1.wav");

	for (i=0 ; i<4 ; i++)
	{
		Com_sprintf (name, sizeof(name), "player/step%i.wav", i+1);
		cl_sfx_footsteps[i] = S_RegisterSound (name);
	}

//PGM
	cl_sfx_lightning = S_RegisterSound ("weapons/tesla.wav");
	cl_sfx_disrexp = S_RegisterSound ("weapons/disrupthit.wav");
//PGM
}	

/*
=================
CL_RegisterTEntModels
=================
*/
void CL_RegisterTEntModels (void)
{
	cl_mod_explode = R_RegisterModel ("models/objects/explode/tris.md2");
	cl_mod_smoke = R_RegisterModel ("models/objects/smoke/tris.md2");
	cl_mod_flash = R_RegisterModel ("models/objects/flash/tris.md2");
	cl_mod_parasite_segment = R_RegisterModel ("models/monsters/parasite/segment/tris.md2");
	cl_mod_grapple_cable = R_RegisterModel ("models/ctf/segment/tris.md2");
	cl_mod_parasite_tip = R_RegisterModel ("models/monsters/parasite/tip/tris.md2");
	cl_mod_explo4 = R_RegisterModel ("models/objects/r_explode/tris.md2");
	cl_mod_bfg_explo = R_RegisterModel ("sprites/s_bfg2.sp2");
	cl_mod_powerscreen = R_RegisterModel ("models/items/armor/effect/tris.md2");

	/*
	R_RegisterModel ("models/objects/laser/tris.md2");
	R_RegisterModel ("models/objects/grenade2/tris.md2");
	R_RegisterModel ("models/weapons/v_machn/tris.md2");
	R_RegisterModel ("models/weapons/v_handgr/tris.md2");
	R_RegisterModel ("models/weapons/v_shotg2/tris.md2");
	R_RegisterModel ("models/objects/gibs/bone/tris.md2");
	R_RegisterModel ("models/objects/gibs/sm_meat/tris.md2");
	R_RegisterModel ("models/objects/gibs/bone2/tris.md2");

	Draw_FindPic ("w_machinegun");
	Draw_FindPic ("a_bullets");
	Draw_FindPic ("i_health");
	Draw_FindPic ("a_grenades");
	*/

//ROGUE
#ifdef ROGUE
	cl_mod_explo4_big = R_RegisterModel ("models/objects/r_explode2/tris.md2");
	cl_mod_lightning = R_RegisterModel ("models/proj/lightning/tris.md2");
	cl_mod_heatbeam = R_RegisterModel ("models/proj/beam/tris.md2");
	cl_mod_monster_heatbeam = R_RegisterModel ("models/proj/widowbeam/tris.md2");
#endif
//ROGUE
}	

/*
=================
CL_ClearTEnts
=================
*/
void CL_ClearTEnts (void)
{
	int i;

	memset (cl_beams, 0, sizeof(cl_beams));
	memset (cl_explosions, 0, sizeof(cl_explosions));
	memset (cl_lasers, 0, sizeof(cl_lasers));

	// link explosions
	cl_free_expl = cl_explosions;
	cl_explosions_headnode.prev = &cl_explosions_headnode;
	cl_explosions_headnode.next = &cl_explosions_headnode;
	for ( i = 0; i < MAX_EXPLOSIONS - 1; i++ ) {
		cl_explosions[i].next = &cl_explosions[i+1];
	}

//ROGUE
	memset (cl_playerbeams, 0, sizeof(cl_playerbeams));
	memset (cl_sustains, 0, sizeof(cl_sustains));
//ROGUE
}

/*
=================
CL_AllocExplosion
=================
*/
explosion_t *CL_AllocExplosion (void)
{
	explosion_t *ex;

	if ( cl_free_expl )
	{ // take a free explosion if possible
		ex = cl_free_expl;
		cl_free_expl = ex->next;
	} 
	else 
	{ // grab the oldest one otherwise
		ex = cl_explosions_headnode.prev;
		ex->prev->next = ex->next;
		ex->next->prev = ex->prev;
	}

	memset ( ex, 0, sizeof (*ex) );
	ex->start = cl.serverTime - 100;

	// put the decal at the start of the list
	ex->prev = &cl_explosions_headnode;
	ex->next = cl_explosions_headnode.next;
	ex->next->prev = ex;
	ex->prev->next = ex;

	return ex;
}

/*
=================
CL_FreeExplosion
=================
*/
void CL_FreeExplosion ( explosion_t *ex )
{
	// remove from linked active list
	ex->prev->next = ex->next;
	ex->next->prev = ex->prev;

	// insert into linked free list
	ex->next = cl_free_expl;
	cl_free_expl = ex;
}

/*
=================
CL_SmokeAndFlash
=================
*/
void CL_SmokeAndFlash(const vec3_t origin)
{
	explosion_t	*ex;
	cdlight_t *dl;

	ex = CL_AllocExplosion ();
	VectorCopy (origin, ex->ent.origin);
	ex->type = ex_misc;
	ex->frames = 4;
	ex->ent.flags = RF_TRANSLUCENT;
	ex->ent.model = cl_mod_smoke;

	ex = CL_AllocExplosion ();
	VectorCopy (origin, ex->ent.origin);
	ex->type = ex_flash;
	ex->ent.flags = RF_FULLBRIGHT;
	ex->frames = 2;
	ex->ent.model = cl_mod_flash;

	dl = CL_AllocDlight(0);
	VectorCopy(origin, dl->origin);
	VectorSet(dl->color, 1.0, 0.5, 0.0);
	dl->radius = 35;
	dl->die = cl.time + 70;
}

/*
=================
CL_ParseParticles
=================
*/
/*void CL_ParseParticles (void)
{
	int		color, count;
	vec3_t	pos, dir;

	MSG_ReadPos (&net_message, pos);
	MSG_ReadDir (&net_message, dir);

	color = MSG_ReadByte (&net_message);

	count = MSG_ReadByte (&net_message);

	CL_ParticleEffect (pos, dir, color, count);
}*/

/*
=================
CL_ParseBeam
=================
*/
static int CL_ParseBeam (sizebuf_t *msg, struct model_s *model)
{
	int		ent;
	vec3_t	start, end;
	beam_t	*b;
	int		i;
	
	ent = MSG_ReadShort (msg);
	
	MSG_ReadPos (msg, start);
	MSG_ReadPos (msg, end);

// override any beam with the same entity
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++) {
		if (b->entity == ent)
		{
			//b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorClear (b->offset);
			return ent;
		}
	}

// find a free beam
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorClear (b->offset);
			return ent;
		}
	}
	Com_Printf ("beam list overflow!\n");	
	return ent;
}

/*
=================
CL_ParseBeam2
=================
*/
static int CL_ParseBeam2 (sizebuf_t *msg, struct model_s *model)
{
	int		ent;
	vec3_t	start, end, offset;
	beam_t	*b;
	int		i;
	
	ent = MSG_ReadShort (msg);
	
	MSG_ReadPos (msg, start);
	MSG_ReadPos (msg, end);
	MSG_ReadPos (msg, offset);

// override any beam with the same entity

	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++) {
		if (b->entity == ent)
		{
			//b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorCopy (offset, b->offset);
			return ent;
		}
	}

// find a free beam
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 200;	
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorCopy (offset, b->offset);
			return ent;
		}
	}
	Com_Printf ("beam list overflow!\n");	
	return ent;
}

// ROGUE
/*
=================
CL_ParsePlayerBeam
  - adds to the cl_playerbeam array instead of the cl_beams array
=================
*/
static int CL_ParsePlayerBeam (sizebuf_t *msg, struct model_s *model)
{
	int		ent;
	vec3_t	start, end, offset;
	beam_t	*b;
	int		i;
	
	ent = MSG_ReadShort (msg);
	
	MSG_ReadPos (msg, start);
	MSG_ReadPos (msg, end);
	// PMM - network optimization
	if (model == cl_mod_heatbeam)
		VectorSet(offset, 2, 7, -3);
	else if (model == cl_mod_monster_heatbeam)
	{
		model = cl_mod_heatbeam;
		VectorSet(offset, 0, 0, 0);
	}
	else
		MSG_ReadPos (msg, offset);

// override any beam with the same entity
// PMM - For player beams, we only want one per player (entity) so..
	for (i=0, b=cl_playerbeams ; i< MAX_BEAMS ; i++, b++)
	{
		if (b->entity == ent)
		{
			//b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorCopy (offset, b->offset);
			return ent;
		}
	}

// find a free beam
	for (i=0, b=cl_playerbeams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 100;		// PMM - this needs to be 100 to prevent multiple heatbeams
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorCopy (offset, b->offset);
			return ent;
		}
	}
	Com_Printf ("beam list overflow!\n");	
	return ent;
}
//rogue

/*
=================
CL_ParseLightning
=================
*/
static int CL_ParseLightning (sizebuf_t *msg, struct model_s *model)
{
	int		srcEnt, destEnt;
	vec3_t	start, end;
	beam_t	*b;
	int		i;
	
	srcEnt = MSG_ReadShort (msg);
	destEnt = MSG_ReadShort (msg);

	MSG_ReadPos (msg, start);
	MSG_ReadPos (msg, end);

// override any beam with the same source AND destination entities
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++) {
		if (b->entity == srcEnt && b->dest_entity == destEnt)
		{
			//b->entity = srcEnt;
			//b->dest_entity = destEnt;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorClear (b->offset);
			return srcEnt;
		}
	}

// find a free beam
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = srcEnt;
			b->dest_entity = destEnt;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorClear (b->offset);
			return srcEnt;
		}
	}
	Com_Printf ("beam list overflow!\n");	
	return srcEnt;
}

/*
=================
CL_ParseLaser
=================
*/
static void CL_ParseLaser (sizebuf_t *msg, int colors)
{
	vec3_t	start;
	vec3_t	end;
	laser_t	*l;
	int		i;

	MSG_ReadPos (msg, start);
	MSG_ReadPos (msg, end);

	for (i = 0, l = cl_lasers; i < MAX_LASERS; i++, l++)
	{
		if (l->endtime < cl.time)
		{
			l->ent.flags = RF_TRANSLUCENT | RF_BEAM;
			VectorCopy (start, l->ent.origin);
			VectorCopy (end, l->ent.oldorigin);
			l->ent.alpha = 0.30f;
			l->ent.skinnum = (colors >> ((rand() % 4)*8)) & 0xff;
			l->ent.model = NULL;
			l->ent.frame = 4;
			l->endtime = cl.time + 100;
#ifdef GL_QUAKE
			AxisClear(l->ent.axis);
#endif
			return;
		}
	}
}

//=============
//ROGUE
static void CL_ParseSteam (sizebuf_t *msg)
{
	vec3_t	pos, dir;
	int		id, i, r, cnt;
	int		color, magnitude;
	cl_sustain_t	*s, *free_sustain;

	id = MSG_ReadShort (msg);		// an id of -1 is an instant effect
	if (id != -1) // sustains
	{
		free_sustain = NULL;
		for (i=0, s=cl_sustains; i<MAX_SUSTAINS; i++, s++)
		{
			if (s->id == 0)
			{
				free_sustain = s;
				break;
			}
		}
		if (free_sustain)
		{
			s->id = id;
			s->count = MSG_ReadByte (msg);
			MSG_ReadPos (msg, s->org);
			MSG_ReadDir (msg, s->dir);
			r = MSG_ReadByte (msg);
			s->color = r & 0xff;
			s->magnitude = MSG_ReadShort (msg);
			s->endtime = cl.time + MSG_ReadLong (msg);
			s->think = CL_ParticleSteamEffect2;
			s->thinkinterval = 100;
			s->nextthink = cl.time;
		}
		else
		{
			// FIXME - read the stuff anyway
			cnt = MSG_ReadByte (msg);
			MSG_ReadPos (msg, pos);
			MSG_ReadDir (msg, dir);
			r = MSG_ReadByte (msg);
			magnitude = MSG_ReadShort (msg);
			magnitude = MSG_ReadLong (msg); // really interval
		}
	}
	else // instant
	{
		cnt = MSG_ReadByte (msg);
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
		r = MSG_ReadByte (msg);
		magnitude = MSG_ReadShort (msg);
		color = r & 0xff;
		CL_ParticleSteamEffect (pos, dir, color, cnt, magnitude);
	}
}

static void CL_ParseWidow (sizebuf_t *msg)
{
	vec3_t	pos;
	int		id, i;
	cl_sustain_t	*s, *free_sustain = NULL;

	id = MSG_ReadShort (msg);

	for (i=0, s=cl_sustains; i<MAX_SUSTAINS; i++, s++)
	{
		if (s->id == 0)
		{
			free_sustain = s;
			break;
		}
	}
	if (free_sustain)
	{
		s->id = id;
		MSG_ReadPos (msg, s->org);
		s->endtime = cl.time + 2100;
		s->think = CL_Widowbeamout;
		s->thinkinterval = 1;
		s->nextthink = cl.time;
	}
	else // no free sustains
	{
		// FIXME - read the stuff anyway
		MSG_ReadPos (msg, pos);
	}
}

static void CL_ParseNuke (sizebuf_t *msg)
{
	vec3_t	pos;
	int		i;
	cl_sustain_t	*s, *free_sustain = NULL;

	for (i=0, s=cl_sustains; i<MAX_SUSTAINS; i++, s++)
	{
		if (s->id == 0)
		{
			free_sustain = s;
			break;
		}
	}
	if (free_sustain)
	{
		s->id = 21000;
		MSG_ReadPos (msg, s->org);
		s->endtime = cl.time + 1000;
		s->think = CL_Nukeblast;
		s->thinkinterval = 1;
		s->nextthink = cl.time;
	}
	else // no free sustains
	{
		// FIXME - read the stuff anyway
		MSG_ReadPos (msg, pos);
	}
}

//ROGUE
//=============


/*
=================
CL_ParseTEnt
=================
*/
static const byte splash_color[] = {0x00, 0xe0, 0xb0, 0x50, 0xd0, 0xe0, 0xe8};

void CL_ParseTEnt (sizebuf_t *msg)
{
	int		type;
	vec3_t	pos, pos2, dir;
	explosion_t	*ex;
	int		cnt;
	int		color;
	int		r;
	int		ent;

	type = MSG_ReadByte (msg);

	switch (type)
	{
	case TE_BLOOD:			// bullet hitting flesh
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
		CL_ParticleEffect (pos, dir, 0xe8, 60);
		break;

	case TE_GUNSHOT:			// bullet hitting wall
	case TE_SPARKS:
	case TE_BULLET_SPARKS:
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
		if (type == TE_GUNSHOT)
			CL_ParticleEffect (pos, dir, 0, 40);
		else
			CL_ParticleEffect (pos, dir, 0xe0, 6);

		if (type != TE_SPARKS)
		{
#ifdef GL_QUAKE
			R_AddDecal(pos, dir, 0, 0, 0, 1.0f, 2 + ((rand()%21*0.05f) - 0.5f), 1, 0, rand()%361);
#endif
			CL_SmokeAndFlash(pos);

			R_AddStain(pos, 1, 5);

			// impact sound
			cnt = rand()&15;
			if (cnt == 1)
				S_StartSound (pos, 0, 0, cl_sfx_ric1, 1, ATTN_NORM, 0);
			else if (cnt == 2)
				S_StartSound (pos, 0, 0, cl_sfx_ric2, 1, ATTN_NORM, 0);
			else if (cnt == 3)
				S_StartSound (pos, 0, 0, cl_sfx_ric3, 1, ATTN_NORM, 0);
		}

		break;
		
	case TE_SCREEN_SPARKS:
	case TE_SHIELD_SPARKS:
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
		if (type == TE_SCREEN_SPARKS)
			CL_ParticleEffect (pos, dir, 0xd0, 40);
		else
			CL_ParticleEffect (pos, dir, 0xb0, 40);
		//FIXME : replace or remove this sound
		S_StartSound (pos, 0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
		break;
		
	case TE_SHOTGUN:			// bullet hitting wall
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
#ifdef GL_QUAKE
		R_AddDecal(pos, dir, 0, 0, 0, 1.0f, 2 + ((rand()%21*0.05f) - 0.5f), 1, 0, rand()%361);
#endif
		CL_ParticleEffect (pos, dir, 0, 20);
		CL_SmokeAndFlash(pos);

		R_AddStain(pos, 1, 9);
		break;

	case TE_SPLASH:			// bullet hitting water
		cnt = MSG_ReadByte (msg);
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
		r = MSG_ReadByte (msg);
		if (r > 6)
			color = 0x00;
		else
			color = splash_color[r];
		CL_ParticleEffect (pos, dir, color, cnt);

		if (r == SPLASH_SPARKS)
		{
			r = rand() & 3;
			if (r == 0)
				S_StartSound (pos, 0, 0, cl_sfx_spark5, 1, ATTN_STATIC, 0);
			else if (r == 1)
				S_StartSound (pos, 0, 0, cl_sfx_spark6, 1, ATTN_STATIC, 0);
			else
				S_StartSound (pos, 0, 0, cl_sfx_spark7, 1, ATTN_STATIC, 0);
		}
		break;

	case TE_LASER_SPARKS:
		cnt = MSG_ReadByte (msg);
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
		color = MSG_ReadByte (msg);
		CL_ParticleEffect2 (pos, dir, color, cnt);
		break;

	// RAFAEL
	case TE_BLUEHYPERBLASTER:
		MSG_ReadPos (msg, pos);
		MSG_ReadPos (msg, dir);
		CL_BlasterParticles (pos, dir);
		break;

	case TE_BLASTER:			// blaster hitting wall
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
		CL_BlasterParticles (pos, dir);

		R_AddStain(pos, 2, 10);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->ent.angles[0] = (float)acos(dir[2])/M_PI*180;
	// PMM - fixed to correct for pitch of 0
		if (dir[0])
			ex->ent.angles[1] = (float)atan2(dir[1], dir[0])/M_PI*180;
		else if (dir[1] > 0)
			ex->ent.angles[1] = 90;
		else if (dir[1] < 0)
			ex->ent.angles[1] = 270;
		else
			ex->ent.angles[1] = 0;

		ex->type = ex_misc;
		ex->ent.flags = RF_FULLBRIGHT|RF_TRANSLUCENT;
		ex->light = 150;
		VectorSet(ex->lightcolor, 1.0, 0.3, 0.0);
		//ex->lightcolor[0] =	ex->lightcolor[1] = 1;
		ex->ent.model = cl_mod_explode;
		ex->frames = 4;
		S_StartSound (pos,  0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
		break;
		
	case TE_RAILTRAIL:			// railgun effect
		MSG_ReadPos (msg, pos);
		MSG_ReadPos (msg, pos2);
		CL_RailTrail (pos, pos2);
		S_StartSound (pos2, 0, 0, cl_sfx_railg, 1, ATTN_NORM, 0);
		break;

	case TE_EXPLOSION2:
	case TE_GRENADE_EXPLOSION:
	case TE_GRENADE_EXPLOSION_WATER:
		MSG_ReadPos (msg, pos);

		R_AddStain(pos, 3, 35);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->ent.flags = RF_FULLBRIGHT;
		ex->light = 300;
		VectorSet(ex->lightcolor, 1.0f, 0.3f, 0.0f);
		ex->ent.model = cl_mod_explo4;
		ex->frames = 19;
		ex->baseframe = 30;
		ex->ent.angles[1] = rand() % 360;
		CL_ExplosionParticles (pos);
		if (type == TE_GRENADE_EXPLOSION_WATER)
			S_StartSound (pos, 0, 0, cl_sfx_watrexp, 1, ATTN_NORM, 0);
		else
			S_StartSound (pos, 0, 0, cl_sfx_grenexp, 1, ATTN_NORM, 0);
		break;

	// RAFAEL
	case TE_PLASMA_EXPLOSION:
		MSG_ReadPos (msg, pos);
		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->ent.flags = RF_FULLBRIGHT;
		ex->light = 350;
		VectorSet(ex->lightcolor, 1.0f, 0.5f, 0.5f);
		ex->ent.angles[1] = rand() % 360;
		ex->ent.model = cl_mod_explo4;
		if (frand() < 0.5)
			ex->baseframe = 15;
		ex->frames = 15;
		CL_ExplosionParticles (pos);
		S_StartSound (pos, 0, 0, cl_sfx_rockexp, 1, ATTN_NORM, 0);
		break;
	
	case TE_EXPLOSION1:
	case TE_EXPLOSION1_BIG:						// PMM
	case TE_ROCKET_EXPLOSION:
	case TE_ROCKET_EXPLOSION_WATER:
	case TE_EXPLOSION1_NP:						// PMM
		MSG_ReadPos (msg, pos);

		R_AddStain(pos, 3, 35);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->ent.flags = RF_FULLBRIGHT;
		ex->light = 300;
		VectorSet(ex->lightcolor, 1.0f, 0.3f, 0.0f);
		ex->ent.angles[1] = rand() % 360;
		if (type != TE_EXPLOSION1_BIG)				// PMM
			ex->ent.model = cl_mod_explo4;			// PMM
		else
			ex->ent.model = cl_mod_explo4_big;
		if (frand() < 0.5)
			ex->baseframe = 15;
		ex->frames = 15;
		if ((type != TE_EXPLOSION1_BIG) && (type != TE_EXPLOSION1_NP))		// PMM
			CL_ExplosionParticles (pos);									// PMM
		if (type == TE_ROCKET_EXPLOSION_WATER)
			S_StartSound (pos, 0, 0, cl_sfx_watrexp, 1, ATTN_NORM, 0);
		else
			S_StartSound (pos, 0, 0, cl_sfx_rockexp, 1, ATTN_NORM, 0);
		break;

	case TE_BFG_EXPLOSION:
		MSG_ReadPos (msg, pos);
		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->ent.flags = RF_FULLBRIGHT;
		ex->light = 350;
		VectorSet(ex->lightcolor, 0.3f, 1.0f, 0.3f);
		ex->ent.model = cl_mod_bfg_explo;
		ex->ent.flags |= RF_TRANSLUCENT;
		ex->ent.alpha = 0.30f;
		ex->frames = 4;
		break;

	case TE_BFG_BIGEXPLOSION:
		MSG_ReadPos (msg, pos);
		CL_BFGExplosionParticles (pos);
		break;

	case TE_BFG_LASER:
		CL_ParseLaser (msg, 0xd0d1d2d3);
		break;

	case TE_BUBBLETRAIL:
		MSG_ReadPos (msg, pos);
		MSG_ReadPos (msg, pos2);
		CL_BubbleTrail (pos, pos2);
		break;

	case TE_PARASITE_ATTACK:
	case TE_MEDIC_CABLE_ATTACK:
		ent = CL_ParseBeam (msg, cl_mod_parasite_segment);
		break;

	case TE_BOSSTPORT:			// boss teleporting to station
		MSG_ReadPos (msg, pos);
		CL_BigTeleportParticles (pos);
		S_StartSound (pos, 0, 0, S_RegisterSound ("misc/bigtele.wav"), 1, ATTN_NONE, 0);
		break;

	case TE_GRAPPLE_CABLE:
		ent = CL_ParseBeam2 (msg, cl_mod_grapple_cable);
		break;

	// RAFAEL
	case TE_WELDING_SPARKS:
		cnt = MSG_ReadByte (msg);
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
		color = MSG_ReadByte (msg);
		CL_ParticleEffect2 (pos, dir, color, cnt);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_flash;
		// note to self
		// we need a better no draw flag
		ex->ent.flags = RF_BEAM;
		ex->light = 100 + (rand()%75);
		VectorSet(ex->lightcolor, 1.0f, 1.0f, 0.3f);
		ex->ent.model = cl_mod_flash;
		ex->frames = 2;
		break;

	case TE_GREENBLOOD:
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
		CL_ParticleEffect2 (pos, dir, 0xdf, 30);
		break;

	// RAFAEL
	case TE_TUNNEL_SPARKS:
		cnt = MSG_ReadByte (msg);
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
		color = MSG_ReadByte (msg);
		CL_ParticleEffect3 (pos, dir, color, cnt);
		break;

//=============
//PGM
		// PMM -following code integrated for flechette (different color)
	case TE_BLASTER2:			// green blaster hitting wall
	case TE_FLECHETTE:			// flechette
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
		
		// PMM
		if (type == TE_BLASTER2)
			CL_BlasterParticles2 (pos, dir, 0xd0);
		else
			CL_BlasterParticles2 (pos, dir, 0x6f); // 75

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->ent.angles[0] = (float)acos(dir[2])/M_PI*180;
	// PMM - fixed to correct for pitch of 0
		if (dir[0])
			ex->ent.angles[1] = (float)atan2(dir[1], dir[0])/M_PI*180;
		else if (dir[1] > 0)
			ex->ent.angles[1] = 90;
		else if (dir[1] < 0)
			ex->ent.angles[1] = 270;
		else
			ex->ent.angles[1] = 0;

		ex->type = ex_misc;
		ex->ent.flags = RF_FULLBRIGHT|RF_TRANSLUCENT;

		// PMM
		if (type == TE_BLASTER2)
			ex->ent.skinnum = 1;
		else // flechette
			ex->ent.skinnum = 2;

		ex->light = 150;
		// PMM
		if (type == TE_BLASTER2)
			ex->lightcolor[1] = 1;
		else // flechette
			VectorSet(ex->lightcolor, 0.19f, 0.41f, 0.75f);

		ex->ent.model = cl_mod_explode;
		ex->frames = 4;
		S_StartSound (pos,  0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
		break;


	case TE_LIGHTNING:
		ent = CL_ParseLightning(msg, cl_mod_lightning);
		S_StartSound (NULL, ent, CHAN_WEAPON, cl_sfx_lightning, 1, ATTN_NORM, 0);
		break;

	case TE_DEBUGTRAIL:
		MSG_ReadPos (msg, pos);
		MSG_ReadPos (msg, pos2);
		CL_DebugTrail (pos, pos2);
		break;

	case TE_PLAIN_EXPLOSION:
		MSG_ReadPos (msg, pos);

		R_AddStain(pos, 3, 35);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->ent.flags = RF_FULLBRIGHT;
		ex->light = 350;
		VectorSet(ex->lightcolor, 1.0f, 0.5f, 0.5f);
		ex->ent.angles[1] = rand() % 360;
		ex->ent.model = cl_mod_explo4;
		if (frand() < 0.5)
			ex->baseframe = 15;
		ex->frames = 15;
		if (type == TE_ROCKET_EXPLOSION_WATER)
			S_StartSound (pos, 0, 0, cl_sfx_watrexp, 1, ATTN_NORM, 0);
		else
			S_StartSound (pos, 0, 0, cl_sfx_rockexp, 1, ATTN_NORM, 0);
		break;

	case TE_FLASHLIGHT:
		MSG_ReadPos(msg, pos);
		ent = MSG_ReadShort(msg);
		CL_Flashlight(ent, pos);
		break;

	case TE_FORCEWALL:
		MSG_ReadPos(msg, pos);
		MSG_ReadPos(msg, pos2);
		color = MSG_ReadByte (msg);
		CL_ForceWall(pos, pos2, color);
		break;

	case TE_HEATBEAM:
		ent = CL_ParsePlayerBeam (msg, cl_mod_heatbeam);
		break;

	case TE_MONSTER_HEATBEAM:
		ent = CL_ParsePlayerBeam (msg, cl_mod_monster_heatbeam);
		break;

	case TE_HEATBEAM_SPARKS:
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
		CL_ParticleSteamEffect (pos, dir, 0x8, 50, 60);
		S_StartSound (pos,  0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
		break;
	
	case TE_HEATBEAM_STEAM:
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
		CL_ParticleSteamEffect (pos, dir, 0xe0, 20, 60);
		S_StartSound (pos,  0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
		break;

	case TE_STEAM:
		CL_ParseSteam(msg);
		break;

	case TE_BUBBLETRAIL2:
		MSG_ReadPos (msg, pos);
		MSG_ReadPos (msg, pos2);
		CL_BubbleTrail2 (pos, pos2, 8);
		S_StartSound (pos,  0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
		break;

	case TE_MOREBLOOD:
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
		CL_ParticleEffect (pos, dir, 0xe8, 250);
		break;

	case TE_CHAINFIST_SMOKE:
		VectorSet(dir, 0, 0, 1);
		MSG_ReadPos(msg, pos);
		CL_ParticleSmokeEffect (pos, dir, 0, 20, 20);
		break;

	case TE_ELECTRIC_SPARKS:
		MSG_ReadPos (msg, pos);
		MSG_ReadDir (msg, dir);
		CL_ParticleEffect (pos, dir, 0x75, 40);
		//FIXME : replace or remove this sound
		S_StartSound (pos, 0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
		break;

	case TE_TRACKER_EXPLOSION:
		MSG_ReadPos (msg, pos);
		CL_ColorFlash (pos, 0, 150, -1, -1, -1);
		CL_ColorExplosionParticles (pos, 0, 1);
		S_StartSound (pos, 0, 0, cl_sfx_disrexp, 1, ATTN_NORM, 0);
		break;

	case TE_TELEPORT_EFFECT:
	case TE_DBALL_GOAL:
		MSG_ReadPos (msg, pos);
		CL_TeleportParticles (pos);
		break;

	case TE_WIDOWBEAMOUT:
		CL_ParseWidow (msg);
		break;

	case TE_NUKEBLAST:
		CL_ParseNuke (msg);
		break;

	case TE_WIDOWSPLASH:
		MSG_ReadPos (msg, pos);
		CL_WidowSplash (pos);
		break;
//PGM
//==============

	default:
		Com_Error (ERR_DROP, "CL_ParseTEnt: bad type");
	}
}

/*
=================
CL_AddBeams
=================
*/
void CL_AddBeams (void)
{
	int			i;
	beam_t		*b;
	vec3_t		dist, org;
	float		d;
	entity_t	ent;
	float		yaw, pitch;
	float		forward;
	float		len, steps;
	float		model_length;
	
// update beams
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
			continue;

		// if coming from the player, update the start position
		if (b->entity == cl.playernum+1)	// entity 0 is the world
		{
			VectorCopy (cl.refdef.vieworg, b->start);
			b->start[2] -= 22;	// adjust for view height
		}
		VectorAdd (b->start, b->offset, org);

	// calculate pitch and yaw
		VectorSubtract (b->end, org, dist);

		if (dist[1] == 0.0f && dist[0] == 0.0f)
		{
			yaw = 0;
			if (dist[2] > 0.0f)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
	// PMM - fixed to correct for pitch of 0
			if (dist[0])
				yaw = ((float)atan2(dist[1], dist[0]) * 180.0f / M_PI);
			else if (dist[1] > 0.0f)
				yaw = 90;
			else
				yaw = 270;
			if (yaw < 0.0f)
				yaw += 360;
	
			forward = (float)sqrt(dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = ((float)atan2(dist[2], forward) * -180.0 / M_PI);
			if (pitch < 0.0f)
				pitch += 360.0f;
		}

	// add new entities for the beams
		d = VectorNormalize(dist);

		memset (&ent, 0, sizeof(ent));
		if (b->model == cl_mod_lightning)
		{
			model_length = 35.0f;
			d-= 20.0f;  // correction so it doesn't end in middle of tesla
		}
		else
		{
			model_length = 30.0f;
		}
		steps = (float)ceil(d/model_length);
		len = (d-model_length)/(steps-1);

		// PMM - special case for lightning model .. if the real length is shorter than the model,
		// flip it around & draw it from the end to the start.  This prevents the model from going
		// through the tesla mine (instead it goes through the target)
		if ((b->model == cl_mod_lightning) && (d <= model_length))
		{
			VectorCopy (b->end, ent.origin);
			ent.model = b->model;
			ent.flags = RF_FULLBRIGHT;
			VectorSet(ent.angles, pitch, yaw, rand()%360);
#ifdef GL_QUAKE
			AnglesToAxis(ent.angles, ent.axis);
#endif
			V_AddEntity (&ent);
			return;
		}
		while (d > 0)
		{
			VectorCopy (org, ent.origin);
			ent.model = b->model;
			if (b->model == cl_mod_lightning) {
				ent.flags = RF_FULLBRIGHT;
				VectorSet(ent.angles, -pitch, yaw + 180.0f, rand()%360);
			}
			else {
				VectorSet(ent.angles, pitch, yaw, rand()%360);
			}
			
#ifdef GL_QUAKE
			AnglesToAxis(ent.angles, ent.axis);
#endif
			V_AddEntity (&ent);

			VectorMA (org, len, dist, org);

			d -= model_length;
		}
	}
}

extern cvar_t *info_hand;

/*
=================
ROGUE - draw player locked beams
CL_AddPlayerBeams
=================
*/
void CL_AddPlayerBeams (void)
{
	int			i,j;
	beam_t		*b;
	vec3_t		dist, org;
	float		d;
	entity_t	ent;
	float		yaw, pitch;
	float		forward;
	float		len, steps;
	int			framenum = 0;
	float		model_length;
	
	float		hand_multiplier;
	frame_t		*oldframe;
	player_state_t	*ps, *ops;

//PMM
	if (info_hand->integer == 2)
		hand_multiplier = 0;
	else if (info_hand->integer == 1)
		hand_multiplier = -1;
	else
		hand_multiplier = 1;
//PMM

// update beams
	for (i=0, b=cl_playerbeams ; i< MAX_BEAMS ; i++, b++)
	{
		vec3_t		f,r,u;
		if (!b->model || b->endtime < cl.time)
			continue;

		if(cl_mod_heatbeam && (b->model == cl_mod_heatbeam))
		{

			// if coming from the player, update the start position
			if (b->entity == cl.playernum+1)	// entity 0 is the world
			{	
				// set up gun position
				// code straight out of CL_AddViewWeapon
				ps = &cl.frame.playerstate;
				j = (cl.frame.serverframe - 1) & UPDATE_MASK;
				oldframe = &cl.frames[j];
				if (oldframe->serverframe != cl.frame.serverframe-1 || !oldframe->valid)
					oldframe = &cl.frame;		// previous frame was dropped or involid
				ops = &oldframe->playerstate;
				for (j=0 ; j<3 ; j++)
				{
					b->start[j] = cl.refdef.vieworg[j] + ops->gunoffset[j]
						+ cl.lerpfrac * (ps->gunoffset[j] - ops->gunoffset[j]);
				}
				VectorMA (b->start, (hand_multiplier * b->offset[0]), cl.v_right, org);
				VectorMA (     org, b->offset[1], cl.v_forward, org);
				VectorMA (     org, b->offset[2], cl.v_up, org);
				if (info_hand->integer == 2) {
					VectorMA (org, -1, cl.v_up, org);
				}
				// FIXME - take these out when final
				VectorCopy (cl.v_right, r);
				VectorCopy (cl.v_forward, f);
				VectorCopy (cl.v_up, u);

			}
			else
				VectorCopy (b->start, org);
		}
		else
		{
			// if coming from the player, update the start position
			if (b->entity == cl.playernum+1)	// entity 0 is the world
			{
				VectorCopy (cl.refdef.vieworg, b->start);
				b->start[2] -= 22;	// adjust for view height
			}
			VectorAdd (b->start, b->offset, org);
		}

	// calculate pitch and yaw
		VectorSubtract (b->end, org, dist);

//PMM
		if(cl_mod_heatbeam && (b->model == cl_mod_heatbeam) && (b->entity == cl.playernum+1))
		{
			len = (float)VectorLength (dist);
			VectorScale (f, len, dist);
			VectorMA (dist, (hand_multiplier * b->offset[0]), r, dist);
			VectorMA (dist, b->offset[1], f, dist);
			VectorMA (dist, b->offset[2], u, dist);
			if (info_hand->integer == 2)
				VectorMA (org, -1, cl.v_up, org);
		}
//PMM

		if (dist[1] == 0.0f && dist[0] == 0.0f)
		{
			yaw = 0;
			if (dist[2] > 0.0f)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
	// PMM - fixed to correct for pitch of 0
			if (dist[0])
				yaw = ((float)atan2(dist[1], dist[0]) * 180.0f / M_PI);
			else if (dist[1] > 0)
				yaw = 90;
			else
				yaw = 270;
			if (yaw < 0)
				yaw += 360;
	
			forward = (float)sqrt(dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = ((float)atan2(dist[2], forward) * -180.0f / M_PI);
			if (pitch < 0)
				pitch += 360.0f;
		}
		
		if (cl_mod_heatbeam && (b->model == cl_mod_heatbeam))
		{
			if (b->entity != cl.playernum+1)
			{
				framenum = 2;
				VectorSet(ent.angles, -pitch, yaw + 180.0f, 0);
				AngleVectors(ent.angles, f, r, u);
					
				// if it's a non-origin offset, it's a player, so use the hardcoded player offset
				if (!VectorCompare (b->offset, vec3_origin))
				{
					VectorMA (org, -(b->offset[0])+1, r, org);
					VectorMA (org, -(b->offset[1]), f, org);
					VectorMA (org, -(b->offset[2])-10, u, org);
				}
				else
				{
					// if it's a monster, do the particle effect
					CL_MonsterPlasma_Shell(b->start);
				}
			}
			else
			{
				framenum = 1;
				// if it's the heatbeam, draw the particle effect
				CL_Heatbeam (org, dist);
			}
		}

	// add new entities for the beams
		d = VectorNormalize(dist);

		memset (&ent, 0, sizeof(ent));
		if (b->model == cl_mod_heatbeam)
		{
			model_length = 32.0f;
		}
		else if (b->model == cl_mod_lightning)
		{
			model_length = 35.0f;
			d-= 20.0f;  // correction so it doesn't end in middle of tesla
		}
		else
		{
			model_length = 30.0f;
		}
		steps = (float)ceil(d/model_length);
		len = (d-model_length)/(steps-1);

		// PMM - special case for lightning model .. if the real length is shorter than the model,
		// flip it around & draw it from the end to the start.  This prevents the model from going
		// through the tesla mine (instead it goes through the target)
		if ((b->model == cl_mod_lightning) && (d <= model_length))
		{
			VectorCopy (b->end, ent.origin);
			ent.model = b->model;
			ent.flags = RF_FULLBRIGHT;
			VectorSet(ent.angles, pitch, yaw, rand()%360);
#ifdef GL_QUAKE
			AnglesToAxis(ent.angles, ent.axis);
#endif
			V_AddEntity (&ent);			
			return;
		}
		while (d > 0)
		{
			VectorCopy (org, ent.origin);
			ent.model = b->model;
			if(cl_mod_heatbeam && (b->model == cl_mod_heatbeam))
			{
				ent.flags = RF_FULLBRIGHT;
				VectorSet(ent.angles, -pitch, yaw + 180.0f, (cl.time) % 360);
				ent.frame = framenum;
			}
			else if (b->model == cl_mod_lightning)
			{
				ent.flags = RF_FULLBRIGHT;
				VectorSet(ent.angles, -pitch, yaw + 180.0f, rand()%360);
			}
			else
			{
				VectorSet(ent.angles, pitch, yaw, rand()%360);
			}
#ifdef GL_QUAKE
			AnglesToAxis(ent.angles, ent.axis);
#endif
			V_AddEntity (&ent);

			VectorMA (org, len, dist, org);

			d -= model_length;
		}
	}
}

/*
=================
CL_AddExplosions
=================
*/
void CL_AddExplosions (void)
{
	entity_t	*ent;
	explosion_t	*ex, *next, *hnode;
	float		frac;
	int			f;

	//memset (&ent, 0, sizeof(ent));

	hnode = &cl_explosions_headnode;
	for (ex = hnode->next; ex != hnode; ex = next)
	{
		next = ex->next;
		ent = &ex->ent;

		// translucent is prettier
		ent->flags |= RF_TRANSLUCENT;

		frac = (cl.time - ex->start)*0.01f;
		f = (int)floor(frac);

		switch (ex->type)
		{
		case ex_mflash:
			if (f >= ex->frames-1)
				ex->type = ex_free;
			break;
		case ex_misc:
			if (f >= ex->frames-1)
			{
				ex->type = ex_free;
				break;
			}
			ent->alpha = 1.0f - frac/(ex->frames-1);
			break;
		case ex_flash:
			if (f >= 1)
			{
				ex->type = ex_free;
				break;
			}
			ent->alpha = 1.0;
			break;
		case ex_poly:
			if (f >= ex->frames-1)
			{
				ex->type = ex_free;
				break;
			}

			ent->alpha = (16.0f - (float)f)/16.0f;

			if (f < 10)
			{
				ent->skinnum = (f>>1);
				if (ent->skinnum < 0)
					ent->skinnum = 0;
			}
			else
			{
				if (f < 13)
					ent->skinnum = 5;
				else
					ent->skinnum = 6;
			}
			break;
		case ex_poly2:
			if (f >= ex->frames-1)
			{
				ex->type = ex_free;
				break;
			}

			ent->alpha = (5.0f - (float)f)/5.0f;
			ent->skinnum = 0;
			break;
		default:
			break;
		}

		if (ex->type == ex_free) 
		{
			CL_FreeExplosion (ex);
			continue;
		}

		if (ex->light)
		{
			V_AddLight (ent->origin, ex->light*ent->alpha,
				ex->lightcolor[0], ex->lightcolor[1], ex->lightcolor[2]);
		}

		// now that the light has been setup, scale alpha back
		ent->alpha *= 0.25;

		VectorCopy (ent->origin, ent->oldorigin);

		if (f < 0)
			f = 0;
		ent->frame = ex->baseframe + f + 1;
		ent->oldframe = ex->baseframe + f;
		ent->backlerp = 1.0f - cl.lerpfrac;

#ifdef GL_QUAKE
		AnglesToAxis(ent->angles, ent->axis);
#endif
		V_AddEntity (ent);
	}
}


/*
=================
CL_AddLasers
=================
*/
void CL_AddLasers (void)
{
	laser_t		*l;
	int			i;

	for (i = 0, l = cl_lasers; i < MAX_LASERS; i++, l++)
	{
		if (l->endtime >= cl.time)
			V_AddEntity (&l->ent);
	}
}

/* PMM - CL_Sustains */
void CL_ProcessSustain (void)
{
	cl_sustain_t	*s;
	int				i;

	for (i = 0, s = cl_sustains; i < MAX_SUSTAINS; i++, s++)
	{
		if (s->id)
		{
			if (s->endtime >= cl.time && cl.time >= s->nextthink)
				s->think (s);
			else if (s->endtime < cl.time)
				s->id = 0;
		}
	}
}

/*
=================
CL_AddTEnts
=================
*/
void CL_AddTEnts (void)
{
	CL_AddBeams ();
	// PMM - draw plasma beams
	CL_AddPlayerBeams ();
	CL_AddExplosions ();
	CL_AddLasers ();
	// PMM - set up sustain
	CL_ProcessSustain();
}
