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

#include "ui_local.h"

/*
=======================================================================

DEMOS MENU

=======================================================================
*/
#define DM_TITLE "-[ Demos Menu ]-"

#define FFILE_UP		1
#define FFILE_FOLDER	2
#define FFILE_DEMO		3
#define DEF_FOLDER		"demos"

typedef struct m_demos_s {
	menuframework_s		menu;
	menulist_s		list;

	char			**names;
	int				*types;
} m_demos_t;

char	gameFolder[128] = "\0";
char	demoFolder[1024] = "\0";

static unsigned int	demo_count = 0;
static m_demos_t	m_demos;

static void Demos_MenuDraw( menuframework_s *self )
{

	DrawString((viddef.width - (strlen(DM_TITLE)*8))>>1, 10, DM_TITLE);
	DrawAltString(20, 20, va("Directory: demos%s/", demoFolder));

	Menu_Draw( self );

	if(!m_demos.list.count)
		return;

	switch(m_demos.types[m_demos.list.curvalue]) {
	case FFILE_UP:
		DrawAltString(20, 30+m_demos.list.height, "Go one directory up");
		break;
	case FFILE_FOLDER:
		DrawAltString(20, 30+m_demos.list.height, va("Go to directory demos%s/%s", demoFolder, m_demos.names[m_demos.list.curvalue]));
		break;
	case FFILE_DEMO:
		DrawAltString(20, 30+m_demos.list.height, va("Selected demo: %s", m_demos.names[m_demos.list.curvalue]));
		break;
	}
}

static void Demos_Free( void ) {
	unsigned int i;

	if(!demo_count)
		return;

	for( i=0 ; i<demo_count ; i++ ) {
		Z_Free( m_demos.names[i] );
	}
	Z_Free (m_demos.names);
	Z_Free (m_demos.types);
	m_demos.names = NULL;
	m_demos.types = NULL;
	demo_count = 0;
	m_demos.list.itemnames = NULL;
}

static void Demos_Scan( void) {
	char	findname[1024];
	int		numFiles = 0, numDirs = 0, numTotal = 0;
	char	**fileList = NULL, **dirList = NULL;
	int		i, skip = 0;

	Demos_Free();
	m_demos.names = NULL;

	if(demoFolder[0])
		numTotal++;

	Com_sprintf(findname, sizeof(findname), "%s/demos%s/*", gameFolder, demoFolder);
	dirList = FS_ListFiles( findname, &numDirs, SFF_SUBDIR, SFF_HIDDEN | SFF_SYSTEM );

	if(dirList)
		numTotal += numDirs - 1;

	Com_sprintf(findname, sizeof(findname), "%s/demos%s/*.dm2", gameFolder, demoFolder);
	fileList = FS_ListFiles( findname, &numFiles, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM );

	if(fileList)
		numTotal += numFiles - 1;

	if(!numTotal)
		return;

	m_demos.names = Z_TagMalloc ( sizeof(char *) * numTotal, TAG_MENU );
	m_demos.types = Z_TagMalloc ( sizeof(int) * numTotal, TAG_MENU );

	if(demoFolder[0])
	{
		m_demos.names[demo_count] = CopyString("..", TAG_MENU);
		m_demos.types[demo_count++] = FFILE_UP;
		skip = 1;
	}

	if( dirList )
	{
		for( i = 0; i < numDirs - 1; i++ )
		{
			if (strrchr( dirList[i], '/' ))
				m_demos.names[demo_count] = CopyString( strrchr( dirList[i], '/' ) + 1, TAG_MENU );
			else
				m_demos.names[demo_count] = CopyString( dirList[i], TAG_MENU );

			m_demos.types[demo_count++] = FFILE_FOLDER;
			Z_Free( dirList[i] );
		}
		Z_Free( dirList );

		if(demo_count - skip > 1) {
			qsort( m_demos.names + skip, demo_count - skip, sizeof( m_demos.names[0] ), SortStrcmp );
		}
		skip += demo_count;
	}

	if( fileList )
	{
		for( i = 0; i < numFiles - 1; i++ )
		{
			if (strrchr( fileList[i], '/' ))
				m_demos.names[demo_count] = CopyString( strrchr( fileList[i], '/' ) + 1, TAG_MENU);
			else
				m_demos.names[demo_count] = CopyString( fileList[i], TAG_MENU);

			m_demos.types[demo_count++] = FFILE_DEMO;
			Z_Free( fileList[i] );
		}
		Z_Free( fileList );

		if(demo_count - skip > 1) {
			qsort( m_demos.names + skip, demo_count - skip, sizeof( m_demos.names[0] ), SortStrcmp );
		}
	}
}

static void Build_List(void)
{
	Demos_Scan();
	m_demos.list.curvalue = 0;
	m_demos.list.prestep = 0;
	m_demos.list.itemnames = (const char **)m_demos.names;
	m_demos.list.count = demo_count;
}

static void Load_Demo (void *s)
{
	char *p;

	if(!m_demos.list.count)
		return;

	switch( m_demos.types[m_demos.list.curvalue] ) {
	case FFILE_UP:
		if ((p = strrchr(demoFolder, '/')) != NULL)
			*p = 0;
		Build_List();
		break;
	case FFILE_FOLDER:
		Q_strncatz (demoFolder, "/", sizeof(demoFolder));
		Q_strncatz (demoFolder, m_demos.names[m_demos.list.curvalue], sizeof(demoFolder));
		Build_List();
		break;
	case FFILE_DEMO:
		if(demoFolder[0])
			Cbuf_AddText( va( "demo \"%s/%s\"\n", demoFolder + 1, m_demos.names[m_demos.list.curvalue] ) );
		else
			Cbuf_AddText( va( "demo \"%s\"\n", m_demos.names[m_demos.list.curvalue] ) );
		Demos_Free();
		M_ForceMenuOff();
		break;
	}

	return;
}

const char *Demos_MenuKey( menuframework_s *self, int key ) {
	switch( key ) {
	case K_ESCAPE:
		Demos_Free();
		M_PopMenu();
		return NULL;
	}
		
	return Default_MenuKey( self, key );
}

void Demos_MenuInit( void ) {
	memset( &m_demos.menu, 0, sizeof( m_demos.menu ) );

	if(!gameFolder[0] || strcmp(FS_Gamedir(), gameFolder)) {
		strcpy(gameFolder, FS_Gamedir());
		demoFolder[0] = 0;
	}

	m_demos.menu.x = 0;
	m_demos.menu.y = 0;

	m_demos.list.generic.type		= MTYPE_LIST;
	m_demos.list.generic.flags		= QMF_LEFT_JUSTIFY;
	m_demos.list.generic.name		= NULL;
	m_demos.list.generic.callback	= Load_Demo;
	m_demos.list.generic.x			= 20;
	m_demos.list.generic.y			= 30;
	m_demos.list.width				= viddef.width - 40;
	m_demos.list.height				= viddef.height - 60;
	MenuList_Init(&m_demos.list);

	Build_List();

	m_demos.menu.draw = Demos_MenuDraw;
	m_demos.menu.key = Demos_MenuKey;
	Menu_AddItem( &m_demos.menu, (void *)&m_demos.list );

	Menu_SetStatusBar( &m_demos.menu, NULL );

}



void M_Menu_Demos_f( void ) {
	Demos_MenuInit();
	M_PushMenu( &m_demos.menu );
}

