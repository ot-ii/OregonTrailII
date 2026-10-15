# otii_c: the C port of ports\modifications, built with nmake and the MSVC toolchain.

# The target of the Visual Studio command prompt (vcvars32/vcvars64) gives the bits; BITS= on the command line wins.
!IF "$(VSCMD_ARG_TGT_ARCH)" == "x64"
BITS = 64
!ELSEIF "$(VSCMD_ARG_TGT_ARCH)" == "x86"
BITS = 32
!ELSE
!ERROR VSCMD_ARG_TGT_ARCH is "$(VSCMD_ARG_TGT_ARCH)": run nmake from an x86 or x64 Visual Studio command prompt
!ENDIF

NAME = otii$(BITS)
EXTRA =
AUTO =

CC = cl
LINKER = link

DEFS = /D WIN32_LEAN_AND_MEAN /D _CRT_SECURE_NO_WARNINGS /D NOMINMAX
WARN = /W4 /wd4100 /wd4201
OPT = /O2
CFLAGS = /nologo /c /TC /std:c11 $(OPT) /Zi /FS /Fd$(NAME).pdb $(DEFS) $(WARN) $(EXTRA) $(FEATURE_DEFS)

!INCLUDE version.make
VER_DEFS = /DVER_MAJOR=$(VER_MAJOR) /DVER_MINOR=$(VER_MINOR) /DVER_BUILD=$(VER_BUILD)

LIBS = user32.lib gdi32.lib winmm.lib kernel32.lib bcrypt.lib

MAIN_OBJ = src\app\windows\main.obj

FOUNDATION_OBJS = \
	src\base\byteorder.obj \
	src\base\cstr.obj \
	src\base\date.obj \
	src\base\geom.obj \
	src\base\list.obj \
	src\base\log.obj \
	src\base\memory.obj \
	src\base\rng.obj \
	src\base\str.obj \
	src\base\windows\clock.obj \
	src\base\windows\file.obj

DATA_OBJS = \
	src\app\windows\replay.obj \
	src\base\windows\ini.obj \
	src\data\assets.obj \
	src\data\datafile.obj \
	src\data\decode.obj \
	src\data\gfxdev.obj \
	src\data\image.obj \
	src\data\mecclib.obj \
	src\data\rsrc.obj \
	src\data\strrsrc.obj

ENGINE_OBJS = \
	src\base\object.obj \
	src\app\event.obj \
	src\app\evthandler.obj \
	src\dialogs\about.obj \
	src\dialogs\alert.obj \
	src\draw\animsprite.obj \
	src\app\windows\app.obj \
	src\app\windows\appvideo.obj \
	src\ui\blinkview.obj \
	src\ui\windows\button.obj \
	src\draw\canvas.obj \
	src\ui\cliplabelview.obj \
	src\ui\clipview.obj \
	src\draw\collgrid.obj \
	src\ui\windows\combobox.obj \
	src\ui\windows\control.obj \
	src\ui\windows\cursor.obj \
	src\ui\cursormgr.obj \
	src\draw\windows\dc.obj \
	src\draw\dispobj.obj \
	src\ui\windows\dlgwin.obj \
	src\data\docfile.obj \
	src\app\docmanager.obj \
	src\draw\drawable.obj \
	src\draw\drwegad.obj \
	src\ui\windows\edit.obj \
	src\ui\windows\floatwin.obj \
	src\draw\windows\font.obj \
	src\draw\fontrsrc.obj \
	src\draw\windows\fonttable.obj \
	src\ui\frametemplate.obj \
	src\ui\windows\framewin.obj \
	src\draw\windows\label.obj \
	src\ui\labeledcontrol.obj \
	src\app\windows\license.obj \
	src\ui\windows\listbox.obj \
	src\draw\mediabase.obj \
	src\ui\mediabutton.obj \
	src\ui\mediaview.obj \
	src\ui\windows\menu.obj \
	src\ui\windows\menubar.obj \
	src\ui\menuitem.obj \
	src\trail\meter.obj \
	src\dialogs\windows\modaldialog.obj \
	src\draw\multidraw.obj \
	src\draw\multiimage.obj \
	src\draw\objbase.obj \
	src\draw\windows\offscreendc.obj \
	src\draw\windows\oval.obj \
	src\draw\windows\palette.obj \
	src\base\windows\path.obj \
	src\ui\piccontrol.obj \
	src\ui\pictctrl.obj \
	src\draw\pictserver.obj \
	src\ui\picview.obj \
	src\ui\windows\polyctrl.obj \
	src\draw\rectrgnlist.obj \
	src\draw\windows\region.obj \
	src\app\registrar.obj \
	src\draw\windows\roundrect.obj \
	src\ui\windows\scaler.obj \
	src\draw\windows\screendc.obj \
	src\ui\scrollbar.obj \
	src\ui\scrollview.obj \
	src\draw\server.obj \
	src\draw\serversub.obj \
	src\draw\windows\shape.obj \
	src\draw\shapebase.obj \
	src\draw\sprite.obj \
	src\draw\sprobj.obj \
	src\draw\statefuldispobj.obj \
	src\base\windows\sysinfo.obj \
	src\draw\windows\text.obj \
	src\draw\textimage.obj \
	src\draw\textlabel.obj \
	src\ui\textview.obj \
	src\trail\tilemap.obj \
	src\base\windows\timer.obj \
	src\ui\uicontainer.obj \
	src\ui\view.obj \
	src\draw\viewsurface.obj \
	src\draw\windows\windc.obj \
	src\ui\windows\window.obj \
	src\ui\windows\windowborder.obj \
	src\ui\windows\windowpos.obj \
	src\ui\windows\wndmouse.obj

SOUND_OBJS = \
	src\media\stream.obj \
	src\media\bufstrm.obj \
	src\media\filstrm.obj \
	src\media\memstream.obj \
	src\media\sndstrm.obj \
	src\media\sndunpack.obj \
	src\media\sndupkstrm.obj \
	src\media\pcmdecode.obj \
	src\media\mixstrm.obj \
	src\media\xfadestrm.obj \
	src\media\sndobj.obj \
	src\media\sndbase.obj \
	src\media\filesnd.obj \
	src\media\memsnd.obj \
	src\media\winmixedstreamedsnd.obj \
	src\media\sndstream.obj \
	src\media\strmdsnd.obj \
	src\media\windows\ressnd.obj \
	src\media\windows\strmdsndw.obj \
	src\media\windows\xaudio2sink.obj \
	src\media\windows\audiodevicemenu.obj

VIDEO_OBJS = \
	src\media\smcdecoder.obj \
	src\media\cinepakframe.obj \
	src\media\movfile.obj \
	src\media\framesource.obj

VIDEO_WIN_OBJS = \
	src\media\windows\cinepakframew.obj \
	src\media\windows\movplayer.obj \
	src\media\windows\vfwprobe.obj \
	src\media\windows\mcivideo.obj

GAME_OBJS = \
	src\dialogs\aboutotiidlg.obj \
	src\game\ailment.obj \
	src\hunt\animal.obj \
	src\hunt\animalbase.obj \
	src\hunt\animals.obj \
	src\app\windows\autosave.obj \
	src\dialogs\clickdlg.obj \
	src\dialogs\convdlg.obj \
	src\data\dataversion.obj \
	src\features\debugscreen.obj \
	src\features\windows\debugscreenw.obj \
	src\game\draftanimalitem.obj \
	src\dialogs\dropdlg.obj \
	src\screens\endframe.obj \
	src\game\endgame.obj \
	src\trail\fordwagon.obj \
	src\game\game.obj \
	src\game\gamedoc.obj \
	src\dialogs\glossarydlg.obj \
	src\screens\healthwin.obj \
	src\trail\hillrules.obj \
	src\dialogs\hiscoredlg.obj \
	src\hunt\hunting.obj \
	src\dialogs\huntweapondlg.obj \
	src\screens\invgrid.obj \
	src\game\invitem.obj \
	src\game\invlist.obj \
	src\game\journal.obj \
	src\dialogs\journaldlg.obj \
	src\game\journalentry.obj \
	src\screens\mainui.obj \
	src\trail\maparea.obj \
	src\dialogs\mapdlg.obj \
	src\trail\mapview.obj \
	src\game\member.obj \
	src\game\msg.obj \
	src\dialogs\msgbox.obj \
	src\game\msgcontroller.obj \
	src\game\msgcontrollerhealth.obj \
	src\game\msgqueue.obj \
	src\dialogs\newgamedlg.obj \
	src\trail\node.obj \
	src\game\occupmgr.obj \
	src\app\otiiapp.obj \
	src\app\windows\otiiappw.obj \
	src\app\otiicmd.obj \
	src\dialogs\overviewmapdlg.obj \
	src\game\party.obj \
	src\dialogs\pickerdlg.obj \
	src\screens\progressbar.obj \
	src\dialogs\prologuedlg.obj \
	src\dialogs\purchasedlg.obj \
	src\dialogs\quickstartdlg.obj \
	src\trail\raftframe.obj \
	src\dialogs\restdlg.obj \
	src\trail\riverrules.obj \
	src\screens\sceneassets.obj \
	src\screens\sceneframe.obj \
	src\app\settings.obj \
	src\dialogs\skilldlg.obj \
	src\dialogs\speeddlg.obj \
	src\screens\textlist.obj \
	src\screens\titleanim.obj \
	src\dialogs\tradedlg.obj \
	version.obj \
	src\game\wagonitem.obj \
	src\game\weather.obj

!IF "$(AUTO)" == "1"
AUTO_OBJS = \
	src\features\windows\controlstate.obj \
	src\features\windows\autopilot.obj \
	src\features\windows\autobatch.obj
!ELSE
AUTO_OBJS =
!ENDIF

# The feature builds: each switch adds its define, its objects and its libraries.
#   TWITCH_PARTY=1  the Twitch party Redemptions window (offline, online and crowd builds)
#   OTII_TWITCH=1   Twitch plays the game (the twitch build)
#   CROWD=1         Crowd Control (crowd and twitch builds)
#   ONLINE=1        online high scores (the online build)
#   CLI=1           the CLI console control (the cli build)
#   DIAG=1          the crash log, free guard and memory tracking (the diag build)
#   TEST=1          the in-game test suites (the test build, with AUTO=1)
TWITCH_PARTY = 1
OTII_TWITCH =
CROWD =
ONLINE =
CLI =
DIAG =
TEST =
FEATURE_DEFS =
FEATURE_OBJS =
FEATURE_LIBS =

MTY_JSON_OBJS = \
	src\third_party\mty.obj \
	src\third_party\mty_hash.obj \
	src\third_party\mty_json.obj

TWITCH_IRC_OBJS =

!IF "$(TWITCH_PARTY)" == "1" || "$(OTII_TWITCH)" == "1"
TWITCH_IRC_OBJS = src\features\twitchirc.obj src\features\windows\twitchircw.obj
FEATURE_OBJS = $(FEATURE_OBJS) $(TWITCH_IRC_OBJS)
FEATURE_LIBS = $(FEATURE_LIBS) ws2_32.lib secur32.lib
!ENDIF

!IF "$(TWITCH_PARTY)" == "1"
FEATURE_DEFS = $(FEATURE_DEFS) /DTWITCH_PARTY
FEATURE_OBJS = $(FEATURE_OBJS) src\features\windows\twitchparty.obj
!ENDIF

!IF "$(OTII_TWITCH)" == "1"
FEATURE_DEFS = $(FEATURE_DEFS) /DOTII_TWITCH
FEATURE_OBJS = $(FEATURE_OBJS) src\features\windows\twitchcontrol.obj
!IF "$(AUTO)" != "1"
FEATURE_OBJS = $(FEATURE_OBJS) src\features\windows\controlstate.obj
!ENDIF
!ENDIF

!IF "$(CROWD)" == "1"
FEATURE_DEFS = $(FEATURE_DEFS) /DCROWD_CONTROL
FEATURE_OBJS = $(FEATURE_OBJS) src\features\crowdcontrol.obj src\features\windows\crowdcontrolw.obj src\features\windows\crowdnet.obj $(MTY_JSON_OBJS)
FEATURE_LIBS = $(FEATURE_LIBS) winhttp.lib crypt32.lib
!ENDIF

!IF "$(ONLINE)" == "1"
FEATURE_DEFS = $(FEATURE_DEFS) /DONLINE_SCORES
FEATURE_OBJS = $(FEATURE_OBJS) src\features\onlinescores.obj src\features\onlinescoresnet.obj src\features\windows\onlinescoresnetw.obj
FEATURE_LIBS = $(FEATURE_LIBS) winhttp.lib bcrypt.lib
!ENDIF

!IF "$(CLI)" == "1"
FEATURE_DEFS = $(FEATURE_DEFS) /DCLI_CONTROL
FEATURE_OBJS = $(FEATURE_OBJS) src\features\windows\controlstate.obj src\features\windows\clicontrol.obj
!ENDIF

!IF "$(DIAG)" == "1"
FEATURE_DEFS = $(FEATURE_DEFS) /DDIAG_BUILD
FEATURE_OBJS = $(FEATURE_OBJS) src\features\windows\crashlog.obj src\features\windows\freeguard.obj src\features\windows\memtrack.obj
!ENDIF

!IF "$(TEST)" == "1"
FEATURE_DEFS = $(FEATURE_DEFS) /DOTII_TEST
FEATURE_OBJS = $(FEATURE_OBJS) \
	tests\testlist.obj \
	tests\testframework.obj \
	tests\windows\testframeworkw.obj \
	tests\testsupport.obj \
	tests\windows\testsupportw.obj \
	tests\windows\testhost.obj \
	tests\scn_daily.obj \
	tests\scn_endgame.obj \
	tests\scn_events.obj \
	tests\scn_hill.obj \
	tests\scn_host.obj \
	tests\scn_hunt.obj \
	tests\scn_illness.obj \
	tests\scn_nodes.obj \
	tests\scn_river.obj \
	tests\scn_store.obj \
	tests\windows\scn_crowd.obj \
	tests\windows\scn_raft.obj \
	tests\windows\scn_saveload.obj \
	tests\windows\scn_scaler.obj \
	tests\windows\scn_towns.obj \
	tests\windows\scn_twitch.obj \
	tests\windows\scn_twitchplays.obj \
	tests\unit_crowd.obj \
	tests\unit_data.obj \
	tests\unit_date.obj \
	tests\unit_foundation.obj \
	tests\unit_ini.obj \
	tests\unit_inv.obj \
	tests\unit_rng.obj \
	tests\unit_rsrc.obj \
	tests\unit_scaler.obj \
	tests\unit_snd.obj \
	tests\unit_str.obj \
	tests\windows\unit_autopilot.obj \
	tests\windows\unit_cinepak.obj \
	tests\windows\unit_mov.obj \
	tests\windows\unit_quickstart.obj \
	tests\windows\unit_twitch.obj
!ENDIF

RES = resources.res

# The game.
game: $(NAME).exe

$(RES): resources\resources.rc src\app\version.h version.make
	rc /nologo /dVER_MAJOR=$(VER_MAJOR) /dVER_MINOR=$(VER_MINOR) /dVER_BUILD=$(VER_BUILD) /fo$(RES) resources\resources.rc

ALL_ENGINE_OBJS = $(FOUNDATION_OBJS) $(DATA_OBJS) $(ENGINE_OBJS) $(SOUND_OBJS) $(VIDEO_OBJS) $(VIDEO_WIN_OBJS)
WIN_LIBS = $(LIBS) dwmapi.lib comdlg32.lib vfw32.lib shell32.lib ole32.lib Winhttp.lib Crypt32.lib Ws2_32.lib Secur32.lib

$(NAME).exe: clean $(ALL_ENGINE_OBJS) $(GAME_OBJS) $(AUTO_OBJS) $(FEATURE_OBJS) $(MAIN_OBJ) $(RES)
	$(LINKER) /nologo /subsystem:windows /debug /opt:ref /map:$(NAME).map /out:$@ *.obj $(RES) $(WIN_LIBS) $(FEATURE_LIBS)

# The variant builds, each in its own object directory.
auto:
	$(MAKE) /nologo /f makefile BITS=$(BITS) NAME=otii$(BITS)_auto EXTRA=/DOTII_AUTO_MODE AUTO=1 TWITCH_PARTY= game

online:
	$(MAKE) /nologo /f makefile BITS=$(BITS) OBJDIR=obj$(BITS)o NAME=otii$(BITS)_online ONLINE=1 TWITCH_PARTY=1 game

crowd:
	$(MAKE) /nologo /f makefile BITS=$(BITS) OBJDIR=obj$(BITS)cc NAME=otii$(BITS)_crowd CROWD=1 game

cli:
	$(MAKE) /nologo /f makefile BITS=$(BITS) OBJDIR=obj$(BITS)c NAME=otii$(BITS)_cli CLI=1 TWITCH_PARTY= game

twitch:
	$(MAKE) /nologo /f makefile BITS=$(BITS) OBJDIR=obj$(BITS)t NAME=otii$(BITS)_twitch OTII_TWITCH=1 CROWD=1 TWITCH_PARTY= game

# The twitch build with the test suites (scn_twitchplays drives it with chat from tools\twitch_testserver.py) and
# the test build's checks: [twitch] server= is honoured only here.
twitchtest:
	$(MAKE) /nologo /f makefile BITS=$(BITS) OBJDIR=obj$(BITS)tx NAME=otii$(BITS)_twitchtest "OPT=/Od /RTC1 /GS" EXTRA=/DOTII_AUTO_MODE AUTO=1 TEST=1 DIAG=1 OTII_TWITCH=1 CROWD=1 TWITCH_PARTY= game

diag:
	$(MAKE) /nologo /f makefile BITS=$(BITS) OBJDIR=obj$(BITS)d NAME=otii$(BITS)_diag "OPT=/Od /RTC1 /GS" DIAG=1 TWITCH_PARTY= game

test:
	$(MAKE) /nologo /f makefile BITS=$(BITS) OBJDIR=obj$(BITS)x NAME=otii$(BITS)_test "OPT=/Od /RTC1 /GS" EXTRA=/DOTII_AUTO_MODE AUTO=1 TEST=1 DIAG=1 CROWD=1 TWITCH_PARTY=1 game

enginelink: $(ALL_ENGINE_OBJS) $(MAIN_OBJ)
	-$(LINKER) /nologo /subsystem:windows /debug /force:unresolved /out:enginelink.exe *.obj $(WIN_LIBS)

engine: $(ENGINE_OBJS)

version.obj: src\app\version.c src\app\version.h version.make
	$(CC) $(CFLAGS) $(VER_DEFS) src\app\version.c

unit: unit_foundation$(BITS).exe

datadump: datadump$(BITS).exe

unit_foundation$(BITS).exe: clean $(FOUNDATION_OBJS) $(DATA_OBJS) tests/unit_foundation.obj
	$(LINKER) /nologo /subsystem:console /debug /out:$@ *.obj $(LIBS) $(WIN_LIBS)

datadump$(BITS).exe: clean $(FOUNDATION_OBJS) $(DATA_OBJS) tests/datadump.obj
	$(LINKER) /nologo /subsystem:console /debug /out:$@ *.obj $(LIBS)

clean:
	-@del /q *.obj *.pdb unit_foundation$(BITS).* $(NAME).* 2>nul
