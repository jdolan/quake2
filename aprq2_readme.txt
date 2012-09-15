::   AprQ2   ::
:: by Maniac ::


Installation
===============
Extract all files from the zip to quake2 main directory and run aq2.exe instead of quake2.exe.


New cvars/commands:

Cvar / Command			Value		Description
==============			=====		===========
cl_async			0 | 1		Enables asyncronous network/rendering FPS.
r_maxfps			5 ..		Changes the renderer maxfps when using cl_async 1.
m_xpfix 			0 | 1		Apply the mouse XP fix.
m_restart					Restarts the mouse subsystem.
m_autosens			0 | 1		1 to enable Mouse FOV Sensitivity Autoscaling.
m_directinput			0 | 1		Enables mouse direct input.
m_accel				0 .. 1		Mouse acceleration (default 0 - disabled).
cl_fps				0 .. 9		1 to show FPS. Different value changes the text color.
cl_fpsx				x-axis		Horizontal position of FPS counter.
cl_fpsy				y-axis		Vertical position of FPS counter.
cl_clock			0 .. 9		Enables clock to HUD. Different value changes the text color.
cl_clockx			x-axis		Horizontal position of clock
cl_clocky			y-axis		Vertical position of clock
cl_clockformat			<time>		Allow you change the cl_clock time format (you can change the time to 12hour style etc.). More info later in this readme.
cl_chathud			0 | 1 | 2	Enables chathud. 2 to change the color.
cl_chathudx			x-axis		Position in pixels of the chathud on the x axis.
cl_chathudy			y-axis		Position in pixels of the chathud on the y axis.
cl_chathudlines			1 .. 8		Number of lines to show in chathud.
cl_chathudtime			0..		Adjust how long text stays max at chathud.
cl_maptime			0 .. 19		1 - 9 to show elapset maptime. 11 - 19 to show round time wich only work in aq2 mod with teamplay mode.
cl_maptimex			x-axis		Position in pixels of the maptime on the x axis.
cl_maptimey			y-axis		Position in pixels of the maptime on the y axis.
cl_timestamps			0 | 1 | 2	1 to show timestamps in chat msgs, 2 to show all server msgs. 0 to disable.
cl_timestampsformat		<time>		Allow you change the timestamps time format. More info later in this readme.
cl_hudalpha			0.0 .. 1	Adjust hud pics transparenty, 1 to disable.
ch_alpha			0.0 .. 1	Control crosshairs transparenty.
ch_pulse			0 ..		Adds pulse effect to crosshair.
ch_scale			0 ..		With this you can scale crosshair size.
ch_red				0.0 .. 1	Crosshair red color control.
ch_green			0.0 .. 1	Crosshair green color control.
ch_blue				0.0 .. 1	Crosshair green color control.
ch_health			0 | 1		crosshair color change when your health change. This ignores ch_<color>'s.
ch_x				0 ..		Adjust crosshair x position.
ch_y				0 ..		Adjust crosshair y position.
cl_gunalpha			0.0 .. 1	Adjust your weapon transparenty.
cl_gun_x			0.0 ..		Adjust the gun x position.
cl_gun_y			0.0 ..		Adjust the gun y position.
cl_gun_z			0.0 ..		Adjust the gun z position.
cl_wsfov			0 | 1		Sets automaticly correct fov for widescreen modes.

scr_drawlagometer		0|1|2|3		Enables lagometer. value 1 draw ping and 2 draw ping, upload and download speed also.
scr_drawlagometer_x		0 ..		Adjust lagometer x position.
scr_drawlagometer_y		0 ..		Adjust lagometer y position.
scr_draw2d			0 | 1		You can disable hud with this (default 1).

draw 		<name> <x> <y> [color [time]]	Add drawing cvar/macro string to screen.
undraw				<name>		Remove drawing cvar/macro.

s_swapstereo			0 | 1		Swap stereo.
s_ambient			0 | 1		Disable/enable ambient sounds.
s_oldresample			0 | 1		When enabled, uses the old sound resamble code from original q2.


cl_highlight			0|1|2|3		Enables highlight. 1 different sound (talk1.wav), 2 different colour, 3 do both.
cl_highlightmode		0 | 1		0 - check only from msg highlight texts, 1 - also nick is included.
highlight			<text>		Adds text to highlight list. If text is in chat msg, it will be highlighted.
unhighlight			<text>		Removes the typed highlight.
cl_textcolors			0 | 1		This enable other colors in some text. More info later in this readme.
cl_highlightcolor		0 - 9		Color of highlighted msg's. Need cl_highlight to be 2 or 3 and cl_colortexts enabled.
cl_mychatcolor			0 - 9		Color of your own chat text. Need cl_colortexts to be enabled.

cl_ignoremode			0 | 1 | 2	Mode for ignoretext. 0 = disabled, 1 = enabled, 2 = reverse mode which mean all will be ignored if its not in the list.
ignoretext			<text>		Text which will be ignored. Wildcard support.
unignoretext			<text>		Removes text from ignorelist. Parameter "all" removes all.
cl_autoscreenshot		0 | 1 | 2	Take automatic screenshot after map ends (1=tga, 2=jpg)
cl_autorecord			0 | 1		Enables autorecording.
cl_clan				text		If set, using different demo naming when use record without parameters.
demolist					List all demos in demos\ directory with id number.
demoplay			<id>		Plays demo with given id.

scr_conheight			0.0 .. 1.0	Console height (default 0.5)
con_notifylines			1 .. 8		Numbers of notifylines (lines that appears top of the screen to show latest messages). Default is 4.
con_notifyfade			0|1		Enables notifylines fading.
con_scrlines			1 ..		Controls the lines scrolled up/down in console when using the Page Up/Down keys and mousewheel. Default is 2.
con_alpha			0.0 .. 1	Adjust console transparenty.
con_cmdcomplete			0-4		Change between different commandcomplete.
net_port			0 ...		Allows you to select the local port (default 27901). The value 0 means it will choose a random port.
cl_maxpackets			0 ...		Control how many packets client send max at per frame to the servers (default 0). More info later in this readme.
fs_allpakloading		0 | 1		Enables *.pak loading.
cfg_save			filename	Write all binds, settings and aliases to given file.
serverstatus		    [server address]	Gives you the info about server. What map its running, players and theyr ping & score. Without parameter it gives the current server status.

-Scripting-
unalias				<alias name>	remove alias
aliaslist			[text]		list aliases
trigger				<cmd> <string>	If string match to print msg from server, trigger execute cmd. Wildcard support.
untrigger			<cmd> <string>	Removes specified trigger. Parameter "all" removes all.
macrolist					List all current macros.
toggle	  		<cvar> [val1 .. val2]	Toggle the variable var between given values. If no values are supplied then var will be toggled between 0 & 1.
inc			     <cvar> [value]	Increase a cvar by the specified value. If no value supplied, it will increase by 1.
dec			     <cvar> [value]	Decrease a cvar by the specified value. If no value supplied, it will decrease by 1.
random 			<cvar> <from> <to> 	Sets cvar a random value between from and to.


r_customheight			<height>	Custom resolution height value.
r_customwidth			<width>		Custom resolution widht value.
modelist					List all vid mode values.
vid_minimize					Minimize the game.

-GL MODE-
skydistance			0 .. 16384	Sky box size (default 2300).
screenshotjpg					Takes screenshot in JPG format.
gl_screenshot_quality		0 .. 100	Screenshot jpg quality (default 85).
gl_replacetga			0 | 1		If enabled it tries to replace ALL wal files with png, tga or jpg ones if found any. Sequence is png->tga->jpg->wal.
gl_replacepcx			0 | 1		If enabled it tries to replace ALL pcx files with png, tga or jpg ones if found any. Sequence is png->tga->jpg->pcx.
gl_replacemd2			0 | 1		If enebled it tries to replace ALL md2 files with md3.
gl_shadows			0 | 1 | 2	1 enables standard Quake 2 shadows, 2 enables improved stencil buffered shadows if your video card supports it.
gl_waterwaves			0 ... 4		Enables water wave effect to still water.
gl_motionblur			0 | 1		Enables motion blur effect if your hardware support it.
gl_particle			0 | 1		1 enables new particles. 0 is original ones.
gl_fontshadow			0 | 1		Enables shadow to all text.
gl_sgis_mipmap			0 | 1		Enables hardware mipmap generation if hardware supports it.
gl_ext_texture_compression	0 | 1		Enables texture compression if hardware supports it.
gl_celshading			0 | 1		This supposedly simulates cartoon drawing and shading.
gl_celshading_width		1 .. 10		(default 5) to set size of lines for celshading outlines
gl_scale			1 ..		(default 1). This allow you to scale font text and hud pictures size.
gl_fog				0 | 1		Enables fog effect.
gl_fog_density			0 ..		Controls fog density.
gl_decals			0 | 1		Enable bullet hole decals.
gl_decals_max			256 - 4096	Maximum decals count.
gl_decals_time			1 .. 		Controls how long decals stays.
gl_gammapics			0 | 1		(default 1) 0 to disable vid_gamma to added hud images
vid_displayfrequency 		0 .. 		(default 0 disabled) set displayfrequency wich game will use WARNING: set this so you monitor can handle it!!!!
gl_shelleffect			0 | 1		New shell effect.
gl_ext_texture_filter_anisotropic 0 | 1		Allow user to set anisotropic level.
gl_ext_max_anisotropy 		0 ..		Set the anisotropy level.
gl_coloredlightmaps		0 .. 1		Change the lightmap level.


-openAL-
s_initsound 2 to enable
al_driver  			<text>		The filename of the OpenAL driver library. Default: openal32 (Win32) or libopenal.so.0 (Linux/FreeBSD)
al_device  			<text>		Set the preferred device you'd like to use for OpenAL.  Default: ""
al_errorcheck 			0 | 1		Check for OpenAL errors. Default: 0

-aviexport (win32)-
aviexport    <framerate> <demo name> <avi name> Exports the demo to an avi at the given framerate.
avirecord			<fps>		Start recording avi while in demo playback. Names avi with 'demoxx.avi'.
avistop						Stop exporting avi.
avi_codec			<codec>		Select codec. Use fourCC to specify the codec (ec 'xvid')
avi_codecmenu			0|1		Enable window list to select avi codec.

-screenshot dumping (gl)-
cl_avidump			0...		This enables the dumping and is also the fps.
cl_avidemoformat		0|1		Screenshot format, 0 tga, 1 jpg.

-Winamp controls-
winampnext					Next Track 
winamppause					Pause 
winampplay		      [track nro]	Play, if optional parameter is typed, its try to play that track number from playlist.
winampprev					Previous Track
winampstop					Stop
winamprestart					Restart Winamp Integration
winampshuffle					Toggle Shuffle
winamprepeat					Toggle Repeat
winampvolume			<value>		Set Volume to given value (0-100) in percent
winampvolup					Volume up 1%.
winampvoldown					Volume down 1%
winamptitle					Display playing title
winampsonginfo					Display name, length, elapsed, remaining, bitrate and samplerate info about current s 
winampsearch			<text>		List all songs with track number in playlist wich got given text in it.
cl_winampmessages		0 | 1		Winamp console messages (On-Default).
cl_winamp_dir			<dir>		Directory where winamp is. (default: 'C:/program files/winamp')

xmms/mpd controls have almoust all same as above, exept all start with 'xmms_'/'mpd_'

===============================================================================================================

Locs
======
Biteme / nitro2 / q2ace / nocheat / R1Q2 locs are compatible. Loc files are in the format [mapname].loc
To use a loc you need to use $loc_here & $loc_there macros.

Example:
bind l "say_team Im at $loc_here and looking at [$loc_there$]"

Locs:
$loc_here			Location where you are standing.
$loc_there$			Location where you are looking at.

Loc Commands:
loc_add		<location name> Adds a location to your current position on the map. 
loc_list			Display a list of locations. 
loc_del				Deletes the nearest location on the map.
loc_save 	<filename>	Saves the loc file with the current locs in memory. File name SHOULD be the map name to work.
cl_drawlocs	0 | 1		Draw all loc positions near you. The one you are nearest will bounce up and down
loc_dir		<dir>		Dir where loc's is loaded.


Info about color values
========================
0 = black
1 = red
2 = green
3 = yellow
4 = blue
5 = cyan
6 = mageta
7 = white

To use these in echo messages or cl_highlightcolor you need to enable cl_textcolors.

NOTICE: Colors work best if your conchars.pcx lower byte characters are white like in orginal q2 conchar.pcx.


Info about how to set time to 
cl_timestampformat or cl_clockformat
==================================
%a - Abbreviated weekday name
%A - Full weekday name
%b - Abbreviated month name
%B - Full month name
%c - Appropriate date and time representation
%C - Century number ("19" if year is 1997)
%d - Day of the month as a decimal number (01-31)
%H - Hour (24-hour clock) as a decimal number (00-23)
%I - Hour (12-hour clock) as a decimal number (01-12)
%j - Day of the year as a decimal number (001-366)
%m - Month as a decimal number (01-12)
%M - Minute as a decimal number (00-59)
%p - Equivalent of either a.m. or p.m. notation
%S - Second as a decimal number (00-59)
%U - Week number of the year as a decimal number (00-53) where the first Sunday in January is the first day of the week 1
%w - Weekday as a decimal number (0-6) where 0 is Sunday
%W - Week number of the year as a decimal number (00-53). If the week containing January 1 has four or more days of the new year, it is considered week 1. Otherwise it is the last week of the previous year, and the next week is week 1.
%x - Appropriate date representation
%X - Appropriate time representation
%y - Last two digits of the year as a decimal number (00-99)
%Y - Year as a four-digit decimal number
%Z - Timezone name or abbreviation, or by no characters if no timezone exists

 Example	 Output
 -------------	 -------------
 [%H:%M:%S]	 [21:30:59]
 <%X>		 <21:30:59>
 [%I:%M:%S %p]	 [09:30:59 PM]
 %Y-%m-%d	 2003-07-28


if command
----------------------

if <a> <operator> <b> then <command1> [else <command2>]
 If specified condition is true <command1> is executed, otherwise, <command2>.
    
Possible <operators>:
    
op          case sens.   condition
------------------------------------------
==             yes      values/strings are equal
!=, <>         yes      values/strings are not equal
isin           yes      string <b> contains string <a>
!isin          yes      string <b> does not contain string <a>
isini          no       string <b> contains string <a>
!isini         no       string <b> does not contain string <a>
eq             no       strings are equal
ne             no       strings are not equal
      
For numeric values comparsion operators <, <=, >, >= can also be used.


---------------------------
About cl_maxpackets (from q2pro)
---------------------------
VQ2 sends client movement packets to the server each client frame.
    
If your framerate is high (more than 100 fps), and your network bandwidth is low (modem),
this results in bandwidth overrun and, therefore, consistent lag. That's why every modem user
had to to degrade performance of their machine by clamping framerate with cl_maxfps cvar.
 
cl_maxpackets specifies maximum number of packets
sent to the server each second, so you don't longer need to clamp framerate.
    
This uses a small hack from Fuzzquake2 mod, where packets are generated each frame,
but some of them are dropped.
 
Normally, this will result in prediction errors, but if there are less than 3 packets dropped,
everything works fine, thanks to q2 engine architecture.
    
That's why minium cl_maxpacket is: cl_maxfps / 3


Notes
======
To get rid of a boarder when scaling crosshair make crosshair pic two times larger.

Credits
========
Heffo, MrG, Idle, Echon, R1CH, [SkulleR], Vic and all other who have made these features in first place.

Uses: Zlib 1.2.31 Library, Copyright (C) 1995-2004 Jean-loup Gailly and Mark Adler.
      libpng 1.2.20 Library, Copyright (c) 1998-2004 Glenn Randers-Pehrson.
      JPEG Library 6, Copyright (C) 1991-1998, Thomas G. Lane.
      Libcurl 7.15.1 Library, Copyright (c) 1996 - 2005 Daniel Stenberg.

Other
=======
Source can be downloaded from http://apprime.0wns.org/q2/

-Maniac