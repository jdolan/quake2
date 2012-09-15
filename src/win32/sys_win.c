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
// sys_win.h
#define USE_DBGHELP
#include "../qcommon/qcommon.h"
#include "winquake.h"
#include "resource.h"
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include <mmsystem.h>
#include "../win32/conproc.h"
#ifdef USE_DBGHELP
#include <dbghelp.h>
#endif

#define MINIMUM_WIN_MEMORY	0x0a00000
#define MAXIMUM_WIN_MEMORY	0x1000000

//#define DEMO

qboolean s_win95;

int			starttime;
qboolean	ActiveApp;
qboolean	Minimized;

static HANDLE		hinput, houtput;

unsigned	sys_msg_time;
unsigned	sys_frame_time;


//static HANDLE		qwclsemaphore;

#define	MAX_NUM_ARGVS	128
int			argc;
char		*argv[MAX_NUM_ARGVS];

static qboolean qDedConsole = true;
static cvar_t	*win_consolelogging;

/*
===============================================================================

SYSTEM IO

===============================================================================
*/


void Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	timeEndPeriod( 1 );

	CL_Shutdown ();
	Qcommon_Shutdown ();

	if (qDedConsole)
	{
		MSG		msg;
		BOOL	bRet;

		Conbuf_AppendText( text );
		Conbuf_AppendText( "\n" );

		Sys_SetErrorText( text );
		Sys_ShowConsole( 1, true );

		// wait for the user to quit
		while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
			if (bRet == -1)
				break;

			TranslateMessage (&msg);
      		DispatchMessage (&msg);

			Sleep(25);
		}
	}
	else
	{
		MessageBox(NULL, text, "Error", 0 /* MB_OK */ );
	}

	Sys_DestroyConsole();

	exit (1);
}

void Sys_Quit (void)
{
	timeEndPeriod( 1 );

	CL_Shutdown();

	Qcommon_Shutdown();

	Sys_DestroyConsole();

	exit (0);
}

//================================================================


/*
================
Sys_ScanForCD

================
*/
char *Sys_ScanForCD (void)
{
	static char	cddir[MAX_OSPATH];
	static qboolean	done;
#ifndef DEMO
	char		drive[4];
	FILE		*f;
	char		test[MAX_QPATH];

	if (done)		// don't re-check
		return cddir;

	// no abort/retry/fail errors
	SetErrorMode (SEM_FAILCRITICALERRORS);

	drive[0] = 'c';
	drive[1] = ':';
	drive[2] = '\\';
	drive[3] = 0;

	done = true;

	// scan the drives
	for (drive[0] = 'c' ; drive[0] <= 'z' ; drive[0]++)
	{
		// where activision put the stuff...
		sprintf (cddir, "%sinstall\\data", drive);
		sprintf (test, "%sinstall\\data\\quake2.exe", drive);
		f = fopen(test, "r");
		if (f)
		{
			fclose (f);
			if (GetDriveType (drive) == DRIVE_CDROM)
				return cddir;
		}
	}
#endif

	cddir[0] = 0;
	
	return NULL;
}

/*
================
Sys_CopyProtect

================
*/
void	Sys_CopyProtect (void)
{
#ifndef DEMO
	char	*cddir;

	cddir = Sys_ScanForCD();
	if (!cddir[0])
		Com_Error (ERR_FATAL, "You must have the Quake2 CD in the drive to play.");
#endif
}


//================================================================


/*
================
Sys_Init
================
*/
void Sys_Init (void)
{
	OSVERSIONINFO	vinfo;

	timeBeginPeriod( 1 );

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error ("Couldn't get OS info");

	if (vinfo.dwMajorVersion < 4)
		Sys_Error ("%s requires windows version 4 or greater", APPLICATION);
	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32s)
		Sys_Error ("%s doesn't run on Win32s", APPLICATION);
	else if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
		s_win95 = true;

	win_consolelogging = Cvar_Get("win_consolelogging", "0", 0);
/*	if (dedicated->integer)
	{
		if (!AllocConsole ())
			Sys_Error ("Couldn't create dedicated server console");
		hinput = GetStdHandle (STD_INPUT_HANDLE);
		houtput = GetStdHandle (STD_OUTPUT_HANDLE);
	
		// let QHOST hook in
		InitConProc (argc, argv);
	}*/
}





/*
================
Sys_ConsoleOutput

Print text to the dedicated console
================
*/
void Sys_ConsoleOutput (const char *string)
{
	if (qDedConsole)
		Conbuf_AppendText( string );
}


/*
================
Sys_SendKeyEvents

Send Key_Event calls
================
*/
void Sys_SendKeyEvents (void)
{
    MSG        msg;

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
	{
		//if (!GetMessage (&msg, NULL, 0, 0))
		//	Sys_Quit ();
		sys_msg_time = msg.time;
      	TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}

	// grab frame time 
	sys_frame_time = timeGetTime();	// FIXME: should this be at start?
}



/*
================
Sys_GetClipboardData

================
*/
char *Sys_GetClipboardData( void )
{
	char *data = NULL;
	char *cliptext;

	if ( OpenClipboard( NULL ) != FALSE )
	{
		HANDLE hClipboardData;

		if ( ( hClipboardData = GetClipboardData( CF_TEXT ) ) != FALSE )
		{
			if ( ( cliptext = GlobalLock( hClipboardData ) ) != FALSE ) {
				data = CopyString(cliptext, TAG_CLIPBOARD);
				GlobalUnlock( hClipboardData );
			}
		}
		CloseClipboard();
	}
	return data;
}

/*
================
Sys_SetClipboardData

================
*/
void Sys_SetClipboardData( const char *data )
{
	char *cliptext;
	int	length;

	if( OpenClipboard( NULL ) != FALSE ) {
		HANDLE hClipboardData;

		length = strlen( data );
		hClipboardData = GlobalAlloc( GMEM_MOVEABLE | GMEM_DDESHARE, length + 1 );

		if( SetClipboardData( CF_TEXT, hClipboardData ) != NULL ) {
			if( ( cliptext = GlobalLock( hClipboardData ) ) != NULL ) {
				strcpy( cliptext, data );
				cliptext[length] = 0;
				GlobalUnlock( hClipboardData );
			}
		}
		CloseClipboard();
	}
}

/*
==============================================================================

 WINDOWS CRAP

==============================================================================
*/

/*
=================
Sys_AppActivate
=================
*/
void Sys_AppActivate (void)
{
	ShowWindow ( cl_hwnd, SW_RESTORE);
	SetForegroundWindow ( cl_hwnd );
}

/*
========================================================================

GAME DLL

========================================================================
*/

static HINSTANCE	game_library;

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame (void)
{
	if (!game_library)
		return;

	if (!FreeLibrary(game_library))
		Com_Error (ERR_FATAL, "FreeLibrary failed for game library with error %lu", GetLastError());

	game_library = NULL;
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void *Sys_GetGameAPI (void *parms)
{
	void	*(*GetGameAPI) (void *);
	char	name[MAX_OSPATH];
	char	*path;
	char	cwd[MAX_OSPATH];
#if defined _M_IX86
	const char *gamename = "gamex86.dll";

#ifdef NDEBUG
	const char *debugdir = "release";
#else
	const char *debugdir = "debug";
#endif

#elif defined _M_ALPHA
	const char *gamename = "gameaxp.dll";

#ifdef NDEBUG
	const char *debugdir = "releaseaxp";
#else
	const char *debugdir = "debugaxp";
#endif

#elif defined _WIN64

	const char *gamename = "gamex86_64.dll";

#ifdef NDEBUG
	const char *debugdir = "release";
#else
	const char *debugdir = "debug";
#endif

#else
#error Don't know what kind of dynamic objects to use for this architecture.
#endif

	if (game_library) {
		Sys_UnloadGame();
		Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");
	}

	// check the current debug directory first for development purposes
	_getcwd (cwd, sizeof(cwd));
	Com_sprintf (name, sizeof(name), "%s/%s/%s", cwd, debugdir, gamename);
	game_library = LoadLibrary ( name );
	if (!game_library)
	{
#ifdef DEBUG
		// check the current directory for other development purposes
		Com_sprintf (name, sizeof(name), "%s/%s", cwd, gamename);
		game_library = LoadLibrary ( name );
		if (!game_library)
#endif
		{
			// now run through the search paths
			path = NULL;
			while (1)
			{
				path = FS_NextPath (path);
				if (!path)
					return NULL;		// couldn't find one anywhere
				Com_sprintf (name, sizeof(name), "%s/%s", path, gamename);
				game_library = LoadLibrary (name);
				if (game_library) {
					break;
				}
			}
		}
	}

	if( !game_library ) {
		return NULL;
	}

	Com_DPrintf ("LoadLibrary (%s)\n", name);

	GetGameAPI = (void *)GetProcAddress (game_library, "GetGameAPI");
	if (!GetGameAPI) {
		Com_DPrintf("GetProcAddress from %s failed with error %lu\n", name, GetLastError());
		Sys_UnloadGame ();		
		return NULL;
	}

	return GetGameAPI(parms);
}

//=======================================================================


/*
==================
ParseCommandLine

==================
*/
void ParseCommandLine (LPSTR lpCmdLine)
{
	argc = 1;
	argv[0] = "exe";

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			argv[argc] = lpCmdLine;
			argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
			
		}
	}

}

void FixWorkingDirectory (void)
{
	char *p, curDir[MAX_PATH];

	GetModuleFileName (NULL, curDir, sizeof(curDir)-1);
	curDir[sizeof(curDir)-1] = 0;

	p = strrchr(curDir, '\\');
	if(!p)
		return;

	p[0] = 0;

	SetCurrentDirectory(curDir);
}
void VID_Shutdown (void);
#ifdef USE_DBGHELP

typedef DWORD (WINAPI *SETSYMOPTIONS)( DWORD );
typedef BOOL (WINAPI *SYMGETMODULEINFO)( HANDLE, DWORD, PIMAGEHLP_MODULE );
typedef BOOL (WINAPI *SYMINITIALIZE)( HANDLE, PSTR, BOOL );
typedef BOOL (WINAPI *SYMCLEANUP)( HANDLE );
typedef BOOL (WINAPI *ENUMERATELOADEDMODULES)( HANDLE, PENUMLOADED_MODULES_CALLBACK, PVOID );
typedef BOOL (WINAPI *STACKWALK)( DWORD, HANDLE, HANDLE, LPSTACKFRAME, PVOID,
	PREAD_PROCESS_MEMORY_ROUTINE, PFUNCTION_TABLE_ACCESS_ROUTINE, PGET_MODULE_BASE_ROUTINE,
	PTRANSLATE_ADDRESS_ROUTINE );
typedef BOOL (WINAPI *SYMFROMADDR)( HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO );
typedef PVOID (WINAPI *SYMFUNCTIONTABLEACCESS)( HANDLE, DWORD );
typedef DWORD (WINAPI *SYMGETMODULEBASE)( HANDLE, DWORD );

typedef HINSTANCE (WINAPI *SHELLEXECUTE)( HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT );

static SETSYMOPTIONS pSymSetOptions;
static SYMGETMODULEINFO pSymGetModuleInfo;
static SYMINITIALIZE pSymInitialize;
static SYMCLEANUP pSymCleanup;
static ENUMERATELOADEDMODULES pEnumerateLoadedModules;
static STACKWALK pStackWalk;
static SYMFROMADDR pSymFromAddr;
static SYMFUNCTIONTABLEACCESS pSymFunctionTableAccess;
static SYMGETMODULEBASE pSymGetModuleBase;
static SHELLEXECUTE pShellExecute;

static HANDLE processHandle, threadHandle;

static FILE *crashReport;

static CHAR moduleName[MAX_PATH];

static BOOL CALLBACK EnumModulesCallback(
	PSTR  ModuleName,
	ULONG ModuleBase,
	ULONG ModuleSize,
	PVOID UserContext )
{
	IMAGEHLP_MODULE moduleInfo;
	DWORD pc = ( DWORD )UserContext;
	BYTE buffer[4096];
	PBYTE data;
	UINT numBytes;
	VS_FIXEDFILEINFO *info;
	char version[64];
	char *symbols;

	strcpy( version, "unknown" );
	if( GetFileVersionInfo( ModuleName, 0, sizeof( buffer ), buffer ) ) {
		if( VerQueryValue( buffer, "\\", &data, &numBytes ) ) {
			info = ( VS_FIXEDFILEINFO * )data;
			Com_sprintf( version, sizeof( version ), "%u.%u.%u.%u",
				HIWORD( info->dwFileVersionMS ),
				LOWORD( info->dwFileVersionMS ),
				HIWORD( info->dwFileVersionLS ),
				LOWORD( info->dwFileVersionLS ) );
		}
	}
	
	symbols = "failed";
	moduleInfo.SizeOfStruct = sizeof( moduleInfo );
	if( pSymGetModuleInfo( processHandle, ModuleBase, &moduleInfo ) ) {
		ModuleName = moduleInfo.ModuleName;
		switch( moduleInfo.SymType ) {
			case SymCoff: symbols = "COFF"; break;
			case SymExport: symbols = "export"; break;
			case SymNone: symbols = "none"; break;
			case SymPdb: symbols = "PDB"; break;
			default: symbols = "unknown"; break;
		}
	}
	
	fprintf( crashReport, "%08x %08x %s (version %s, symbols %s) ",
		ModuleBase, ModuleBase + ModuleSize, ModuleName, version, symbols );
	if( pc >= ModuleBase && pc < ModuleBase + ModuleSize ) {
		Q_strncpyz( moduleName, ModuleName, sizeof( moduleName ) );
		fprintf( crashReport, "*\n" );
	} else {
		fprintf( crashReport, "\n" );
	}

	return TRUE;
}

static DWORD Sys_ExceptionHandler( DWORD exceptionCode, LPEXCEPTION_POINTERS exceptionInfo ) {
	STACKFRAME stackFrame;
	PCONTEXT context;
	SYMBOL_INFO *symbol;
	int count, ret;
	DWORD64 offset;
	BYTE buffer[sizeof( SYMBOL_INFO ) + 256 - 1];
	IMAGEHLP_MODULE moduleInfo;
	char path[MAX_PATH];
	char execdir[MAX_PATH];
	char *p;
	HMODULE helpModule, shellModule;
	SYSTEMTIME systemTime;
	static char *monthNames[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	OSVERSIONINFO	vinfo;

#ifndef DEDICATED_ONLY
	VID_Shutdown();
#endif

	ret = MessageBox( NULL, APPLICATION " has encountered an unhandled exception and needs to be terminated.\n"
		"Would you like to generate a crash report?",
		"Unhandled Exception",
		MB_ICONERROR|MB_YESNO );
	if( ret == IDNO ) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

	helpModule = LoadLibrary( "dbghelp.dll" );
	if( !helpModule ) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

#define GPA( x, y )															\
	do {																	\
		p ## y = ( x )GetProcAddress( helpModule, #y );						\
		if( !p ## y ) { \
			FreeLibrary(helpModule); \
			return EXCEPTION_CONTINUE_SEARCH;								\
		}																	\
	} while( 0 )

	GPA( SETSYMOPTIONS, SymSetOptions );
	GPA( SYMGETMODULEINFO, SymGetModuleInfo );
	GPA( SYMCLEANUP, SymCleanup );
	GPA( SYMINITIALIZE, SymInitialize );
	GPA( ENUMERATELOADEDMODULES, EnumerateLoadedModules );
	GPA( STACKWALK, StackWalk );
	GPA( SYMFROMADDR, SymFromAddr );
	GPA( SYMFUNCTIONTABLEACCESS, SymFunctionTableAccess );
	GPA( SYMGETMODULEBASE, SymGetModuleBase );

	pSymSetOptions( SYMOPT_LOAD_ANYTHING|SYMOPT_DEBUG|SYMOPT_FAIL_CRITICAL_ERRORS );
	processHandle = GetCurrentProcess();
	threadHandle = GetCurrentThread();

	GetModuleFileName( NULL, execdir, sizeof( execdir ) - 1 );
	execdir[sizeof( execdir ) - 1] = 0;
	p = strrchr( execdir, '\\' );
	if( p ) {
		*p = 0;
	}
	
	GetSystemTime( &systemTime );

	Com_sprintf( path, sizeof( path ), "%s\\" APPLICATION "_CrashReport.txt", execdir );
	crashReport = fopen( path, "a" );
	if( !crashReport ) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

	pSymInitialize( processHandle, execdir, TRUE );

	fprintf( crashReport, "Crash report generated %s %u %u, %02u:%02u:%02u UTC\n",
		monthNames[systemTime.wMonth % 12], systemTime.wDay, systemTime.wYear,
		systemTime.wHour, systemTime.wMinute, systemTime.wSecond );
	fprintf( crashReport, "by " APPLICATION " v" VERSION ", built " __DATE__", " __TIME__ "\n" );

	vinfo.dwOSVersionInfoSize = sizeof( vinfo );
	if( GetVersionEx( &vinfo ) ) {
		fprintf( crashReport, "\nWindows version: %u.%u (build %u) %s\n",
			vinfo.dwMajorVersion, vinfo.dwMinorVersion, vinfo.dwBuildNumber, vinfo.szCSDVersion );
	}

	strcpy( moduleName, "unknown" );

	context = exceptionInfo->ContextRecord;

	fprintf( crashReport, "\nLoaded modules:\n" );
	pEnumerateLoadedModules( processHandle, EnumModulesCallback, ( PVOID )context->Eip );

	fprintf( crashReport, "\nException information:\n" );
	fprintf( crashReport, "Code: %08x\n", exceptionCode );
	fprintf( crashReport, "Address: %08x (%s)\n",
		context->Eip, moduleName );

	fprintf( crashReport, "\nThread context:\n" );
	fprintf( crashReport, "EIP: %08x EBP: %08x ESP: %08x\n",
		context->Eip, context->Ebp, context->Esp );
	fprintf( crashReport, "EAX: %08x EBX: %08x ECX: %08x\n",
		context->Eax, context->Ebx, context->Ecx );
	fprintf( crashReport, "EDX: %08x ESI: %08x EDI: %08x\n",
		context->Edx, context->Esi, context->Edi );

	memset( &stackFrame, 0, sizeof( stackFrame ) );
	stackFrame.AddrPC.Offset = context->Eip;
	stackFrame.AddrPC.Mode = AddrModeFlat;
	stackFrame.AddrFrame.Offset = context->Ebp;
	stackFrame.AddrFrame.Mode = AddrModeFlat;
	stackFrame.AddrStack.Offset = context->Esp;
	stackFrame.AddrStack.Mode = AddrModeFlat;	

	fprintf( crashReport, "\nStack trace:\n" );
	count = 0;
	symbol = ( SYMBOL_INFO * )buffer;
	symbol->SizeOfStruct = sizeof( *symbol );
	symbol->MaxNameLen = 256;
	while( pStackWalk( IMAGE_FILE_MACHINE_I386,
		processHandle,
		threadHandle,
		&stackFrame,
		context,
		NULL,
		pSymFunctionTableAccess,
		pSymGetModuleBase,
		NULL ) )
	{
		fprintf( crashReport, "%d: %08x %08x %08x %08x ",
			count,
			stackFrame.Params[0],
			stackFrame.Params[1],
			stackFrame.Params[2],
			stackFrame.Params[3] );

		moduleInfo.SizeOfStruct = sizeof( moduleInfo );
		if( pSymGetModuleInfo( processHandle, stackFrame.AddrPC.Offset, &moduleInfo ) ) {
			if( moduleInfo.SymType != SymNone && moduleInfo.SymType != SymExport &&
				pSymFromAddr( processHandle, stackFrame.AddrPC.Offset, &offset, symbol ) )
			{
				fprintf( crashReport, "%s!%s+%#x\n", 
					moduleInfo.ModuleName,
					symbol->Name, offset );
			} else {
				fprintf( crashReport, "%s!%#x\n",
					moduleInfo.ModuleName,
					stackFrame.AddrPC.Offset );
			}
		} else {
			fprintf( crashReport, "%#x\n",
				stackFrame.AddrPC.Offset );
		}
		count++;
	}

	fclose( crashReport );

	shellModule = LoadLibrary( "shell32.dll" );
	if( shellModule ) {
		pShellExecute = ( SHELLEXECUTE )GetProcAddress( shellModule, "ShellExecuteA" );
		if( pShellExecute ) {
			pShellExecute( NULL, "open", path, NULL, execdir, SW_SHOW );
		}
	}

	pSymCleanup( processHandle );

	ExitProcess( 1 );
	return EXCEPTION_CONTINUE_SEARCH;
}

#endif /* USE_DBGHELP */

/*
==================
WinMain

==================
*/
HINSTANCE	global_hInstance;

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG				msg;
	unsigned int	time, oldtime, newtime;
//	char			*cddir;

    /* previous instances do not exist in Win32 */
    if (hPrevInstance)
        return 0;

	global_hInstance = hInstance;

	// done before Com/Sys_Init since we need this for error output
	Sys_CreateConsole();

	// no abort/retry/fail errors
	SetErrorMode (SEM_FAILCRITICALERRORS);

	ParseCommandLine (lpCmdLine);

	FixWorkingDirectory ();

	// if we find the CD, add a +set cddir xxx command line
#if 0
	cddir = Sys_ScanForCD ();
	if (cddir && argc < MAX_NUM_ARGVS - 3)
	{
		int		i;

		// don't override a cddir on the command line
		for (i=0 ; i<argc ; i++)
			if (!strcmp(argv[i], "cddir"))
				break;
		if (i == argc)
		{
			argv[argc++] = "+set";
			argv[argc++] = "cddir";
			argv[argc++] = cddir;
		}
	}
#endif
	#ifdef USE_DBGHELP
#ifdef _MSC_VER
	__try {
#else
	__try1( Sys_ExceptionHandler );
#endif
#endif /* USE_DBGHELP */
	Qcommon_Init (argc, argv);
	oldtime = Sys_Milliseconds ();

	//Com_Error (ERR_FATAL, "Testing");

	if (dedicated->integer) {
		Sys_ShowConsole(1, false);
	}
	else if(!win_consolelogging->integer) {
		qDedConsole = false;
		Sys_DestroyConsole();
	}


	//_controlfp( _PC_24, _MCW_PC );

    /* main window message loop */
	while (1)
	{
		// if at a full screen console, don't update unless needed
		if (!ActiveApp || dedicated->integer)
			Sleep (3);

		while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
			sys_msg_time = msg.time;
			TranslateMessage(&msg);
   			DispatchMessage(&msg);
		}

		do
		{
			newtime = Sys_Milliseconds ();
			time = newtime - oldtime;
		} while (time < 1);

		_controlfp( _PC_24, _MCW_PC );
		Qcommon_Frame( time );

		oldtime = newtime;
	}
#ifdef USE_DBGHELP
#ifdef _MSC_VER
	} __except( Sys_ExceptionHandler( GetExceptionCode(), GetExceptionInformation() ) ) {
		return 1;
	}
#else
	__except1;
#endif
#endif /* USE_DBGHELP */
	// never gets here
    return 0;
}



////ANTICHEAT

#ifdef ANTICHEAT
#ifndef DEDICATED_ONLY

typedef struct
{
	void (*Check) (void);
} anticheat_export_t;

static anticheat_export_t *anticheat;

typedef VOID * (*FNINIT) (VOID);

int Sys_GetAntiCheatAPI (void)
{
	qboolean	updated = false;
	HMODULE		hAC;
	static FNINIT	init;

	//already loaded, just reinit
	if (anticheat) {
		anticheat = (anticheat_export_t *)init ();
		return (anticheat) ? 1 : 0;
	}

reInit:

	hAC = LoadLibrary ("anticheat");
	if (!hAC)
		return 0;

	init = (FNINIT)GetProcAddress (hAC, "Initialize");
	if (!init)
		Sys_Error("Couldn't GetProcAddress Initialize on anticheat.dll!\r\n\r\nPlease check you are using a valid anticheat.dll from http://antiche.at/");

	anticheat = (anticheat_export_t *)init ();

	if (!updated && !anticheat) {
		updated = true;
		FreeLibrary (hAC);
		hAC = NULL;
		init = NULL;
		goto reInit;
	}

	return (anticheat) ? 1 : 0;
}
#endif
#endif
