#
# Quake2 Makefile for Linux, BSD and OS X.
#

# Check OS type.
PLATFORM=$(shell uname -s|tr A-Z a-z)

# Installation directory
DESTDIR=$(HOME)/.quake2

ifneq ($(PLATFORM),linux)
	ifneq ($(PLATFORM),freebsd)
		ifneq ($(PLATFORM),darwin)
			$(error OS $(PLATFORM) is not supported)
		endif
	endif
endif

CC=gcc
CFLAGS=-funsigned-char -pipe $(shell sdl-config --cflags) -DGL_QUAKE -DUSE_SDL -DUSE_CURL

ifeq ($(PLATFORM),darwin)
	CFLAGS += -D__APPLE__ -I/opt/local/include
endif

DEBUG_CFLAGS=$(CFLAGS) -g -Wall
RELEASE_CFLAGS=$(CFLAGS) -g -O2 -Wall -DNDEBUG

ifeq ($(PLATFORM),freebsd)
	LDFLAGS=-lm
else
	LDFLAGS=-lm -ldl
endif

LDFLAGS += $(shell sdl-config --libs)
LDFLAGS += $(shell curl-config --libs)
LDFLAGS += -ljpeg -lpng -lz

all: debug

bin:
	mkdir -p bin

# Debug target.
debug: bin
	$(MAKE) targets CFLAGS="$(DEBUG_CFLAGS)"

# Release target.
release: bin
	$(MAKE) targets CFLAGS="$(RELEASE_CFLAGS)"

# Engine and game module targets.
targets: bin/quake2 bin/game.so

# Game module compilation target.
src/game/%.o : src/game/%.c
	@mkdir -p $(shell dirname $@)
	@echo " [CC] $(shell basename $@)"
	@$(CC) $(CFLAGS) -fPIC -o $@ -c $<
	
# Executable compilation target.
src/%.o : src/%.c
	@mkdir -p $(shell dirname $@)
	@echo " [CC] $(shell basename $@)"
	@$(CC) $(CFLAGS) -o $@ -c $<

QUAKE2_OBJS = \
	src/client/cl_cin.o \
	src/client/cl_demo.o \
	src/client/cl_draw.o \
	src/client/cl_ents.o \
	src/client/cl_fx.o \
	src/client/cl_http.o \
	src/client/cl_input.o \
	src/client/cl_inv.o \
	src/client/cl_loc.o \
	src/client/cl_main.o \
	src/client/cl_newfx.o \
	src/client/cl_parse.o \
	src/client/cl_pred.o \
	src/client/cl_scrn.o \
	src/client/cl_tent.o \
	src/client/cl_view.o \
	src/client/console.o \
	src/client/keys.o \
	src/client/snd_dma.o \
	src/client/snd_mem.o \
	src/client/snd_mix.o \
	\
	src/game/m_flash.o \
	src/game/q_shared.o \
	\
	src/include/minizip/ioapi.o \
	src/include/minizip/unzip.o \
	\
	src/linux/cd_linux.o \
	src/linux/glob.o \
	src/linux/qgl_linux.o \
	src/linux/q_shlinux.o \
	src/linux/rw_linux.o \
	src/linux/rw_sdl.o \
	src/linux/snd_sdl.o \
	src/linux/sys_linux.o \
	src/linux/vid_so.o \
	\
	src/qcommon/cmd.o \
	src/qcommon/cmodel.o \
	src/qcommon/common.o \
	src/qcommon/crc.o \
	src/qcommon/cvar.o \
	src/qcommon/files.o \
	src/qcommon/md4.o \
	src/qcommon/net_chan.o \
	src/qcommon/net.o \
	src/qcommon/pmove.o \
	src/qcommon/q_msg.o \
	\
	src/ref_gl/gl_decal.o \
	src/ref_gl/gl_draw.o \
	src/ref_gl/gl_image.o \
	src/ref_gl/gl_light.o \
	src/ref_gl/gl_mesh.o \
	src/ref_gl/gl_model.o \
	src/ref_gl/gl_rmain.o \
	src/ref_gl/gl_rmisc.o \
	src/ref_gl/gl_rsurf.o \
	src/ref_gl/gl_warp.o \
	\
	src/server/sv_ccmds.o \
	src/server/sv_ents.o \
	src/server/sv_game.o \
	src/server/sv_init.o \
	src/server/sv_main.o \
	src/server/sv_send.o \
	src/server/sv_user.o \
	src/server/sv_world.o \
	\
	src/ui/qmenu.o \
	src/ui/ui_addressbook.o \
	src/ui/ui_atoms.o \
	src/ui/ui_controls.o \
	src/ui/ui_credits.o \
	src/ui/ui_demos.o \
	src/ui/ui_dmoptions.o \
	src/ui/ui_download.o \
	src/ui/ui_game.o \
	src/ui/ui_joinserver.o \
	src/ui/ui_keys.o \
	src/ui/ui_loadsavegame.o \
	src/ui/ui_main.o \
	src/ui/ui_multiplayer.o \
	src/ui/ui_newoptions.o \
	src/ui/ui_playerconfig.o \
	src/ui/ui_quit.o \
	src/ui/ui_startserver.o \
	src/ui/ui_video.o

# The game executable.
bin/quake2 : $(QUAKE2_OBJS)
	@echo "[LD] quake2"
	@$(CC) -o $@ $(QUAKE2_OBJS) $(LDFLAGS)

GAME_OBJS = \
	src/game/g_ai.o \
	src/game/g_chase.o \
	src/game/g_cmds.o \
	src/game/g_combat.o \
	src/game/g_func.o \
	src/game/g_items.o \
	src/game/g_main.o \
	src/game/g_misc.o \
	src/game/g_monster.o \
	src/game/g_phys.o \
	src/game/g_save.o \
	src/game/g_spawn.o \
	src/game/g_svcmds.o \
	src/game/g_target.o \
	src/game/g_trigger.o \
	src/game/g_turret.o \
	src/game/g_utils.o \
	src/game/g_weapon.o \
	src/game/m_actor.o \
	src/game/m_berserk.o \
	src/game/m_boss2.o \
	src/game/m_boss31.o \
	src/game/m_boss32.o \
	src/game/m_boss3.o \
	src/game/m_brain.o \
	src/game/m_chick.o \
	src/game/m_flash.o \
	src/game/m_flipper.o \
	src/game/m_float.o \
	src/game/m_flyer.o \
	src/game/m_gladiator.o \
	src/game/m_gunner.o \
	src/game/m_hover.o \
	src/game/m_infantry.o \
	src/game/m_insane.o \
	src/game/m_medic.o \
	src/game/m_move.o \
	src/game/m_mutant.o \
	src/game/m_parasite.o \
	src/game/m_soldier.o \
	src/game/m_supertank.o \
	src/game/m_tank.o \
	src/game/p_client.o \
	src/game/p_hud.o \
	src/game/p_trail.o \
	src/game/p_view.o \
	src/game/p_weapon.o \
	src/game/q_shared.o
	
# The game shared library.
bin/game.so : $(GAME_OBJS)
	@echo "[LD] game.so"
	@$(CC) $(CFLAGS) -shared -o $@ $(GAME_OBJS)

# The install target
install: targets
	install -d "$(DESTDIR)/baseq2"
	install bin/quake2 "$(DESTDIR)"
	install bin/game.so "$(DESTDIR)/baseq2"
	install data/install-data.py "$(DESTDIR)"

# The clean target.
clean:
	@rm -rf $(QUAKE2_OBJS) $(GAME_OBJS) bin/*
