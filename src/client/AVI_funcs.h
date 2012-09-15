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

#ifndef AVI_FUNC
#define AVI_FUNC(ret, func, params)
#define UNDEF_AVI_FUNC
#endif

AVI_FUNC (void, AVIFileInit, (void))
AVI_FUNC (HRESULT, AVIFileOpen, (PAVIFILE *, LPCTSTR, UINT, LPCLSID))
AVI_FUNC (HRESULT, AVIFileCreateStream, (PAVIFILE, PAVISTREAM *, AVISTREAMINFO *))
AVI_FUNC (HRESULT, AVIMakeCompressedStream, (PAVISTREAM *, PAVISTREAM, AVICOMPRESSOPTIONS *, CLSID *))
AVI_FUNC (HRESULT, AVIStreamSetFormat, (PAVISTREAM, LONG, LPVOID, LONG))
AVI_FUNC (HRESULT, AVIStreamWrite, (PAVISTREAM, LONG, LONG, LPVOID, LONG, DWORD, LONG *, LONG *))
AVI_FUNC (BOOL, AVISaveOptions, (HWND, UINT, int, PAVISTREAM *, LPAVICOMPRESSOPTIONS *))
AVI_FUNC (LONG, AVISaveOptionsFree, (int, LPAVICOMPRESSOPTIONS *))
AVI_FUNC (ULONG, AVIStreamRelease, (PAVISTREAM))
AVI_FUNC (ULONG, AVIFileRelease, (PAVIFILE))
AVI_FUNC (void, AVIFileExit, (void))

#ifdef UNDEF_AVI_FUNC
#undef AVI_FUNC
#undef UNDEF_AVI_FUNC
#endif

#ifndef ACM_FUNC
#define ACM_FUNC(ret, func, params)
#define UNDEF_ACM_FUNC
#endif

ACM_FUNC (MMRESULT, acmDriverOpen, (LPHACMDRIVER, HACMDRIVERID, DWORD))
ACM_FUNC (MMRESULT, acmDriverDetailsA, (HACMDRIVERID, LPACMDRIVERDETAILS, DWORD))
ACM_FUNC (MMRESULT, acmDriverEnum, (ACMDRIVERENUMCB, DWORD, DWORD))
ACM_FUNC (MMRESULT, acmFormatTagDetailsA, (HACMDRIVER, LPACMFORMATTAGDETAILS, DWORD))
ACM_FUNC (MMRESULT, acmStreamOpen, (LPHACMSTREAM, HACMDRIVER, LPWAVEFORMATEX, LPWAVEFORMATEX, LPWAVEFILTER, DWORD, DWORD, DWORD))
ACM_FUNC (MMRESULT, acmStreamSize, (HACMSTREAM, DWORD, LPDWORD, DWORD))
ACM_FUNC (MMRESULT, acmStreamPrepareHeader, (HACMSTREAM, LPACMSTREAMHEADER, DWORD))
ACM_FUNC (MMRESULT, acmStreamUnprepareHeader, (HACMSTREAM, LPACMSTREAMHEADER, DWORD))
ACM_FUNC (MMRESULT, acmStreamConvert, (HACMSTREAM, LPACMSTREAMHEADER, DWORD))
ACM_FUNC (MMRESULT, acmStreamClose, (HACMSTREAM, DWORD))
ACM_FUNC (MMRESULT, acmDriverClose, (HACMDRIVER, DWORD))

#ifdef UNDEF_ACM_FUNC
#undef ACM_FUNC
#undef UNDEF_ACM_FUNC
#endif

