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

#include "q_shared.h"

vec3_t vec3_origin = {0,0,0};

const vec3_t bytedirs[NUMVERTEXNORMALS] =
{
#include "../client/anorms.h"
};

int DirToByte( const vec3_t dir )
{
	int		i, best = 0;
	float	d, bestd = 0.0f;

	if (!dir)
		return 0;

	for( i = 0; i < NUMVERTEXNORMALS; i++ ) {
		d = DotProduct( dir, bytedirs[i] );
		if( d > bestd ) {
			bestd = d;
			best = i;
		}
	}

	return best;
}

void ByteToDir (int b, vec3_t dir)
{
	if ((unsigned)b >= NUMVERTEXNORMALS)
		VectorClear(dir);
	else
		VectorCopy(bytedirs[b], dir);
}

//============================================================================

void MakeNormalVectors (const vec3_t forward, vec3_t right, vec3_t up)
{
	float		d;

	// this rotate and negat guarantees a vector
	// not colinear with the original
	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];

	d = DotProduct (right, forward);
	VectorMA (right, -d, forward, right);
	VectorNormalize (right);
	CrossProduct (right, forward, up);
}

void Q_sincos( float angle, float *s, float *c )
{
#ifdef _WIN32
	_asm {
		fld		angle
		fsincos
		mov		ecx, c
		mov		edx, s
		fstp	dword ptr [ecx]
		fstp	dword ptr [edx]
	}
#else
	*s = (float)sin( angle );
	*c = (float)cos( angle );
#endif
}

void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees )
{
	float		t0, t1, c, s;
	vec3_t		vr, vu, vf;

	Q_sincos(DEG2RAD(degrees), &s, &c);

	VectorCopy (dir, vf);
	MakeNormalVectors (vf, vr, vu);

	t0 = vr[0] * c + vu[0] * -s;
	t1 = vr[0] * s + vu[0] *  c;
	dst[0] = (t0 * vr[0] + t1 * vu[0] + vf[0] * vf[0]) * point[0]
		   + (t0 * vr[1] + t1 * vu[1] + vf[0] * vf[1]) * point[1]
		   + (t0 * vr[2] + t1 * vu[2] + vf[0] * vf[2]) * point[2];

	t0 = vr[1] * c + vu[1] * -s;
	t1 = vr[1] * s + vu[1] *  c;
	dst[1] = (t0 * vr[0] + t1 * vu[0] + vf[1] * vf[0]) * point[0]
		   + (t0 * vr[1] + t1 * vu[1] + vf[1] * vf[1]) * point[1]
		   + (t0 * vr[2] + t1 * vu[2] + vf[1] * vf[2]) * point[2];

	t0 = vr[2] * c + vu[2] * -s;
	t1 = vr[2] * s + vu[2] *  c;
	dst[2] = (t0 * vr[0] + t1 * vu[0] + vf[2] * vf[0]) * point[0]
		   + (t0 * vr[1] + t1 * vu[1] + vf[2] * vf[1]) * point[1]
		   + (t0 * vr[2] + t1 * vu[2] + vf[2] * vf[2]) * point[2];
}

void AngleVectors (const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float sr, sp, sy, cr, cp, cy, t;

	Q_sincos(DEG2RAD(angles[YAW]), &sy, &cy);
	Q_sincos(DEG2RAD(angles[PITCH]), &sp, &cp);

	if (forward)
	{
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}

	if (right) {
		Q_sincos(DEG2RAD(angles[ROLL]), &sr, &cr);

		t = sr*sp;
		right[0] = -t*cy+cr*sy;
		right[1] = -t*sy-cr*cy;
		right[2] = -sr*cp;

		if (up) {
			t = cr*sp;
			up[0] = t*cy+sr*sy;
			up[1] = t*sy-sr*cy;
			up[2] = cr*cp;
		}
	}
	else if (up) {
		Q_sincos(DEG2RAD(angles[ROLL]), &sr, &cr);

		t = cr*sp;
		up[0] = t*cy+sr*sy;
		up[1] = t*sy-sr*cy;
		up[2] = cr*cp;
	}
}

void VecToAngles (const vec3_t vec, vec3_t angles)
{
	float	forward, yaw, pitch;
	
	if (vec[1] == 0 && vec[0] == 0)
	{
		yaw = 0;
		if (vec[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		if (vec[0]) {
			yaw = RAD2DEG( (float)atan2(vec[1], vec[0]) );
			if (yaw < 0)
				yaw += 360;
		} else if (vec[1] > 0) {
			yaw = 90;
		} else {
			yaw = 270;
		}

		forward = (float)sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
		pitch = RAD2DEG((float)atan2(vec[2], forward));
		if (pitch < 0)
			pitch += 360;
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}

void AnglesToAxis (const vec3_t angles, vec3_t axis[3])
{
	float	sp, sy, sr, cp, cy, cr, t;

	Q_sincos(DEG2RAD(angles[YAW]), &sy, &cy);
	Q_sincos(DEG2RAD(angles[PITCH]), &sp, &cp);
	Q_sincos(DEG2RAD(angles[ROLL]), &sr, &cr);

	axis[0][0] = cp*cy;
	axis[0][1] = cp*sy;
	axis[0][2] = -sp;
	t = sr*sp;
	axis[1][0] = t*cy+cr*-sy;
	axis[1][1] = t*sy+cr*cy;
	axis[1][2] = sr*cp;
	t = cr*sp;
	axis[2][0] = t*cy+sr*sy;
	axis[2][1] = t*sy-sr*cy;
	axis[2][2] = cr*cp;
}

void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal )
{
	float d;

	d = 1.0f / DotProduct( normal, normal );

	d = DotProduct( normal, p ) * d * d;

	dst[0] = p[0] - d * normal[0];
	dst[1] = p[1] - d * normal[1];
	dst[2] = p[2] - d * normal[2];
}

/*
** assumes "src" is normalized
*/
void PerpendicularVector( vec3_t dst, const vec3_t src )
{
	if (!src[0]) {
		VectorSet(dst, 1, 0, 0);
	} else if (!src[1]) {
		VectorSet(dst, 0, 1, 0);
	} else if (!src[2]) {
		VectorSet(dst, 0, 0, 1);
	} else {
		VectorSet(dst, -src[1], src[0], 0);
		VectorNormalize(dst);
	}
}

/*
================
R_ConcatRotations
================
*/
void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +	in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +	in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +	in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +	in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +	in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +	in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +	in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +	in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +	in1[2][2] * in2[2][2];
}

/*
================
R_ConcatTransforms
================
*/
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +	in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +	in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +	in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +	in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +	in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +	in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +	in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +	in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +	in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +	in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +	in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +	in1[2][2] * in2[2][3] + in1[2][3];
}


//============================================================================


/*
===============
LerpAngle

===============
*/
float LerpAngle (float a2, float a1, float frac)
{
	if (a1 - a2 > 180)
		a1 -= 360;
	else if (a1 - a2 < -180)
		a1 += 360;

	return a2 + frac * (a1 - a2);
}

/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
	if (fov_x < 1 || fov_x > 179)
		Com_Error (ERR_DROP, "Bad fov: %f", fov_x);

	width = width/(float)tan(fov_x/360*M_PI);

	return (float)atan2(height, width)*360/M_PI;
}

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
int BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const struct cplane_s *p)
{
	//the following optimisation is performed by BOX_ON_PLANE_SIDE macro
	//if (p->type < 3)
	//	return ((emaxs[p->type] >= p->dist) | ((emins[p->type] < p->dist) << 1));
	switch(p->signbits) {
	default:
	case 0:
		return	(((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) >= p->dist) |
				(((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 1:
		return	(((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) >= p->dist) |
				(((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 2:
		return	(((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) >= p->dist) |
				(((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 3:
		return	(((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) >= p->dist) |
				(((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 4:
		return	(((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) >= p->dist) |
				(((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 5:
		return	(((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) >= p->dist) |
				(((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 6:
		return	(((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) >= p->dist) |
				(((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 7:
		return	(((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) >= p->dist) |
				(((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	}
}

/*
=================
PlaneTypeForNormal
=================
*/
int	PlaneTypeForNormal (const vec3_t normal)
{
	vec_t	ax, ay, az;
	
	// NOTE: should these have an epsilon around 1.0?		
	if (normal[0] >= 1.0f)
		return PLANE_X;
	if (normal[1] >= 1.0f)
		return PLANE_Y;
	if (normal[2] >= 1.0f)
		return PLANE_Z;
		
	ax = (float)fabs( normal[0] );
	ay = (float)fabs( normal[1] );
	az = (float)fabs( normal[2] );

	if (ax >= ay && ax >= az)
		return PLANE_ANYX;
	if (ay >= ax && ay >= az)
		return PLANE_ANYY;
	return PLANE_ANYZ;
}

void AddPointToBounds (const vec3_t v, vec3_t mins, vec3_t maxs)
{
	if (v[0] < mins[0])
		mins[0] = v[0];
	if (v[0] > maxs[0])
		maxs[0] = v[0];

	if (v[1] < mins[1])
		mins[1] = v[1];
	if (v[1] > maxs[1])
		maxs[1] = v[1];

	if (v[2] < mins[2])
		mins[2] = v[2];
	if (v[2] > maxs[2])
		maxs[2] = v[2];
}

float RadiusFromBounds (const vec3_t mins, const vec3_t maxs)
{
	vec3_t	corner;
	float	val1, val2;

	val1 = (float)fabs(mins[0]);
	val2 = (float)fabs(maxs[0]);
	corner[0] = (val1 > val2) ? val1 : val2;
	val1 = (float)fabs(mins[1]);
	val2 = (float)fabs(maxs[1]);
	corner[1] = (val1 > val2) ? val1 : val2;
	val1 = (float)fabs(mins[2]);
	val2 = (float)fabs(maxs[2]);
	corner[2] = (val1 > val2) ? val1 : val2;

	return (float)VectorLength(corner);
}

vec_t VectorNormalize (vec3_t v)
{
	float	length, ilength;

	length = (float)sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]); // FIXME

	if (length) {
		ilength = 1.0f/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}

vec_t VectorNormalize2 (const vec3_t v, vec3_t out)
{
	float	length, ilength;

	length = (float)sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]); // FIXME

	if (length) {
		ilength = 1.0f/length;
		out[0] = v[0]*ilength;
		out[1] = v[1]*ilength;
		out[2] = v[2]*ilength;
	} else {
		VectorClear(out);
	}

	return length;
}

/*
 * @brief Combines a fraction of the second vector with the first.
 */
void VectorMix(const vec3_t v1, const vec3_t v2, float mix, vec3_t out) {
	int32_t i;

	for (i = 0; i < 3; i++)
		out[i] = v1[i] * (1.0 - mix) + v2[i] * mix;
}

int Q_log2(int val)
{
	int answer=0;
	while (val>>=1)
		answer++;
	return answer;
}

//====================================================================================
/*
============
COM_FixPath

Change '\\' to '/', removes ./ and leading/ending '/'
"something/a/../b" -> "something/b"
============
*/
void COM_FixPath (char *path)
{
	int	i, j, len = 0, lastLash = -1;

	for (i = 0; path[i]; i++) {
		switch (path[i]) {
		case '\\':
		case '/':
			if(!len)
				break;

			if (path[len-1] == '/') //remove multiple /
				break;

			if(path[len-1] == '.')
			{
				if(len == 1 || (len >= 2 && path[len-2] != '.'))
				{	//remove "./"
					len--;
					break;
				}
			}

			lastLash = len;
			path[len++] = '/';
			break;
		case '.':
			if(len >= 2 && path[len-1] == '.')
			{
				if(lastLash > 0 && path[lastLash-1] != '.')
				{	//theres lastlash and its not "../"
					for (j = lastLash-1; j >= 0; j--)
					{
						if(path[j] == '/')
							break;
					}
					lastLash = j;
					len = lastLash+1;			
					break;
				}
				if(path[len-2] == '.')
					break;
			}
			//fallthrough
		default:
			path[len++] = path[i];
			break;
		}
	}
	path[len] = '\0';

	if (len && path[len-1] == '/')
		path[len-1] = '\0';
}

/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (const char *pathname)
{
	const char	*last;
	
	last = pathname;
	while (*pathname)
	{
		if (*pathname == '/' || *pathname=='\\')
			last = pathname+1;

		pathname++;
	}
	return (char *)last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension (const char *in, char *out)
{
	char *dot;

	if (!(dot = strrchr(in, '.'))) {
		strcpy(out, in);
		return;
	}
	while (*in && in != dot)
		*out++ = *in++;
	*out = 0;
}

/*
============
COM_FileExtension
============
*/
char *COM_FileExtension( const char *in )
{
	const char *s, *last;

	last = s = in + strlen( in );
	while( s != in ) {
		if( *s == '/' ) {
			break;
		}
		if( *s == '.' ) {
			return (char *)s;
		}
		s--;
	}

	return (char *)last;
}

/*
============
COM_FilePath

Returns the path up to, but not including the last /
============
*/
void COM_FilePath (const char *in, char *out)
{
	const char *s;
	
	s = in + strlen(in);
	while (s != in && *s != '/' && *s != '\\')
		s--;

	strncpy (out,in, s-in);
	out[s-in] = 0;
}


/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension (char *path, size_t size, const char *extension)
{
	char    *src;
//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	src = path + strlen(path);

	while (*src != '/' && *src != '\\' && src != path)
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	Q_strncatz (path, extension, size);
}

// TODO: This source file is encoded in Latin-1 (ISO-8859-1). Newer GCC and
// Clang want UTF8, and bark about these high-bit character literals. The
// "right" way to handle this, would be to use wchar, convert the literals
// to their wchar equivalents, and save the file as UTF-8. Oh well.
void COM_MakePrintable (char *s)
{
	char *string = s;
	int	c;

	while((c = *string++) != 0)
	{
		switch (c) {
		case 'å':
		case 'ä': *s++ = 'a'; break;
		case 'Å':
		case 'Ä': *s++ = 'A'; break;
		case 'ö': *s++ = 'o'; break;
		case 'Ö': *s++ = 'O'; break;
		case '`':
		case '´': *s++ = '\''; break;
		default:	
			if ( c >= 0x20 && c <= 0x7E )
				*s++ = c;
			break;
		}
	}
	*s = '\0';
}

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/
int16 ShortSwap (int16 l)
{
	byte    b1, b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

int32 LongSwap (int32 l)
{
	byte    b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int32)b1<<24) + ((int32)b2<<16) + ((int32)b3<<8) + b4;
}

float FloatSwap (float f)
{
	union
	{
		float	f;
		byte	b[4];
	} dat1, dat2;
	
	
	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
int16   (*BigShort) (int16 l);
int16   (*LittleShort) (int16 l);
int32   (*BigLong) (int l);
int32   (*LittleLong) (int l);
float   (*BigFloat) (const float l);
float   (*LittleFloat) (const float l);
qboolean bigendien;

int16 ShortNoSwap (short l)
{
	return l;
}

int32	LongNoSwap (int l)
{
	return l;
}

float FloatNoSwap (const float *f)
{
	return f;
}

/*
================
Swap_Init
================
*/
void Swap_Init (void)
{
	byte	swaptest[2] = {1,0};

// set the byte swapping variables in a portable manner	
	if ( *(int16 *)swaptest == 1)
	{
		bigendien = false;
		_BigShort = ShortSwap;
		_LittleShort = ShortNoSwap;
		_BigLong = LongSwap;
		_LittleLong = LongNoSwap;
		_BigFloat = FloatSwap;
		_LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendien = true;
		_BigShort = ShortNoSwap;
		_LittleShort = ShortSwap;
		_BigLong = LongNoSwap;
		_LittleLong = LongSwap;
		_BigFloat = FloatNoSwap;
		_LittleFloat = FloatSwap;
	}
}
#endif

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
const char *COM_Parse (char **data_p)
{
	int		c, len = 0;
	char	*data;
	static char	com_token[MAX_TOKEN_CHARS];

	data = *data_p;
	com_token[0] = 0;
	
	if (!data)
	{
		*data_p = NULL;
		return com_token;
	}
		
// skip whitespace
	do
	{
		while ((c = *data) <= ' ')
		{
			if (c == 0)
			{
				*data_p = NULL;
				return com_token;
			}
			data++;
		}
		
		// skip // comments
		if (c == '/' && data[1] == '/')
		{
			data += 2;

			while (*data && *data != '\n')
				data++;
		}
		else
			break;
	} while(1);


	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c == '\"' || !c)
				break;

			if (len < MAX_TOKEN_CHARS)
				com_token[len++] = c;
		}
	}
	else
	{
		// parse a regular word
		do
		{
			if (len < MAX_TOKEN_CHARS)
				com_token[len++] = c;

			data++;
			c = *data;
		} while (c>32);
	}

	if (len == MAX_TOKEN_CHARS)
		len = 0;

	com_token[len] = 0;

	*data_p = data;
	return com_token;
}


/*
===============
Com_PageInMemory

===============
*/
static int	paged_total = 0;

void Com_PageInMemory (const byte *buffer, int size)
{
	int		i;

	for (i=size-1 ; i>0 ; i-=4096)
		paged_total += buffer[i];
}

/*
 =================
 Com_WildCmp

 Wildcard compare.
 Returns non-zero if matches, zero otherwise.
 =================
*/
int Com_WildCmp (const char *filter, const char *text)
{
	int fLen, len, i, j;

	while(*filter) {
		if (*filter == '*') {
			filter++;
			while(*filter == '*' || *filter == '?') {
				if(*filter++ == '?' && *text++ == '\0')
					return false;
			}
			if(!*filter)
				return true;
		
			for (fLen = 0; filter[fLen]; fLen++) {
				if (filter[fLen] == '*' || filter[fLen] == '?')
					break;
			}

			len = strlen(text) - fLen;
			if(len < 0)
				return false;

			if(!filter[fLen]) {
				for (text += len, j = 0; j < fLen; j++) {
					if (toupper(text[j]) != toupper(filter[j]))
						return false;
				}
				return true;
			}
			for (i = 0; i <= len; i++, text++) {
				for (j = 0; j < fLen; j++) {
					if (toupper(text[j]) != toupper(filter[j]))
						break;
				}
				if (j == fLen && Com_WildCmp(filter+fLen, text+fLen))
					return true;
			}
			return false;
		}
		else if (*filter == '?') {
			if(*text++ == '\0')
				return false;
			filter++;
		}
		else {
			if (toupper(*filter) != toupper(*text))
				return false;

			filter++;
			text++;
		}
	}
	return !*text;
}

/*
==========
Com_HashKey

Returns hash key for a string
==========
*/

unsigned int Com_HashValue (const char *name)
{
	unsigned int hash = 0;

	while(*name)
		hash = hash * 33 + Q_tolower(*name++);

	return hash + (hash >> 5);
}

unsigned int Com_HashValuePath (const char *name)
{
	unsigned int c, hash = 0;

	while(*name) {
		c = Q_tolower(*name++);
		if( c == '\\' )
			c = '/';
		hash = hash * 33 + c;
	}
	return hash + (hash >> 5);
}

/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

#ifndef Q_strnicmp
int Q_strnicmp (const char *s1, const char *s2, size_t size)
{
	int		c1, c2;
	
	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!size--)
			return 0;		// strings are equal until end point
		
		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return c1 < c2 ? -1 : 1;	// strings not equal
		}
	} while (c1);
	
	return 0;		// strings are equal
}
#endif

/*
============
Q_stristr
============
*/
char *Q_stristr(const char *str1, const char *str2)
{
	int len, i, j;

	len = strlen(str1) - strlen(str2);
	for (i = 0; i <= len; i++, str1++) {
		for (j = 0; str2[j]; j++) {
			if (toupper(str1[j]) != toupper(str2[j]))
				break;
		}
		if (!str2[j])
			return (char *)str1;
	}
	return NULL;
}

#ifndef NDEBUG
void Com_Error (int code, const char *fmt, ...);
#endif

void Q_strncpyz( char *dest, const char *src, size_t size )
{
#ifndef NDEBUG
	if ( !dest )
		Com_Error(ERR_FATAL, "Q_strncpyz: NULL dest" );

	if ( !src )
		Com_Error(ERR_FATAL, "Q_strncpyz: NULL src" );

	if ( size < 1 )
		Com_Error(ERR_FATAL, "Q_strncpyz: size < 1" ); 
#endif

	if( size ) {
		while( --size && (*dest++ = *src++) );
		*dest = '\0';
	}
}
/*
==============
Q_strncatz
==============
*/
void Q_strncatz( char *dest, const char *src, size_t size )
{
#ifndef NDEBUG
	if ( !dest )
		Com_Error(ERR_FATAL, "Q_strncatz: NULL dest" );

	if ( !src )
		Com_Error(ERR_FATAL, "Q_strncatz: NULL src" );

	if ( size < 1 )
		Com_Error(ERR_FATAL, "Q_strncatz: size < 1" ); 
#endif
	if(size) {
		while( --size && *dest++ );
		if( size ) {
			dest--;
			while( --size && (*dest++ = *src++) );
		}
		*dest = '\0';
	}
}

void Com_sprintf (char *dest, size_t size, const char *fmt, ...)
{
	va_list		argptr;

#ifndef NDEBUG
	if ( !dest )
		Com_Error(ERR_FATAL, "Q_strncatz: NULL dest" );

	if ( size < 1 )
		Com_Error(ERR_FATAL, "Q_strncatz: size < 1" ); 
#endif

	if(size) {
		va_start( argptr, fmt );
		vsnprintf( dest, size, fmt, argptr );
		va_end( argptr );
	}
}

/*
============
va
does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
============
*/
char	*va(const char *format, ...)
{
	va_list		argptr;
	static int  index = 0;
	static char	string[2][2048];

	index  ^= 1;
	va_start (argptr, format);
	vsnprintf (string[index], sizeof(string[index]), format, argptr);
	va_end (argptr);

	return string[index];
}


int Q_tolower( int c ) {
	if( Q_isupper( c ) ) {
		c += ( 'a' - 'A' );
	}

	return c;
}

int Q_toupper( int c ) {
	if( Q_islower( c ) ) {
		c -= ( 'a' - 'A' );
	}

	return c;
}

/*
==============
Q_strlwr
==============
*/
char *Q_strlwr( char *s )
{
	char *p;

	for( p = s; *s; s++ ) {
		if(Q_isupper(*s))
			*s += 'a' - 'A';
	}

	return p;
}

char *Q_strupr( char *s )
{
	char *p;

	for( p = s; *s; s++ ) {
		if(Q_islower(*s))
			*s -= 'a' - 'A';
	}

	return p;
}

/*
==============
Q_IsNumeric
==============
*/
qboolean Q_IsNumeric (const char *s)
{
	qboolean dot;

	if(!s)
		return false;

	if (*s == '-')
		s++;

	dot = false;
	do
	{
		if(!Q_isdigit(*s)) {
			if(*s == '.' && !dot)
				dot = true;
			else
				return false;
		}
		s++;
	} while(*s);

	return true;
}
/*
=====================================================================

  INFO STRINGS

=====================================================================
*/

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey (const char *s, const char *key)
{
	char	pkey[MAX_INFO_STRING];
	static	char value[2][MAX_INFO_STRING];	// use two buffers so compares
											// work without stomping on each other
	static	int	valueindex = 0;
	char	*o;
	
	if ( !s || !key )
		return "";

	valueindex ^= 1;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			break;
		s++;
	}

	return "";
}

/*
===================
Info_NextPair

Used to itterate through all the key/value pairs in an info string
===================
*/
void Info_NextPair( const char **head, char *key, char *value ) {
	char	*o;
	const char	*s;

	key[0] = 0;
	value[0] = 0;

	s = *head;
	if( !s )
		return;

	if ( *s == '\\' )
		s++;

	o = key;
	while( *s && *s != '\\' )
		*o++ = *s++;

	*o = 0;

	if( !*s ) {
		*head = s;
		return;
	}

	s++;
	o = value;
	while( *s && *s != '\\' )
		*o++ = *s++;

	*o = 0;

	*head = s;
}

/*
===================
Info_RemoveKey
===================
*/
void Info_RemoveKey (char *s, const char *key)
{
	char	*start;
	char	pkey[MAX_INFO_KEY];
	char	value[MAX_INFO_VALUE];
	char	*o;

	if (strchr (key, '\\'))
	{
		Com_Printf ("Info_RemoveKey: Tried to remove illegal key '%s'\n", key);
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp(key, pkey)) {
			strcpy(start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}
}


/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
qboolean Info_Validate (const char *s)
{
	while( *s ) {
		if( *s == '\"' || *s == ';' ) {
			return false;
		}
		s++;
	}
	return true;
}

void Info_SetValueForKey (char *s, const char *key, const char *value)
{
	char	newi[MAX_INFO_STRING], *v;
	int		c;

	if (!key || !value)
		return;

	if (strchr (key, '\\') || strchr (value, '\\') )
	{
		Com_Printf ("Can't use keys or values with a \\ (attempted to set key '%s')\n", key);
		return;
	}

	if (strchr (key, ';') || strchr (value, ';') )
	{
		Com_Printf ("Can't use keys or values with a semicolon (attempted to set key '%s')\n", key);
		return;
	}

	if (strchr (key, '"') || strchr (value, '"') )
	{
		Com_Printf ("Can't use keys or values with a \" (attempted to set key '%s')\n", key);
		return;
	}

	if (strlen(key) > MAX_INFO_KEY-1 || strlen(value) > MAX_INFO_KEY-1)
	{
		Com_Printf ("Keys and values must be < 64 characters (attempted to set key '%s')\n", key);
		return;
	}

	Info_RemoveKey (s, key);

	if (!value[0])
		return;

	Com_sprintf (newi, sizeof(newi), "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) >= MAX_INFO_STRING)
	{
		Com_Printf ("Info string length exceeded while trying to set '%s'\n", newi);
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = newi;
	while (*v)
	{
		c = *v++;
		c &= 127;		// strip high bits
		if (c >= 32 && c < 127)
			*s++ = c;
	}
	*s = 0;
}
