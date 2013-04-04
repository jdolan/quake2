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
// net.c

#ifdef _WIN32
#ifndef _INC_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#if !(defined(_WINSOCKAPI_) || defined(_WINSOCK_H))
#include <winsock2.h>
#endif
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <errno.h>
#include <arpa/inet.h>
#define SOCKET int
#define INVALID_SOCKET 0
#define SOCKET_ERROR -1
#define closesocket close
#define ioctlsocket ioctl
#else
#error Unknown target OS
#endif
#include "../qcommon/qcommon.h"


#define	MAX_LOOPBACK	4

typedef struct
{
	byte	data[MAX_MSGLEN];
	int		datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t	msgs[MAX_LOOPBACK];
	int			get, send;
} loopback_t;


static cvar_t	*client_port;
static cvar_t	*net_ignore_icmp;

static loopback_t	loopbacks[NS_COUNT];
static SOCKET		ip_sockets[NS_COUNT] = { INVALID_SOCKET, INVALID_SOCKET };

static int			net_active = NET_NONE;

char *NET_ErrorString (void)
{
	int		code;
#ifdef _WIN32
	code = WSAGetLastError ();
	switch (code) {
	case WSAEINTR: return "WSAEINTR: Interrupted function call (your TCP stack is likely broken/corrupt)";
	case WSAEBADF: return "WSAEBADF";
	case WSAEACCES: return "WSAEACCES: Permission denied";
	case WSAEDISCON: return "WSAEDISCON";
	case WSAEFAULT: return "WSAEFAULT: Network failure";
	case WSAEINVAL: return "WSAEINVAL";
	case WSAEMFILE: return "WSAEMFILE";
	case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK: Resource temporarily unavailable";
	case WSAEINPROGRESS: return "WSAEINPROGRESS";
	case WSAEALREADY: return "WSAEALREADY";
	case WSAENOTSOCK: return "WSAENOTSOCK";
	case WSAEDESTADDRREQ: return "WSAEDESTADDRREQ";
	case WSAEMSGSIZE: return "WSAEMSGSIZE: Message too long";
	case WSAEPROTOTYPE: return "WSAEPROTOTYPE";
	case WSAENOPROTOOPT: return "WSAENOPROTOOPT";
	case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
	case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
	case WSAEOPNOTSUPP: return "WSAEOPNOTSUPP";
	case WSAEPFNOSUPPORT: return "WSAEPFNOSUPPORT";
	case WSAEAFNOSUPPORT: return "WSAEAFNOSUPPORT";
	case WSAEADDRINUSE: return "WSAEADDRINUSE: Address already in use";
	case WSAEADDRNOTAVAIL: return "WSAEADDRNOTAVAIL: Cannot assign requested address";
	case WSAENETDOWN: return "WSAENETDOWN: Network is down";
	case WSAEHOSTUNREACH: return "WSAEHOSTUNREACH: Host is unreachable";
	case WSAENETUNREACH: return "WSAENETUNREACH: No route to host";
	case WSAENETRESET: return "WSAENETRESET";
	case WSAECONNABORTED: return "WSWSAECONNABORTEDAEINTR";
	case WSAECONNRESET: return "WSAECONNRESET: Connection reset by peer";
	case WSAENOBUFS: return "WSAENOBUFS";
	case WSAEISCONN: return "WSAEISCONN";
	case WSAENOTCONN: return "WSAENOTCONN";
	case WSAESHUTDOWN: return "WSAESHUTDOWN";
	case WSAETOOMANYREFS: return "WSAETOOMANYREFS";
	case WSAETIMEDOUT: return "WSAETIMEDOUT";
	case WSAECONNREFUSED: return "WSAECONNREFUSED: Connection refused";
	case WSAELOOP: return "WSAELOOP";
	case WSAENAMETOOLONG: return "WSAENAMETOOLONG";
	case WSAEHOSTDOWN: return "WSAEHOSTDOWN";
	case WSASYSNOTREADY: return "WSASYSNOTREADY: Network subsystem is unavailable";
	case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
	case WSANOTINITIALISED: return "WSANOTINITIALISED";
	case WSAHOST_NOT_FOUND: return "WSAHOST_NOT_FOUND";
	case WSATRY_AGAIN: return "WSATRY_AGAIN";
	case WSANO_RECOVERY: return "WSANO_RECOVERY";
	case WSANO_DATA: return "WSANO_DATA";
	default: return "NO ERROR";
	}
#else
	code = errno;
	return strerror( code );
#endif
}

//=============================================================================

static void NetadrToSockadr ( const netadr_t *a, struct sockaddr_in *s )
{
	memset (s, 0, sizeof(*s));

	switch( a->type ) {
	case NA_BROADCAST:
		s->sin_family = AF_INET;
		s->sin_port = a->port;
		s->sin_addr.s_addr = INADDR_BROADCAST;
		break;
	case NA_IP:
		s->sin_family = AF_INET;
		s->sin_addr.s_addr = *(uint32 *)&a->ip;
		s->sin_port = a->port;
		break;
	default:
		Com_Error( ERR_FATAL, "NetadrToSockadr: bad address type" );
		break;
	}
}

/*
===================
SockadrToNetadr
===================
*/
static void SockadrToNetadr (struct sockaddr *s, netadr_t *a)
{
	a->type = NA_IP;
	*(uint32 *)&a->ip = ((struct sockaddr_in *)s)->sin_addr.s_addr;
	a->port = ((struct sockaddr_in *)s)->sin_port;
}

/*
===================
NET_CompareAdr
===================
*/
qboolean	NET_CompareAdr (const netadr_t *a, const netadr_t *b)
{
	if (a->type != b->type)
		return false;

	switch( a->type ) {
	case NA_LOOPBACK:
		return true;
	case NA_IP:
	case NA_BROADCAST:
		if( *( uint32 * )a->ip == *( uint32 * )b->ip && a->port == b->port) {
			return true;
		}
		return false;
	default:
		Com_Error( ERR_FATAL, "NET_CompareAdr: bad address type: %i", a->type );
		break;
	}

	return false;
}

/*
===================
NET_CompareBaseAdr

Compares without the port
===================
*/
qboolean NET_CompareBaseAdr (const netadr_t *a, const netadr_t *b)
{
	if (a->type != b->type)
		return false;

	switch( a->type ) {
	case NA_LOOPBACK:
		return true;
	case NA_IP:
	case NA_BROADCAST:
		if( *( uint32 * )a->ip == *( uint32 * )b->ip ) {
			return true;
		}
		return false;
	default:
		break;
	}

	return false;
}

/*
===================
NET_AdrToString
===================
*/
char	*NET_AdrToString (const netadr_t *a)
{
	static	char	s[32];

	switch( a->type ) {
	case NA_LOOPBACK:
		strcpy( s, "loopback" );
		return s;
	case NA_IP:
	case NA_BROADCAST:
		Com_sprintf(s, sizeof( s ), "%i.%i.%i.%i:%i", a->ip[0], a->ip[1], a->ip[2], a->ip[3], ntohs(a->port));
		return s;
	default:
		Com_Error( ERR_FATAL, "NET_AdrToString: bad address type: %i", a->type );
		break;
	}
	s[0] = 0;

	return s;
}


/*
=============
NET_StringToAdr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/

qboolean	NET_StringToSockaddr (const char *s, struct sockaddr *sadr)
{
	struct hostent	*h;
	char	copy[128], *colon;
	
	memset (sadr, 0, sizeof(*sadr));

	if ((strlen(s) >= 23) && (s[8] == ':') && (s[21] == ':'))	// check for an IPX address
		return false;

	((struct sockaddr_in *)sadr)->sin_family = AF_INET;
	((struct sockaddr_in *)sadr)->sin_port = 0;

	Q_strncpyz (copy, s, sizeof(copy));
	// strip off a trailing :port if present
	for (colon = copy ; *colon ; colon++) {
		if (*colon == ':') {
			*colon = 0;
			((struct sockaddr_in *)sadr)->sin_port = htons((u_short)atoi(colon+1));	
			break;
		}
	}
	
	if (copy[0] >= '0' && copy[0] <= '9')
	{
		*(uint32 *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	}
	else
	{
		if ( !(h = gethostbyname(copy)) )
			return false;
		*(uint32 *)&((struct sockaddr_in *)sadr)->sin_addr = *(uint32 *)h->h_addr_list[0];
	}
	
	return true;
}


/*
=============
NET_StringToAdr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
qboolean	NET_StringToAdr (const char *s, netadr_t *a)
{
	struct sockaddr sadr;
	
	if (!Q_stricmp(s, "localhost")) {
		memset (a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return true;
	}

	if (!NET_StringToSockaddr (s, &sadr))
		return false;
	
	SockadrToNetadr (&sadr, a);

	return true;
}


/*
=============
NET_IsLANAddress
=============
*/
qboolean NET_IsLANAddress (const netadr_t *adr)
{
	switch( adr->type ) {
	case NA_LOOPBACK:
		return true;
	case NA_IP:
	case NA_BROADCAST:
		if( adr->ip[0] == 127 || adr->ip[0] == 10 ) {
			return true;
		}
		if( ( adr->ip[0] == 192 && adr->ip[1] == 168 ) ||
			( adr->ip[0] == 172 && adr->ip[1] == 16 ) ) {
			return true;
		}
		return false;
	default:
		Com_Error( ERR_FATAL, "NET_IsLANAddress: bad address type: %i", adr->type );
		break;
	}

	return false;
}
/*
=============================================================================

LOOPBACK BUFFERS FOR LOCAL PLAYER

=============================================================================
*/
/*
=============
NET_GetLoopPacket
=============
*/
qboolean	NET_GetLoopPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if (loop->get >= loop->send)
		return false;

	i = loop->get & (MAX_LOOPBACK-1);
	loop->get++;

	memcpy (net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	net_message->cursize = loop->msgs[i].datalen;
	memset (net_from, 0, sizeof(*net_from));
	net_from->type = NA_LOOPBACK;
	return true;

}

/*
=============
NET_SendLoopPacket
=============
*/
static void NET_SendLoopPacket (netsrc_t sock, int length, const void *data)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock^1];

	i = loop->send & (MAX_LOOPBACK-1);
	loop->send++;

	memcpy (loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}

//=============================================================================
/*
=============
NET_GetPacket
=============
*/
int NET_GetPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int 	ret, err;
	socklen_t fromlen;
	struct sockaddr from;

	if (NET_GetLoopPacket (sock, net_from, net_message))
		return 1;

	if (ip_sockets[sock] == INVALID_SOCKET)
		return 0;

	fromlen = sizeof(from);
	ret = recvfrom (ip_sockets[sock], (char *)net_message->data, net_message->maxsize
		, 0, (struct sockaddr *)&from, &fromlen);

	SockadrToNetadr (&from, net_from);

	if (ret == SOCKET_ERROR)
	{
#ifdef _WIN32
		err = WSAGetLastError();

		switch( err ) {
		case WSAEWOULDBLOCK:
			// wouldblock is silent
			break;
		case WSAECONNRESET:
			if( !net_ignore_icmp->integer ) {
				return -1;
			}
			break;
		case WSAEMSGSIZE:
			Com_Printf ("NET_GetPacket: Oversize packet from %s\n", NET_AdrToString(net_from));
			break;
		default:
			Com_Printf ("NET_GetPacket: %s from %s\n", NET_ErrorString(), NET_AdrToString(net_from));
			break;
		}
#else
		err = errno;

		switch( err ) {
		case EWOULDBLOCK:
			// wouldblock is silent
			break;
		case ECONNREFUSED:
			if( !net_ignore_icmp->integer ) {
				return -1;
			}
			break;
		default:
			Com_Printf ("NET_GetPacket: %s from %s\n", NET_ErrorString(), NET_AdrToString(net_from));
			break;
		}
#endif
		return 0;
	}

	if (ret == net_message->maxsize) {
		Com_Printf ("Oversize packet from %s\n", NET_AdrToString (net_from));
		return 0;
	}

	net_message->cursize = ret;
	return 1;
}

//=============================================================================
/*
=============
NET_SendPacket
=============
*/
int NET_SendPacket (netsrc_t sock, int length, const void *data, const netadr_t *to)
{
	int		ret, err;
	struct sockaddr_in	addr;

	switch( to->type ) {
	case NA_LOOPBACK:
		NET_SendLoopPacket (sock, length, data);
		return 1;
	case NA_IP:
	case NA_BROADCAST:
		break;
	default:
		Com_Error (ERR_FATAL, "NET_SendPacket: bad address type");
		return 0;
	}

	if (ip_sockets[sock] == INVALID_SOCKET)
		return 0;

	NetadrToSockadr (to, &addr);

	ret = sendto (ip_sockets[sock], data, length, 0, (struct sockaddr *) &addr, sizeof(addr) );
	if (ret == SOCKET_ERROR)
	{
#ifdef _WIN32
		err = WSAGetLastError();

		switch( err ) {
		case WSAEWOULDBLOCK:
		case WSAEINTR:
			// wouldblock is silent
			break;
		case WSAECONNRESET:
		case WSAEHOSTUNREACH:
			if( !net_ignore_icmp->integer ) {
				return -1;
			}
			break;
		case WSAEADDRNOTAVAIL:
			// some PPP links dont allow broadcasts
			if (to->type == NA_BROADCAST)
				break;
			// intentional fallthrough
		default:
			Com_Printf ("NET_SendPacket ERROR: %s (%d) to %s\n",
				NET_ErrorString(), err, NET_AdrToString(to));
			break;
		}
#else
		err = errno;

		switch( err ) {
		case EWOULDBLOCK:
			// wouldblock is silent
			break;
		case ECONNRESET:
		case EHOSTUNREACH:
		case ENETUNREACH:
		case ENETDOWN:
			if( !net_ignore_icmp->integer ) {
				return -1;
			}
			break;
		default:
			Com_Printf ("NET_SendPacket ERROR: %s (%d) to %s\n",
				NET_ErrorString(), err, NET_AdrToString( to ) );
			break;
		}
#endif
		return 0;
	}
	return 1;
}


//=============================================================================

SOCKET NET_IPSocket( const char *net_interface, int port ) {
	SOCKET				newsocket;
	struct sockaddr_in	address;
	u_long			    _true = 1;
	int					i = 1;

	if( !net_interface || !net_interface[0] ) {
		net_interface = "localhost";
	}

	Com_Printf( "Opening IP socket: %s:%i\n", net_interface, port );

	if( ( newsocket = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP ) ) == INVALID_SOCKET ) {
#ifdef _WIN32
		if( WSAGetLastError() != WSAEAFNOSUPPORT )
#endif
			Com_Printf ("WARNING: UDP_OpenSocket: socket: %s", NET_ErrorString() );
		return INVALID_SOCKET;
	}

	// make it non-blocking
	if( ioctlsocket( newsocket, FIONBIO, &_true ) == SOCKET_ERROR ) {
		Com_Printf ("WARNING: UDP_OpenSocket: ioctl FIONBIO: %s\n", NET_ErrorString() );
		closesocket( newsocket );
		return INVALID_SOCKET;
	}

	// make it broadcast capable
	if( setsockopt( newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof( i ) ) == SOCKET_ERROR ) {
		Com_Printf ("WARNING: UDP_OpenSocket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString() );
		closesocket( newsocket );
		return INVALID_SOCKET;
	}

	if( !Q_stricmp( net_interface, "localhost" ) ) {
		address.sin_addr.s_addr = INADDR_ANY;
	} else {
		NET_StringToSockaddr( net_interface, (struct sockaddr *)&address );
	}

	if( port == PORT_ANY ) {
		address.sin_port = 0;
	} else {
		address.sin_port = htons( (u_short)port );
	}

	address.sin_family = AF_INET;

	if( bind( newsocket, (void *)&address, sizeof( address ) ) == SOCKET_ERROR ) {
		Com_Printf ("WARNING: UDP_OpenSocket: bind: %s\n", NET_ErrorString() );
		closesocket( newsocket );
		return INVALID_SOCKET;
	}

	return newsocket;
}


/*
====================
NET_OpenIP
====================
*/
static void NET_OpenIP (int flag)
{
	cvar_t	*ip;
	int		port;
	int		dedicated;

	ip = Cvar_Get ("ip", "localhost", CVAR_NOSET);

	dedicated = Cvar_VariableIntValue ("dedicated");

	if (flag & NET_SERVER && ip_sockets[NS_SERVER] == INVALID_SOCKET)
	{
		port = Cvar_Get("ip_hostport", "0", CVAR_NOSET)->integer;
		if (!port)
		{
			port = Cvar_Get("hostport", "0", CVAR_NOSET)->integer;
			if (!port)
			{
				port = Cvar_Get("port", va("%i", PORT_SERVER), CVAR_NOSET)->integer;
			}
		}
		ip_sockets[NS_SERVER] = NET_IPSocket(ip->string, port);
		if (ip_sockets[NS_SERVER] == INVALID_SOCKET && dedicated)
			Com_Error (ERR_FATAL, "Couldn't allocate dedicated server IP port on %s:%d.", ip->string, port);
	}

	// dedicated servers don't need client ports
	if (dedicated)
		return;

	if (flag & NET_CLIENT && ip_sockets[NS_CLIENT] == INVALID_SOCKET)
	{
		ip_sockets[NS_CLIENT] = NET_IPSocket(ip->string, client_port->integer);

		if (ip_sockets[NS_CLIENT] == INVALID_SOCKET)
			ip_sockets[NS_CLIENT] = NET_IPSocket(ip->string, PORT_ANY);

		if (ip_sockets[NS_CLIENT] == INVALID_SOCKET)
			Com_Error (ERR_DROP, "Couldn't allocate client IP port.");
	}
}

/*
====================
NET_Config

A single player game will only use the loopback code
====================
*/
void NET_Config (int flag)
{
	netsrc_t	sock;

	if (flag == net_active)
		return;

	if (flag == NET_NONE)
	{	// shut down any existing sockets
		for( sock = 0; sock < NS_COUNT; sock++ ) {
			if( ip_sockets[sock] != INVALID_SOCKET ) {
				closesocket( ip_sockets[sock] );
				ip_sockets[sock] = INVALID_SOCKET;
			}
		}
		net_active = NET_NONE;
		return;
	}

	NET_OpenIP(flag);

	net_active |= flag;
}

// sleeps msec or until net socket is ready
void NET_Sleep(int msec)
{
    struct timeval timeout;
	fd_set	fdset;
	SOCKET i;

	if (!dedicated->integer)
		return; // we're not a server, just run full speed

	FD_ZERO(&fdset);
	i = 0;
	if (ip_sockets[NS_SERVER] != INVALID_SOCKET) {
		FD_SET(ip_sockets[NS_SERVER], &fdset); // network socket
		i = ip_sockets[NS_SERVER];
	}

	timeout.tv_sec = msec/1000;
	timeout.tv_usec = (msec%1000)*1000;
	select(i+1, &fdset, NULL, NULL, &timeout);
}

//===================================================================

/*
====================
NET_Restart_f
====================
*/
static void NET_Restart_f( void ) {
	int flag = net_active;

	NET_Config( NET_NONE );
	NET_Config( flag );
}

/*
====================
NET_Init
====================
*/
void NET_Init (void)
{
#ifdef _WIN32
	WSADATA		winsockdata;
	int		r;
	char *errmsg;

	r = WSAStartup( MAKEWORD( 1, 1 ), &winsockdata );
	if( r ) {
		errmsg = va( "Winsock initialization failed, returned %i", r );
		if( dedicated->integer ) {
			Com_Error( ERR_FATAL, "%s", errmsg );
		}

		Com_Printf( "%s\n", errmsg );
		return;
	}
	Com_Printf( "Winsock Initialized\n" );
#endif

	client_port = Cvar_Get ("net_port", va("%i", PORT_CLIENT), 0);
	net_ignore_icmp = Cvar_Get("net_ignore_icmp", "0", 0);

	Cmd_AddCommand( "net_restart", NET_Restart_f );
}


/*
====================
NET_Shutdown
====================
*/
void NET_Shutdown (void)
{
	NET_Config(NET_NONE);	// close sockets

#ifdef _WIN32
	WSACleanup();
#endif

	Cmd_RemoveCommand( "net_restart" );
}

