$(LD) ?= $(CC)
TARGET ?= PicoDrive
DEBUG ?= 0
CFLAGS += -I$(PWD)
CYCLONE_CC ?= gcc
CYCLONE_CXX ?= g++

all: config.mak target_

ifndef NO_CONFIG_MAK
ifneq ($(wildcard config.mak),)
config.mak: ./configure
	@echo $@ is out-of-date, running configure
	@sed -n "/.*Configured with/s/[^:]*: //p" $@ | sh
include config.mak
else
config.mak:
	@echo "Please run ./configure before running make!"
	@exit 1
endif
else # NO_CONFIG_MAK
config.mak:
endif

# This is actually needed, believe me - one bit is used as a flag in some tables
# If you really have to disable this, set NO_ALIGN_FUNCTIONS elsewhere.
ifndef NO_ALIGN_FUNCTIONS
CFLAGS += -falign-functions=2
endif

# profiling
pprof ?= 0
gperf ?= 0

ifneq ("$(PLATFORM)", "libretro")
	CFLAGS += -Wall -g
ifneq ("$(PLATFORM)", "psp")
ifneq ($(findstring gcc,$(shell $(CC) -v 2>&1)),)
	CFLAGS += -ffunction-sections -fdata-sections
	LDFLAGS += -Wl,--gc-sections
endif
endif

ifeq "$(DEBUG)" "0"
	CFLAGS += -O3 -DNDEBUG
endif
	LD = $(CC)
	OBJOUT ?= -o
	LINKOUT ?= -o
endif

ifeq ("$(PLATFORM)",$(filter "$(PLATFORM)","gp2x" "opendingux" "rpi1" "trimui"))
# very small caches, avoid optimization options making the binary much bigger
CFLAGS += -finline-limit=42 -fno-unroll-loops -fno-ipa-cp -ffast-math
# this gets you about 20% better execution speed on 32bit arm/mips
CFLAGS += -fno-common -fno-stack-protector -fno-guess-branch-probability -fno-caller-saves -fno-tree-loop-if-convert -fno-regmove
endif

# default settings
use_libchdr ?= 1
ifeq "$(ARCH)" "arm"
use_cyclone ?= 1
use_drz80 ?= 1
use_sh2drc ?= 1
use_svpdrc ?= 1

asm_memory ?= 1
asm_render ?= 1
asm_ym2612 ?= 1
asm_misc ?= 1
asm_cdmemory ?= 1
asm_mix ?= 1
asm_32xdraw ?= 1
asm_32xmemory ?= 1
else
use_fame ?= 1
use_cz80 ?= 1
ifneq (,$(filter x86% i386% mips% aarch% riscv% powerpc% ppc%, $(ARCH)))
use_sh2drc ?= 1
endif
endif

-include Makefile.local

ifeq "$(PLATFORM)" "trimui"
OBJS += platform/opendingux/inputmap.o
use_inputmap ?= 1
PLATFORM := generic
endif

ifeq "$(PLATFORM)" "opendingux"

# TODO this should somehow go to the platform/opendingux directory?
.od_data: $(TARGET)
	$(RM) -rf .od_data
	cp -r platform/opendingux/data .od_data
	cp $< .od_data/PicoDrive
	$(STRIP) .od_data/PicoDrive

ifneq (,$(filter %__OPENDINGUX__, $(CFLAGS)))
# "legacy" opendingux without opk support
$(TARGET)-dge.zip: .od_data
	rm -f .od_data/default.*.desktop
	cd .od_data && zip -9 -r ../$@ *
all: $(TARGET)-dge.zip
else
$(TARGET).opk: .od_data
	rm -f .od_data/PicoDrive.dge
	mksquashfs .od_data $@ -all-root -noappend -no-exports -no-xattrs
all: $(TARGET).opk
endif

ifneq (,$(filter %__GCW0__ %__RG350__, $(CFLAGS)))
CFLAGS += -DMIPS_USE_SYNCI # mips32r2 clear_cache uses SYNCI instead of syscall
endif

OBJS += platform/opendingux/inputmap.o
use_inputmap ?= 1

# OpenDingux is a generic platform, really.
PLATFORM := generic
endif
ifeq ("$(PLATFORM)",$(filter "$(PLATFORM)","rpi1" "rpi2"))
CFLAGS += -DHAVE_GLES -DRASPBERRY
CFLAGS += -I/opt/vc/include/ -I/opt/vc/include/interface/vcos/pthreads/ -I/opt/vc/include/interface/vmcs_host/linux/
LDFLAGS += -ldl -lbcm_host -L/opt/vc/lib
# Stupid renaming occured in latest raspbian...
ifneq (,$(wildcard /opt/vc/lib/libbrcmGLESv2.so))
LDFLAGS += -lbrcmEGL -lbrcmGLESv2
else
LDFLAGS += -lEGL -lGLESv2 # on raspi GLESv1_CM is included in GLESv2
endif
OBJS += platform/linux/emu.o platform/linux/blit.o # FIXME
OBJS += platform/common/plat_sdl.o platform/common/input_sdlkbd.o
OBJS += platform/libpicofe/plat_sdl.o platform/libpicofe/in_sdl.o
OBJS += platform/libpicofe/plat_dummy.o platform/libpicofe/linux/plat.o
OBJS += platform/libpicofe/gl.o
OBJS += platform/libpicofe/gl_platform.o
USE_FRONTEND = 1
endif
ifeq "$(PLATFORM)" "generic"
# CFLAGS += -DSDL_OVERLAY_2X -DSDL_BUFFER_3X # TRIMUI
OBJS += platform/linux/emu.o platform/linux/blit.o # FIXME
ifeq "$(use_inputmap)" "1"
OBJS += platform/common/plat_sdl.o platform/opendingux/inputmap.o
else
OBJS += platform/common/plat_sdl.o platform/common/inputmap_kbd.o
endif
OBJS += platform/libpicofe/plat_sdl.o platform/libpicofe/in_sdl.o
OBJS += platform/libpicofe/plat_dummy.o platform/libpicofe/linux/plat.o
USE_FRONTEND = 1
endif
ifeq "$(PLATFORM)" "pandora"
platform/common/menu_pico.o: CFLAGS += -DPANDORA
platform/libpicofe/linux/plat.o: CFLAGS += -DPANDORA
OBJS += platform/pandora/plat.o
OBJS += platform/pandora/asm_utils.o
OBJS += platform/common/arm_utils.o 
OBJS += platform/libpicofe/linux/in_evdev.o
OBJS += platform/libpicofe/linux/fbdev.o 
OBJS += platform/libpicofe/linux/xenv.o
OBJS += platform/libpicofe/linux/plat.o
OBJS += platform/libpicofe/pandora/plat.o
USE_FRONTEND = 1
endif
ifeq "$(PLATFORM)" "gp2x"
OBJS += platform/common/arm_utils.o 
OBJS += platform/libpicofe/linux/in_evdev.o
OBJS += platform/libpicofe/linux/plat.o
OBJS += platform/libpicofe/gp2x/in_gp2x.o
OBJS += platform/libpicofe/gp2x/soc.o 
OBJS += platform/libpicofe/gp2x/soc_mmsp2.o 
OBJS += platform/libpicofe/gp2x/soc_pollux.o 
OBJS += platform/libpicofe/gp2x/plat.o 
OBJS += platform/libpicofe/gp2x/pollux_set.o 
OBJS += platform/gp2x/940ctl.o 
OBJS += platform/gp2x/plat.o 
OBJS += platform/gp2x/emu.o 
OBJS += platform/gp2x/vid_mmsp2.o 
OBJS += platform/gp2x/vid_pollux.o 
OBJS += platform/gp2x/warm.o 
USE_FRONTEND = 1
PLATFORM_MP3 = 1
PLATFORM_ZLIB = 1
endif
ifeq "$(PLATFORM)" "psp"
CFLAGS += -DUSE_BGR565 -G8 # -DLPRINTF_STDIO -DFW15
LDLIBS += -lpspgu -lpspge -lpsppower -lpspaudio -lpspdisplay -lpspaudiocodec
LDLIBS += -lpsprtc -lpspctrl -lpspsdk -lc -lpspnet_inet -lpspuser -lpspkernel
platform/common/main.o: CFLAGS += -Dmain=pico_main
OBJS += platform/psp/plat.o
OBJS += platform/psp/emu.o
OBJS += platform/psp/in_psp.o
OBJS += platform/psp/psp.o
OBJS += platform/psp/asm_utils.o
OBJS += platform/psp/mp3.o
USE_FRONTEND = 1
endif
ifeq "$(PLATFORM)" "libretro"
OBJS += platform/libretro/libretro.o
ifeq "$(USE_LIBRETRO_VFS)" "1"
OBJS += platform/libretro/libretro-common/compat/compat_posix_string.o
OBJS += platform/libretro/libretro-common/compat/compat_strl.o
OBJS += platform/libretro/libretro-common/compat/fopen_utf8.o
OBJS += platform/libretro/libretro-common/encodings/encoding_utf.o
OBJS += platform/libretro/libretro-common/streams/file_stream.o
OBJS += platform/libretro/libretro-common/streams/file_stream_transforms.o
OBJS += platform/libretro/libretro-common/vfs/vfs_implementation.o
endif
PLATFORM_ZLIB ?= 1
endif

ifeq "$(USE_FRONTEND)" "1"

# common
OBJS += platform/common/main.o platform/common/emu.o \
	platform/common/menu_pico.o platform/common/config_file.o

# libpicofe
OBJS += platform/libpicofe/input.o platform/libpicofe/readpng.o \
	platform/libpicofe/fonts.o

# libpicofe - sound
OBJS += platform/libpicofe/sndout.o
ifneq ($(findstring oss,$(SOUND_DRIVERS)),)
platform/libpicofe/sndout.o: CFLAGS += -DHAVE_OSS
OBJS += platform/libpicofe/linux/sndout_oss.o
endif
ifneq ($(findstring alsa,$(SOUND_DRIVERS)),)
platform/libpicofe/sndout.o: CFLAGS += -DHAVE_ALSA
OBJS += platform/libpicofe/linux/sndout_alsa.o
endif
ifneq ($(findstring sdl,$(SOUND_DRIVERS)),)
platform/libpicofe/sndout.o: CFLAGS += -DHAVE_SDL
OBJS += platform/libpicofe/sndout_sdl.o
endif

ifeq "$(ARCH)" "arm"
OBJS += platform/libpicofe/arm_linux.o
endif

endif # USE_FRONTEND

ifneq "$(PLATFORM)" "psp"
OBJS += platform/common/mp3.o platform/common/mp3_sync.o
ifeq "$(PLATFORM_MP3)" "1"
OBJS += platform/common/mp3_helix.o
else ifeq "$(HAVE_LIBAVCODEC)" "1"
OBJS += platform/common/mp3_libavcodec.o
else
OBJS += platform/common/mp3_minimp3.o
endif
endif

ifeq (1,$(use_libchdr))
CFLAGS += -DUSE_LIBCHDR

# chdr
CHDR = pico/cd/libchdr
CHDR_OBJS += $(CHDR)/src/libchdr_chd.o $(CHDR)/src/libchdr_cdrom.o
CHDR_OBJS += $(CHDR)/src/libchdr_flac.o
CHDR_OBJS += $(CHDR)/src/libchdr_bitstream.o $(CHDR)/src/libchdr_huffman.o

# lzma
LZMA = $(CHDR)/deps/lzma-19.00
LZMA_OBJS += $(LZMA)/src/CpuArch.o $(LZMA)/src/Alloc.o $(LZMA)/src/LzmaEnc.o
LZMA_OBJS += $(LZMA)/src/Sort.o $(LZMA)/src/LzmaDec.o $(LZMA)/src/LzFind.o
LZMA_OBJS += $(LZMA)/src/Delta.o
$(LZMA_OBJS): CFLAGS += -D_7ZIP_ST

OBJS += $(CHDR_OBJS) $(LZMA_OBJS)
# ouf... prepend includes to overload headers available in the toolchain
CHDR_I = $(shell find $(CHDR) -name 'include')
CFLAGS := $(patsubst %, -I%, $(CHDR_I)) $(CFLAGS)
endif

ifeq "$(PLATFORM_ZLIB)" "1"
# zlib
OBJS += zlib/gzio.o zlib/inffast.o zlib/inflate.o zlib/inftrees.o zlib/trees.o \
	zlib/deflate.o zlib/crc32.o zlib/adler32.o zlib/zutil.o zlib/compress.o zlib/uncompr.o
CFLAGS += -Izlib
endif
# unzip
OBJS += unzip/unzip.o


include platform/common/common.mak

OBJS += $(OBJS_COMMON)
CFLAGS += $(addprefix -D,$(DEFINES))

ifneq (,$(findstring sdl,$(OBJS)))
CFLAGS += -DUSE_SDL
endif

ifneq ($(findstring gcc,$(CC)),)
ifneq ($(findstring SunOS,$(shell uname -a)),SunOS)
ifeq ($(findstring Darwin,$(shell uname -a)),Darwin)
LDFLAGS += -Wl,-map,$(TARGET).map
else
LDFLAGS += -Wl,-Map=$(TARGET).map
endif
endif
endif

target_: $(TARGET)

clean:
	$(RM) $(TARGET) $(OBJS) pico/pico_int_offs.h
	$(RM) -r .od_data

$(TARGET): $(OBJS)

ifeq ($(STATIC_LINKING), 1)
	$(AR) rcs $@ $^
else
	$(LD) $(LINKOUT)$@ $^ $(LDFLAGS) $(LDLIBS)
endif
	$(STRIP) $@ # TRIMUI

ifeq "$(PLATFORM)" "psp"
PSPSDK ?= $(shell psp-config --pspsdk-path)
TARGET = PicoDrive
PSP_EBOOT_TITLE = PicoDrive
PSP_EBOOT_ICON = platform/psp/data/icon.png
LIBS += -lpng -lm -lz -lpspgu -lpsppower -lpspaudio -lpsprtc -lpspaudiocodec
EXTRA_TARGETS = EBOOT.PBP
include $(PSPSDK)/lib/build.mak
# TODO image generation
endif

pprof: platform/linux/pprof.c
	$(CC) $(CFLAGS) -O2 -ggdb -DPPROF -DPPROF_TOOL -I../../ -I. $^ -o $@ $(LDFLAGS) $(LDLIBS)

pico/pico_int_offs.h: tools/mkoffsets.sh
	make -C tools/ XCC="$(CC)" XCFLAGS="$(CFLAGS)" XPLATFORM="$(platform)"

%.o: %.c
	$(CC) -c $(OBJOUT)$@ $< $(CFLAGS)

.s.o:
	$(CC) $(CFLAGS) -c $< -o $@

.S.o:
	$(CC) $(CFLAGS) -c $< -o $@

# special flags - perhaps fix this someday instead?
pico/draw.o: CFLAGS += -fno-strict-aliasing
pico/draw2.o: CFLAGS += -fno-strict-aliasing
pico/mode4.o: CFLAGS += -fno-strict-aliasing
pico/cd/memory.o: CFLAGS += -fno-strict-aliasing
pico/cd/cd_file.o: CFLAGS += -fno-strict-aliasing
pico/cd/pcm.o: CFLAGS += -fno-strict-aliasing
pico/cd/LC89510.o: CFLAGS += -fno-strict-aliasing
pico/cd/gfx_cd.o: CFLAGS += -fno-strict-aliasing
ifeq (1,$(use_sh2drc))
ifneq (,$(findstring -flto,$(CFLAGS)))
# if using the DRC, memory and sh2soc directly use the DRC register for SH2 SR
# to avoid saving and reloading it. However, this collides with the use of LTO.
pico/32x/memory.o: CFLAGS += -fno-lto
pico/32x/sh2soc.o: CFLAGS += -fno-lto
endif
endif

# fame needs ~2GB of RAM to compile on gcc 4.8
# on x86, this is reduced by ~300MB when debug info is off (but not on ARM)
# not using O3 and -fno-expensive-optimizations seems to also help, but you may
# want to remove this stuff for better performance if your compiler can handle it
ifeq "$(DEBUG)" "0"
ifeq (,$(findstring msvc,$(platform)))
cpu/fame/famec.o: CFLAGS += -g0 -O2 -fno-expensive-optimizations
else
cpu/fame/famec.o: CFLAGS += -Od
endif
endif

pico/carthw_cfg.c: pico/carthw.cfg
	tools/make_carthw_c $< $@

# preprocessed asm files most probably include the offsets file
$(filter %.S,$(SRCS_COMMON)): pico/pico_int_offs.h

# random deps
pico/carthw/svp/compiler.o : cpu/drc/emit_arm.c
cpu/sh2/compiler.o : cpu/drc/emit_arm.c cpu/drc/emit_arm64.c cpu/drc/emit_ppc.c
cpu/sh2/compiler.o : cpu/drc/emit_x86.c cpu/drc/emit_mips.c cpu/drc/emit_riscv.c
cpu/sh2/mame/sh2pico.o : cpu/sh2/mame/sh2.c
pico/pico.o pico/cd/mcd.o pico/32x/32x.o : pico/pico_cmn.c pico/pico_int.h
pico/memory.o pico/cd/memory.o pico/32x/memory.o : pico/pico_int.h pico/memory.h
# pico/cart.o : pico/carthw_cfg.c
cpu/fame/famec.o: cpu/fame/famec.c cpu/fame/famec_opcodes.h
