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

#include "gl_local.h"

#include <jpeglib.h>

#ifndef WITHOUT_PNG
# include <png.h>
#endif


#define IMAGES_HASH_SIZE	128

image_t			gltextures[MAX_GLTEXTURES];
static image_t	*images_hash[IMAGES_HASH_SIZE];
int				numgltextures = 0;

//static byte	intensitytable[256];
static byte	gammatable[256];
static byte gammaintensitytable[256];

static cvar_t *gl_intensity;
static cvar_t *gl_texturelighting;
static cvar_t *gl_brightness;
static cvar_t *gl_contrast;
static cvar_t *gl_saturation;
static cvar_t *gl_monochrome;
static cvar_t *gl_invert;

uint32		d_8to24table[256];

//static int	gl_solid_format = 3;
//static int	gl_alpha_format = 4;

int		gl_tex_solid_format = 3;
int		gl_tex_alpha_format = 4;

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;

extern cvar_t *gl_gammapics;
extern cvar_t *gl_ext_max_anisotropy;

static qboolean GL_Upload8 (byte *data, int width, int height,  qboolean mipmap, qboolean is_sky, qboolean is_pic );

void GL_EnableMultitexture( qboolean enable )
{
	if ( !gl_state.multiTexture )
		return;

	GL_SelectTexture( 1 );

	if ( enable )
		qglEnable( GL_TEXTURE_2D );
	else
		qglDisable( GL_TEXTURE_2D );

	GL_TexEnv( GL_REPLACE );

	GL_SelectTexture( 0 );
	GL_TexEnv( GL_REPLACE );
}

void GL_SelectTexture( int tmu )
{
	if ( !gl_state.multiTexture )
		return;

	if ( tmu == gl_state.currentTMU )
		return;

	gl_state.currentTMU = tmu;

	if ( qglActiveTextureARB ) {
		qglActiveTextureARB( tmu + GL_TEXTURE0_ARB );
		qglClientActiveTextureARB( tmu + GL_TEXTURE0_ARB );
	} else if ( qglSelectTextureSGIS ) {
		qglSelectTextureSGIS( tmu + GL_TEXTURE0_SGIS );
	}

}

void GL_TexEnv( GLenum mode )
{
	if ( ( int )mode != gl_state.currentEnvModes[gl_state.currentTMU] )
	{
		qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode );
		gl_state.currentEnvModes[gl_state.currentTMU] = ( int )mode;
	}
}

extern	image_t	*draw_chars;

void GL_Bind (int texnum)
{
	if (gl_nobind->integer && draw_chars)		// performance evaluation option
		texnum = draw_chars->texnum;
	if ( gl_state.currentTextures[gl_state.currentTMU] == texnum)
		return;
	gl_state.currentTextures[gl_state.currentTMU] = texnum;
	qglBindTexture (GL_TEXTURE_2D, texnum);
}

void GL_MBind( int tmu, int texnum )
{
	GL_SelectTexture( tmu );

	if ( gl_state.currentTextures[tmu] == texnum )
		return;
	
	GL_Bind( texnum );
}

typedef struct
{
	char *name;
	int	minimize, maximize;
} glmode_t;

static const glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

//#define NUM_GL_MODES (sizeof(modes) / sizeof (glmode_t))
static const int NUM_GL_MODES = (sizeof(modes) / sizeof (glmode_t));

/*
===============
GL_TextureMode
===============
*/
void GL_TextureMode( const char *string )
{
	int		i;
	image_t	*glt;

	for (i=0 ; i< NUM_GL_MODES ; i++)
	{
		if ( !Q_stricmp( modes[i].name, string ) )
			break;
	}

	if (i == NUM_GL_MODES)
	{
		Com_Printf ("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i = 0, glt=gltextures; i < numgltextures; i++, glt++)
	{
		if (glt->type != it_pic && glt->type != it_sky )
		{
			GL_Bind (glt->texnum);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

/*
===============
GL_TextureBits
===============
*/
void GL_TextureBits(void)
{
	if (gl_texturebits->integer == 16) {
		gl_tex_solid_format = GL_RGB5;
		gl_tex_alpha_format = GL_RGBA4;
	}
	else if (gl_texturebits->integer == 32) {
		gl_tex_solid_format = GL_RGB8;
		gl_tex_alpha_format = GL_RGBA8;
	}
	else {
		gl_tex_solid_format = 3;
		gl_tex_alpha_format = 4;
	}
}

/*
===============
GL_ImageList_f
===============
*/

static int ImageSort( const image_t *a, const image_t *b )
{
	return strcmp (a->name, b->name);
}

void	GL_ImageList_f (void)
{
	int		i, count, texels;
	image_t	*image, *sortedList;

	for (i=0, count = 0, image=gltextures; i<numgltextures ; i++, image++)
	{
		if(image->texnum <= 0)
			continue;

		count++;
	}

	sortedList = Z_TagMalloc(count * sizeof(image_t), TAG_RENDER_IMAGE);

	count = 0;
	for (i=0, image=gltextures; i<numgltextures ; i++, image++)
	{
		if(image->texnum <= 0)
			continue;

		sortedList[count++] = *image;
	}

	qsort (sortedList, count, sizeof(sortedList[0]), (int (*)(const void *, const void *))ImageSort);

	Com_Printf ("------------------\n");
	texels = 0;

	for (i=0; i<count; i++)
	{
		image = &sortedList[i];

		texels += image->upload_width*image->upload_height;
		switch (image->type)
		{
		case it_skin:
			Com_Printf ("M");
			break;
		case it_sprite:
			Com_Printf ("S");
			break;
		case it_wall:
			Com_Printf ("W");
			break;
		case it_pic:
			Com_Printf ("P");
			break;
		default:
			Com_Printf (" ");
			break;
		}

		if (image->extension && *image->extension) {
			Com_Printf (" %3i %3i %s: %s.%s\n",
				image->upload_width, image->upload_height, (image->flags & IT_PALETTED) ? "PAL" : "RGB", image->name, image->extension);
		} else {
			Com_Printf (" %3i %3i %s: %s\n",
				image->upload_width, image->upload_height, (image->flags & IT_PALETTED) ? "PAL" : "RGB", image->name);
		}
	}
	Com_Printf ("%i images\n", count);
	Com_Printf ("Total texel count (not counting mipmaps): %i\n", texels);

	Z_Free(sortedList);
}


/*
=============================================================================

  scrap allocation

  Allocate all the little status bar obejcts into a single texture
  to crutch up inefficient hardware / drivers

=============================================================================
*/

#define	SCRAP_BLOCK_WIDTH	256
#define	SCRAP_BLOCK_HEIGHT	256

static int		scrap_allocated[SCRAP_BLOCK_WIDTH];
static byte		scrap_texels[SCRAP_BLOCK_WIDTH*SCRAP_BLOCK_HEIGHT];
static qboolean	scrap_dirty, scrap_uploaded;

// returns a texture number and the position inside it
static qboolean Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j, best, best2;

	best = SCRAP_BLOCK_HEIGHT;
	for (i = 0; i < SCRAP_BLOCK_WIDTH - w; i++)
	{
		best2 = 0;

		for (j = 0; j < w; j++)
		{
			if (scrap_allocated[i + j] >= best)
				break;
			if (scrap_allocated[i + j] > best2)
				best2 = scrap_allocated[i + j];
		}
		if (j == w) { // this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > SCRAP_BLOCK_HEIGHT)
		return false;

	for (i = 0; i < w; i++)
		scrap_allocated[*x + i] = best + h;

	return true;
}

void Scrap_Upload (void)
{
	GL_Bind(TEXNUM_SCRAPS);
	GL_Upload8 (scrap_texels, SCRAP_BLOCK_WIDTH, SCRAP_BLOCK_HEIGHT, false, false, it_pic);
	scrap_dirty = false;
	scrap_uploaded = true;
}

/*
=================================================================

PCX LOADING

=================================================================
*/


/*
==============
LoadPCX
==============
*/
static void LoadPCX (const char *filename, byte **pic, byte **palette, int *width, int *height)
{
	byte	*pix, *out, *raw, *end;
	pcx_t	*pcx;
	int		x, y, w, h, len;
	int		dataByte, runLength, exeedCount = 0, exeedBytes = 0;

	// load the file
	len = FS_LoadFile (filename, (void **)&raw);
	if (!raw)
		return;

	if (len < sizeof(*pcx) + 768) {
		Com_Printf ("LoadPCX: Bad pcx file: %s\n", filename);
		FS_FreeFile ((void *)raw);
		return;
	}

	// parse the PCX file
	pcx = (pcx_t *)raw;
	if (pcx->manufacturer != 0x0a || pcx->version != 5 || pcx->encoding != 1) {
		Com_Printf ("LoadPCX: Invalid PCX header: %s\n", filename);
		FS_FreeFile ((void *)pcx);
		return;
	}

	if (pcx->bits_per_pixel != 8) {
		Com_Printf ("LoadPCX: Only 8-bit PCX images are supported: %s\n", filename);
		FS_FreeFile ((void *)pcx);
		return;
	}

  	w = LittleShort(pcx->xmax) + 1;
    h = LittleShort(pcx->ymax) + 1;
	if (w > 640 || h > 480) {
		Com_Printf ("LoadPCX: Bad PCX file dimensions: %s: %i x %i\n", filename, w, h);
		FS_FreeFile ((void *)pcx);
		return;
	}

	if (palette){
		*palette = Z_TagMalloc( 768, TAG_RENDER_IMAGE );
		memcpy(*palette, (byte *)pcx + len - 768, 768);
	}

	if (!pic){
		FS_FreeFile((void *)pcx);
		return;	// only the palette was requested
	}

	pix = out = Z_TagMalloc( w * h, TAG_RENDER_IMAGE );

	raw = &pcx->data;
	end = ( byte * )pcx + len; //- 768;

	for ( y = 0; y < h; y++, pix += w )
	{
		for ( x = 0; x < w; )
		{
			if (raw == end) {
				Com_Printf ( "LoadPCX: %s is malformed\n", filename);
				Z_Free(out);
				FS_FreeFile ((void *)pcx);
				return;
			}
			dataByte = *raw++;

			if((dataByte & 0xC0) == 0xC0) {
				runLength = dataByte & 0x3F;
				if( x + runLength > w ) {
					exeedCount++;
					exeedBytes += x + runLength - w;
					runLength = w - x;
				}
				if (raw == end) {
					Com_Printf ( "LoadPCX: %s is malformed\n", filename);
					Z_Free(out);
					FS_FreeFile ((void *)pcx);
					return;
				}
				dataByte = *raw++;

				while(runLength--)
					pix[x++] = dataByte;
			} else {
				pix[x++] = dataByte;
			}
		}
	}

	if (exeedCount) {
		Com_DPrintf ( "LoadPCX: %s: runLength exceeds %d times with total %d bytes\n", exeedCount, exeedBytes);
	}

	*width = w;
	*height = h;
	*pic = out;

	FS_FreeFile ((void *)pcx);
}

/*
=========================================================

TARGA LOADING

=========================================================
*/
typedef struct _TargaHeader {
	byte 	idLength, colorMapType, imageType;
	byte	colorMapIndexLo, colorMapIndexHi;
	byte	colorMapLengthLo, colorMapLengthHi;
	byte	colorMapSize;
	byte	xOriginLo, xOriginHi, yOriginLo, yOriginHi;

	byte	widthLo, widthHi, heightLo, heightHi;
	byte	pixelSize, attributes;
} TargaHeader;

// Definitions for image types
#define TGA_Null		0	// no image data
#define TGA_Map			1	// Uncompressed, color-mapped images
#define TGA_RGB			2	// Uncompressed, RGB images
#define TGA_Mono		3	// Uncompressed, black and white images
#define TGA_RLEMap		9	// Runlength encoded color-mapped images
#define TGA_RLERGB		10	// Runlength encoded RGB images
#define TGA_RLEMono		11	// Compressed, black and white images
#define TGA_CompMap		32	// Compressed color-mapped data, using Huffman, Delta, and runlength encoding
#define TGA_CompMap4	33	// Compressed color-mapped data, using Huffman, Delta, and runlength encoding. 4-pass quadtree-type process

// Definitions for interleave flag
#define TGA_IL_None		0	// non-interleaved
#define TGA_IL_Two		1	// two-way (even/odd) interleaving
#define TGA_IL_Four		2	// four way interleaving
#define TGA_IL_Reserved	3	// reserved

// Definitions for origin flag
#define TGA_O_UPPER		0	// Origin in lower left-hand corner
#define TGA_O_LOWER		1	// Origin in upper left-hand corner

/*
=============
LoadTGA
=============
*/
static void LoadTGA( const char *filename, byte **pic, int *width, int *height, int *samples )
{
	int			w, h, x, y, i, colorMapLenght, imageType, pixelSize, bytesPerPixel;
	int			truerow, baserow, dstInc, interleave, length;
	qboolean	mapped, rlencoded;
	TargaHeader	*tgaHeader;
	byte		color[4], ColorMap[256*4], j, k, RLE_count, RLE_flag;
	uint32		*dst, *dstBase, *cSrc, *cmSrc;
	byte		*data, *pdata, *cmDst, *endData, *out;

	// load file
	length = FS_LoadFile( filename, (void **)&data );
	if( !data )
		return;

	if( length < sizeof( *tgaHeader ) ) {
		Com_Printf( "LoadTGA: %s: bad tga file.\n", filename );
		FS_FreeFile((void *)data);
		return;
	}

	tgaHeader = (TargaHeader *)data;

	endData = data + length;
	pdata = data + sizeof( *tgaHeader ) + tgaHeader->idLength;
	if( pdata + 4 > endData ) {
		Com_Printf( "LoadTGA: %s: malformed targa image.\n", filename );
		FS_FreeFile((void *)data);
		return;
	}

	w = (tgaHeader->widthHi << 8) | tgaHeader->widthLo;
	h = (tgaHeader->heightHi << 8) | tgaHeader->heightLo;
	if( !w || !h || w > 2048 || h > 2048 ) {
		Com_Printf( "LoadTGA: %s: has invalid dimensions: %dx%d\n", filename, w, h );
		FS_FreeFile((void *)data);
		return;
	}

	imageType = tgaHeader->imageType;
	pixelSize = tgaHeader->pixelSize;
	rlencoded = mapped = false;

	// validate TGA type
	switch( imageType ) {
	case TGA_RLEMap:
		rlencoded = true;
	case TGA_Map:
		// uncompressed colormapped image
		if (tgaHeader->colorMapType != 1) {
			Com_Printf( "LoadTGA: %s: Bad colormap type: %i != 1\n", filename, tgaHeader->colorMapType);
			FS_FreeFile((void *)data);
			return;
		}
		if( pixelSize != 8 ) {
			Com_Printf( "LoadTGA: %s: Only 8 bit images supported for type 1 and 9. Thise one is %i.", filename, pixelSize );
			FS_FreeFile((void *)data);
			return;
		}

		if ((tgaHeader->colorMapIndexHi << 8) | tgaHeader->colorMapIndexLo) {
			Com_Printf( "LoadTGA: %s: Colormap index not supported\n", filename);
			FS_FreeFile((void *)data);
			return;
		}

		colorMapLenght = (tgaHeader->colorMapLengthHi << 8) | tgaHeader->colorMapLengthLo;
		if( (unsigned)colorMapLenght > 256 ) {
			Com_Printf( "LoadTGA: %s: Only colormaps size up to 256 are supported. This one is %i.", filename, colorMapLenght);
			FS_FreeFile((void *)data);
			return;
		}

		bytesPerPixel = 1;
		if (pdata + ((tgaHeader->colorMapSize + 1) >> 3) * colorMapLenght + 1 > endData) {
			Com_Printf("LoadTGA: %s: malformed targa image\n", filename );
			FS_FreeFile((void *)data);
			return;
		}

		cmDst = ColorMap;
		switch( tgaHeader->colorMapSize ) {
		case 8:
			for(i = 0; i < colorMapLenght; i++, cmDst += 4 ) {
				cmDst[0] = cmDst[2] = cmDst[3] = *pdata++;
				cmDst[3] = 255;
			}
			break;
		case 15:
		case 16:
			for(i = 0; i < colorMapLenght; i++, cmDst += 4 ) {
				j = *pdata++;
				k = *pdata++;
				cmDst[0] = ((k & 0x7C) >> 2) << 3;
				cmDst[1] = (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3;
				cmDst[2] = (j & 0x1F) << 3;
				cmDst[3] = (k & 0x80) ? 255 : 0;
			}
			break;
		case 24:
			for(i = 0; i < colorMapLenght; i++, cmDst += 4 ) {
				cmDst[2] = *pdata++;
				cmDst[1] = *pdata++;
				cmDst[0] = *pdata++;
				cmDst[3] = 255;
			}
			break;
		case 32:
			for(i = 0; i < colorMapLenght; i++, cmDst += 4 ) {
				cmDst[2] = *pdata++;
				cmDst[1] = *pdata++;
				cmDst[0] = *pdata++;
				cmDst[3] = *pdata++;
			}
			break;
		default:
			Com_Printf ("LoadTGA: %s: Only 8, 15, 16, 24 and 32 bit colormaps supported. This one is %i.\n", filename, tgaHeader->colorMapSize );
			FS_FreeFile((void *)data);
			return;
		}
		for(; i < 256; i++, cmDst += 4) {
			cmDst[0] = cmDst[1] = cmDst[2] = cmDst[3] = 255;
		}
		mapped = true;
		break;
	case TGA_RLERGB:
		rlencoded = true;
	case TGA_RGB:
		// uncompressed or RLE compressed RGB
		switch( pixelSize ) {
		case 8:	bytesPerPixel = 1; break;
		case 15:
		case 16: bytesPerPixel = 2; break;
		case 24: bytesPerPixel = 3; break;
		case 32: bytesPerPixel = 4; break;
		default:
			Com_Printf ("LoadTGA: %s: Only 8, 15, 16, 24 and 32 bit Targas supported for type 2 and 10. This one is %i bit.\n", filename, pixelSize );
			FS_FreeFile((void *)data);
			return;
		}
		break;
	case TGA_RLEMono:
		rlencoded = true;
	case TGA_Mono:
		// uncompressed or RLE compressed greyscale
		if( pixelSize != 8 ) {
			Com_Printf ("LoadTGA: %s: Only 8 bit Targas supported for type 3 and 11. This one is %i bit.\n", filename, pixelSize );
			FS_FreeFile((void *)data);
			return;
		}
		bytesPerPixel = 1;
		break;
	default:
		Com_Printf ("LoadTGA: %s: Only type 1, 2, 3, 9, 10 and 11 Targas supported. This one is %i.\n", filename, imageType );
		FS_FreeFile((void *)data);
		return;
	}

	if (!rlencoded && pdata + bytesPerPixel * w * h > endData) {
		Com_Printf("LoadTGA: %s: malformed targa image\n", filename );
		FS_FreeFile((void *)data);
		return;
	}

	// check run-length encoding
	RLE_count = RLE_flag = 0;

	// read the Targa file body and convert to portable format
	interleave = tgaHeader->attributes >> 6;
	if (interleave == TGA_IL_Four)
		interleave = 4;
	else if (interleave == TGA_IL_Two)
		interleave = 2;
	else
		interleave = 1;

	truerow = baserow = 0;
	j = k = 0;

	color[0] = color[1] = color[2] = color[3] = 255;
	cSrc = (uint32 *)color;
	cmSrc = (uint32 *)ColorMap;

	out = Z_TagMalloc( w * h * 4, TAG_RENDER_IMAGE);
	dstBase = (uint32 *)out;
	if (tgaHeader->attributes & 0x20) { //Up to down
		i = w;
	} else {
		i = -w;
		dstBase += h * w - w;
	}
	if (tgaHeader->attributes & 0x10) { //Right to left
		dstInc = -1;
		dstBase += w - 1;
	} else {
		dstInc = 1;
	}

	for( y = 0; y < h; y++ ) {

		dst = dstBase + truerow * i;

		for( x = 0; x < w; x++ ) {
			// check if run length encoded
			if( rlencoded ) {
				if( !RLE_count ) { // have to restart run
					k = *pdata++;
					RLE_flag  = ( k & 0x80 );
					RLE_count = ( k & 0x7f );
					if (pdata + bytesPerPixel + !(RLE_flag) * bytesPerPixel * RLE_count > endData) {
						Com_Printf("LoadTGA: %s: malformed targa image %i %i\n", filename );
						FS_FreeFile((void *)data);
						Z_Free(out);
						return;
					}
				} else {
					RLE_count--;
					if( RLE_flag ) { // replicated pixels
						*dst = *cSrc;
						dst += dstInc;
						continue;
					}
				}
			}

			// read appropriate number of bytes, break into RGB
			switch( pixelSize ) {
			case 8:
				if (mapped) {
					*cSrc = cmSrc[*pdata++];
				} else {
					color[0] = color[1] = color[2] = *pdata++;
				}
				break;
			case 15:
			case 16:
				j = *pdata++;
				k = *pdata++;
				color[0] = ((k & 0x7C) >> 2) << 3;
				color[1] = (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3;
				color[2] = (j & 0x1F) << 3;
				color[3] = (k & 0x80) ? 255 : 0;
				break;
			case 24:
				color[2] = *pdata++;
				color[1] = *pdata++;
				color[0] = *pdata++;
				break;
			case 32:
				color[2] = *pdata++;
				color[1] = *pdata++;
				color[0] = *pdata++;
				color[3] = *pdata++;
				break;
			}
			*dst = *cSrc;
			dst += dstInc;
		}

		truerow += interleave;
		if (truerow >= h)
			truerow = ++baserow;
	}
	
	*samples = 3;
	if (mapped)
		bytesPerPixel = ((tgaHeader->colorMapSize + 1) >> 3);

	if (bytesPerPixel == 2 || bytesPerPixel == 4) {
		y = w*h;
		pdata = out + 3;
		for (i = 0; i < y; i++, pdata += 4) {
			if ( *pdata != 255 ) {
				*samples = 4;
				break;
			}
		}
	}

	*width = w;
	*height = h;
	*pic = out;

	FS_FreeFile ((void *)data );
}

qboolean WriteTGA( const char *name, byte *buffer, int width, int height )
{
	FILE		*f;
	int			i, c, temp;
	size_t		size;

	if( !(f = fopen( name, "wb" ) ) ) {
		Com_Printf( "WriteTGA: Couldn't create a file: %s\n", name ); 
		return false;
	}

	memset(buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width&255;
	buffer[13] = width>>8;
	buffer[14] = height&255;
	buffer[15] = height>>8;
	buffer[16] = 24;	// pixel size

	// swap rgb to bgr
	c = 18+width*height*3;
	for( i = 18; i < c; i += 3 ) {
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}
	size = fwrite( buffer, 1, c, f );
	fclose( f );

	return true;
} 

/*
=================================================================

JPEG LOADING

=================================================================
*/

static void jpg_null( j_decompress_ptr cinfo )
{
}

static boolean jpg_fill_input_buffer( j_decompress_ptr cinfo )
{
    Com_DPrintf("Premature end of JPEG data\n");
    return 1;
}

static void jpg_skip_input_data( j_decompress_ptr cinfo, long num_bytes )
{
    cinfo->src->next_input_byte += (size_t) num_bytes;
    cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
}

static void jpg_mem_src(j_decompress_ptr cinfo, byte *mem, int len)
{
    cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(struct jpeg_source_mgr));
    cinfo->src->init_source = jpg_null;
    cinfo->src->fill_input_buffer = jpg_fill_input_buffer;
    cinfo->src->skip_input_data = jpg_skip_input_data;
    cinfo->src->resync_to_restart = jpeg_resync_to_restart;
    cinfo->src->term_source = jpg_null;
    cinfo->src->bytes_in_buffer = len;
    cinfo->src->next_input_byte = mem;
}

/*
==============
LoadJPG
==============
*/
static void LoadJPG (const char *filename, byte **pic, int *width, int *height, int *samples)
{
	struct jpeg_decompress_struct	cinfo;
	struct jpeg_error_mgr			jerr;
	byte							*rawdata, *out, *scanline, *src, *dst;
	unsigned int					rawsize, i, w, h;
	int								components;

	// Load JPEG file into memory
	rawsize = FS_LoadFile(filename, (void **)&rawdata);
	if(!rawdata)
		return;	

	if (rawsize < 10 || rawdata[6] != 'J' || rawdata[7] != 'F' || rawdata[8] != 'I' || rawdata[9] != 'F') { 
		Com_Printf ("LoadJPG: Invalid JPEG header: %s\n", filename); 
		FS_FreeFile(rawdata); 
		return; 
	} 

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpg_mem_src(&cinfo, rawdata, rawsize);
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);

	components = cinfo.output_components;
	if(components != 3 && components != 1) {
		Com_Printf ( "LoadJPG: Invalid JPEG colour components %i (%s)\n", components, filename);
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(rawdata);
		return;
	}

	w = cinfo.output_width;
	h = cinfo.output_height;

	dst = out = Z_TagMalloc(w * h * 4, TAG_RENDER_IMAGE);
	scanline = Z_TagMalloc(w * components, TAG_RENDER_IMAGE);

	// Read Scanlines, and expand from RGB to RGBA
	if (components == 1) {
		while (cinfo.output_scanline < cinfo.output_height) {
			src = scanline;
			jpeg_read_scanlines(&cinfo, &scanline, 1);
			for (i = 0; i < cinfo.output_width; i++, dst += 4) {
				dst[0] = dst[1] = dst[2] = *src++;
				dst[3] = 255;
			}
		}
	} else {
		while (cinfo.output_scanline < cinfo.output_height) {
			src = scanline;
			jpeg_read_scanlines(&cinfo, &scanline, 1);
			for (i = 0; i < cinfo.output_width; i++, dst += 4, src += 3) {
				dst[0] = src[0], dst[1] = src[1], dst[2] = src[2];
				dst[3] = 255;
			}
		}
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	Z_Free(scanline);

	*width = w;
	*height = h;
	*pic = out;
	*samples = 3;

	FS_FreeFile (rawdata);
}

qboolean WriteJPG( const char *name, byte *buffer, int width, int height, int quality )
{
	struct jpeg_compress_struct		cinfo;
	struct jpeg_error_mgr			jerr;
	FILE							*f;
	JSAMPROW						s[1];
	int								offset, w3;

	if( !(f = fopen( name, "wb" )) ) {
		Com_Printf( "WriteJPG: Couldn't create a file: %s\n", name ); 
		return false;
	}

	// initialize the JPEG compression object
	cinfo.err = jpeg_std_error( &jerr );
	jpeg_create_compress( &cinfo );
	jpeg_stdio_dest( &cinfo, f );

	// setup JPEG parameters
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;

	jpeg_set_defaults( &cinfo );

	clamp(quality, 1, 100);

	jpeg_set_quality( &cinfo, quality, TRUE );

	// start compression
	jpeg_start_compress( &cinfo, true );

	// feed scanline data
	w3 = cinfo.image_width * 3;
	offset = w3 * cinfo.image_height - w3;
	while( cinfo.next_scanline < cinfo.image_height ) {
		s[0] = &buffer[offset - cinfo.next_scanline * w3];
		jpeg_write_scanlines( &cinfo, s, 1 );
	}

	// finish compression
	jpeg_finish_compress( &cinfo );
	jpeg_destroy_compress( &cinfo );

	fclose ( f );

	return true;
}


/*
=============
LoadPNG
by R1CH
=============
*/
#ifndef WITHOUT_PNG
typedef struct {
    byte *Buffer;
    size_t Pos;
} TPngFileBuffer;

static void PngReadFunc(png_struct *Png, png_bytep buf, png_size_t size)
{
    TPngFileBuffer *PngFileBuffer=(TPngFileBuffer*)png_get_io_ptr(Png);
    memcpy(buf,PngFileBuffer->Buffer+PngFileBuffer->Pos,size);
    PngFileBuffer->Pos+=size;
}

static void LoadPNG (const char *filename, byte **pic, int *width, int *height, int *samples)
{
	png_uint_32		rowbytes, row, w, h;
	png_structp		png_ptr;
	png_infop		info_ptr;
	png_byte		bit_depth, color_type;
	byte			**row_pointers, *out, *dst;
	TPngFileBuffer	PngFileBuffer = {NULL,0};

	FS_LoadFile (filename, (void **)&PngFileBuffer.Buffer);
    if (!PngFileBuffer.Buffer)
		return;

	if ((png_check_sig(PngFileBuffer.Buffer, 8)) == 0) {
		Com_Printf ("LoadPNG: Not a PNG file: %s\n", filename);
		FS_FreeFile (PngFileBuffer.Buffer); 
		return;
    }

	PngFileBuffer.Pos = 0;

    png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL,  NULL, NULL);
    if (!png_ptr) {
		Com_Printf ("LoadPNG: couldnt create read struct\n");
		FS_FreeFile (PngFileBuffer.Buffer);
		return;
	}

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
		Com_Printf ("LoadPNG: couldnt create info struct\n", filename);
        png_destroy_read_struct(&png_ptr, NULL, NULL);
		FS_FreeFile (PngFileBuffer.Buffer);
		return;
    }
    
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		FS_FreeFile (PngFileBuffer.Buffer);
		return;
	}

	png_set_read_fn (png_ptr,(png_voidp)&PngFileBuffer,(png_rw_ptr)PngReadFunc);

	png_read_info(png_ptr, info_ptr);

	bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);

	if (color_type == PNG_COLOR_TYPE_PALETTE)
	{
		png_set_palette_to_rgb (png_ptr);
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
	}

	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	if (color_type != PNG_COLOR_TYPE_RGBA)
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

	if (bit_depth < 8)
        png_set_expand(png_ptr);
	else if (bit_depth == 16)
		png_set_strip_16(png_ptr);

	png_read_update_info(png_ptr, info_ptr);

	rowbytes = png_get_rowbytes(png_ptr, info_ptr);

	w = png_get_image_width(png_ptr, info_ptr);
	h = png_get_image_height(png_ptr, info_ptr);

	dst = out = Z_TagMalloc(h * rowbytes, TAG_RENDER_IMAGE);
	row_pointers = Z_TagMalloc(h * sizeof(*row_pointers), TAG_RENDER_IMAGE);

	for (row = 0; row < h; row++, dst += rowbytes)
		row_pointers[row] = dst;

	png_read_image(png_ptr, row_pointers);

	Z_Free(row_pointers);

	*width = w;
	*height = h;
	*pic = out;
	*samples = png_get_channels(png_ptr, info_ptr);

	png_read_end(png_ptr, info_ptr);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	FS_FreeFile (PngFileBuffer.Buffer);
}
#endif


typedef struct
{
	unsigned int width, height;
	unsigned int num, frame;
	int format;
	byte *buffer;
} aviDump_t;

static aviDump_t aviDump;
/*
==================
R_BeginAviDemo
==================
*/
void R_BeginAviDemo( int format )
{
	if( aviDump.buffer )
		Z_Free( aviDump.buffer );

	aviDump.width = vid.width;
	aviDump.height = vid.height;
	aviDump.buffer = Z_TagMalloc(aviDump.width * aviDump.height * 3 + 18, TAG_RENDER_SCRSHOT);
	aviDump.num++;
	aviDump.frame = 0;
	aviDump.format = format;
}

/* 
==================
R_WriteAviFrame
==================
*/
void R_WriteAviFrame( void )
{
	char checkname[MAX_OSPATH];

	if( !aviDump.buffer )
		return;
	
	if(aviDump.height != vid.height || aviDump.width != vid.width)
	{	//Screen size changed, need to alloc new size for buffer
		if( aviDump.buffer )
			Z_Free( aviDump.buffer );

		aviDump.width = vid.width;
		aviDump.height = vid.height;
		aviDump.buffer = Z_TagMalloc(aviDump.width * aviDump.height * 3 + 18, TAG_RENDER_SCRSHOT);
	}

	// create the avi directory if it doesn't exist
	Com_sprintf( checkname, sizeof( checkname ), "%s/avi", FS_Gamedir() );
	Sys_Mkdir( checkname );
	Com_sprintf( checkname, sizeof( checkname ), "%s/avi/avi%i-%06i.%s", FS_Gamedir(), aviDump.num, aviDump.frame, ( aviDump.format > 0 ) ? "jpg" : "tga" );

	if( aviDump.format > 0 ) {
		qglReadPixels( 0, 0, aviDump.width, aviDump.height, GL_RGB, GL_UNSIGNED_BYTE, aviDump.buffer ); 
		WriteJPG( checkname, aviDump.buffer, aviDump.width, aviDump.height, gl_screenshot_quality->integer );
	} else {
		qglReadPixels( 0, 0, aviDump.width, aviDump.height, GL_RGB, GL_UNSIGNED_BYTE, aviDump.buffer + 18 ); 
		WriteTGA( checkname, aviDump.buffer, aviDump.width, aviDump.height );
	}
	aviDump.frame++;
}

/* 
==================
R_StopAviDemo
==================
*/
void R_StopAviDemo( void )
{
	if( aviDump.buffer )
		Z_Free( aviDump.buffer );

	aviDump.buffer = NULL;
	aviDump.width = aviDump.height = aviDump.frame = 0;
}

/*
====================================================================

IMAGE FLOOD FILLING

====================================================================
*/


/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes
=================
*/

typedef struct
{
	short		x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

static void R_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	byte				fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0, filledcolor = -1, i;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i) {
			if (d_8to24table[i] == 255) { // alpha 1.0
				filledcolor = i;
				break;
			}
		}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255)) {
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		byte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}

//=======================================================

/*
================
GL_ResampleTexture
================
*/
static int		resampleWidth = 0, resampleImgSize = 0;
static byte		*resampleBuffer = NULL;
extern qboolean	r_modelRegistering;

static byte *GL_ResampleTexture (const byte *in, int inwidth, int inheight, int outwidth, int outheight)
{
	int				i, j;
	const byte		*inrow1, *inrow2;
	uint32			frac, fracstep;
	uint32			*p1, *p2;
	const byte		*pix1, *pix2, *pix3, *pix4;
	float			heightScale;
	byte			*out, *buf;

	i = outwidth * outheight * 4;
	if (!gl_state.registering) {
		buf = out = Z_TagMalloc( i + outwidth * sizeof( uint32 ) * 2, TAG_RENDER_IMGRESAMPLE);
	}
	else if( i + outwidth > resampleImgSize + resampleWidth ) {
		if( resampleBuffer )
			Z_Free( resampleBuffer );
		resampleWidth = outwidth;
		resampleImgSize = i;
		buf = out = resampleBuffer = Z_TagMalloc( i + outwidth * sizeof( uint32 ) * 2, TAG_RENDER_IMGRESAMPLE);
	} else {
		buf = out = resampleBuffer;
	}

	p1 = (uint32 *)(out + i);
	p2 = p1 + outwidth;

	fracstep = inwidth*0x10000/outwidth;

	frac = fracstep>>2;
	for (i = 0; i < outwidth; i++) {
		p1[i] = 4*(frac>>16);
		frac += fracstep;
	}

	frac = 3*(fracstep>>2);
	for (i = 0; i < outwidth; i++) {
		p2[i] = 4*(frac>>16);
		frac += fracstep;
	}

	heightScale = (float)inheight / outheight;
	inwidth <<= 2;
	for (i = 0; i < outheight; i++)
	{
		inrow1 = in + inwidth*(int)((i+0.25f) * heightScale);
		inrow2 = in + inwidth*(int)((i+0.75f) * heightScale);

		for (j = 0; j < outwidth; j++)
		{
			pix1 = inrow1 + p1[j];
			pix2 = inrow1 + p2[j];
			pix3 = inrow2 + p1[j];
			pix4 = inrow2 + p2[j];
			out[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			out[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			out[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			out[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
			out += 4;
		}
	}

	return buf;
}

/*
 * @brief Clamps the components of the specified vector to 1.0, scaling the vector
 * down if necessary.
 */
static vec_t ColorNormalize(const vec3_t in, vec3_t out) {
	vec_t max = 0.0;
	int32_t i;

	VectorCopy(in, out);

	for (i = 0; i < 3; i++) { // find the brightest component

		if (out[i] < 0.0) // enforcing positive values
			out[i] = 0.0;

		if (out[i] > max)
			max = out[i];
	}

	if (max > 1.0) // clamp without changing hue
		VectorScale(out, 1.0 / max, out);

	return max;
}

/*
 * @brief Applies brightness, saturation and contrast to the specified input color.
 */
static void ColorFilter(const vec3_t in, vec3_t out, float brightness, float saturation, float contrast) {
	const vec3_t luminosity = { 0.2125, 0.7154, 0.0721 };
	vec3_t intensity;
	float d;
	int32_t i;

	ColorNormalize(in, out);

	if (brightness != 1.0) { // apply brightness
		VectorScale(out, brightness, out);

		ColorNormalize(out, out);
	}

	if (contrast != 1.0) { // apply contrast

		for (i = 0; i < 3; i++) {
			out[i] -= 0.5; // normalize to -0.5 through 0.5
			out[i] *= contrast; // scale
			out[i] += 0.5;
		}

		ColorNormalize(out, out);
	}

	if (saturation != 1.0) { // apply saturation
		d = DotProduct(out, luminosity);

		VectorSet(intensity, d, d, d);
		VectorMix(intensity, out, saturation, out);

		ColorNormalize(out, out);
	}
}

/**
 * @brief Applies Quetoo brightness, saturation and contrast.
 */
static void GL_FilterTexture(byte *data, int w, int h) {
	int i, j, c = w * h;
	byte *p = data;

	for (i = 0; i < c; i++, p += 4) {
		vec3_t temp;

		VectorScale(p, 1.0 / 255.0, temp); // convert to float

		// apply brightness, saturation and contrast
		ColorFilter(temp, temp, gl_brightness->value, gl_saturation->value, gl_contrast->value);

		for (j = 0; j < 3; j++) {
			temp[j] = Clamp(temp[j] * 255, 0, 255); // back to byte
			p[j] = (byte) temp[j];
		}

		if (gl_monochrome->integer) // monochrome
			p[0] = p[1] = p[2] = (p[0] + p[1] + p[2]) / 3;

		if (gl_invert->integer) { // inverted
			p[0] = 255 - p[0];
			p[1] = 255 - p[1];
			p[2] = 255 - p[2];
		}
	}
}

/*
================
GL_LightScaleTexture

Scale up the pixel values in a texture to increase the
lighting range
================
*/
static void GL_LightScaleTexture (byte *in, int inwidth, int inheight, qboolean only_gamma )
{
	int		i, c;
	byte	*p;

	p = in;
	c = inwidth * inheight;

	if ( only_gamma )
	{
		for (i = 0; i < c; i++, p += 4) {
			p[0] = gammatable[p[0]];
			p[1] = gammatable[p[1]];
			p[2] = gammatable[p[2]];
		}
	}
	else
	{
		for (i = 0; i < c; i++, p += 4) {
			p[0] = gammaintensitytable[p[0]];
			p[1] = gammaintensitytable[p[1]];
			p[2] = gammaintensitytable[p[2]];
		}
	}
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
static void GL_MipMap (byte *in, int width, int height)
{
	int		i, j, row;
	byte	*out = in;

	row = width * 4;
	width >>= 1;
	height >>= 1;

	if ( width == 0 || height == 0 ) {
		width += height;	// get largest
		for (i=0 ; i<width ; i++, out+=4, in+=8 ) {
			out[0] = ( in[0] + in[4] )>>1;
			out[1] = ( in[1] + in[5] )>>1;
			out[2] = ( in[2] + in[6] )>>1;
			out[3] = ( in[3] + in[7] )>>1;
		}
		return;
	}

	for (i=0 ; i<height ; i++, in+=row) {
		for (j=0 ; j<width ; j++, out+=4, in+=8) {
			out[0] = (in[0] + in[4] + in[row+0] + in[row+4])>>2;
			out[1] = (in[1] + in[5] + in[row+1] + in[row+5])>>2;
			out[2] = (in[2] + in[6] + in[row+2] + in[row+6])>>2;
			out[3] = (in[3] + in[7] + in[row+3] + in[row+7])>>2;
		}
	}
}

/*
===============
GL_Upload32

Returns has_alpha
===============
*/
static int	upload_width, upload_height;

static qboolean GL_Upload32 (byte *data, int width, int height, qboolean mipmap, qboolean is_pic, int samples)
{
	qboolean	alphaSamples;
	byte		*scaled = NULL;
	int			scaled_width, scaled_height, comp;

	for (scaled_width = 1; scaled_width < width; scaled_width<<=1);
	for (scaled_height = 1; scaled_height < height; scaled_height<<=1);

	if (mipmap)
	{
		if (gl_round_down->integer) {
			if (scaled_width > width)
				scaled_width >>= 1;
			if (scaled_height > height)
				scaled_height >>= 1;
		}
		// let people sample down the world textures for speed
		if (gl_picmip->integer > 0) {
			scaled_width >>= gl_picmip->integer;
			scaled_height >>= gl_picmip->integer;
		}
	}

	// don't ever bother with maxTextureSize textures
	while ( scaled_width > gl_config.maxTextureSize || scaled_height > gl_config.maxTextureSize ) {
		scaled_width >>= 1;
		scaled_height >>= 1;
	}

	if (scaled_width < 1)
		scaled_width = 1;

	if (scaled_height < 1)
		scaled_height = 1;

	upload_width = scaled_width;
	upload_height = scaled_height;

	alphaSamples = (samples == 4);
	if (gl_state.texture_compression)
		comp = (alphaSamples) ? GL_COMPRESSED_RGBA_ARB : GL_COMPRESSED_RGB_ARB;
	else
		comp = (alphaSamples) ? gl_tex_alpha_format : gl_tex_solid_format;

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap) {
			qglTexImage2D (GL_TEXTURE_2D, 0, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		scaled = data;
	}
	else
	{
		//scaled = Z_TagMalloc(scaled_width * scaled_height * 4, TAG_RENDER_IMAGE);
		//GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);
		scaled = GL_ResampleTexture(data, width, height, scaled_width, scaled_height);
	}

	if (gl_texturelighting && gl_texturelighting->integer) {
		GL_FilterTexture(scaled, scaled_width, scaled_height);
	}
	else if(!is_pic || gl_gammapics->integer) {
		GL_LightScaleTexture (scaled, scaled_width, scaled_height, !mipmap );
	}

	if(gl_state.sgis_mipmap && mipmap)
		qglTexParameteri (GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);

	qglTexImage2D( GL_TEXTURE_2D, 0, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled );

	if (mipmap && !gl_state.sgis_mipmap)
	{
		int	miplevel = 0;

		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap (scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;

			qglTexImage2D (GL_TEXTURE_2D, miplevel, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}
	}
	if (!gl_state.registering && scaled && scaled != data)
		Z_Free(scaled);

done: ;

	if (mipmap) {
		if (gl_config.anisotropic)
			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ext_max_anisotropy->value);

		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	} else {
		if (gl_config.anisotropic)
			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0);

		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	return alphaSamples;
}

/*
===============
GL_Upload8

Returns has_alpha
===============
*/

static qboolean GL_Upload8 (byte *data, int width, int height,  qboolean mipmap, qboolean is_sky, qboolean is_pic )
{
	byte		trans[512*256*4], *dest, p;
	int			i, s, samples = 3;

	s = width*height;
	if (s > 512*256)
		Com_Error (ERR_DROP, "GL_Upload8: too large");

	dest = trans;
	for (i = 0; i < s; i++, dest += 4)
	{
		p = data[i];
		*( uint32 * )dest = d_8to24table[p];

		if (p == 255)
		{
			// transparent, so scan around for another color
			// to avoid alpha fringes
			// FIXME: do a full flood fill so mips work...
			if (i > width && data[i-width] != 255)
				p = data[i-width];
			else if (i < s-width && data[i+width] != 255)
				p = data[i+width];
			else if (i > 0 && data[i-1] != 255)
				p = data[i-1];
			else if (i < s-1 && data[i+1] != 255)
				p = data[i+1];
			else
				p = 0;
			// copy rgb components
			dest[0] = ((byte *)&d_8to24table[p])[0];
			dest[1] = ((byte *)&d_8to24table[p])[1];
			dest[2] = ((byte *)&d_8to24table[p])[2];
			samples = 4;
		}
	}

	return GL_Upload32 (trans, width, height, mipmap, is_pic, samples);
}

static char nullExtension[4] = "";
/*
================
GL_LoadPic

This is also used as an entry point for the generated r_notexture
================
*/
image_t *GL_LoadPic (const char *name, byte *pic, int width, int height, imagetype_t type, int flags, int samples)
{
	image_t		*image;
	int			i;
	qboolean	hasAlpha;

	// find a free image_t
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->texnum)
			break;
	}
	if (i == numgltextures)
	{
		if (numgltextures == MAX_GLTEXTURES)
			Com_Error (ERR_DROP, "MAX_GLTEXTURES");
		numgltextures++;
	}
	image = &gltextures[i];
	Q_strncpyz( image->name, name, sizeof(image->name) );
	image->extension = nullExtension;
	image->registration_sequence = registration_sequence;

	image->width = width;
	image->height = height;
	image->type = type;

	if (flags & (IT_REPLACE_WAL|IT_REPLACE_PCX))
	{
		char	buffer[MAX_QPATH];
		int		length;
		fileHandle_t h = 0;

		length = strlen( image->name );
		if (length > 4 && image->name[length - 4] == '.') {
			strncpy( buffer, image->name, length - 4 );
			if (flags & IT_REPLACE_WAL) {
				miptex_t mt;

				strcpy(buffer + length - 4, ".wal");
				length = FS_FOpenFile(buffer, &h, FS_MODE_READ);
				if (h) {
					if(length > sizeof(mt)) {
						FS_Read(&mt, sizeof(mt), h);
						image->width = LittleLong(mt.width);
						image->height = LittleLong(mt.height);
					}
					FS_FCloseFile(h);
				}
			} else {
				pcx_t	pcx;

				strcpy(buffer + length - 4, ".pcx");
				length = FS_FOpenFile(buffer, &h, FS_MODE_READ);
				if (h) {
					if(length > sizeof(pcx)) {
						FS_Read(&pcx, sizeof(pcx), h);
						image->width = LittleShort(pcx.xmax) + 1;
						image->height = LittleShort(pcx.ymax) + 1;
					}
					FS_FCloseFile(h);
				}
			}
		}
	}


	if (flags & IT_PALETTED)
	{
		if (type == it_skin) {
			R_FloodFillSkin(pic, width, height);
		}
		else if (image->type == it_pic && width < 64 && height < 64)
		{	// load little pics into the scrap
			int		x = 0, y = 0, j = 0;

			if( Scrap_AllocBlock( width, height, &x, &y ) )
			{
				byte *src, *dst, *ptr;

				src = pic;
				dst = &scrap_texels[y * SCRAP_BLOCK_WIDTH + x];
				// copy the texels into the scrap block
				for (i = 0; i < height; i++) {
					ptr = dst;
					for (j = 0; j < width; j++) {
						*ptr++ = *src++;;
					}
					dst += SCRAP_BLOCK_WIDTH;
				}

				flags |= IT_SCRAP | IT_TRANS;
				image->texnum = TEXNUM_SCRAPS;
				image->flags = flags;
				image->sl = (x+0.01f)/(float)SCRAP_BLOCK_WIDTH;
				image->sh = (x+image->width-0.01f)/(float)SCRAP_BLOCK_WIDTH;
				image->tl = (y+0.01f)/(float)SCRAP_BLOCK_WIDTH;
				image->th = (y+image->height-0.01f)/(float)SCRAP_BLOCK_WIDTH;

				scrap_dirty = true;
				if( !gl_state.registering ) {
					Scrap_Upload();
				}
				return image;
			}
		}
	}

	image->texnum = TEXNUM_IMAGES + (image - gltextures);
	GL_Bind(image->texnum);
	if (flags & IT_PALETTED)
		hasAlpha = GL_Upload8 (pic, width, height, (type == it_wall || type == it_skin), image->type == it_sky, image->type == it_pic );
	else
		hasAlpha = GL_Upload32(pic, width, height, (type == it_wall || type == it_skin), image->type == it_pic, samples );

	if(hasAlpha)
		flags |= IT_TRANS;

	image->flags = flags;
	image->upload_width = upload_width;		// after power of 2 and scales
	image->upload_height = upload_height;
	image->sl = image->tl = 0;
	image->sh = image->th = 1;

	return image;
}


/*
================
GL_LoadWal
================
*/
static image_t *GL_LoadWal (const char *name)
{
	miptex_t	*mt;
	int			width, height, ofs, length;
	image_t		*image;

	length = FS_LoadFile (name, (void **)&mt);
	if (!mt) {
		Com_Printf ("GL_LoadWal: can't load %s\n", name);
		return NULL;
	}

	width = LittleLong (mt->width);
	height = LittleLong (mt->height);
	ofs = LittleLong (mt->offsets[0]);

	if (width < 1 || height < 1) {
		Com_Printf( "GL_LoadWal: bad WAL file '%s' (%i x %i)\n", name, mt->width, mt->height);
		FS_FreeFile ((void *)mt);
		return NULL;
	}
	if( ofs + width * height > length ) {
		Com_Printf( "GL_LoadWal: '%s' is malformed\n", name );
		FS_FreeFile ((void *)mt);
		return NULL;
	}

	image = GL_LoadPic (name, (byte *)mt + ofs, width, height, it_wall, IT_PALETTED, 0);

	FS_FreeFile ((void *)mt);

	return image;
}

/*
===============
GL_FindImage

Finds or loads the given image
===============
*/
static void GL_LoadImage32 (char *pathname, char *ext, byte **pic, int *width, int *height, int *samples)
{
#ifndef WITHOUT_PNG
	ext[1] = 'p'; ext[2] = 'n'; ext[3] = 'g';
	LoadPNG(pathname, pic, width, height, samples);
	if (*pic)
		return;
#endif

	ext[1] = 't'; ext[2] = 'g'; ext[3] = 'a';
	LoadTGA(pathname, pic, width, height, samples);
	if (*pic)
		return;

	ext[1] = 'j'; ext[2] = 'p'; ext[3] = 'g';
	LoadJPG(pathname, pic, width, height, samples);
}

image_t	*GL_FindImage (const char *name, imagetype_t type)
{
	image_t	*image;
	int		i, len = 0, lastDot = -1;
	byte	*pic;
	int		width = 0, height = 0, flags = 0, samples = 0;
	char	pathname[MAX_QPATH], *ext;
	unsigned int hash;


	if (!name)
		return NULL;

	for( i = ( name[0] == '/' || name[0] == '\\' ); name[i] && len < MAX_QPATH; i++ ) {
		if ( name[i] == '\\' ) {
			pathname[len++] = '/';
		}
		else {
			if ( name[i] == '.' )
				lastDot = len;
			pathname[len++] = name[i];
		}
	}

	if (len < 5)
		return NULL;
	else if( lastDot != -1 )
		len = lastDot;

	if (len > MAX_QPATH-5) {
		Com_Error( ERR_FATAL, "GL_FindImage: oversize name: %d chars", len );
	}

	pathname[len] = 0;

	hash = Com_HashKey(pathname, IMAGES_HASH_SIZE);
	// look for it
	for (image = images_hash[hash]; image; image = image->hashNext)
	{
		if (image->type == type && !strcmp(pathname, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	// load the pic from disk
	pathname[len] = '.';
	pathname[len+4] = 0;

	ext = pathname+len;

	pic = NULL;
	if (!strcmp(ext, ".pcx"))
	{
		if(gl_replacepcx->integer) {
			GL_LoadImage32(pathname, ext, &pic, &width, &height, &samples);
			if(!pic) {
				ext[1] = 'p'; ext[2] = 'c'; ext[3] = 'x';
				LoadPCX(pathname, &pic, NULL, &width, &height);
				if (!pic)
					return NULL;

				flags |= IT_PALETTED;
			}
			else {
				flags |= IT_REPLACE_PCX;
			}
		}
		else {
			LoadPCX(pathname, &pic, NULL, &width, &height);
			if (!pic)
				return NULL;

			flags |= IT_PALETTED;
		}
		image = GL_LoadPic(pathname, pic, width, height, type, flags, samples);
	}
	else if (!strcmp(ext, ".wal"))
	{
		if(gl_replacewal->integer)
		{
			GL_LoadImage32(pathname, ext, &pic, &width, &height, &samples);
			if(!pic)
			{
				ext[1] = 'w'; ext[2] = 'a'; ext[3] = 'l';
				image = GL_LoadWal(pathname);
				if(!image)
					return r_notexture;
			}
			else {
				flags |= IT_REPLACE_WAL;
				image = GL_LoadPic(pathname, pic, width, height, type, flags, samples);
			}
		}
		else
		{
			image = GL_LoadWal(pathname);
			if(!image)
				return r_notexture;
		}
	}
	else //try png, tga and jpg
	{
		GL_LoadImage32(pathname, ext, &pic, &width, &height, &samples);
		if(!pic)
			return NULL;

		image = GL_LoadPic(pathname, pic, width, height, type, flags, samples);
	}

	if (pic)
		Z_Free(pic);

	image->name[len] = 0;
	image->extension = image->name + len + 1;

	image->hashNext = images_hash[hash];
	images_hash[hash] = image;

	return image;
}

/*
===============
R_RegisterSkin
===============
*/
struct image_s *R_RegisterSkin (const char *name)
{
	return GL_FindImage (name, it_skin);
}


/*
================
GL_FreeUnusedImages

Any image that was not touched on this registration sequence
will be freed.
================
*/
void GL_FreeUnusedImages (void)
{
	int		i;
	image_t	*image, *entry, **back;
	unsigned int hash;

	// never free r_notexture or particle texture
	r_notexture->registration_sequence = registration_sequence;
	r_whitetexture->registration_sequence = registration_sequence;
	r_particletexture->registration_sequence = registration_sequence;

	if (r_caustictexture)
		r_caustictexture->registration_sequence = registration_sequence;
	
	if (r_shelltexture)
		r_shelltexture->registration_sequence = registration_sequence;

	GL_FreeUnusedDecalImages();

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
			continue;		// used this sequence
		if (!image->registration_sequence)
			continue;		// free image_t slot
		if (image->type == it_pic)
			continue;		// don't free pics

		// delete it from hash table
		hash = Com_HashKey(image->name, IMAGES_HASH_SIZE);
		for( back=&images_hash[hash], entry=images_hash[hash]; entry; back=&entry->hashNext, entry=entry->hashNext ) {
			if( entry == image ) {
				*back = entry->hashNext;
				break;
			}
		}

		// free it
		if( !(image->flags & IT_SCRAP) )
			qglDeleteTextures (1, (GLuint *)&image->texnum);

		memset (image, 0, sizeof(*image));
	}

	if(resampleBuffer)
		Z_Free (resampleBuffer);

	resampleWidth = resampleImgSize = 0;
	resampleBuffer = NULL;

	if( scrap_dirty ) {
		Scrap_Upload();
	}
}


/*
===============
Draw_GetPalette
===============
*/
void Draw_GetPalette (void)
{
	int			i;	
	byte		*pal = NULL, *src;
	byte	default_pal[] = 
	{
	#include "def_pal.h"
	};

	// get the palette
	LoadPCX("pics/colormap.pcx", NULL, &pal, NULL, NULL);
	if (!pal)
		pal = default_pal;

	for (src = pal, i = 0; i < 256; i++, src += 3) {	
		d_8to24table[i] = LittleLong((255<<24) + (src[0]<<0) + (src[1]<<8) + (src[2]<<16));
	}

	d_8to24table[255] &= LittleLong(0xffffff);	// 255 is transparent

	if (pal != default_pal)
		Z_Free (pal);
}

void R_InitBuildInTextures (void)
{
	byte	data[16*16*4], *dst;
	int		x, y, dx2, dy, d;
	const byte	dottexture[8][8] =
	{
		{0,0,0,0,0,0,0,0},
		{0,0,1,1,0,0,0,0},
		{0,1,1,1,1,0,0,0},
		{0,1,1,1,1,0,0,0},
		{0,0,1,1,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0}
	};

	// r_notexture, use this for bad textures, but without alpha
	for (dst = data, x = 0 ; x < 8 ; x++) {
		for(y = 0; y < 8; y++) {
			dst[0] = dottexture[x&3][y&3]*255;
			dst[1] = dst[2] = 0;
			dst[3] = 255;
			dst += 4;
		}
	}
	r_notexture = GL_LoadPic ("***r_notexture***", data, 8, 8, it_wall, 0, 3);

	// Particle texture
	for (dst = data, x = 0; x < 16; x++) {
		dx2 = x - 8;
		dx2 = dx2 * dx2;
		for (y = 0; y < 16; y++) {
			dy = y - 8;
			d = 255 - 35 * sqrt( dx2 + dy * dy );

			dst[0] = dst[1] = dst[2] = 255;
			dst[3] = bound( 0, d, 255 );
			dst += 4;
		}
	}
	r_particletexture = GL_LoadPic ("***particle***", data, 16, 16, it_sprite, 0, 4);

	// White texture
	memset( data, 255, sizeof(data));
	r_whitetexture = GL_LoadPic ("***r_whitetexture***", data, 8, 8, it_pic, 0, 3);

	// Other
	r_shelltexture = NULL;
	if (gl_shelleffect->OnChange)
		gl_shelleffect->OnChange(gl_shelleffect, gl_shelleffect->resetString);

	r_caustictexture = NULL;
	if (gl_watercaustics->OnChange)
		gl_watercaustics->OnChange(gl_watercaustics, gl_watercaustics->resetString);
}

static void OnChange_ImageParams(cvar_t *self, const char *oldValue) {
	Com_Printf("%s will be changed on vid_restart\n", self->name);
}

/*
===============
GL_InitImages
===============
*/
void	GL_InitImages (void)
{
	int		i, j;
	float	g = vid_gamma->value, inf;

	registration_sequence = 1;

	// init intensity conversions
	gl_intensity = Cvar_Get ("gl_intensity", "2.666", CVAR_ARCHIVE);
	gl_intensity->OnChange = OnChange_ImageParams;

	if ( gl_intensity->value < 1 )
		Cvar_Set( "gl_intensity", "1" );

	gl_state.inverse_intensity = 1 / gl_intensity->value;

	Draw_GetPalette ();

	if (gl_config.renderer & (GL_RENDERER_VOODOO | GL_RENDERER_VOODOO2) || g == 1) {
		for ( i = 0; i < 256; i++ ) {
			gammatable[i] = i;
		}
	} else {
		for ( i = 0; i < 256; i++ ) {
			inf = 255.0f * (float)pow( (i+0.5f)*ONEDIV255_5, g ) + 0.5f;
			clamp(inf, 0, 255);
			gammatable[i] = (byte)inf;
		}
	}

	for ( i = 0; i < 256; i++ ) {
		j = (int)((float)i*gl_intensity->value);
		clamp(j, 0, 255);
		gammaintensitytable[i] = gammatable[j];
	}

	gl_texturelighting = Cvar_Get("gl_texturelighting", "1", CVAR_ARCHIVE);
	gl_texturelighting->OnChange = OnChange_ImageParams;

	gl_brightness = Cvar_Get("gl_brightness", "1.666", CVAR_ARCHIVE);
	gl_brightness->OnChange = OnChange_ImageParams;

	gl_contrast = Cvar_Get("gl_contrast", "1.0", CVAR_ARCHIVE);
	gl_contrast->OnChange = OnChange_ImageParams;

	gl_saturation = Cvar_Get("gl_saturation", "1.0", CVAR_ARCHIVE);
	gl_saturation->OnChange = OnChange_ImageParams;

	gl_monochrome = Cvar_Get("gl_monochrome", "0", 0);
	gl_monochrome->OnChange = OnChange_ImageParams;

	gl_invert = Cvar_Get("gl_invert", "0", 0);
	gl_monochrome->OnChange = OnChange_ImageParams;

	R_InitBuildInTextures();

	gl_state.currentTMU = 0;
	memset( gl_state.currentTextures, -1, sizeof(gl_state.currentTextures) );
	memset( gl_state.currentEnvModes, -1, sizeof(gl_state.currentEnvModes) );
}

/*
===============
GL_ShutdownImages
===============
*/
void GL_ShutdownImages (void)
{
	int		i;
	image_t	*image;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->registration_sequence)
			continue;		// free image_t slot
		// free it
		if( !(image->flags & IT_SCRAP) )
			qglDeleteTextures (1, (GLuint *)&image->texnum);
	}
	if (scrap_uploaded) {
		i = TEXNUM_SCRAPS;
		qglDeleteTextures (1, (GLuint *)&i);
	}

	numgltextures = 0;
	memset(gltextures, 0, sizeof(gltextures));
	memset(images_hash, 0, sizeof(images_hash));
	memset(scrap_allocated, 0, sizeof(scrap_allocated));
	scrap_dirty = scrap_uploaded = false;

	if(resampleBuffer)
		Z_Free (resampleBuffer);

	resampleWidth = resampleImgSize = 0;
	resampleBuffer = NULL;

	R_StopAviDemo();
}

