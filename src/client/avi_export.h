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
// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.

#ifdef AVI_EXPORT
void CL_InitAVIExport( void );
void CL_ShutdownAVIExport( void );
void AVI_ProcessFrame (void);
void AVI_StopExport(void);
qboolean Movie_GetSoundtime(void);
void Movie_TransferStereo16 (void);
qboolean CL_AviRecording(void);
#endif

