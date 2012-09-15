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
// cl_newfx.c -- MORE entity effects parsing and management

#include "client.h"

extern cparticle_t	*active_particles, *free_particles;
extern cparticle_t	particles[MAX_PARTICLES];
extern int			cl_numparticles;

//=============
//=============
void CL_Flashlight (int ent, const vec3_t pos)
{
	cdlight_t	*dl;

	dl = CL_AllocDlight (ent);
	VectorCopy (pos,  dl->origin);
	dl->radius = 400;
	//dl->minlight = 250;
	dl->die = cl.time + 100;
	//dl->decay = 0;
	dl->color[0] = dl->color[1] = dl->color[2] = 1;
}

/*
======
CL_ColorFlash - flash of light
======
*/
void CL_ColorFlash (const vec3_t pos, int ent, int intensity, float r, float g, float b)
{
	cdlight_t	*dl;

#ifndef GL_QUAKE
	if((r < 0) || (g<0) || (b<0))
	{
		intensity = -intensity;
		r = -r;
		g = -g;
		b = -b;
	}
#endif

	dl = CL_AllocDlight (ent);
	VectorCopy (pos,  dl->origin);
	dl->radius = intensity;
	//dl->minlight = 250;
	dl->die = cl.time + 100;
	//dl->decay = 0;
	VectorSet(dl->color, r, g, b);
}


/*
======
CL_DebugTrail
======
*/
void CL_DebugTrail (const vec3_t start, const vec3_t end)
{
	vec3_t		move, vec;
	float		len;
	cparticle_t	*p;
	float		dec = 3;
	vec3_t		right, up;


	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	MakeNormalVectors (vec, right, up);

	VectorScale (vec, dec, vec);
	VectorCopy (start, move);

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		VectorClear (p->accel);
		VectorClear (p->vel);
		p->alpha = 1.0f;
		p->alphavel = -0.1f;
		p->color = 0x74 + (rand()&7);
		VectorCopy (move, p->org);

		VectorAdd (move, vec, move);
	}

}

/*
===============
CL_SmokeTrail
===============
*/
void CL_SmokeTrail (const vec3_t start, const vec3_t end, int colorStart, int colorRun, int spacing)
{
	vec3_t		move, vec;
	float		len;
	cparticle_t	*p;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	VectorScale (vec, spacing, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= spacing;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0f;
		p->alphavel = -1.0f / (1.0f+frand()*0.5f);
		p->color = colorStart + (rand() % colorRun);

		p->org[0] = move[0] + crand()*3;
		p->org[1] = move[1] + crand()*3;
		p->org[2] = move[2] + crand()*3;

		p->vel[2] = 20 + crand()*5;

		VectorAdd (move, vec, move);
	}
}

void CL_ForceWall (const vec3_t start, const vec3_t end, int color)
{
	vec3_t		move, vec;
	float		len, dec = 4;
	cparticle_t	*p;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	VectorScale (vec, dec, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;
		
		if (frand() > 0.3f)
		{
			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;
			VectorClear (p->accel);
			
			p->time = cl.time;

			p->alpha = 1.0f;
			p->alphavel =  -1.0f / (3.0f+frand()*0.5f);
			p->color = color;

			p->org[0] = move[0] + crand()*3;
			p->org[1] = move[1] + crand()*3;
			p->org[2] = move[2] + crand()*3;

			p->vel[0] = p->vel[1] = 0;
			p->vel[2] = -40 - (crand()*10);
		}

		VectorAdd (move, vec, move);
	}
}

/*void CL_FlameEffects (centity_t *ent, vec3_t origin)
{
	int			n, count;
	int			j;
	cparticle_t	*p;

	count = rand() & 0xF;

	for(n=0;n<count;n++)
	{
		if (!free_particles)
			return;
			
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		
		VectorClear (p->accel);
		p->time = cl.time;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1+frand()*0.2);
		p->color = 226 + (rand() % 4);
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = origin[j] + crand()*5;
			p->vel[j] = crand()*5;
		}
		p->vel[2] = crand() * -10;
		p->accel[2] = -PARTICLE_GRAVITY;
	}

	count = rand() & 0x7;

	for(n=0;n<count;n++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1+frand()*0.5);
		p->color = 0 + (rand() % 4);
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = origin[j] + crand()*3;
		}
		p->vel[2] = 20 + crand()*5;
	}

}*/


/*
===============
CL_GenericParticleEffect
===============
*/
void CL_GenericParticleEffect (const vec3_t org, const vec3_t dir, int color, int count, int numcolors, int dirspread, float alphavel)
{
	int			i;
	cparticle_t	*p;
	float		d;

	for (i=0 ; i<count ; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		if (numcolors > 1)
			p->color = color + (rand() & numcolors);
		else
			p->color = color;

		d = rand() & dirspread;
		p->org[0] = org[0] + ((rand()&7)-4) + d*dir[0];
		p->org[1] = org[1] + ((rand()&7)-4) + d*dir[1];
		p->org[2] = org[2] + ((rand()&7)-4) + d*dir[2];
		p->vel[0] = crand()*20;
		p->vel[1] = crand()*20;
		p->vel[2] = crand()*20;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0f;

		p->alphavel = -1.0f / (0.5f + frand()*alphavel);
	}
}

/*
===============
CL_BubbleTrail2 (lets you control the # of bubbles by setting the distance between the spawns)

===============
*/
void CL_BubbleTrail2 (const vec3_t start, const vec3_t end, int dist)
{
	vec3_t		move, vec;
	float		len;
	int			i;
	cparticle_t	*p;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	VectorScale (vec, dist, vec);

	for (i = 0; i < len; i += dist)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorClear (p->accel);
		p->time = cl.time;

		p->alpha = 1.0f;
		p->alphavel = -1.0f / (1.0f+frand()*0.1f);
		p->color = 4 + (rand()&7);

		p->org[0] = move[0] + crand()*2;
		p->org[1] = move[1] + crand()*2;
		p->org[2] = move[2] + crand()*2 - 4;
		p->vel[0] = crand()*10;
		p->vel[1] = crand()*10;
		p->vel[2] = crand()*10 + 20;

		VectorAdd (move, vec, move);
	}
}

//#define CORKSCREW		1
//#define DOUBLE_SCREW	1
#define	RINGS		1
//#define	SPRAY		1

#ifdef CORKSCREW
void CL_Heatbeam (const vec3_t start, const vec3_t end)
{
	vec3_t		move, vec;
	float		len;
	int			j,k;
	cparticle_t	*p;
	vec3_t		right, up;
	int			i;
	float		d, c, s;
	vec3_t		dir;
	float		ltime;
	float		step = 5.0;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

//	MakeNormalVectors (vec, right, up);
	VectorCopy (cl.v_right, right);
	VectorCopy (cl.v_up, up);
	VectorMA (move, -1, right, move);
	VectorMA (move, -1, up, move);

	VectorScale (vec, step, vec);
	ltime = (float) cl.time/1000.0;

	for (i=0 ; i<len ; i+=step)
	{
		d = i * 0.1f - (float)fmod(ltime,16.0)*M_PI;
		c = (float)cos(d)/1.75;
		s = (float)sin(d)/1.75;
#ifdef DOUBLE_SCREW		
		for (k=-1; k<2; k+=2)
		{
#else
		k=1;
#endif
			if (!free_particles)
				return;

			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;
			
			p->time = cl.time;
			VectorClear (p->accel);

			p->alpha = 0.5;
			// only last one frame!
			p->alphavel = INSTANT_PARTICLE;
			p->color = 223;

			// trim it so it looks like it's starting at the origin
			if (i < 10)
			{
				VectorScale (right, c*(i/10.0)*k, dir);
				VectorMA (dir, s*(i/10.0)*k, up, dir);
			}
			else
			{
				VectorScale (right, c*k, dir);
				VectorMA (dir, s*k, up, dir);
			}
			
			for (j=0 ; j<3 ; j++)
				p->org[j] = move[j] + dir[j]*3;

			VectorClear(p->vel);
#ifdef DOUBLE_SCREW
		}
#endif
		VectorAdd (move, vec, move);
	}
}
#endif
#ifdef RINGS
//void CL_Heatbeam (vec3_t start, vec3_t end)
void CL_Heatbeam (const vec3_t start, const vec3_t forward)
{
	vec3_t		move, vec, right, up;
	cparticle_t	*p;
	float		i, c, s, len;
	vec3_t		dir;
	float		ltime;
	float		step = 32.0f, rstep;
	float		start_pt, rot, variance = 0.5f;
	vec3_t		end;

	VectorMA (start, 4096, forward, end);

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	// FIXME - pmm - these might end up using old values?
	VectorCopy (cl.v_right, right);
	VectorCopy (cl.v_up, up);
#ifdef GL_QUAKE
	// GL mode
	VectorMA (move, -0.5f, right, move);
	VectorMA (move, -0.5f, up, move);
#endif
	// otherwise assume SOFT

	ltime = (float)cl.time/1000.0f;
	start_pt = (float)fmod(ltime*96.0f,step);
	VectorMA (move, start_pt, vec, move);

	VectorScale (vec, step, vec);

	rstep = M_PI/10.0f;
	for (i=start_pt ; i<len ; i+=step)
	{
		if (i>step*5) // don't bother after the 5th ring
			break;

		for (rot = 0; rot < M_TWOPI; rot += rstep)
		{
			if (!free_particles)
				return;

			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;
			
			p->time = cl.time;
			VectorClear (p->accel);

			Q_sincos(rot, &s, &c);
			
			// trim it so it looks like it's starting at the origin
			if (i < 10) {
				c *= variance * (i/10.0f);
				s *= variance * (i/10.0f);
			} else {
				c *= variance;
				s *= variance;
			}
		
			VectorScale (right, c, dir);
			VectorMA (dir, s, up, dir);

			p->alpha = 0.5f;
			p->alphavel = -1000.0;
			p->color = 223 - (rand()&7);
			
			p->org[0] = move[0] + dir[0]*3;
			p->org[1] = move[1] + dir[1]*3;
			p->org[2] = move[2] + dir[2]*3;
			VectorClear(p->vel);
		}
		VectorAdd (move, vec, move);
	}
}
#endif
#ifdef SPRAY
void CL_Heatbeam (const vec3_t start, const vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	vec3_t		forward, right, up;
	int			i;
	float		d, c, s;
	vec3_t		dir;
	float		ltime;
	float		step = 32.0, rstep;
	float		start_pt;
	float		rot;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	VectorCopy (cl.v_forward, forward);
	VectorCopy (cl.v_right, right);
	VectorCopy (cl.v_up, up);
	VectorMA (move, -0.5, right, move);
	VectorMA (move, -0.5, up, move);

	for (i=0; i<8; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		
		p->time = cl.time;
		VectorClear (p->accel);
		
		d = crand()*M_PI;
		c = (float)cos(d)*30;
		s = (float)sin(d)*30;

		p->alpha = 1.0;
		p->alphavel = -5.0 / (1+frand());
		p->color = 223 - (rand()&7);

		VectorCopy(move, p->org);
		VectorScale (vec, 450, p->vel);
		VectorMA (p->vel, c, right, p->vel);
		VectorMA (p->vel, s, up, p->vel);
	}

}
#endif

/*
===============
CL_ParticleSteamEffect

Puffs with velocity along direction, with some randomness thrown in
===============
*/
void CL_ParticleSteamEffect (const vec3_t org, const vec3_t dir, int color, int count, int magnitude)
{
	int			i;
	cparticle_t	*p;
	float		d;
	vec3_t		r, u;

//	VecToAngles(dir, angle_dir);
//	AngleVectors (angle_dir, f, r, u);

	MakeNormalVectors (dir, r, u);

	for (i=0 ; i<count ; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		p->color = color + (rand()&7);

		p->org[0] = org[0] + magnitude*0.1f*crand();
		p->org[1] = org[1] + magnitude*0.1f*crand();
		p->org[2] = org[2] + magnitude*0.1f*crand();

		VectorScale (dir, magnitude, p->vel);
		d = crand()*magnitude*0.3333333333f;
		VectorMA (p->vel, d, r, p->vel);
		d = crand()*magnitude*0.3333333333f;
		VectorMA (p->vel, d, u, p->vel);

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY*0.5f;
		p->alpha = 1.0f;

		p->alphavel = -1.0f / (0.5f + frand()*0.3f);
	}
}

void CL_ParticleSteamEffect2 (cl_sustain_t *self)
{
	int			i;
	cparticle_t	*p;
	float		d;
	vec3_t		r, u, dir;

	VectorCopy (self->dir, dir);
	MakeNormalVectors (dir, r, u);

	for (i=0 ; i<self->count ; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		p->color = self->color + (rand()&7);

		p->org[0] = self->org[0] + self->magnitude*0.1f*crand();
		p->org[1] = self->org[1] + self->magnitude*0.1f*crand();
		p->org[2] = self->org[2] + self->magnitude*0.1f*crand();

		VectorScale (dir, self->magnitude, p->vel);
		d = crand()*self->magnitude*0.3333333333f;
		VectorMA (p->vel, d, r, p->vel);
		d = crand()*self->magnitude*0.3333333333f;
		VectorMA (p->vel, d, u, p->vel);

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY*0.5f;
		p->alpha = 1.0f;

		p->alphavel = -1.0f / (0.5f + frand()*0.3f);
	}
	self->nextthink += self->thinkinterval;
}

/*
===============
CL_TrackerTrail
===============
*/
void CL_TrackerTrail (const vec3_t start, const vec3_t end, int particleColor)
{
	vec3_t		move, vec;
	vec3_t		forward,right,up,angle_dir;
	float		len, dist;
	cparticle_t	*p;
	int			dec = 3;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	VectorCopy(vec, forward);
	VecToAngles(forward, angle_dir);
	AngleVectors(angle_dir, forward, right, up);

	VectorScale (vec, dec, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0f;
		p->alphavel = -2.0f;
		p->color = particleColor;
		dist = 8 * (float)cos(DotProduct(move, forward));
		VectorMA(move, dist, up, p->org);

		p->vel[0] = p->vel[1] = 0;
		p->vel[2] = 5;

		VectorAdd (move, vec, move);
	}
}

void CL_Tracker_Shell(const vec3_t origin)
{
	vec3_t			dir;
	int				i;
	cparticle_t		*p;

	for(i=0;i<300;i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0f;
		p->alphavel = INSTANT_PARTICLE;
		p->color = 0;

		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
	
		VectorMA(origin, 40, dir, p->org);
	}
}

void CL_MonsterPlasma_Shell(const vec3_t origin)
{
	vec3_t			dir;
	int				i;
	cparticle_t		*p;

	for(i=0;i<40;i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0f;
		p->alphavel = INSTANT_PARTICLE;
		p->color = 0xe0;

		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
	
		VectorMA(origin, 10, dir, p->org);
	}
}

void CL_Widowbeamout (cl_sustain_t *self)
{
	vec3_t			dir;
	int				i;
	cparticle_t		*p;
	static const int colortable[4] = {2*8,13*8,21*8,18*8};
	float			ratio;

	ratio = 1.0f - (((float)self->endtime - (float)cl.time)/2100.0f);
	ratio *= 45.0f;

	for(i=0;i<300;i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0f;
		p->alphavel = INSTANT_PARTICLE;
		p->color = colortable[rand()&3];

		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
	
		VectorMA(self->org, ratio, dir, p->org);
	}
}

void CL_Nukeblast (cl_sustain_t *self)
{
	vec3_t			dir;
	int				i;
	cparticle_t		*p;
	static const int colortable[4] = {110, 112, 114, 116};
	float			ratio;

	ratio = 1.0f - (((float)self->endtime - (float)cl.time)/1000.0f);
	ratio *= 200.0f;

	for(i=0;i<700;i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0f;
		p->alphavel = INSTANT_PARTICLE;
		p->color = colortable[rand()&3];

		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
	
		VectorMA(self->org, ratio, dir, p->org);
	}
}

void CL_WidowSplash (const vec3_t org)
{
	static const int colortable[4] = {2*8,13*8,21*8,18*8};
	int			i;
	cparticle_t	*p;
	vec3_t		dir;

	for (i=0 ; i<256 ; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		p->color = colortable[rand()&3];

		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
		VectorMA(org, 45.0f, dir, p->org);
		VectorMA(vec3_origin, 40.0f, dir, p->vel);

		p->accel[0] = p->accel[1] = 0;
		p->alpha = 1.0f;

		p->alphavel = -0.8f / (0.5f + frand()*0.3f);
	}

}

void CL_Tracker_Explode(const vec3_t origin)
{
	vec3_t			dir;
	int				i;
	cparticle_t		*p;

	for(i=0;i<300;i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0f;
		p->alphavel = -1.0f;
		p->color = 0;

		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
	
		VectorMA(origin, 64, dir, p->org);
		VectorScale(dir, -64, p->vel);
	}
	
}

/*
===============
CL_TagTrail

===============
*/
void CL_TagTrail (const vec3_t start, const vec3_t end, int color)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	cparticle_t	*p;
	int			dec = 5;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	VectorScale (vec, dec, vec);

	while (len >= 0)
	{
		len -= dec;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0f;
		p->alphavel = -1.0f / (0.8f+frand()*0.2f);
		p->color = color;

		p->org[0] = move[0] + crand()*16;
		p->org[1] = move[1] + crand()*16;
		p->org[2] = move[2] + crand()*16;
		p->vel[0] = crand()*5;
		p->vel[1] = crand()*5;
		p->vel[2] = crand()*5;

		VectorAdd (move, vec, move);
	}
}

/*
===============
CL_ColorExplosionParticles
===============
*/
void CL_ColorExplosionParticles (const vec3_t org, int color, int run)
{
	int			i;
	cparticle_t	*p;

	for (i=0 ; i<128 ; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		p->color = color + (rand() % run);

		p->org[0] = org[0] + ((rand()%32)-16);
		p->org[1] = org[1] + ((rand()%32)-16);
		p->org[2] = org[2] + ((rand()%32)-16);
		p->vel[0] = (rand()%256)-128;
		p->vel[1] = (rand()%256)-128;
		p->vel[2] = (rand()%256)-128;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0f;

		p->alphavel = -0.4f / (0.6f + frand()*0.2f);
	}
}

/*
===============
CL_ParticleSmokeEffect - like the steam effect, but unaffected by gravity
===============
*/
void CL_ParticleSmokeEffect (const vec3_t org, const vec3_t dir, int color, int count, int magnitude)
{
	int			i;
	cparticle_t	*p;
	float		d;
	vec3_t		r, u;

	MakeNormalVectors (dir, r, u);

	for (i=0 ; i<count ; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		p->color = color + (rand()&7);

		p->org[0] = org[0] + magnitude*0.1f*crand();
		p->org[1] = org[1] + magnitude*0.1f*crand();
		p->org[2] = org[2] + magnitude*0.1f*crand();

		VectorScale (dir, magnitude, p->vel);
		d = crand()*magnitude*0.3333333333f;
		VectorMA (p->vel, d, r, p->vel);
		d = crand()*magnitude*0.3333333333f;
		VectorMA (p->vel, d, u, p->vel);

		VectorClear(p->accel);
		p->alpha = 1.0f;

		p->alphavel = -1.0f / (0.5f + frand()*0.3f);
	}
}

/*
===============
CL_BlasterParticles2

Wall impact puffs (Green)
===============
*/
void CL_BlasterParticles2 (const vec3_t org, const vec3_t dir, unsigned int color)
{
	int			i;
	cparticle_t	*p;
	float		d;

	for (i = 0; i < 40; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		p->color = color + (rand()&7);

		d = rand()&15;

		p->org[0] = org[0] + ((rand()&7)-4) + d*dir[0];
		p->org[1] = org[1] + ((rand()&7)-4) + d*dir[1];
		p->org[2] = org[2] + ((rand()&7)-4) + d*dir[2];

		p->vel[0] = dir[0] * 30 + crand()*40;
		p->vel[1] = dir[1] * 30 + crand()*40;
		p->vel[2] = dir[2] * 30 + crand()*40;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0f;

		p->alphavel = -1.0f / (0.5f + frand()*0.3f);
	}
}

/*
===============
CL_BlasterTrail2

Green!
===============
*/
void CL_BlasterTrail2 (const vec3_t start, const vec3_t end)
{
	vec3_t		move, vec;
	float		len;
	cparticle_t	*p;
	int			dec = 5;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	VectorScale (vec, dec, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0f;
		p->alphavel = -1.0f / (0.3f+frand()*0.2f);
		p->color = 0xd0;

		p->org[0] = move[0] + crand();
		p->org[1] = move[1] + crand();
		p->org[2] = move[2] + crand();

		p->vel[0] = crand()*5;
		p->vel[1] = crand()*5;
		p->vel[2] = crand()*5;

		VectorAdd (move, vec, move);
	}
}
