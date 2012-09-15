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

END GAME MENU

=============================================================================
*/
static int credits_start_time;
static const char **credits;
static char *creditsIndex[256];
static char *creditsBuffer;
static const char *idcredits[] =
{
	"+QUAKE II BY ID SOFTWARE",
	"",
	"+PROGRAMMING",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"",
	"+ART",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"",
	"+LEVEL DESIGN",
	"Tim Willits",
	"American McGee",
	"Christian Antkow",
	"Paul Jaquays",
	"Brandon James",
	"",
	"+BIZ",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Donna Jackson",
	"",
	"",
	"+SPECIAL THANKS",
	"Ben Donges for beta testing",
	"",
	"",
	"",
	"",
	"",
	"",
	"+ADDITIONAL SUPPORT",
	"",
	"+LINUX PORT AND CTF",
	"Dave \"Zoid\" Kirsch",
	"",
	"+CINEMATIC SEQUENCES",
	"Ending Cinematic by Blur Studio - ",
	"Venice, CA",
	"",
	"Environment models for Introduction",
	"Cinematic by Karl Dolgener",
	"",
	"Assistance with environment design",
	"by Cliff Iwai",
	"",
	"+SOUND EFFECTS AND MUSIC",
	"Sound Design by Soundelux Media Labs.",
	"Music Composed and Produced by",
	"Soundelux Media Labs.  Special thanks",
	"to Bill Brown, Tom Ozanich, Brian",
	"Celano, Jeff Eisner, and The Soundelux",
	"Players.",
	"",
	"\"Level Music\" by Sonic Mayhem",
	"www.sonicmayhem.com",
	"",
	"\"Quake II Theme Song\"",
	"(C) 1997 Rob Zombie. All Rights",
	"Reserved.",
	"",
	"Track 10 (\"Climb\") by Jer Sypult",
	"",
	"Voice of computers by",
	"Carly Staehlin-Taylor",
	"",
	"+THANKS TO ACTIVISION",
	"+IN PARTICULAR:",
	"",
	"John Tam",
	"Steve Rosenthal",
	"Marty Stratton",
	"Henk Hartong",
	"",
	"Quake II(tm) (C)1997 Id Software, Inc.",
	"All Rights Reserved.  Distributed by",
	"Activision, Inc. under license.",
	"Quake II(tm), the Id Software name,",
	"the \"Q II\"(tm) logo and id(tm)",
	"logo are trademarks of Id Software,",
	"Inc. Activision(R) is a registered",
	"trademark of Activision, Inc. All",
	"other trademarks and trade names are",
	"properties of their respective owners.",
	0
};

static const char *xatcredits[] =
{
	"+QUAKE II MISSION PACK: THE RECKONING",
	"+BY",
	"+XATRIX ENTERTAINMENT, INC.",
	"",
	"+DESIGN AND DIRECTION",
	"Drew Markham",
	"",
	"+PRODUCED BY",
	"Greg Goodrich",
	"",
	"+PROGRAMMING",
	"Rafael Paiz",
	"",
	"+LEVEL DESIGN / ADDITIONAL GAME DESIGN",
	"Alex Mayberry",
	"",
	"+LEVEL DESIGN",
	"Mal Blackwell",
	"Dan Koppel",
	"",
	"+ART DIRECTION",
	"Michael \"Maxx\" Kaufman",
	"",
	"+COMPUTER GRAPHICS SUPERVISOR AND",
	"+CHARACTER ANIMATION DIRECTION",
	"Barry Dempsey",
	"",
	"+SENIOR ANIMATOR AND MODELER",
	"Jason Hoover",
	"",
	"+CHARACTER ANIMATION AND",
	"+MOTION CAPTURE SPECIALIST",
	"Amit Doron",
	"",
	"+ART",
	"Claire Praderie-Markham",
	"Viktor Antonov",
	"Corky Lehmkuhl",
	"",
	"+INTRODUCTION ANIMATION",
	"Dominique Drozdz",
	"",
	"+ADDITIONAL LEVEL DESIGN",
	"Aaron Barber",
	"Rhett Baldwin",
	"",
	"+3D CHARACTER ANIMATION TOOLS",
	"Gerry Tyra, SA Technology",
	"",
	"+ADDITIONAL EDITOR TOOL PROGRAMMING",
	"Robert Duffy",
	"",
	"+ADDITIONAL PROGRAMMING",
	"Ryan Feltrin",
	"",
	"+PRODUCTION COORDINATOR",
	"Victoria Sylvester",
	"",
	"+SOUND DESIGN",
	"Gary Bradfield",
	"",
	"+MUSIC BY",
	"Sonic Mayhem",
	"",
	"",
	"",
	"+SPECIAL THANKS",
	"+TO",
	"+OUR FRIENDS AT ID SOFTWARE",
	"",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"Tim Willits",
	"Christian Antkow",
	"Paul Jaquays",
	"Brandon James",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Dave \"Zoid\" Kirsch",
	"Donna Jackson",
	"",
	"",
	"",
	"+THANKS TO ACTIVISION",
	"+IN PARTICULAR:",
	"",
	"Marty Stratton",
	"Henk \"The Original Ripper\" Hartong",
	"Kevin Kraff",
	"Jamey Gottlieb",
	"Chris Hepburn",
	"",
	"+AND THE GAME TESTERS",
	"",
	"Tim Vanlaw",
	"Doug Jacobs",
	"Steven Rosenthal",
	"David Baker",
	"Chris Campbell",
	"Aaron Casillas",
	"Steve Elwell",
	"Derek Johnstone",
	"Igor Krinitskiy",
	"Samantha Lee",
	"Michael Spann",
	"Chris Toft",
	"Juan Valdes",
	"",
	"+THANKS TO INTERGRAPH COMPUTER SYTEMS",
	"+IN PARTICULAR:",
	"",
	"Michael T. Nicolaou",
	"",
	"",
	"Quake II Mission Pack: The Reckoning",
	"(tm) (C)1998 Id Software, Inc. All",
	"Rights Reserved. Developed by Xatrix",
	"Entertainment, Inc. for Id Software,",
	"Inc. Distributed by Activision Inc.",
	"under license. Quake(R) is a",
	"registered trademark of Id Software,",
	"Inc. Quake II Mission Pack: The",
	"Reckoning(tm), Quake II(tm), the Id",
	"Software name, the \"Q II\"(tm) logo",
	"and id(tm) logo are trademarks of Id",
	"Software, Inc. Activision(R) is a",
	"registered trademark of Activision,",
	"Inc. Xatrix(R) is a registered",
	"trademark of Xatrix Entertainment,",
	"Inc. All other trademarks and trade",
	"names are properties of their",
	"respective owners.",
	0
};

static const char *roguecredits[] =
{
	"+QUAKE II MISSION PACK 2: GROUND ZERO",
	"+BY",
	"+ROGUE ENTERTAINMENT, INC.",
	"",
	"+PRODUCED BY",
	"Jim Molinets",
	"",
	"+PROGRAMMING",
	"Peter Mack",
	"Patrick Magruder",
	"",
	"+LEVEL DESIGN",
	"Jim Molinets",
	"Cameron Lamprecht",
	"Berenger Fish",
	"Robert Selitto",
	"Steve Tietze",
	"Steve Thoms",
	"",
	"+ART DIRECTION",
	"Rich Fleider",
	"",
	"+ART",
	"Rich Fleider",
	"Steve Maines",
	"Won Choi",
	"",
	"+ANIMATION SEQUENCES",
	"Creat Studios",
	"Steve Maines",
	"",
	"+ADDITIONAL LEVEL DESIGN",
	"Rich Fleider",
	"Steve Maines",
	"Peter Mack",
	"",
	"+SOUND",
	"James Grunke",
	"",
	"+GROUND ZERO THEME",
	"+AND",
	"+MUSIC BY",
	"Sonic Mayhem",
	"",
	"+VWEP MODELS",
	"Brent \"Hentai\" Dill",
	"",
	"",
	"",
	"+SPECIAL THANKS",
	"+TO",
	"+OUR FRIENDS AT ID SOFTWARE",
	"",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"Tim Willits",
	"Christian Antkow",
	"Paul Jaquays",
	"Brandon James",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Katherine Anna Kang",
	"Donna Jackson",
	"Dave \"Zoid\" Kirsch",
	"",
	"",
	"",
	"+THANKS TO ACTIVISION",
	"+IN PARTICULAR:",
	"",
	"Marty Stratton",
	"Henk Hartong",
	"Mitch Lasky",
	"Steve Rosenthal",
	"Steve Elwell",
	"",
	"+AND THE GAME TESTERS",
	"",
	"The Ranger Clan",
	"Dave \"Zoid\" Kirsch",
	"Nihilistic Software",
	"Robert Duffy",
	"",
	"And Countless Others",
	"",
	"",
	"",
	"Quake II Mission Pack 2: Ground Zero",
	"(tm) (C)1998 Id Software, Inc. All",
	"Rights Reserved. Developed by Rogue",
	"Entertainment, Inc. for Id Software,",
	"Inc. Distributed by Activision Inc.",
	"under license. Quake(R) is a",
	"registered trademark of Id Software,",
	"Inc. Quake II Mission Pack 2: Ground",
	"Zero(tm), Quake II(tm), the Id",
	"Software name, the \"Q II\"(tm) logo",
	"and id(tm) logo are trademarks of Id",
	"Software, Inc. Activision(R) is a",
	"registered trademark of Activision,",
	"Inc. Rogue(R) is a registered",
	"trademark of Rogue Entertainment,",
	"Inc. All other trademarks and trade",
	"names are properties of their",
	"respective owners.",
	0
};


void M_Credits_MenuDraw( menuframework_s *self )
{
	unsigned int i, y;

	/*
	** draw the credits
	*/
	for ( i = 0, y = viddef.height - ( ( cls.realtime - credits_start_time ) / 40 ); credits[i] && y < viddef.height; y += 10, i++ )
	{
		int j, stringoffset = 0;
		int bold = false;

		if ( y <= -8 )
			continue;

		if ( credits[i][0] == '+' )
		{
			bold = true;
			stringoffset = 1;
		}
		else
		{
			bold = false;
			stringoffset = 0;
		}

		for ( j = 0; credits[i][j+stringoffset]; j++ )
		{
			int x;

			x = ( viddef.width - strlen( credits[i] ) * 8 - stringoffset * 8 ) / 2 + ( j + stringoffset ) * 8;

			if ( bold )
				Draw_Char( x, y, credits[i][j+stringoffset] + 128, COLOR_WHITE, 1 );
			else
				Draw_Char( x, y, credits[i][j+stringoffset], COLOR_WHITE, 1 );
		}
	}

	if ( y < 0 )
		credits_start_time = cls.realtime;
}

const char *M_Credits_Key( menuframework_s *self, int key )
{
	switch (key)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		if (creditsBuffer)
			FS_FreeFile (creditsBuffer);
		M_PopMenu ();
		break;
	}

	return menu_out_sound;

}

static menuframework_s	m_creditsMenu;

static void Credits_MenuInit( void ) {
	memset( &m_creditsMenu, 0, sizeof( m_creditsMenu ) );
	m_creditsMenu.draw = M_Credits_MenuDraw;
	m_creditsMenu.key = M_Credits_Key;
}


void M_Menu_Credits_f( void )
{
	int		n;
	int		count;
	char	*p;
	int		isdeveloper = 0;

	creditsBuffer = NULL;
	count = FS_LoadFile ("credits", (void **)&creditsBuffer);
	if (count != -1)
	{
		p = creditsBuffer;
		for (n = 0; n < 255; n++)
		{
			creditsIndex[n] = p;
			while (*p != '\r' && *p != '\n')
			{
				p++;
				if (--count == 0)
					break;
			}
			if (*p == '\r')
			{
				*p++ = 0;
				if (--count == 0)
					break;
			}
			*p++ = 0;
			if (--count == 0)
				break;
		}
		creditsIndex[++n] = 0;
		credits = (const char **)creditsIndex;
	}
	else
	{
		isdeveloper = Developer_searchpath ();
		
		if (isdeveloper == 1)			// xatrix
			credits = xatcredits;
		else if (isdeveloper == 2)		// ROGUE
			credits = roguecredits;
		else
		{
			credits = idcredits;	
		}

	}

	credits_start_time = cls.realtime;

	Credits_MenuInit();
	M_PushMenu( &m_creditsMenu );
}

