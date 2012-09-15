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

#ifdef __cplusplus
# define QGL_EXTERN extern "C"
#else
# define QGL_EXTERN extern
#endif

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# define VC_LEANMEAN
# include <windows.h>

# define QGL_WGL(type,name,params) QGL_EXTERN type (APIENTRY * q##name) params;
# define QGL_WGL_EXT(type,name,params) QGL_EXTERN type (APIENTRY * q##name) params;
# define QGL_GLX(type,name,params)
# define QGL_GLX_EXT(type,name,params)
#endif

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#if defined( __linux__ ) || defined( __FreeBSD__ )
# include <GL/glx.h>

# define QGL_WGL(type,name,params)
# define QGL_WGL_EXT(type,name,params)
# define QGL_GLX(type,name,params) QGL_EXTERN type (APIENTRY * q##name) params;
# define QGL_GLX_EXT(type,name,params) QGL_EXTERN type (APIENTRY * q##name) params;
#endif

#include "../client/ref.h"

#define QGL_FUNC(type,name,params) QGL_EXTERN type (APIENTRY * q##name) params;
#define QGL_EXT(type,name,params) QGL_EXTERN type (APIENTRY * q##name) params;

#include "qgl.h"

#undef QGL_GLX_EXT
#undef QGL_GLX
#undef QGL_WGL_EXT
#undef QGL_WGL
#undef QGL_EXT
#undef QGL_FUNC

#define	REF_VERSION	"GL 0.01"

extern unsigned int QGL_TEXTURE0, QGL_TEXTURE1;

#ifndef __VIDDEF_T
#define __VIDDEF_T
typedef struct
{
	int		width, height;			// coordinates from main game
} viddef_t;
#endif

extern	viddef_t	vid;

/*

  skins will be outline flood filled and mip mapped
  pics and sprites with alpha will be outline flood filled
  pic won't be mip mapped

  model skin
  sprite frame
  wall texture
  pic

*/

#define IT_TRANS		1
#define IT_PALETTED		2
#define IT_SCRAP		4
#define IT_REPLACE_WAL	8
#define IT_REPLACE_PCX	16


typedef enum 
{
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky
} imagetype_t;

typedef struct image_s
{
	char	name[MAX_QPATH];			// game path without extension
	char	*extension;					// extension
	imagetype_t	type;
	int		width, height;				// source image
	int		upload_width, upload_height;	// after power of two and picmip
	int		registration_sequence;		// 0 = free
	struct msurface_s	*texturechain;	// for sort-by-texture world drawing
	int		texnum;						// gl texture binding
	float	sl, tl, sh, th;				// 0,0 - 1,1 unless part of the scrap

	int		flags;
	struct image_s	*hashNext;
} image_t;

#define MAX_TEXTURE_UNITS	2

#define	TEXNUM_LIGHTMAPS	1024
#define	TEXNUM_SCRAPS		1152
#define	TEXNUM_IMAGES		1153

#define	MAX_GLTEXTURES		1024

//===================================================================

typedef enum
{
	rserr_ok,

	rserr_invalid_fullscreen,
	rserr_invalid_mode,

	rserr_unknown
} rserr_t;

#include "gl_model.h"

void GL_SetDefaultState( void );
void GL_UpdateSwapInterval( void );

extern	double	gldepthmin, gldepthmax;

typedef struct
{
	float	x, y, z;
	float	s, t;
	float	r, g, b;
} glvert_t;


#define	MAX_LBM_HEIGHT		480

#define BACKFACE_EPSILON	0.01


//====================================================

extern	image_t		gltextures[MAX_GLTEXTURES];
extern	int			numgltextures;


extern	image_t		*r_notexture;
extern	image_t		*r_whitetexture;
extern	image_t		*r_particletexture;
extern	image_t		*r_caustictexture;
extern	image_t		*r_bholetexture;
extern	image_t		*r_shelltexture;

extern	entity_t	*currententity;
extern	model_t		*currentmodel;
extern	int			r_visframecount;
extern	int			r_framecount;
extern	cplane_t	frustum[4];
extern	int			c_brush_polys, c_alias_polys;


extern	int			gl_filter_min, gl_filter_max;

//
// view origin
//
extern	vec3_t	r_origin;
extern	vec3_t	viewAxis[3];

//
// screen size info
//
extern	refdef_t	r_newrefdef;
extern	int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

extern	cvar_t	*r_norefresh;
extern	cvar_t	*r_lefthand;
extern	cvar_t	*r_drawentities;
extern	cvar_t	*r_drawworld;
extern	cvar_t	*r_speeds;
extern	cvar_t	*r_fullbright;
extern	cvar_t	*r_novis;
extern	cvar_t	*r_nocull;
extern	cvar_t	*r_lerpmodels;

extern	cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level

extern cvar_t	*gl_vertex_arrays;

extern cvar_t	*gl_ext_swapinterval;
extern cvar_t	*gl_ext_multitexture;
extern cvar_t	*gl_ext_pointparameters;
extern cvar_t	*gl_ext_compiled_vertex_array;

extern cvar_t	*gl_particle_min_size;
extern cvar_t	*gl_particle_max_size;
extern cvar_t	*gl_particle_size;
extern cvar_t	*gl_particle_att_a;
extern cvar_t	*gl_particle_att_b;
extern cvar_t	*gl_particle_att_c;

//extern	cvar_t	*gl_nosubimage;
extern	cvar_t	*gl_bitdepth;
extern	cvar_t	*gl_mode;
extern	cvar_t	*gl_lightmap;
extern	cvar_t	*gl_shadows;
extern	cvar_t	*gl_dynamic;
//extern  cvar_t  *gl_monolightmap;
extern	cvar_t	*gl_nobind;
extern	cvar_t	*gl_round_down;
extern	cvar_t	*gl_picmip;
extern	cvar_t	*gl_skymip;
extern	cvar_t	*gl_showtris;
extern	cvar_t	*gl_finish;
extern	cvar_t	*gl_ztrick;
extern	cvar_t	*gl_clear;
extern	cvar_t	*gl_cull;
extern	cvar_t	*gl_poly;
extern	cvar_t	*gl_texsort;
extern	cvar_t	*gl_polyblend;
extern	cvar_t	*gl_flashblend;
extern	cvar_t	*gl_lightmaptype;
extern	cvar_t	*gl_modulate;
//extern	cvar_t	*gl_playermip;
extern	cvar_t	*gl_drawbuffer;
extern	cvar_t	*gl_3dlabs_broken;
extern  cvar_t  *gl_driver;
extern	cvar_t	*gl_swapinterval;
extern	cvar_t	*gl_texturemode;
extern	cvar_t	*gl_texturebits;
extern  cvar_t  *gl_saturatelighting;
extern  cvar_t  *gl_lockpvs;

extern	cvar_t	*vid_fullscreen;
extern	cvar_t	*vid_gamma;

//extern	cvar_t		*intensity;


//Added gl_variables -Maniac
extern  cvar_t  *skydistance; // DMP - skybox size change

extern	cvar_t	*gl_replacewal;
extern	cvar_t	*gl_replacepcx;
extern	cvar_t	*gl_replacemd2;
extern	cvar_t	*gl_screenshot_quality;
extern  cvar_t	*gl_stainmaps;
extern	cvar_t	*gl_waterwaves;
extern	cvar_t	*gl_celshading;
extern	cvar_t	*gl_celshading_width;
extern	cvar_t	*gl_scale;
extern	cvar_t	*gl_watercaustics;
extern	cvar_t	*gl_fog;

extern	cvar_t	*gl_coloredlightmaps;
extern	cvar_t	*gl_shelleffect;

extern	cvar_t	*gl_minlight_entities;
extern	cvar_t	*gl_multisample;
//End

//extern	int		gl_lightmap_format;
//extern	int		gl_solid_format;
//extern	int		gl_alpha_format;
extern	int		gl_tex_solid_format;
extern	int		gl_tex_alpha_format;

extern	int		c_visible_lightmaps;
extern	int		c_visible_textures;

extern	float	r_WorldViewMatrix[16];
extern	float	r_ModelViewMatrix[16];

void R_TranslatePlayerSkin (int playernum);
void GL_Bind (int texnum);
void GL_MBind( int tmu, int texnum );
void GL_TexEnv( GLenum value );
void GL_EnableMultitexture( qboolean enable );
void GL_SelectTexture( int tmu );

void R_LightPoint (const vec3_t p, vec3_t color);
void R_PushDlights (void);

//====================================================================

extern	bspModel_t	*r_worldmodel;

extern	uint32	d_8to24table[256];

extern	int		registration_sequence;


//void V_AddBlend (float r, float g, float b, float a, float *v_blend);

int 	R_Init( void *hinstance, void *hWnd );
void	R_Shutdown( void );

//void R_RenderView (refdef_t *fd);

void GL_ScreenShot_f (void);
void R_DrawAliasModel (model_t *model);
void R_DrawBrushModel (bspSubmodel_t *subModel);
//void R_DrawSpriteModel (void);
void R_DrawBeam( void );
void R_DrawWorld (void);
void R_RenderDlights (void);
void R_DrawAlphaSurfaces (void);
//void R_RenderBrushPoly (msurface_t *fa);
void R_InitParticleTexture (void);
void Draw_InitLocal (void);
qboolean R_CullBox (const vec3_t mins, const vec3_t maxs);

void R_MarkLeaves (void);

glpoly_t *WaterWarpPolyVerts (glpoly_t *p);
void EmitWaterPolys (const glpoly_t *p, qboolean flowing);
void R_AddSkySurface (const msurface_t *fa);
void R_ClearSkyBox (void);
void R_DrawSkyBox (void);
void R_MarkLights (const dlight_t *light, int bit, const mnode_t *node);


void	Draw_GetPicSize (int *w, int *h, const char *name);
void	Draw_Pic (int x, int y, const char *name, float alpha);
void	Draw_StretchPic (int x, int y, int w, int h, const char *name, float alpha);
void	Draw_ScaledPic (int x, int y, float scale, const char *name, float red, float green, float blue, float alpha);
void	Draw_Char (int x, int y, int c, int color, float alpha);
void	Draw_TileClear (int x, int y, int w, int h, const char *name);
void	Draw_Fill (int x, int y, int w, int h, int c);
void	Draw_FadeScreen (void);
void	Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data);

void	R_BeginFrame( float camera_separation );
void	R_SwapBuffers( int );
void	R_CinematicSetPalette ( const unsigned char *palette);

void	Draw_GetPalette (void);

//void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight);

struct image_s *R_RegisterSkin (const char *name);


//void LoadPCX (const char *filename, byte **pic, byte **palette, int *width, int *height);

image_t *GL_LoadPic (const char *name, byte *pic, int width, int height, imagetype_t type, int flags, int samples);
image_t	*GL_FindImage (const char *name, imagetype_t type);
void	GL_TextureMode( const char *string );
void	GL_ImageList_f (void);

void	GL_SetTexturePalette( unsigned palette[256] );

void	GL_InitImages (void);
void	GL_ShutdownImages (void);

void	GL_FreeUnusedImages (void);

void	GL_TextureBits(void);

/*
** GL extension emulation functions
*/
//void GL_DrawParticles( int n, const particle_t particles[], const unsigned colortable[768] );

/*
** GL config stuff
*/
#define GL_RENDERER_VOODOO		0x00000001
#define GL_RENDERER_VOODOO2   	0x00000002
#define GL_RENDERER_VOODOO_RUSH	0x00000004
#define GL_RENDERER_BANSHEE		0x00000008
#define	GL_RENDERER_3DFX		0x0000000F

#define GL_RENDERER_PCX1		0x00000010
#define GL_RENDERER_PCX2		0x00000020
#define GL_RENDERER_PMX			0x00000040
#define	GL_RENDERER_POWERVR		0x00000070

#define GL_RENDERER_PERMEDIA2	0x00000100
#define GL_RENDERER_GLINT_MX	0x00000200
#define GL_RENDERER_GLINT_TX	0x00000400
#define GL_RENDERER_3DLABS_MISC	0x00000800
#define	GL_RENDERER_3DLABS		0x00000F00

#define GL_RENDERER_REALIZM		0x00001000
#define GL_RENDERER_REALIZM2	0x00002000
#define	GL_RENDERER_INTERGRAPH	0x00003000

#define GL_RENDERER_3DPRO		0x00004000
#define GL_RENDERER_REAL3D		0x00008000
#define GL_RENDERER_RIVA128		0x00010000
#define GL_RENDERER_DYPIC		0x00020000

#define GL_RENDERER_V1000		0x00040000
#define GL_RENDERER_V2100		0x00080000
#define GL_RENDERER_V2200		0x00100000
#define	GL_RENDERER_RENDITION	0x001C0000

#define GL_RENDERER_O2          0x00100000
#define GL_RENDERER_IMPACT      0x00200000
#define GL_RENDERER_RE			0x00400000
#define GL_RENDERER_IR			0x00800000
#define	GL_RENDERER_SGI			0x00F00000

#define GL_RENDERER_MCD			0x01000000
#define GL_RENDERER_OTHER		0x80000000

typedef struct
{
	int         renderer;
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *extensions_string;

	qboolean	allow_cds;

	int			maxTextureSize;
	int			maxTextureUnits;

	qboolean	anisotropic;
	float		maxAnisotropic;
} glconfig_t;

typedef struct
{
	float		inverse_intensity;
	qboolean	fullscreen;

	int			prev_mode;

	//unsigned char *d_16to8table;

	int			lightmap_textures;

	int			currentTMU;
	int			currentTextures[MAX_TEXTURE_UNITS];
	int			currentEnvModes[MAX_TEXTURE_UNITS];

	float		camera_separation;
	qboolean	stereo_enabled;

	qboolean	multiTexture;
	qboolean	sgis_mipmap;
	qboolean	texture_compression;
	qboolean	stencil;
	qboolean	compiledVertexArray;

	qboolean	tex_rectangle;

	qboolean	registering;

} glstate_t;

// vertex arrays
#define TESS_MAX_VERTICES   4096
#define TESS_MAX_INDICES    ( 3 * TESS_MAX_VERTICES )

typedef struct vArrays_s {
    int		numVertices;
    int		numIndices;

    vec_t	vertices[4*TESS_MAX_VERTICES];
    vec_t	colors[4*TESS_MAX_VERTICES];
    vec2_t	tcoords[TESS_MAX_VERTICES];
    int		indices[TESS_MAX_INDICES];
	
	int		texnum;
} vArrays_t;
extern vArrays_t r_arrays;

extern glconfig_t  gl_config;
extern glstate_t   gl_state;

#include "gl_decal.h"

void R_TranslateForEntity (const vec3_t origin);
void R_RotateForEntity (const vec3_t origin, vec3_t axis[3]);

qboolean R_GetModeInfo( int *width, int *height, int mode );

extern const vec4_t	colorWhite;
extern vec4_t	colorBlack;

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

void		GLimp_BeginFrame( float camera_separation );
void		GLimp_EndFrame( void );
int 		GLimp_Init( void *hinstance, void *hWnd );
void		GLimp_Shutdown( void );
rserr_t    	GLimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen );
void		GLimp_AppActivate( qboolean active );

#define WIREFRAME_OFF 0
#define	WIREFRAME_LINE 1
#define WIREFRAME_DOT 2
#define WIREFRAME_DASH 3
#define WIREFRAME_DOT_DASH 4

#define BG_TEXTURED_LIGHTMAPED 0
#define BG_TEXTURED 1
#define BG_LIGHTMAPED 2
#define BG_FLAT_COLOR 3

extern	cvar_t	*gl_eff_world_wireframe;
extern	cvar_t	*gl_eff_world_bg_type;
extern	cvar_t	*gl_eff_world_bg_color_r;
extern	cvar_t	*gl_eff_world_bg_color_g;
extern	cvar_t	*gl_eff_world_bg_color_b;
extern	cvar_t	*gl_eff_world_lines_color_r;
extern	cvar_t	*gl_eff_world_lines_color_g;
extern	cvar_t	*gl_eff_world_lines_color_b;

