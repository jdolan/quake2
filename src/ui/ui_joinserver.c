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
=============================================================================

JOIN SERVER MENU

=============================================================================
*/
static menuframework_s	s_joinserver_menu;
static menuaction_s		s_joinserver_search_action;
static menuaction_s		s_joinserver_address_book_action;
static menulist_s		s_joinserver_server_list;
static menulist_s		s_joinserver_sort_box;

#define JOIN_M_TITLE		"-[ Join Server ]-"
#define ADDRESS_FILE		"addresses.txt"
#define MAX_MENU_SERVERS	32

// user readable information
static serverStatus_t localServers[MAX_MENU_SERVERS];
static char local_server_names[MAX_MENU_SERVERS][64];
static char local_server_addresses[MAX_MENU_SERVERS][32];
static char *server_shit[MAX_MENU_SERVERS];
static int	sortedSList[MAX_MENU_SERVERS];

static int		m_num_servers = 0;
static int		m_num_adr_cvar = 0, m_num_adr_file = 0, m_num_addresses = 0;
static qboolean addressesLoaded = false;
static qboolean joinMenuInitialized = false;

cvar_t	*menu_serversort;

static void M_LoadServers(void)
{
	char name[32], *adrstring;
	int  i;

	if(m_num_adr_file == MAX_MENU_SERVERS)
		return;

	m_num_addresses -= m_num_adr_cvar;
	m_num_adr_cvar = 0;

	for (i=0 ; i<9 ; i++)
	{
		Com_sprintf (name, sizeof(name), "adr%i", i);
		adrstring = Cvar_VariableString (name);
		if (!adrstring || !adrstring[0])
			continue;

		Q_strncpyz(local_server_addresses[m_num_addresses++], adrstring, sizeof(local_server_addresses[0]));
		m_num_adr_cvar++;

		if(m_num_addresses == MAX_MENU_SERVERS)
			break;
	}
}

static void M_LoadServersFromFile(void)
{
	char name[32];
	char *buffer = NULL;
	int line = 0;
	char *s, *p;

	if(addressesLoaded)
		return;

	FS_LoadFile( ADDRESS_FILE, (void **)&buffer );
	if (!buffer) {
		Com_DPrintf ("M_LoadServers: " ADDRESS_FILE  " not found\n");
		return;
	}

	s = buffer;

	m_num_adr_file = 0;
	m_num_addresses = 0;

	while( *s ) {
		p = s;
		line++;

		Q_strncpyz (name, COM_Parse( &p ), sizeof(name));
		if( strlen(name) > 2 )
			strcpy(local_server_addresses[m_num_adr_file++], name);

		if(m_num_adr_file == MAX_MENU_SERVERS)
			break;

		s = strchr( s, '\n' );
		if( !s )
			break;

		*s = '\0';
		s++;
	}

	m_num_addresses = m_num_adr_file;

	addressesLoaded = true;

	FS_FreeFile( buffer );
}

void CL_SendUIStatusRequests( netadr_t *adr );

static void M_PingServers (void)
{
	int			i;
	netadr_t	adr = {0};
	char		*adrstring;

	if(!m_num_addresses)
		return;

	NET_Config (NET_CLIENT);		// allow remote

	adr.type = NA_BROADCAST;
	adr.port = BigShort(PORT_SERVER);
	CL_SendUIStatusRequests(&adr);

	// send a packet to each address book entry
	for (i=0; i<m_num_addresses; i++)
	{
		adrstring = local_server_addresses[i];
		Com_Printf ("pinging %s...\n", adrstring);
		if (!NET_StringToAdr (adrstring, &adr))
		{
			Com_Printf ("Bad address: %s\n", adrstring);
			continue;
		}
		if (!adr.port)
			adr.port = BigShort(PORT_SERVER);

		CL_SendUIStatusRequests(&adr);
	}
}

static int SortServers( void const *a, void const *b )
{
	int anum, bnum;

	anum = *(int *) a;
	bnum = *(int *) b;

	if(localServers[anum].numPlayers > localServers[bnum].numPlayers)
		return -1;

	if(localServers[anum].numPlayers < localServers[bnum].numPlayers)
		return 1;

	return 0;
}

static int titleMax = 0;

void CopyServerTitle(char *buf, int size, const serverStatus_t *status)
{
	char map[64], *s;
	int clients, len, mlen, i, maxLen;

	s = Info_ValueForKey(status->infostring, "hostname");
	Q_strncpyz(buf, s, size);

	s = Info_ValueForKey(status->infostring, "mapname");
	clients = atoi(Info_ValueForKey(status->infostring, "maxclients"));
	Com_sprintf(map, sizeof(map), " %-10s %2i/%2i", s, status->numPlayers, clients );

	len = strlen(buf);
	mlen = strlen(map);
	maxLen = min(titleMax, size);

	if(len + mlen > maxLen) {
		if(maxLen < mlen-3)
			buf[0] = 0;
		else
			buf[maxLen-mlen-3] = 0;
		Q_strncatz(buf, "...", size);
	}
	else
	{
		for(i = len; i < maxLen-mlen; i++)
			buf[i] = ' ';

		buf[i] = 0;
	}

	Q_strncatz(buf, map, size);
}

void M_AddToServerList (const serverStatus_t *status)
{
	serverStatus_t *s;
	int		i;

	if(!joinMenuInitialized)
		return;

	if (m_num_servers >= MAX_MENU_SERVERS)
		return;

	// ignore if duplicated
	for( i=0, s=localServers ; i<m_num_servers ; i++, s++ )
		if( !strcmp( status->address, s->address ) )
			return;

	localServers[m_num_servers++] = *status;

	titleMax = s_joinserver_server_list.width - (MLIST_BSIZE*2);
	if(m_num_servers > s_joinserver_server_list.maxItems)
		titleMax -= MLIST_SSIZE;

	titleMax /= 8;
		

	s = &localServers[m_num_servers-1];
	CopyServerTitle(local_server_names[i], sizeof(local_server_names[i]), s);
	server_shit[i] = local_server_names[i];
	sortedSList[i] = i;

	if(menu_serversort->integer) {
		qsort(sortedSList, m_num_servers, sizeof(sortedSList[0]), SortServers);

		for(i = 0; i < m_num_servers; i++)
		{
			server_shit[i] = local_server_names[sortedSList[i]];
		}
	}

	s_joinserver_server_list.count = m_num_servers;
}


static void JoinServerFunc( void *self )
{
	char	buffer[128];
	int		index;

	index = s_joinserver_server_list.curvalue;

	if (index >= m_num_servers)
		return;

	Com_sprintf (buffer, sizeof(buffer), "connect %s\n", localServers[sortedSList[index]].address);
	Cbuf_AddText (buffer);
	M_ForceMenuOff ();
}

static void AddressBookFunc( void *self )
{
	M_Menu_AddressBook_f();
}

static void SearchLocalGames( void )
{
	M_DrawTextBox( 8, 120 - 48, 36, 3 );
	M_Print( 16 + 16, 120 - 48 + 8,  "Searching for local servers, this" );
	M_Print( 16 + 16, 120 - 48 + 16, "could take up to a minute, so" );
	M_Print( 16 + 16, 120 - 48 + 24, "please be patient." );

	// the text box won't show up unless we do a buffer swap
	R_EndFrame();

	m_num_servers = 0;
	s_joinserver_server_list.count = 0;

	M_LoadServersFromFile();
	M_LoadServers();
	// send out info packets
	M_PingServers();
}

static void SearchLocalGamesFunc( void *self )
{
	SearchLocalGames();
}


static void JoinServer_InfoDraw( void )
{
	char key[MAX_INFO_KEY];
	char value[MAX_INFO_VALUE];
	const char *info;
	int x = s_joinserver_server_list.width + 40;
	int y = 80;
	int index;
	serverStatus_t *server;
	playerStatus_t *player;
	int i;

	// Never draw on low resolutions
	if( viddef.width < 512 )
		return;

	index = s_joinserver_server_list.curvalue;
	if( index < 0 || index >= MAX_MENU_SERVERS )
		return;

	server = &localServers[sortedSList[index]];

	DrawAltString( x, y, "Name            Score Ping");
	y += 8;
	DrawAltString( x, y, "--------------- ----- ----");
	y += 10;

	if( !server->numPlayers ) {
		DrawAltString( x, y, "No players");
		y += 8;
	}
	else
	{
		for( i=0, player=server->players ; i<server->numPlayers ; i++, player++ ) {
			DrawString( x, y, va( "%-15s %5i %4i\n", player->name, player->score, player->ping ));
			y += 8;
		}
	}

	y+=8;
	DrawAltString( x, y, "Server info");
	y += 8;
	DrawAltString( x, y, "--------------------------");
	y += 10;

	info = (const char *)server->infostring;
	while( *info ) {
		Info_NextPair( &info, key, value );

		if(!key[0] || !value[0])
			break;

		if( strlen( key ) > 15 ) {
			strcpy( key + 12, "..." );
		}

		DrawString( x, y, va( "%-8s", key ));
		DrawString( x + 16 * 8, y, va( "%-16s", value ));
		y += 8;
	}

}

static void JoinServer_MenuDraw( menuframework_s *self )
{
	//M_Banner( "m_banner_join_server" );
	DrawString((viddef.width - (strlen(JOIN_M_TITLE)*8))>>1, 10, JOIN_M_TITLE);
	JoinServer_InfoDraw();
	Menu_Draw( self );
}

static void SortFunc( void *unused )
{
	Cvar_SetValue( "menu_serversort", s_joinserver_sort_box.curvalue );
}

static void JoinServer_MenuInit( void )
{
	int y, x;

	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	menu_serversort = Cvar_Get("menu_serversort", "1", CVAR_ARCHIVE);

	memset(&s_joinserver_menu, 0, sizeof(s_joinserver_menu));
	s_joinserver_menu.x = 0;
	s_joinserver_menu.nitems = 0;

	s_joinserver_server_list.generic.type		= MTYPE_LIST;
	s_joinserver_server_list.generic.flags		= QMF_LEFT_JUSTIFY;
	s_joinserver_server_list.generic.name		= NULL;
	s_joinserver_server_list.generic.callback	= JoinServerFunc;
	s_joinserver_server_list.generic.x			= 20;
	s_joinserver_server_list.generic.y			= 40;
	s_joinserver_server_list.width				= (viddef.width - 40) / 2;
	s_joinserver_server_list.height				= viddef.height - 110;
	s_joinserver_server_list.generic.statusbar = "press ENTER to connect";
	s_joinserver_server_list.itemnames			= (const char **)server_shit;
	s_joinserver_server_list.count				= 0;
	MenuList_Init(&s_joinserver_server_list);

	x = s_joinserver_server_list.width + 60;
	y = 40;

	s_joinserver_address_book_action.generic.type	= MTYPE_ACTION;
	s_joinserver_address_book_action.generic.name	= "address book";
	s_joinserver_address_book_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_joinserver_address_book_action.generic.x		= x;
	s_joinserver_address_book_action.generic.y		= y;
	s_joinserver_address_book_action.generic.callback = AddressBookFunc;

	s_joinserver_search_action.generic.type		= MTYPE_ACTION;
	s_joinserver_search_action.generic.name		= "refresh server list";
	s_joinserver_search_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_joinserver_search_action.generic.x		= x;
	s_joinserver_search_action.generic.y		= y+10;
	s_joinserver_search_action.generic.callback = SearchLocalGamesFunc;
	s_joinserver_search_action.generic.statusbar = "search for servers";

	s_joinserver_sort_box.generic.type		= MTYPE_SPINCONTROL;
	s_joinserver_sort_box.generic.flags		= QMF_LEFT_JUSTIFY;
	s_joinserver_sort_box.generic.cursor_offset = 24;
	s_joinserver_sort_box.generic.name		= "list sorting";
	s_joinserver_sort_box.generic.x			= x-10+strlen(s_joinserver_sort_box.generic.name)*8;
	s_joinserver_sort_box.generic.y			= y+20;
	s_joinserver_sort_box.generic.callback	= SortFunc;
	s_joinserver_sort_box.itemnames			= yesno_names;
	s_joinserver_sort_box.curvalue			= menu_serversort->integer;

	s_joinserver_menu.draw = JoinServer_MenuDraw;
	s_joinserver_menu.key = NULL;

	Menu_AddItem( &s_joinserver_menu, &s_joinserver_address_book_action );
	Menu_AddItem( &s_joinserver_menu, &s_joinserver_search_action );
	Menu_AddItem( &s_joinserver_menu, &s_joinserver_sort_box.generic );

	Menu_AddItem( &s_joinserver_menu, &s_joinserver_server_list );

	//Menu_Center( &s_joinserver_menu );

	joinMenuInitialized = true;

	SearchLocalGames();
}

const char *JoinServer_MenuKey( int key )
{
	return Default_MenuKey( &s_joinserver_menu, key );
}

void M_Menu_JoinServer_f (void)
{
	JoinServer_MenuInit();
	M_PushMenu( &s_joinserver_menu );
}

