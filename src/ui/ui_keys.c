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

KEYS MENU

=======================================================================
*/
char *bindnames[][2] =
{
{"+attack", 		"attack"},
{"weapnext", 		"next weapon"},
{"+forward", 		"walk forward"},
{"+back", 			"backpedal"},
{"+left", 			"turn left"},
{"+right", 			"turn right"},
{"+speed", 			"run"},
{"+moveleft", 		"step left"},
{"+moveright", 		"step right"},
{"+strafe", 		"sidestep"},
{"+lookup", 		"look up"},
{"+lookdown", 		"look down"},
{"centerview", 		"center view"},
{"+mlook", 			"mouse look"},
{"+klook", 			"keyboard look"},
{"+moveup",			"up / jump"},
{"+movedown",		"down / crouch"},

{"inven",			"inventory"},
{"invuse",			"use item"},
{"invdrop",			"drop item"},
{"invprev",			"prev item"},
{"invnext",			"next item"},

{"cmd help", 		"help computer" }, 
{ 0, 0 }
};

int				keys_cursor;
static int		bind_grab;

static menuframework_s	s_keys_menu;
static menuaction_s		s_keys_attack_action;
static menuaction_s		s_keys_change_weapon_action;
static menuaction_s		s_keys_walk_forward_action;
static menuaction_s		s_keys_backpedal_action;
static menuaction_s		s_keys_turn_left_action;
static menuaction_s		s_keys_turn_right_action;
static menuaction_s		s_keys_run_action;
static menuaction_s		s_keys_step_left_action;
static menuaction_s		s_keys_step_right_action;
static menuaction_s		s_keys_sidestep_action;
static menuaction_s		s_keys_look_up_action;
static menuaction_s		s_keys_look_down_action;
static menuaction_s		s_keys_center_view_action;
static menuaction_s		s_keys_mouse_look_action;
static menuaction_s		s_keys_keyboard_look_action;
static menuaction_s		s_keys_move_up_action;
static menuaction_s		s_keys_move_down_action;
static menuaction_s		s_keys_inventory_action;
static menuaction_s		s_keys_inv_use_action;
static menuaction_s		s_keys_inv_drop_action;
static menuaction_s		s_keys_inv_prev_action;
static menuaction_s		s_keys_inv_next_action;

static menuaction_s		s_keys_help_computer_action;

static void M_UnbindCommand (char *command)
{
	int		j;
	int		l;
	char	*b;

	l = strlen(command);

	for (j=0 ; j<MAX_KEYS ; j++)
	{
		b = keys[j].binding;
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_SetBinding (j, "");
	}
}

static void M_FindKeysForCommand (char *command, int *twokeys)
{
	int		count;
	int		j;
	int		l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j=0 ; j<MAX_KEYS ; j++)
	{
		b = keys[j].binding;
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

static void KeyCursorDrawFunc( menuframework_s *menu )
{
	if ( bind_grab )
		Draw_Char( menu->x, menu->y + menu->cursor * 9, '=', COLOR_WHITE, 1 );
	else
		Draw_Char( menu->x, menu->y + menu->cursor * 9, 12 + ( ( int ) ( Sys_Milliseconds() / 250 ) & 1 ), COLOR_WHITE, 1 );
}

static void DrawKeyBindingFunc( void *self )
{
	int mkeys[2];
	menuaction_s *a = ( menuaction_s * ) self;

	M_FindKeysForCommand( bindnames[a->generic.localdata[0]][0], mkeys);
		
	if (mkeys[0] == -1)
	{
		Menu_DrawString( a->generic.x + a->generic.parent->x + 16, a->generic.y + a->generic.parent->y, "???" );
	}
	else
	{
		int x;
		const char *name;

		name = Key_KeynumToString (mkeys[0]);

		Menu_DrawString( a->generic.x + a->generic.parent->x + 16, a->generic.y + a->generic.parent->y, name );

		x = strlen(name) * 8;

		if (mkeys[1] != -1)
		{
			Menu_DrawString( a->generic.x + a->generic.parent->x + 24 + x, a->generic.y + a->generic.parent->y, "or" );
			Menu_DrawString( a->generic.x + a->generic.parent->x + 48 + x, a->generic.y + a->generic.parent->y, Key_KeynumToString (mkeys[1]) );
		}
	}
}

static void KeyBindingFunc( void *self )
{
	menuaction_s *a = ( menuaction_s * ) self;
	int mkeys[2];

	M_FindKeysForCommand( bindnames[a->generic.localdata[0]][0], mkeys );

	if (mkeys[1] != -1)
		M_UnbindCommand( bindnames[a->generic.localdata[0]][0]);

	bind_grab = true;

	Menu_SetStatusBar( &s_keys_menu, "press a key or button for this action" );
}

static void Keys_MenuDraw ( menuframework_s *self )
{
	Menu_AdjustCursor( self, 1 );
	Menu_Draw( self );
}

static const char *Keys_MenuKey( menuframework_s *self, int key )
{
	menuaction_s *item = ( menuaction_s * ) Menu_ItemAtCursor( &s_keys_menu );

	if ( bind_grab )
	{	
		if ( key != K_ESCAPE && key != '`' )
		{
			char cmd[1024];

			Com_sprintf (cmd, sizeof(cmd), "bind \"%s\" \"%s\"", Key_KeynumToString(key), bindnames[item->generic.localdata[0]][0]);
			Cbuf_InsertText (cmd);
		}
		
		Menu_SetStatusBar( &s_keys_menu, "enter to change, backspace to clear" );
		bind_grab = false;
		return menu_out_sound;
	}

	switch ( key )
	{
	case K_KP_ENTER:
	case K_ENTER:
	case K_MOUSE1:
		KeyBindingFunc( item );
		return menu_in_sound;
	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
	case K_KP_DEL:
		M_UnbindCommand( bindnames[item->generic.localdata[0]][0] );
		return menu_out_sound;
	default:
		return Default_MenuKey( self, key );
	}
}

static void Keys_MenuInit( void )
{
	int y = 0;
	int i = 0;

	memset(&s_keys_menu, 0, sizeof(s_keys_menu));
	s_keys_menu.x = viddef.width * 0.50;
	s_keys_menu.nitems = 0;
	s_keys_menu.cursordraw = KeyCursorDrawFunc;

	s_keys_attack_action.generic.type	= MTYPE_ACTION;
	s_keys_attack_action.generic.flags  = QMF_GRAYED;
	s_keys_attack_action.generic.x		= 0;
	s_keys_attack_action.generic.y		= y;
	s_keys_attack_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_attack_action.generic.localdata[0] = i;
	s_keys_attack_action.generic.name	= bindnames[s_keys_attack_action.generic.localdata[0]][1];

	s_keys_change_weapon_action.generic.type	= MTYPE_ACTION;
	s_keys_change_weapon_action.generic.flags  = QMF_GRAYED;
	s_keys_change_weapon_action.generic.x		= 0;
	s_keys_change_weapon_action.generic.y		= y += 9;
	s_keys_change_weapon_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_change_weapon_action.generic.localdata[0] = ++i;
	s_keys_change_weapon_action.generic.name	= bindnames[s_keys_change_weapon_action.generic.localdata[0]][1];

	s_keys_walk_forward_action.generic.type	= MTYPE_ACTION;
	s_keys_walk_forward_action.generic.flags  = QMF_GRAYED;
	s_keys_walk_forward_action.generic.x		= 0;
	s_keys_walk_forward_action.generic.y		= y += 9;
	s_keys_walk_forward_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_walk_forward_action.generic.localdata[0] = ++i;
	s_keys_walk_forward_action.generic.name	= bindnames[s_keys_walk_forward_action.generic.localdata[0]][1];

	s_keys_backpedal_action.generic.type	= MTYPE_ACTION;
	s_keys_backpedal_action.generic.flags  = QMF_GRAYED;
	s_keys_backpedal_action.generic.x		= 0;
	s_keys_backpedal_action.generic.y		= y += 9;
	s_keys_backpedal_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_backpedal_action.generic.localdata[0] = ++i;
	s_keys_backpedal_action.generic.name	= bindnames[s_keys_backpedal_action.generic.localdata[0]][1];

	s_keys_turn_left_action.generic.type	= MTYPE_ACTION;
	s_keys_turn_left_action.generic.flags  = QMF_GRAYED;
	s_keys_turn_left_action.generic.x		= 0;
	s_keys_turn_left_action.generic.y		= y += 9;
	s_keys_turn_left_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_turn_left_action.generic.localdata[0] = ++i;
	s_keys_turn_left_action.generic.name	= bindnames[s_keys_turn_left_action.generic.localdata[0]][1];

	s_keys_turn_right_action.generic.type	= MTYPE_ACTION;
	s_keys_turn_right_action.generic.flags  = QMF_GRAYED;
	s_keys_turn_right_action.generic.x		= 0;
	s_keys_turn_right_action.generic.y		= y += 9;
	s_keys_turn_right_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_turn_right_action.generic.localdata[0] = ++i;
	s_keys_turn_right_action.generic.name	= bindnames[s_keys_turn_right_action.generic.localdata[0]][1];

	s_keys_run_action.generic.type	= MTYPE_ACTION;
	s_keys_run_action.generic.flags  = QMF_GRAYED;
	s_keys_run_action.generic.x		= 0;
	s_keys_run_action.generic.y		= y += 9;
	s_keys_run_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_run_action.generic.localdata[0] = ++i;
	s_keys_run_action.generic.name	= bindnames[s_keys_run_action.generic.localdata[0]][1];

	s_keys_step_left_action.generic.type	= MTYPE_ACTION;
	s_keys_step_left_action.generic.flags  = QMF_GRAYED;
	s_keys_step_left_action.generic.x		= 0;
	s_keys_step_left_action.generic.y		= y += 9;
	s_keys_step_left_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_step_left_action.generic.localdata[0] = ++i;
	s_keys_step_left_action.generic.name	= bindnames[s_keys_step_left_action.generic.localdata[0]][1];

	s_keys_step_right_action.generic.type	= MTYPE_ACTION;
	s_keys_step_right_action.generic.flags  = QMF_GRAYED;
	s_keys_step_right_action.generic.x		= 0;
	s_keys_step_right_action.generic.y		= y += 9;
	s_keys_step_right_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_step_right_action.generic.localdata[0] = ++i;
	s_keys_step_right_action.generic.name	= bindnames[s_keys_step_right_action.generic.localdata[0]][1];

	s_keys_sidestep_action.generic.type	= MTYPE_ACTION;
	s_keys_sidestep_action.generic.flags  = QMF_GRAYED;
	s_keys_sidestep_action.generic.x		= 0;
	s_keys_sidestep_action.generic.y		= y += 9;
	s_keys_sidestep_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_sidestep_action.generic.localdata[0] = ++i;
	s_keys_sidestep_action.generic.name	= bindnames[s_keys_sidestep_action.generic.localdata[0]][1];

	s_keys_look_up_action.generic.type	= MTYPE_ACTION;
	s_keys_look_up_action.generic.flags  = QMF_GRAYED;
	s_keys_look_up_action.generic.x		= 0;
	s_keys_look_up_action.generic.y		= y += 9;
	s_keys_look_up_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_look_up_action.generic.localdata[0] = ++i;
	s_keys_look_up_action.generic.name	= bindnames[s_keys_look_up_action.generic.localdata[0]][1];

	s_keys_look_down_action.generic.type	= MTYPE_ACTION;
	s_keys_look_down_action.generic.flags  = QMF_GRAYED;
	s_keys_look_down_action.generic.x		= 0;
	s_keys_look_down_action.generic.y		= y += 9;
	s_keys_look_down_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_look_down_action.generic.localdata[0] = ++i;
	s_keys_look_down_action.generic.name	= bindnames[s_keys_look_down_action.generic.localdata[0]][1];

	s_keys_center_view_action.generic.type	= MTYPE_ACTION;
	s_keys_center_view_action.generic.flags  = QMF_GRAYED;
	s_keys_center_view_action.generic.x		= 0;
	s_keys_center_view_action.generic.y		= y += 9;
	s_keys_center_view_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_center_view_action.generic.localdata[0] = ++i;
	s_keys_center_view_action.generic.name	= bindnames[s_keys_center_view_action.generic.localdata[0]][1];

	s_keys_mouse_look_action.generic.type	= MTYPE_ACTION;
	s_keys_mouse_look_action.generic.flags  = QMF_GRAYED;
	s_keys_mouse_look_action.generic.x		= 0;
	s_keys_mouse_look_action.generic.y		= y += 9;
	s_keys_mouse_look_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_mouse_look_action.generic.localdata[0] = ++i;
	s_keys_mouse_look_action.generic.name	= bindnames[s_keys_mouse_look_action.generic.localdata[0]][1];

	s_keys_keyboard_look_action.generic.type	= MTYPE_ACTION;
	s_keys_keyboard_look_action.generic.flags  = QMF_GRAYED;
	s_keys_keyboard_look_action.generic.x		= 0;
	s_keys_keyboard_look_action.generic.y		= y += 9;
	s_keys_keyboard_look_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_keyboard_look_action.generic.localdata[0] = ++i;
	s_keys_keyboard_look_action.generic.name	= bindnames[s_keys_keyboard_look_action.generic.localdata[0]][1];

	s_keys_move_up_action.generic.type	= MTYPE_ACTION;
	s_keys_move_up_action.generic.flags  = QMF_GRAYED;
	s_keys_move_up_action.generic.x		= 0;
	s_keys_move_up_action.generic.y		= y += 9;
	s_keys_move_up_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_move_up_action.generic.localdata[0] = ++i;
	s_keys_move_up_action.generic.name	= bindnames[s_keys_move_up_action.generic.localdata[0]][1];

	s_keys_move_down_action.generic.type	= MTYPE_ACTION;
	s_keys_move_down_action.generic.flags  = QMF_GRAYED;
	s_keys_move_down_action.generic.x		= 0;
	s_keys_move_down_action.generic.y		= y += 9;
	s_keys_move_down_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_move_down_action.generic.localdata[0] = ++i;
	s_keys_move_down_action.generic.name	= bindnames[s_keys_move_down_action.generic.localdata[0]][1];

	s_keys_inventory_action.generic.type	= MTYPE_ACTION;
	s_keys_inventory_action.generic.flags  = QMF_GRAYED;
	s_keys_inventory_action.generic.x		= 0;
	s_keys_inventory_action.generic.y		= y += 9;
	s_keys_inventory_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inventory_action.generic.localdata[0] = ++i;
	s_keys_inventory_action.generic.name	= bindnames[s_keys_inventory_action.generic.localdata[0]][1];

	s_keys_inv_use_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_use_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_use_action.generic.x		= 0;
	s_keys_inv_use_action.generic.y		= y += 9;
	s_keys_inv_use_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_use_action.generic.localdata[0] = ++i;
	s_keys_inv_use_action.generic.name	= bindnames[s_keys_inv_use_action.generic.localdata[0]][1];

	s_keys_inv_drop_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_drop_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_drop_action.generic.x		= 0;
	s_keys_inv_drop_action.generic.y		= y += 9;
	s_keys_inv_drop_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_drop_action.generic.localdata[0] = ++i;
	s_keys_inv_drop_action.generic.name	= bindnames[s_keys_inv_drop_action.generic.localdata[0]][1];

	s_keys_inv_prev_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_prev_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_prev_action.generic.x		= 0;
	s_keys_inv_prev_action.generic.y		= y += 9;
	s_keys_inv_prev_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_prev_action.generic.localdata[0] = ++i;
	s_keys_inv_prev_action.generic.name	= bindnames[s_keys_inv_prev_action.generic.localdata[0]][1];

	s_keys_inv_next_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_next_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_next_action.generic.x		= 0;
	s_keys_inv_next_action.generic.y		= y += 9;
	s_keys_inv_next_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_next_action.generic.localdata[0] = ++i;
	s_keys_inv_next_action.generic.name	= bindnames[s_keys_inv_next_action.generic.localdata[0]][1];

	s_keys_help_computer_action.generic.type	= MTYPE_ACTION;
	s_keys_help_computer_action.generic.flags  = QMF_GRAYED;
	s_keys_help_computer_action.generic.x		= 0;
	s_keys_help_computer_action.generic.y		= y += 9;
	s_keys_help_computer_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_help_computer_action.generic.localdata[0] = ++i;
	s_keys_help_computer_action.generic.name	= bindnames[s_keys_help_computer_action.generic.localdata[0]][1];

	s_keys_menu.draw = Keys_MenuDraw;
	s_keys_menu.key = Keys_MenuKey;

	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_attack_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_change_weapon_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_walk_forward_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_backpedal_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_turn_left_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_turn_right_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_run_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_step_left_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_step_right_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_sidestep_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_look_up_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_look_down_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_center_view_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_mouse_look_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_keyboard_look_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_move_up_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_move_down_action );

	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inventory_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_use_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_drop_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_prev_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_next_action );

	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_help_computer_action );
	
	Menu_SetStatusBar( &s_keys_menu, "enter to change, backspace to clear" );
	Menu_Center( &s_keys_menu );
}


void M_Menu_Keys_f (void)
{
	Keys_MenuInit();
	M_PushMenu( &s_keys_menu );
}
