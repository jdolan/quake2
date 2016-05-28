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
//#include <zlib.h>
#include "../include/minizip/unzip.h"

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/
#define FS_MAX_HASH_SIZE	1024
#define MAX_FILES_IN_PK2	0x4000
#define MAX_FILE_HANDLES	8

//
// in memory
//

typedef struct packfile_s
{
	char	*name;
	int		filepos;
	int		filelen;

	struct packfile_s *hashNext;
} packfile_t;

typedef struct pack_s
{
	unzFile		zFile;
	FILE		*fp;
	int			numfiles;
	packfile_t	*files;
	packfile_t	**fileHash;
	unsigned int hashSize;
	char		filename[1];
} pack_t;

typedef enum fsFileType_e {
	FS_FREE,
	FS_REAL,
	FS_PAK,
	FS_PK2,
	FS_GZIP,
	FS_BAD
} fsFileType_t;

typedef struct fsFile_s {
	char	fullpath[MAX_OSPATH];
	fsFileType_t type;
	uint32	mode;
	FILE *fp;
	void *zfp;
	packfile_t	*pak;
	qboolean unique;
	int	length;
} fsFile_t;

typedef struct searchpath_s
{
	pack_t		*pack;		// only one of filename / pack will be used
	struct		searchpath_s *next;
	char		filename[1];
} searchpath_t;

static char		fs_gamedir[MAX_OSPATH];

static cvar_t	*fs_basedir = &nullCvar;
//cvar_t	*fs_cddir;
cvar_t	*fs_gamedirvar;
static cvar_t	*fs_allpakloading;
static cvar_t	*fs_developer = &nullCvar;
#ifndef _WIN32
static cvar_t	*fs_usehomedir;
#endif

static searchpath_t	*fs_searchpaths;
static searchpath_t	*fs_base_searchpaths;	// without gamedirs

static fsFile_t		fs_files[MAX_FILE_HANDLES];

static qboolean fs_initialized = false;


#define FS_DPrintf (!fs_developer->integer) ? (void)0 : Com_Printf

/*

All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.

*/

/*
================
FS_AllocHandle
================
*/
static fsFile_t *FS_AllocHandle( fileHandle_t *f ) {
	fsFile_t *file;
	int i;

	for( i = 0, file = fs_files; i < MAX_FILE_HANDLES; i++, file++ ) {
		if( file->type == FS_FREE ) {
			break;
		}
	}

	if( i == MAX_FILE_HANDLES ) {
		Com_Error( ERR_FATAL, "FS_AllocHandle: none free" );
	}

	*f = i + 1;
	return file;
}

/*
================
FS_FileForHandle
================
*/
static fsFile_t *FS_FileForHandle( fileHandle_t f ) {
	fsFile_t *file;

	if( f < 1 || f > MAX_FILE_HANDLES ) {
		Com_Error( ERR_FATAL, "FS_FileForHandle: invalid handle: %i", f );
	}

	file = &fs_files[f - 1];
	if( file->type == FS_FREE ) {
		Com_Error( ERR_FATAL, "FS_FileForHandle: free file: %i", f );
	}

	if( file->type < FS_FREE || file->type >= FS_BAD ) {
		Com_Error( ERR_FATAL, "FS_FileForHandle: invalid file type: %i", file->type );
	}

	return file;
}

/*
================
FS_GetFileLength

Returns current length for files opened for writing.
Returns cached length for files opened for reading.
Returns compressed length for GZIP files.
================
*/
int FS_GetFileLength( fileHandle_t f ) {
	fsFile_t *file = FS_FileForHandle( f );
	int pos, length;

    if( file->type == FS_GZIP ) {
        return -1;
    }

	if( ( file->mode & FS_MODE_MASK ) == FS_MODE_READ) {
		return file->length;
	}

	if( file->type != FS_REAL ) {
		Com_Error( ERR_FATAL, "FS_GetFileLength: bad file type" );
    }

	pos = ftell( file->fp );
	fseek( file->fp, 0, SEEK_END );
	length = ftell( file->fp );
	fseek( file->fp, pos, SEEK_SET );

	return length;
}

/*
================
FS_GetFileFullPath
================
*/
const char *FS_GetFileFullPath( fileHandle_t f ) {
	return ( FS_FileForHandle( f ) )->fullpath;
}

/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
void FS_CreatePath (const char *path)
{
	char	*ofs, temp;
	
	for (ofs = (char *)path+1 ; *ofs ; ofs++) {
		if (*ofs == '/' || *ofs == '\\')
		{	// create the directory
			temp = *ofs;
			*ofs = 0;
			Sys_Mkdir (path);
			*ofs = temp;
		}
	}
}

/*
==============
FS_FCloseFile
==============
*/
void FS_FCloseFile( fileHandle_t f )
{
	fsFile_t *file = FS_FileForHandle( f );

	switch( file->type ) {
	case FS_REAL:
		fclose( file->fp );
		break;
	case FS_PAK:
		if( file->unique ) {
			fclose( file->fp );
		}
		break;
	case FS_GZIP:
		gzclose( file->zfp );
		break;
	case FS_PK2:
		unzCloseCurrentFile( file->zfp );
		if( file->unique ) {
			unzClose( file->zfp );
		}
		break;
	default:
		break;
	}

	/* don't clear name and mode, so post-restart reopening works */
	file->type = FS_FREE;
	file->fp = NULL;
	file->zfp = NULL;
	file->pak = NULL;
	file->unique = false;
}



static int FS_FOpenFileWrite( fsFile_t *file, const char *name ) {
	FILE *fp;
	gzFile zfp;
	char *modeStr, *ext;
	fsFileType_t type;
	uint32 mode;

	Com_sprintf( file->fullpath, sizeof( file->fullpath ), "%s/%s", fs_gamedir, name );

	mode = file->mode & FS_MODE_MASK;
	switch( mode ) {
	case FS_MODE_APPEND:
		modeStr = "ab";
		break;
	case FS_MODE_WRITE:
		modeStr = "wb";
		break;
	case FS_MODE_RDWR:
		modeStr = "r+b";
		break;
	default:
		Com_Error( ERR_FATAL, "FS_FOpenFileWrite( '%s' ): invalid mode mask", file->fullpath );
		modeStr = NULL;
		break;
	}

	//FS_ConvertToSysPath( file->fullpath );

	FS_CreatePath( file->fullpath );

	fp = fopen( file->fullpath, modeStr );
	if( !fp ) {
		FS_DPrintf( "FS_FOpenFileWrite: fopen( '%s', '%s' ) failed\n", file->fullpath, modeStr );
		return -1;
	}

	type = FS_REAL;
	if( !( file->mode & FS_FLAG_RAW ) ) {
		ext = COM_FileExtension( file->fullpath );
		if( !strcmp( ext, ".gz" ) ) {
			zfp = gzdopen( fileno( fp ), modeStr );
			if( !zfp ) {
				FS_DPrintf( "FS_FOpenFileWrite: gzopen( '%s', '%s' ) failed\n", file->fullpath, modeStr );
				fclose( fp );
				return -1;
			}
			file->zfp = zfp;
			type = FS_GZIP;
		}
	}

	FS_DPrintf( "FS_FOpenFileWrite( '%s' )\n", file->fullpath );

	file->fp = fp;
	file->type = type;
	file->length = 0;
	file->unique = true;

	if( mode == FS_MODE_WRITE ) {
		return 0;
	}

	if( mode == FS_MODE_RDWR ) {
		fseek( fp, 0, SEEK_END );
	}
	
	return ftell( fp );
}

/*
===========
FS_FOpenFileRead

Finds the file in the search path.
returns filesize and fills the fsFile_t
Used for streaming data out of either a pak file or
a seperate file.
In case of GZIP files, returns *raw* (compressed) length!
===========
*/
qboolean fs_fileFromPak = false;

static int FS_FOpenFileRead ( fsFile_t *file, const char *name, qboolean unique )
{
	unsigned int	hash;
	const searchpath_t	*search, *end = NULL;
	const pack_t	*pak;
	packfile_t		*pakfile;
	FILE			*fp;
	void			*zfp;
	int				pos, length;
	fsFileType_t	type;
	char			*ext;

	fs_fileFromPak = false;

	switch( file->mode & FS_PATH_MASK ) {
	case FS_PATH_BASE:
		search = fs_base_searchpaths;
		break;
	case FS_PATH_GAME:
		if (fs_searchpaths != fs_base_searchpaths) {
			end = fs_base_searchpaths;
		}
	default:
		search = fs_searchpaths;
		break;
	}
//
// search through the path, one element at a time
//
	hash = Com_HashValuePath(name);
	for (; search != end; search = search->next)
	{
	// is the element a pak file?
		if (search->pack)
		{
			if( ( file->mode & FS_TYPE_MASK ) == FS_TYPE_REAL ) {
				continue;
			}
		// look through all the pak file elements
			pak = search->pack;
			for ( pakfile = pak->fileHash[hash & (pak->hashSize-1)]; pakfile; pakfile = pakfile->hashNext) {
				if (Q_stricmp( pakfile->name, name ))
					continue;

				// found it!
				fs_fileFromPak = true;
				
				// open a new file on the pakfile
				if( pak->zFile ) {
					FS_DPrintf("FS_FOpenFileRead: pkz file %s : %s\n", pak->filename, name);
					if( unique ) {
						zfp = unzReOpen( pak->filename, pak->zFile );
						if( !zfp ) {
							Com_Error( ERR_FATAL, "FS_FOpenFileRead: unzReOpen( '%s' ) failed", pak->filename );
						}
					} else {
						zfp = pak->zFile;
					}
					if( unzSetOffset( zfp, pakfile->filepos ) != UNZ_OK ) {
						Com_Error( ERR_FATAL, "FS_FOpenFileRead: unzSetCurrentFileInfoPosition( '%s/%s' ) failed", pak->filename, pakfile->name );
					}
					if( unzOpenCurrentFile( zfp ) != UNZ_OK ) {
						Com_Error( ERR_FATAL, "FS_FOpenFileRead: unzReOpen( '%s/%s' ) failed", pak->filename, pakfile->name );
					}

					file->zfp = zfp;
					file->type = FS_PK2;
				} else {
					FS_DPrintf("FS_FOpenFileRead: pack file %s : %s\n", pak->filename, name);
					if( unique ) {
						fp = fopen( pak->filename, "rb" );
						if( !fp ) {
							Com_Error( ERR_FATAL, "Couldn't reopen %s", pak->filename );
						}
					} else {
						fp = pak->fp;
					}

					fseek( fp, pakfile->filepos, SEEK_SET );

					file->fp = fp;
					file->type = FS_PAK;
				}

				file->pak = pakfile;
				file->length = pakfile->filelen;
				file->unique = unique;

				return file->length;
			}
		}
		else
		{
			if( ( file->mode & FS_TYPE_MASK ) == FS_TYPE_PAK ) {
				continue;
			}
	// check a file in the directory tree
			Com_sprintf(file->fullpath, sizeof(file->fullpath), "%s/%s", search->filename, name);
			
			fp = fopen(file->fullpath, "rb");
#ifndef _WIN32
			if (!fp) {
				Q_strlwr(file->fullpath);
				fp = fopen(file->fullpath, "rb");
			}
#endif
			if (!fp)
				continue;
			
			type = FS_REAL;
			if( !( file->mode & FS_FLAG_RAW ) ) {
				ext = COM_FileExtension( file->fullpath );
				if( !strcmp( ext, ".gz" ) ) {
					zfp = gzdopen( fileno( fp ), "rb" );
					if( !zfp ) {
						Com_Printf( "FS_FOpenFileRead: gzopen( '%s', 'rb' ) failed, "
							"not a GZIP file?\n", file->fullpath );
						fclose( fp );
						return -1;
					}
					file->zfp = zfp;
					type = FS_GZIP;
				}
			}

			FS_DPrintf ("FS_FOpenFileRead: %s\n", file->fullpath);

			file->fp = fp;
			file->type = type;
			file->unique = true;

			pos = ftell( fp );
			fseek( fp, 0, SEEK_END );
			length = ftell( fp );
			fseek( fp, pos, SEEK_SET );

			file->length = length;

			return length;
		}
	}
	
	FS_DPrintf ("FS_FOpenFileRead: can't find %s\n", name);
	return -1;
}

/*
=================
FS_ReadFile

Properly handles partial reads
=================
*/
#ifdef CD_AUDIO
void CDAudio_Stop(void);
#endif
#define	MAX_READ	0x40000		// read in blocks of 256k
int FS_Read (void *buffer, int len, fileHandle_t hFile)
{
	int		block, remaining = len;
	int		read = 0;
	byte	*buf = (byte *)buffer;
	fsFile_t	*file = FS_FileForHandle( hFile );
#ifdef CD_AUDIO
	int		tries = 0;
#endif

	while (remaining)
	{
		block = min(remaining, MAX_READ);
		switch( file->type ) {
		case FS_REAL:
		case FS_PAK:
			read = fread( buf, 1, block, file->fp );
			break;
		case FS_GZIP:
			read = gzread( file->zfp, buf, block );
			break;
		case FS_PK2:
			read = unzReadCurrentFile( file->zfp, buf, block );
			break;
		default:
			break;
		}
		if (read == 0) {
#ifdef CD_AUDIO
			if (!tries) { // we might have been trying to read from a CD
				tries = 1;
				CDAudio_Stop();
			}
			else
#endif
			return len - remaining;
		} else if (read == -1) {
			Com_Error (ERR_FATAL, "FS_Read: -1 bytes read");
		}

		remaining -= read;
		buf += read;
	}
	return len;
}

/*
=================
FS_Write

Properly handles partial writes
=================
*/
#define	MAX_WRITE	0x40000		// write in blocks of 256k
int FS_Write( const void *buffer, int len, fileHandle_t hFile ) {
	int		block, remaining = len;
	int		write = 0;
	byte	*buf = (byte *)buffer;
	fsFile_t	*file = FS_FileForHandle( hFile );

	// read in chunks for progress bar
	while( remaining ) {
		block = min(remaining, MAX_WRITE);
		switch( file->type ) {
		case FS_REAL:
			write = fwrite( buf, 1, block, file->fp );
			break;
		case FS_GZIP:
			write = gzwrite( file->zfp, buf, block );
			break;
		default:
			Com_Error( ERR_FATAL, "FS_Write: illegal file type" );
			break;
		}
		if( write == 0 ) {
			return len - remaining;
		} else if( write == -1 ) {
			Com_Error( ERR_FATAL, "FS_Write: -1 bytes written" );
		}

		remaining -= write;
		buf += write;
	}


	if( ( file->mode & FS_FLUSH_MASK ) == FS_FLUSH_SYNC ) {
		switch( file->type ) {
		case FS_REAL:
			fflush( file->fp );
			break;
		case FS_GZIP:
			gzflush( file->zfp, Z_FINISH );
			break;
		default:
			break;
		}
	}

	return len;
}

/*
============
FS_FOpenFile
============
*/
int FS_FOpenFile (const char *filename, fileHandle_t *f, uint32 mode )
{
	fsFile_t	*file;
	fileHandle_t hFile;
	int			ret = -1;

	if( !filename || !f ) {
		Com_Error( ERR_FATAL, "FS_FOpenFile: NULL" );
	}

	*f = 0;

	if( !fs_searchpaths ) {
		return -1; // not yet initialized
	}

	/*if( !FS_ValidatePath( filename ) ) {
		FS_DPrintf( "FS_FOpenFile: refusing invalid path: %s\n", filename );
		return -1;
	}*/

	// allocate new file handle
	file = FS_AllocHandle( &hFile );
	file->mode = mode;

	mode &= FS_MODE_MASK;
	switch( mode ) {
	case FS_MODE_READ:
		ret = FS_FOpenFileRead( file, filename, true );
		break;
	case FS_MODE_WRITE:
	case FS_MODE_APPEND:
	case FS_MODE_RDWR:
		ret = FS_FOpenFileWrite( file, filename );
		break;
	default:
		Com_Error( ERR_FATAL, "FS_FOpenFile: illegal mode: %u on file %s", mode, filename );
		break;
	}

	// if succeeded, store file handle
	if( ret != -1 ) {
		*f = hFile;
	}

	return ret;
}

/*
============
FS_Tell
============
*/
int FS_Tell( fileHandle_t f ) {
	fsFile_t *file = FS_FileForHandle( f );
	int length;

	switch( file->type ) {
	case FS_REAL:
		length = ftell( file->fp );
		break;
	case FS_PAK:
		length = ftell( file->fp ) - file->pak->filepos;
		break;
	case FS_GZIP:
		length = gztell( file->zfp );
		break;
	case FS_PK2:
		length = unztell( file->zfp );
		break;
	default:
		length = -1;
		break;
	}

	return length;
}

/*
============
FS_RawTell
============
*/
int FS_RawTell( fileHandle_t f ) {
	fsFile_t *file = FS_FileForHandle( f );
	int length;

	switch( file->type ) {
	case FS_REAL:
		length = ftell( file->fp );
		break;
	case FS_PAK:
		length = ftell( file->fp ) - file->pak->filepos;
		break;
	case FS_GZIP:
		length = ftell( file->fp );
		break;
	case FS_PK2:
		length = unztell( file->zfp );
		break;
	default:
		length = -1;
		break;
	}

	return length;
}

/*
============
FS_LoadFile

Filename are relative to the quake search path
a null buffer will just return the file length without loading
============
*/
int FS_LoadFileEx( const char *path, void **buffer, uint32 flags ) {
	fsFile_t *file;
	fileHandle_t f;
	byte	*buf;
	int		length;

	if( !path ) {
		Com_Error( ERR_FATAL, "FS_LoadFile: NULL" );
	}

	if( buffer ) {
		*buffer = NULL;
	}

	if( !fs_searchpaths ) {
		return -1; // not yet initialized
	}

	// allocate new file handle
	file = FS_AllocHandle( &f );
	flags &= ~FS_MODE_MASK;
	file->mode = flags | FS_MODE_READ;

	// look for it in the filesystem or pack files
	length = FS_FOpenFileRead( file, path, false );
	if( length == -1 ) {
		return -1;
	}

	// get real file length
	length = FS_GetFileLength( f );
	if( buffer ) {
		*buffer = buf = Z_TagMalloc(length + 1, TAG_FS_LOADFILE);

		FS_Read( buf, length, f );
		buf[length] = 0;
	}

	FS_FCloseFile( f );

	return length;
}

int FS_LoadFile( const char *path, void **buffer ) {
	return FS_LoadFileEx( path, buffer, 0 );
}


/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile (void *buffer)
{
	if( !buffer ) {
		Com_Error( ERR_FATAL, "FS_FreeFile: NULL" );
	}
	Z_Free( buffer );
}

/*
=============
FS_CopyFile
=============
*/
qboolean FS_CopyFile( const char *src, const char *dst )
{
	int		len, size;
	fileHandle_t hSrc, hDst;
	byte	buffer[MAX_READ];

	FS_DPrintf( "FS_CopyFile('%s', '%s')\n", src, dst );

	FS_FOpenFile( src, &hSrc, FS_MODE_READ );
	if( !hSrc ) {
		FS_DPrintf("FS_CopyFile: Failed to open src %s\n", src);
		return false;
	}

	FS_FOpenFile( dst, &hDst, FS_MODE_WRITE );
	if( !hDst ) {
		FS_DPrintf("FS_CopyFile: Failed to open dst %s\n", dst);
		FS_FCloseFile( hSrc );
		return false;
	}

	size = FS_GetFileLength( hSrc );
	while( size ) {
		len = min(size, sizeof(buffer));
		if( !( len = FS_Read( buffer, len, hSrc ) ) ) {
			break;
		}
		FS_Write( buffer, len, hDst );
		size -= len;
	}

	FS_FCloseFile( hSrc );
	FS_FCloseFile( hDst );

	if(size) {
		FS_DPrintf("FS_CopyFile: Failed to write dst %s\n", dst);
		return false;
	}

	return true;
}

/*
================
FS_RemoveFile
================
*/
qboolean FS_RemoveFile( const char *filename )
{
	FS_DPrintf( "FS_RemoveFile( \"%s\" ): ", filename );

	if(!Sys_RemoveFile( filename )) {
		FS_DPrintf( "failed\n" );
		return false;
	}
	FS_DPrintf( "ok\n" );
	return true;
}

/*
================
FS_RemoveFile
================
*/
qboolean FS_RenameFile( const char *src, const char *dst )
{
	FS_DPrintf( "FS_RenameFile( '%s', '%s' ): ", src, dst );

	if(!Sys_RenameFile( src, dst )) {
		FS_DPrintf( "failed\n" );
		return false;
	}
	FS_DPrintf( "ok\n" );

	return true;
}

/*
================
FS_FPrintf
================
*/
void FS_FPrintf( fileHandle_t f, const char *format, ... ) {
	va_list argptr;
	char string[MAXPRINTMSG];
	int len;

	va_start( argptr, format );
	len = vsnprintf( string, sizeof( string ), format, argptr );
	va_end( argptr );

	FS_Write( string, len, f );
}

/*
=================
FS_LoadPakFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
static pack_t *FS_LoadPakFile (const char *packfile)
{
	dpackheader_t	header;
	dpackfile_t		*dfile, *info;
	packfile_t		*file;
	pack_t			*pack;
	FILE			*packhandle;
	int				i, numpackfiles, namesLength, len;
	unsigned int	hashSize, hash;
	char			*names;

	packhandle = fopen(packfile, "rb");
	if (!packhandle)
		return NULL;

	if (fread (&header, sizeof(header), 1, packhandle) != 1) {
		fclose(packhandle);
		Com_Error (ERR_FATAL, "FS_LoadPakFile: couldn't read pak header from %s", packfile);
	}

	if (LittleLong(header.ident) != IDPAKHEADER) {
		fclose(packhandle);
		Com_Error (ERR_FATAL, "FS_LoadPakFile: %s is not a packfile", packfile);
	}

	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	if (header.dirlen % sizeof(dpackfile_t)) {
		fclose(packhandle);
		Com_Error (ERR_FATAL, "FS_LoadPakFile: bad packfile %s (directory length %u is not a multiple of %d)", packfile, header.dirlen, (int)sizeof(dpackfile_t));
	}

	numpackfiles = header.dirlen / sizeof(dpackfile_t);
	if (numpackfiles <= 0) {
		Com_Printf("FS_LoadPakFile: '%s' has %i files", packfile, numpackfiles);
		fclose(packhandle);
		return NULL;
	}

	if(fseek(packhandle, header.dirofs, SEEK_SET)) {
		fclose(packhandle);
		Com_Error (ERR_FATAL, "FS_LoadPakFile: fseek() to offset %u in %s failed (corrupt packfile?)", header.dirofs, packfile);
		return NULL;
	}

	info = Z_TagMalloc (numpackfiles * sizeof(dpackfile_t), TAG_FS_LOADPAK);
	if (fread(info, 1, header.dirlen, packhandle) != header.dirlen) {
		fclose(packhandle);
		Z_Free(info);
		Com_Error (ERR_FATAL, "FS_LoadPakFile: error reading packfile directory from %s (failed to read %u bytes at %u)", packfile, header.dirofs, header.dirlen);
		return NULL;
	}

	namesLength = 0;
	for (i = 0, dfile = info; i<numpackfiles; i++, dfile++) {
		dfile->name[sizeof( dfile->name ) - 1] = 0;
		namesLength += strlen( dfile->name ) + 1;
	}

	for (hashSize = 1; hashSize < numpackfiles; hashSize <<= 1);
	
	if( hashSize > 32 ) {
		hashSize >>= 1;
	}

	len = strlen(packfile);
	pack = Z_TagMalloc(sizeof(pack_t) +
			numpackfiles * sizeof(packfile_t) +
			hashSize * sizeof(packfile_t *) +
			namesLength + len, TAG_FS_LOADPAK);

	strcpy(pack->filename, packfile);
	pack->files = ( packfile_t * )((byte *)pack + sizeof(pack_t) + len);
	pack->fileHash = ( packfile_t **)((byte *)pack->files + numpackfiles * sizeof( packfile_t ));
	pack->fp = packhandle;
	pack->zFile = NULL;
	pack->numfiles = numpackfiles;
	pack->hashSize = hashSize;

	names = ( char * )((byte *)pack->fileHash + hashSize * sizeof(packfile_t *));
	memset( pack->fileHash, 0, hashSize * sizeof( packfile_t * ) );

// parse the directory
	for (i = 0, file = pack->files, dfile = info; i < numpackfiles; i++, file++, dfile++)
	{
		len = strlen( dfile->name ) + 1;
		file->name = names;
		strcpy(file->name, dfile->name);

		file->filepos = LittleLong(dfile->filepos);
		file->filelen = LittleLong(dfile->filelen);

		hash = Com_HashValuePath(file->name) & (hashSize - 1);
		file->hashNext = pack->fileHash[hash];
		pack->fileHash[hash] = file;

		names += len;
	}
	Z_Free(info);

	Com_Printf ("Added pakfile '%s' (%d files).\n", packfile, numpackfiles );
	return pack;
}

/*
=================
FS_LoadPK2File
=================
*/
static pack_t *FS_LoadZipFile( const char *packfile )
{
	packfile_t		*file;
	pack_t			*pack;
	unzFile			zFile;
	unz_global_info	zGlobalInfo;
	unz_file_info	zInfo;
	int				i, numTotalFiles, numFiles, namesLength, len;
	unsigned int	hashSize, hash;
	char			name[MAX_QPATH], *names;

	zFile = unzOpen( packfile );
	if( !zFile ) {
		Com_Printf( "FS_LoadZipFile: unzOpen() failed on %s\n", packfile );
		return NULL;
	}

	if( unzGetGlobalInfo( zFile, &zGlobalInfo ) != UNZ_OK ) {
		Com_Printf( "FS_LoadZipFile: unzGetGlobalInfo() failed on %s\n", packfile );
		unzClose( zFile );
		return NULL;
	}

	if( unzGoToFirstFile( zFile ) != UNZ_OK ) {
		Com_Printf( "FS_LoadZipFile: unzGoToFirstFile() failed on %s\n", packfile );
		unzClose( zFile );
		return NULL;
	}

	numTotalFiles = zGlobalInfo.number_entry;
	namesLength = numFiles = 0;
	for( i = 0; i < numTotalFiles; i++ ) {
		if( unzGetCurrentFileInfo( zFile, &zInfo, name, sizeof( name ), NULL, 0, NULL, 0 ) != UNZ_OK ) {
			Com_Printf( "FS_LoadZipFile: unzGetCurrentFileInfo() failed on %s\n", packfile );
			unzClose( zFile );
			return NULL;
		}
		len = strlen(name);
		if (len > 1 && name[len-1] != '/' && name[len-1] != '\\') {
			namesLength += len + 1;	
			numFiles++;
		}

		if( i != numTotalFiles - 1 && unzGoToNextFile( zFile ) != UNZ_OK ) {
			Com_Printf( "FS_LoadZipFile: unzGoToNextFile() failed on %s\n", packfile );
		}
	}

	if( numFiles > MAX_FILES_IN_PK2 ) {
		Com_Printf( "FS_LoadZipFile: %s has too many files, %i > %i\n", packfile, numFiles, MAX_FILES_IN_PK2 );
		unzClose( zFile );
		return NULL;
	}

	for (hashSize = 1; hashSize < numFiles; hashSize <<= 1);
	
	if( hashSize > 32 ) {
		hashSize >>= 1;
	}

	len = strlen( packfile );
	len = ( len + 3 ) & ~3;
	pack = Z_TagMalloc( sizeof( pack_t ) +
		numFiles * sizeof( packfile_t ) +
		hashSize * sizeof( packfile_t * ) +
		namesLength + len, TAG_FS_LOADPAK);
	strcpy( pack->filename, packfile );
	pack->zFile = zFile;
	pack->fp = NULL;
	pack->numfiles = numFiles;
	pack->hashSize = hashSize;
	pack->files = ( packfile_t * )( ( byte * )pack + sizeof( pack_t ) + len );
	pack->fileHash = ( packfile_t ** )( pack->files + numFiles );
	names = ( char * )( pack->fileHash + hashSize );
	memset( pack->fileHash, 0, hashSize * sizeof( packfile_t * ) );

// parse the directory
	unzGoToFirstFile( zFile );

	for( i = 0, file = pack->files; i < numTotalFiles; i++ ) {
		unzGetCurrentFileInfo( zFile, &zInfo, name, sizeof( name ), NULL, 0, NULL, 0 );
		//Q_strlwr( name );

		len = strlen( name );
		if (len > 1 && name[len-1] != '/' && name[len-1] != '\\') {
			file->name = names;

			strcpy( file->name, name );
			file->filepos = unzGetOffset( zFile );
			file->filelen = zInfo.uncompressed_size;

			hash = Com_HashValuePath( file->name ) & (hashSize - 1);
			file->hashNext = pack->fileHash[hash];
			pack->fileHash[hash] = file;

			names += len + 1;
			file++;
		}
		
		if( i != numTotalFiles - 1 ) {
			unzGoToNextFile( zFile );
		}
	}

	Com_Printf("Added zipfile '%s' (%d files).\n", packfile, numFiles);
	return pack;
}

/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ... 
================
*/
static int pakcmp (const void *a, const void *b)
{
	if (*(int *)a > *(int *)b)
		return 1;
	else if (*(int *)a == *(int *)b)
		return 0;
	return -1;
}

#define MAX_NUMPAKS		256
#define MAX_NAMEPAKS	1024

static void FS_LoadPakFiles (void)
{
	searchpath_t	*search;
	int				i, j, pakMatchLen;
	pack_t			*pak;
	char			pakfile[MAX_OSPATH], pakmatch[MAX_OSPATH];
	char			*s, *fileNames[MAX_NAMEPAKS];
	int				pakfiles[100], numFiles, numPakFiles;
	qboolean		numPak;

	Com_sprintf (pakfile, sizeof(pakfile), "%s/*.pak", fs_gamedir);
	Com_sprintf (pakmatch, sizeof(pakmatch), "%s/pak", fs_gamedir);
	pakMatchLen = strlen(pakmatch);

	numFiles = numPakFiles = 0;

	s = Sys_FindFirst (pakfile, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);
	while (s)
	{
		i = (int)strlen(s);
		if (i > 4 && *(s+(i-4)) == '.' && !Q_stricmp(s+(i-3), "pak"))
		{
			numPak = false;

			//Check for pakxx.pak
			if (i > pakMatchLen + 4 && !Q_strnicmp(s, pakmatch, pakMatchLen) && i < pakMatchLen + 7)
			{
				for(j = pakMatchLen; j < i-4; j++) {
					if(s[j] < '0' || s[j] > '9')
						break;
				}
				if(j == i-4 && numPakFiles < 100)
					numPak = true;
			}

			if(numPak)
			{
				pakfiles[numPakFiles++] = atoi(s+pakMatchLen);
			}
			else if(fs_allpakloading->integer)
			{
				if(numFiles < MAX_NAMEPAKS)
					fileNames[numFiles++] = strdup(s);
				else
					Com_Printf("Warning: more than %i pak's found, ignoring %s.\n", MAX_NAMEPAKS, s);
			}
		}
		s = Sys_FindNext (0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);
	}
	Sys_FindClose ();

	if (numPakFiles) {
		// add any pak files in the format pak0.pak pak1.pak, ...
		qsort(pakfiles, numPakFiles, sizeof(pakfiles[0]), pakcmp);
		for (i = 0; i < numPakFiles; i++)
		{
			Com_sprintf (pakfile, sizeof(pakfile), "%s/pak%i.pak", fs_gamedir, pakfiles[i]);
			pak = FS_LoadPakFile(pakfile);
			if (pak) {
				search = Z_TagMalloc (sizeof(searchpath_t), TAG_FS_SEARCHPATH);
				search->pack = pak;
				search->filename[0] = 0;
				search->next = fs_searchpaths;
				fs_searchpaths = search;
			}
		}
	}

	if (numFiles) {
		qsort(fileNames, numFiles, sizeof(fileNames[0]), SortStrcmp);
		for (i = 0; i < numFiles; i++)
		{
			pak = FS_LoadPakFile(fileNames[i]);
			if (pak) {
				search = Z_TagMalloc (sizeof(searchpath_t), TAG_FS_SEARCHPATH);
				search->pack = pak;
				search->filename[0] = 0;
				search->next = fs_searchpaths;
				fs_searchpaths = search;
			}
			free(fileNames[i]);
		}
	}
}

static void FS_LoadPkzFiles (const char *ext)
{
	searchpath_t	*search;
	int				i, numFiles = 0;
	pack_t			*pak;
	char			pakfile[MAX_OSPATH], *s;
	char			*fileNames[MAX_NAMEPAKS];

	Com_sprintf (pakfile, sizeof(pakfile), "%s/*.%s", fs_gamedir, ext);

	s = Sys_FindFirst (pakfile, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);
	while (s)
	{
		i = (int)strlen(s);
		if (i > 4 && *(s+(i-4)) == '.' && !Q_stricmp(s+(i-3), ext))
		{
			if(numFiles < MAX_NAMEPAKS)
				fileNames[numFiles++] = strdup(s);
			else
				Com_Printf("Warning: more than %i %s's found, ignoring %s.\n", MAX_NAMEPAKS, ext, s);
		}
		s = Sys_FindNext (0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);
	}
	Sys_FindClose ();

	if(numFiles) {
		qsort (fileNames, numFiles, sizeof(fileNames[0]), SortStrcmp);
		for (i = 0; i < numFiles; i++)
		{
			pak = FS_LoadZipFile(fileNames[i]);
			if (pak) {
				search = Z_TagMalloc(sizeof(searchpath_t), TAG_FS_SEARCHPATH);
				search->pack = pak;
				search->filename[0] = 0;
				search->next = fs_searchpaths;
				fs_searchpaths = search;
			}
			free (fileNames[i]);
		}
	}
}

static void FS_AddGameDirectory (const char *dir)
{
	searchpath_t	*search;


	Q_strncpyz(fs_gamedir, dir, sizeof(fs_gamedir));

	//
	// add the directory to the search path
	//
	search = Z_TagMalloc(sizeof(searchpath_t) + strlen(fs_gamedir), TAG_FS_SEARCHPATH);
	strcpy(search->filename, fs_gamedir);
	search->pack = NULL;
	search->next = fs_searchpaths;
	fs_searchpaths = search;

	FS_LoadPakFiles();

	FS_LoadPkzFiles("pkz");
}

/*
================
FS_AddHomeAsGameDirectory

Use ~/.quake2/dir as fs_gamedir
================
*/
#ifndef _WIN32
static void FS_AddHomeAsGameDirectory (const char *dir)
{
	char gdir[MAX_OSPATH];
	char *homedir = getenv("HOME");

	if(homedir) {
		int len = snprintf(gdir,sizeof(gdir),"%s/.quake2/%s/", homedir, dir);
		Com_Printf("using %s for writing\n",gdir);
		FS_CreatePath (gdir);

		if ((len > 0) && (len < sizeof(gdir)) && (gdir[len-1] == '/'))
			gdir[len-1] = 0;

		FS_AddGameDirectory (gdir);
	}
}
#endif
/*
============
FS_Gamedir

Called to find where to write a file (demos, savegames, etc)
============
*/
char *FS_Gamedir (void)
{
	if (fs_gamedir[0])
		return fs_gamedir;
	else
		return BASEDIRNAME;
}

static void FS_FreeSearchPath( searchpath_t *path )
{
	pack_t *pak;

	if( ( pak = path->pack ) != NULL ) {
		if( pak->zFile ) {
			unzClose( pak->zFile );
		} else {
			fclose( pak->fp );
		}
		Z_Free( pak );
	}

	Z_Free( path );
}

/*
=============
FS_ExecAutoexec
=============
*/
void FS_ExecConfig (const char *filename)
{
	const char *dir;
	char name[MAX_QPATH];

	dir = Cvar_VariableString("gamedir");
	if (*dir)
		Com_sprintf(name, sizeof(name), "%s/%s/%s", fs_basedir->string, dir, filename); 
	else
		Com_sprintf(name, sizeof(name), "%s/%s/%s", fs_basedir->string, BASEDIRNAME, filename); 
	if (Sys_FindFirst(name, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM))
		Cbuf_AddText (va("exec %s\n", filename));
	Sys_FindClose();
}


/*
================
FS_DefaultGamedir
================
*/
static void FS_DefaultGamedir( void ) {

   	Com_sprintf( fs_gamedir, sizeof( fs_gamedir ), "%s/"BASEDIRNAME, fs_basedir->string );

#ifndef _WIN32
	if(fs_usehomedir->integer) {
		Com_sprintf(fs_gamedir, sizeof(fs_gamedir ), "%s/.quake2/"BASEDIRNAME, getenv("HOME"));
	}
#endif

	//Cvar_Set( "game", "" );
	//Cvar_Set( "gamedir", "" );

	Cvar_FullSet ("gamedir", "", CVAR_SERVERINFO|CVAR_NOSET);
	Cvar_FullSet ("game", "", CVAR_LATCHED|CVAR_SERVERINFO);
	fs_gamedirvar->flags |= CVAR_SYSTEM_FILES;
}

/*
================
FS_SetGamedir

Sets the gamedir and path to a different directory.
================
*/
static void FS_SetupGamedir(void)
{
	searchpath_t	*path, *next;
	const char	*dir;

	fs_gamedirvar = Cvar_Get("game", "", CVAR_LATCHED|CVAR_SERVERINFO);
	fs_gamedirvar->flags |= CVAR_SYSTEM_FILES;

	dir = fs_gamedirvar->string;
	if( !dir[0] || !strcmp( dir, BASEDIRNAME ) ) {
		FS_DefaultGamedir();
		return;
	}

	if (strstr(dir, "..") || strchr(dir, '/') || strchr(dir, '\\') || strchr(dir, ':') )
	{
		Com_Printf ("Gamedir should be a single filename, not a path\n");
		FS_DefaultGamedir();
		return;
	}

	//
	// free up any current game dir info
	//
	for( path = fs_searchpaths; path != fs_base_searchpaths; path = next ) {
		next = path->next;
		FS_FreeSearchPath( path );
	}
	fs_searchpaths = fs_base_searchpaths;

	Cvar_FullSet ("gamedir", dir, CVAR_SERVERINFO|CVAR_NOSET);

	FS_AddGameDirectory (va("%s/%s", fs_basedir->string, dir) );

#ifndef _WIN32
	if(fs_usehomedir->integer)
		FS_AddHomeAsGameDirectory(dir);
#endif
}

/*
============
FS_ExistsInGameDir

See if a file exists in the mod directory/paks (ignores baseq2)
============
*/
qboolean FS_ExistsInGameDir (const char *filename)
{
	unsigned int		hash;
	const searchpath_t	*search, *end;
	char				netpath[MAX_OSPATH];
	const pack_t		*pak;
	const packfile_t	*pakfile;
	FILE				*file;

	if ( fs_searchpaths != fs_base_searchpaths ) {
		end = fs_base_searchpaths;
	} else {
		end = NULL;
	}

	hash = Com_HashValuePath(filename);
	for (search = fs_searchpaths; search != end; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			pak = search->pack;
			for ( pakfile = pak->fileHash[hash & (pak->hashSize-1)]; pakfile; pakfile = pakfile->hashNext) {
				if (!Q_stricmp( pakfile->name, filename ) )	{	// found it!
					return true;
				}
			}
		}
		else
		{
			// check a file in the directory tree
			Com_sprintf (netpath, sizeof(netpath), "%s/%s", search->filename, filename);

			file = fopen (netpath, "rb");
#ifndef _WIN32
			if (!file) {
				Q_strlwr(netpath);
				file = fopen (netpath, "rb");
			}
#endif
			if (file) {
				fclose(file);
				return true;
			}
		}	
	}
	
	return false;
}

/*
** FS_ListFiles
*/
char **FS_ListFiles( const char *findname, int *numfiles, unsigned musthave, unsigned canthave )
{
	const char *s;
	int nfiles = 0;
	char **list = 0;

	s = Sys_FindFirst( findname, musthave, canthave );
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
			nfiles++;
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	if ( !nfiles )
		return NULL;

	nfiles++; // add space for a guard
	*numfiles = nfiles;

	list = Z_TagMalloc( sizeof( char * ) * nfiles, TAG_FS_FILELIST );
	memset( list, 0, sizeof( char * ) * nfiles );

	s = Sys_FindFirst( findname, musthave, canthave );
	nfiles = 0;
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
		{
			list[nfiles] = CopyString( s, TAG_FS_FILELIST );
#ifdef _WIN32
			Q_strlwr( list[nfiles] );
#endif
			nfiles++;
		}
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	return list;
}

/*
=================
FS_CopyFile_f

extract file from *.pak, *.pk2 or *.gz
=================
*/
static void FS_CopyFile_f( void ) {
	if( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: %s <sourcePath> <destPath>\n", Cmd_Argv( 0 ) );
		return;
	}

	if( FS_CopyFile( Cmd_Argv( 1 ), Cmd_Argv( 2 ) ) ) {
		Com_Printf( "File copied successfully\n" );
	} else {
		Com_Printf( "Failed to copy file\n" );
	}
}

static void FS_WhereIs_f (void)
{
	unsigned int	hash;
	const searchpath_t	*search;
	char			netpath[MAX_OSPATH];
	const pack_t			*pak;
	const packfile_t		*pakfile;
	char			*filename;
	FILE			*file;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("Purpose: Find where a file is being loaded from on the filesystem.\n"
					"Syntax : %s <path>\n"
					"Example: %s maps/q2dm1.bsp\n", Cmd_Argv(0), Cmd_Argv(0));
		return;
	}

	filename = Cmd_Argv(1);

	hash = Com_HashValuePath(filename);
	for (search = fs_searchpaths ; search ; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			pak = search->pack;
			for ( pakfile = pak->fileHash[hash & (pak->hashSize-1)]; pakfile; pakfile = pakfile->hashNext) {
				if (!Q_stricmp( pakfile->name, filename ) )	{	// found it!
					Com_Printf ("%s is found in pakfile %s as %s, %d bytes.\n", filename, pak->filename, pakfile->name, pakfile->filelen);
					return;
				}
			}
		}
		else
		{		
			// check a file in the directory tree
			Com_sprintf (netpath, sizeof(netpath), "%s/%s", search->filename, filename);
			
			file = fopen (netpath, "rb");
#ifndef _WIN32
			if (!file)
			{
				Q_strlwr(netpath);
				file = fopen (netpath, "rb");
			}
#endif
			if (file) {
				fclose(file);
				Com_Printf ("%s is found on disk as %s\n", filename, netpath);
				return;
			}
		}	
	}
	
	Com_Printf ("Can't find %s\n", filename);
}


/*
** FS_Dir_f
*/
static void FS_Dir_f( void )
{
	char	*path = NULL;
	char	findname[1024];
	char	wildcard[1024] = "*.*";
	char	**dirnames;
	int		ndirs;

	if ( Cmd_Argc() != 1 )
	{
		Q_strncpyz(wildcard, Cmd_Argv(1), sizeof(wildcard));
	}

	while ( ( path = FS_NextPath( path ) ) != NULL )
	{
		char *tmp = findname;

		Com_sprintf( findname, sizeof(findname), "%s/%s", path, wildcard );

		while ( *tmp != 0 )
		{
			if ( *tmp == '\\' ) 
				*tmp = '/';
			tmp++;
		}
		Com_Printf( "Directory of %s\n", findname );
		Com_Printf( "----\n" );

		if ( ( dirnames = FS_ListFiles( findname, &ndirs, 0, 0 ) ) != 0 )
		{
			int i;

			for ( i = 0; i < ndirs-1; i++ )
			{
				if ( strrchr( dirnames[i], '/' ) )
					Com_Printf( "%s\n", strrchr( dirnames[i], '/' ) + 1 );
				else
					Com_Printf( "%s\n", dirnames[i] );

				Z_Free( dirnames[i] );
			}
			Z_Free( dirnames );
		}
		Com_Printf( "\n" );
	}
}

/*
============
FS_Path_f

============
*/
static void FS_Path_f (void)
{
	const searchpath_t	*s;

	Com_Printf ("Current search path:\n");
	for (s=fs_searchpaths ; s ; s=s->next)
	{
		if (s == fs_base_searchpaths)
			Com_Printf ("----------\n");
		if (s->pack)
			Com_Printf ("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Com_Printf ("%s\n", s->filename);
	}
}

/*
================
FS_NextPath

Allows enumerating all of the directories in the search path
================
*/
char *FS_NextPath(const char *prevpath)
{
	searchpath_t *s;
	char *prev;

	prev = NULL; // fs_gamedir is the first directory in the searchpath
	for (s = fs_searchpaths; s; s = s->next) {
		if (s->pack)
			continue;
		if (prevpath == prev)
			return s->filename;
		prev = s->filename;
	}

	return NULL;
}


static void FS_PakList_f( void )
{
	searchpath_t *s;

	for (s = fs_searchpaths; s; s = s->next)
	{
		if (s->pack)
			Com_Printf ("PackFile: %s (%i files)\n", s->pack->filename, s->pack->numfiles);
	}
}

static void FS_Stats_f( void ) {
	searchpath_t *path;
	pack_t *pack, *maxpack = NULL;
	packfile_t *file, *max = NULL;
	int i;
	int len, maxLen = 0;
	int totalHashSize, totalLen;

	totalHashSize = totalLen = 0;
	for( path = fs_searchpaths; path; path = path->next ) {
		if( !( pack = path->pack ) ) {
			continue;
		}
		for( i = 0; i < pack->hashSize; i++ ) {
			if( !( file = pack->fileHash[i] ) ) {
				continue;
			}
			len = 0;
			for( ; file ; file = file->hashNext ) {
				len++;
			}
			if( maxLen < len ) {
				max = pack->fileHash[i];
				maxpack = pack;
				maxLen = len;
			}
			totalLen += len;
			totalHashSize++;
		}
		//totalHashSize += pack->hashSize;
		Com_Printf("%s hashSize: %i\n", pack->filename, pack->hashSize);
	}

	if( !totalHashSize ) {
		Com_Printf( "No stats to display\n" );
		return;
	}

	Com_Printf( "Maximum hash bucket length is %d, average is %.2f\n", maxLen, ( float )totalLen / totalHashSize );
	if( max ) {
		Com_Printf( "Dumping longest bucket (%s):\n", maxpack->filename );
		for( file = max; file ; file = file->hashNext ) {
			Com_Printf( "%s\n", file->name );
		}
	}
}

qboolean FS_SafeToRestart( void ) {
	fsFile_t	*file;
	int			i;
    
	// make sure no files are opened for reading
	for( i = 0, file = fs_files; i < MAX_FILE_HANDLES; i++, file++ ) {
		if( file->type == FS_FREE ) {
			continue;
		}
		if( file->mode == FS_MODE_READ ) {
            return false;
		}
    }

    return true;
}

/*
================
FS_Restart
================
*/
void FS_Restart( void )
{
	searchpath_t *path, *next;
	fsFile_t	*file;
	int			i;
	
	Com_Printf( "---------- FS_Restart ----------\n" );
	
	// make sure no files are opened for reading
	for( i = 0, file = fs_files; i < MAX_FILE_HANDLES; i++, file++ ) {
		if( file->type == FS_FREE ) {
			continue;
		}
		if( file->mode == FS_MODE_READ ) {
			Com_Error( ERR_FATAL, "FS_Restart: closing handle %i: %s", i + 1, file->fullpath );
		}
	}
	
	// just change gamedir
	for( path = fs_searchpaths; path != fs_base_searchpaths; path = next ) {
		next = path->next;
		FS_FreeSearchPath( path );
	}

	fs_searchpaths = fs_base_searchpaths;

	FS_SetupGamedir();

	Com_Printf( "--------------------------------\n" );
}

/*
================
FS_InitFilesystem
================
*/
void FS_InitFilesystem (void)
{
	Cmd_AddCommand ("path", FS_Path_f);
	Cmd_AddCommand ("dir", FS_Dir_f );
	Cmd_AddCommand ("fs_stats", FS_Stats_f );
	Cmd_AddCommand ("paklist", FS_PakList_f );
	Cmd_AddCommand ("copyfile", FS_CopyFile_f );
	Cmd_AddCommand ("whereis", FS_WhereIs_f);

	//
	// basedir <path>
	// allows the game to run from outside the data tree
	//
	fs_developer = Cvar_Get ("fs_developer", "0", 0);
	fs_basedir = Cvar_Get ("basedir", ".", 0);
	fs_allpakloading = Cvar_Get ("fs_allpakloading", "1", CVAR_ARCHIVE);

	//
	// cddir <path>
	// Logically concatenates the cddir after the basedir for 
	// allows the game to run from outside the data tree
	//
	//fs_cddir = Cvar_Get ("cddir", "", CVAR_NOSET);
	//if (fs_cddir->string[0])
	//	FS_AddGameDirectory (va("%s/"BASEDIRNAME, fs_cddir->string) );

#ifdef __APPLE__
	// add Contents/MacOS and Contents/Resources to the search path
	char path[MAX_OSPATH], *c;
	unsigned int i = sizeof(path);

	if (_NSGetExecutablePath(path, &i) > -1) {

		FS_DPrintf("FS_InitFilesystem: Resolved application bundle: %s\n", path);

		if ((c = strstr(path, "Quake II.app"))) {
			strcpy(c + strlen("Quake II.app/Contents/"), "MacOS/"BASEDIRNAME);
			FS_AddGameDirectory(path);

			strcpy(c + strlen("Quake II.app/Contents/"), "Resources/"BASEDIRNAME);
			FS_AddGameDirectory(path);

			*strrchr(path, '/') = '\0';
			Cvar_FullSet("basedir", path, CVAR_NOSET);
		}
	}
	else {
		FS_DPrintf("FS_InitFilesystem: Failed to resolve executable path\n");
	}
#elif __linux
	// add ./lib/baseq2 and ./share/baseq2 to the search path
	char path[MAX_OSPATH], *c;

	memset(path, 0, sizeof(path));

	if (readlink(va("/proc/%d/exe", getpid()), path, sizeof(path)) > -1) {

		FS_DPrintf("FS_InitFilesystem: Resolved executable path %s\n", path);

		if ((c = strstr(path, "quake2/bin"))) {
			strcpy(c + strlen("quake2/"), "bin/"BASEDIRNAME);
			FS_AddGameDirectory(path);

			strcpy(c + strlen("quake2/"), "share/"BASEDIRNAME);
			FS_AddGameDirectory(path);

			*strrchr(path, '/') = '\0';
			Cvar_FullSet("basedir", path, CVAR_NOSET);
		}
	}
	else {
		FS_DPrintf("FS_InitFilesystem: Failed to read /proc/%d/exe\n", getpid());
	}
#endif
	//
	// add baseq2 to search path
	//
	FS_AddGameDirectory (va("%s/"BASEDIRNAME, fs_basedir->string));

	//
	// then add a '.quake2/baseq2' directory in home directory by default
	//
#ifndef _WIN32
	fs_usehomedir = Cvar_Get ("fs_usehomedir", "1", 0);
	if(fs_usehomedir->integer)
		FS_AddHomeAsGameDirectory(BASEDIRNAME);
#endif

	// any set gamedirs will be freed up to here
	fs_base_searchpaths = fs_searchpaths;

	// check for game override
	FS_SetupGamedir();

	fs_initialized = true;
}

/*
================
FS_NeedRestart
================
*/
qboolean FS_NeedRestart( void ) {
	if( fs_gamedirvar->latched_string ) {
		return true;
	}
	
	return false;
}

// RAFAEL
/*
	Developer_searchpath
*/
int	Developer_searchpath (void)
{
	if( !strcmp( fs_gamedirvar->string, "xatrix" ) ) {
		return 1;
	}

	if( !strcmp( fs_gamedirvar->string, "rogue" ) ) {
		return 1;
	}

	return 0;
}

/*
 * Finds *.bsp files from Pakfiles _only_.
 */
char **FS_FindMaps(void) {
	searchpath_t *path;
	pack_t *pack;
	packfile_t *file;

	static char *maps[1024];
	int mapCount = 0;

	for(path = fs_searchpaths; path; path = path->next) {
		if(!(pack = path->pack))
			continue;
		for(int i = 0; i < pack->hashSize; i++) {
			if(!(file = pack->fileHash[i]))
				continue;
			for( ; file ; file = file->hashNext) {
				char *fileName = strdup(file->name);
				
				if(!fileName)
					continue;

				char *dot = strrchr(fileName, '.');
				if (dot && !strcmp(dot, ".bsp")) {

					char *token = strtok(fileName, "/");
					char *mapName;
					while(token != NULL) {
						mapName = token;
			      token = strtok(NULL, "/");
			   	}
			   	
			    mapName[strlen(mapName)-4] = '\0';

			    qboolean alreadyFound = false;
			    for(int o = 0; o < mapCount; o++) {
			    	if(strcmp(maps[o], mapName) == 0) {
			    		alreadyFound = true;
			    		break;
			    	}
			    }

			    if(!alreadyFound) {
			    	maps[mapCount++] = mapName;
			    }
				}
			}
		}
	}

	maps[mapCount] = "";
	return maps;
}
