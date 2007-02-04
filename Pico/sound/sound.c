// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include <string.h>
#include "sound.h"
#include "ym2612.h"
#include "sn76496.h"

#ifndef __GNUC__
#pragma warning (disable:4244)
#endif

#if defined(_USE_MZ80)
#include "../../cpu/mz80/mz80.h"
#elif defined(_USE_DRZ80)
#include "../../cpu/DrZ80/drz80.h"
#endif

#include "../PicoInt.h"
#include "../cd/pcm.h"
#include "mix.h"

// master int buffer to mix to
static int PsndBuffer[2*44100/50];

//int z80CycleAim = 0;

// dac
short *dac_out;
unsigned short dac_info[312]; // pppppppp ppppllll, p - pos in buff, l - length to write for this sample

// for Pico
int PsndRate=0;
int PsndLen=0; // number of mono samples, multiply by 2 for stereo
short *PsndOut=NULL; // PCM data buffer

// from ym2612.c
extern int   *ym2612_dacen;
extern INT32 *ym2612_dacout;
void YM2612TimerHandler(int c,int cnt);

// sn76496
extern int *sn76496_regs;


static void dac_recalculate()
{
  int i, dac_cnt, pos, len, lines = Pico.m.pal ? 312 : 262, mid = Pico.m.pal ? 68 : 93;

  if(PsndLen <= lines) {
    // shrinking algo
    dac_cnt = 0;//lines - PsndLen;
    len=1; pos=0;
    dac_info[225] = 1;

    for(i=226; i != 225; i++) {
      if (i >= lines) i = 0;
      len = 0;
      if(dac_cnt < 0) {
        len=1;
        pos++;
        dac_cnt += lines;
      }
      dac_cnt -= PsndLen;
      dac_info[i] = (pos<<4)|len;
    }
  } else {
    // stretching
    dac_cnt = PsndLen/2;
    pos=0;
    for(i = 225; i != 224; i++) {
      if (i >= lines) i = 0;
      len=0;
      while(dac_cnt >= 0) {
        dac_cnt -= lines;
        len++;
      }
      if (i == mid) // midpoint
        while(pos+len < PsndLen/2) {
          dac_cnt -= lines;
          len++;
        }
      dac_cnt += PsndLen;
      dac_info[i] = (pos<<4)|len;
      pos+=len;
    }
    // last sample
    for(len = 0, i = pos; i < PsndLen; i++) len++;
    dac_info[224] = (pos<<4)|len;
  }
//  dprintf("rate is %i, len %i", PsndRate, PsndLen);
//  for(i=0; i < lines; i++)
//    dprintf("%03i : %03i : %i", i, dac_info[i]>>4, dac_info[i]&0xf);
//exit(8);
}


void sound_reset()
{
  extern int z80stopCycle;
  void *ym2612_regs;

  // init even if we are not going to use them, just in case we ever enable it
  YM2612Init(Pico.m.pal ? OSC_PAL/7 : OSC_NTSC/7, PsndRate);
  // also clear the internal registers+addr line
  ym2612_regs = YM2612GetRegs();
  memset(ym2612_regs, 0, 0x200+4);

  SN76496_init(Pico.m.pal ? OSC_PAL/15 : OSC_NTSC/15, PsndRate);

  // calculate PsndLen
  PsndLen=PsndRate/(Pico.m.pal ? 50 : 60);

  // recalculate dac info
  dac_recalculate();
  z80stopCycle = 0;
}


// to be called after changing sound rate or chips
void sound_rerate()
{
  unsigned int state[28];

  // not all rates are supported in MCD mode due to mp3 decoder
  if (PicoMCD & 1) {
    if (PsndRate != 11025 && PsndRate != 22050 && PsndRate != 44100) PsndRate = 22050;
    if (!(PicoOpt & 8)) PicoOpt |= 8;
  }

  if ((PicoMCD & 1) && Pico_mcd->m.audio_track) Pico_mcd->m.audio_offset = mp3_get_offset();
  YM2612Init(Pico.m.pal ? OSC_PAL/7 : OSC_NTSC/7, PsndRate);
  // feed it back it's own registers, just like after loading state
  YM2612PicoStateLoad();
  if ((PicoMCD & 1) && Pico_mcd->m.audio_track)
    mp3_start_play(Pico_mcd->TOC.Tracks[Pico_mcd->m.audio_track].F, Pico_mcd->m.audio_offset);

  memcpy(state, sn76496_regs, 28*4); // remember old state
  SN76496_init(Pico.m.pal ? OSC_PAL/15 : OSC_NTSC/15, PsndRate);
  memcpy(sn76496_regs, state, 28*4); // restore old state

  // calculate PsndLen
  PsndLen=PsndRate/(Pico.m.pal ? 50 : 60);

  // recalculate dac info
  dac_recalculate();

  if (PicoMCD & 1)
    pcm_set_rate(PsndRate);
}


// This is called once per raster (aka line), but not necessarily for every line
void sound_timers_and_dac(int raster)
{
  int pos, len;
  int do_dac = PsndOut && (PicoOpt&1) && *ym2612_dacen;
//  int do_pcm = PsndOut && (PicoMCD&1) && (PicoOpt&0x400);

  // Our raster lasts 63.61323/64.102564 microseconds (NTSC/PAL)
  YM2612PicoTick(1);

  if (!do_dac /*&& !do_pcm*/) return;

  pos=dac_info[raster], len=pos&0xf;
  if (!len) return;

  pos>>=4;

  if (do_dac) {
    short *d = PsndOut + pos*2;
    int dout = *ym2612_dacout;
    if(PicoOpt&8) {
      // some manual loop unrolling here :)
      d[0] = dout;
      if (len > 1) {
        d[2] = dout;
        if (len > 2) {
          d[4] = dout;
          if (len > 3)
            d[6] = dout;
        }
      }
    } else {
      short *d = PsndOut + pos;
      d[0] = dout;
      if (len > 1) {
        d[1] = dout;
        if (len > 2) {
          d[2] = dout;
          if (len > 3)
            d[3] = dout;
        }
      }
    }
  }

#if 0
  if (do_pcm) {
    int *d = PsndBuffer;
    d += (PicoOpt&8) ? pos*2 : pos;
    pcm_update(d, len, 1);
  }
#endif
}


void sound_clear(void)
{
  if (PicoOpt & 8) memset32((int *) PsndOut, 0, PsndLen);
  else memset(PsndOut, 0, PsndLen<<1);
//  memset(PsndBuffer, 0, (PicoOpt & 8) ? (PsndLen<<3) : (PsndLen<<2));
}


int sound_render(int offset, int length)
{
  int *buf32 = PsndBuffer+offset;
  int stereo = (PicoOpt & 8) >> 3;
  // emulating CD && PCM option enabled && PCM chip on && have enabled channels
  int do_pcm = (PicoMCD&1) && (PicoOpt&0x400) && (Pico_mcd->pcm.control & 0x80) && Pico_mcd->pcm.enabled;
  offset <<= stereo;

  // PSG
  if (PicoOpt & 2)
    SN76496Update(PsndOut+offset, length, stereo);

  // Add in the stereo FM buffer
  if (PicoOpt & 1)
    YM2612UpdateOne(buf32, length, stereo, 1);

  if (do_pcm)
    pcm_update(buf32, length, stereo);

  // CDDA audio
//  if ((PicoMCD & 1) && (PicoOpt & 0x800))
//    mp3_update(PsndBuffer+offset, length, stereo);

  // convert + limit to normal 16bit output
  if (stereo)
       mix_32_to_16l_stereo(PsndOut+offset, buf32, length);
  else mix_32_to_16_mono   (PsndOut+offset, buf32, length);

  return 0;
}



#if defined(_USE_MZ80)

// memhandlers for mz80 core
unsigned char mz80_read(UINT32 a,  struct MemoryReadByte *w)  { return z80_read(a); }
void mz80_write(UINT32 a, UINT8 d, struct MemoryWriteByte *w) { z80_write(d, a); }

// structures for mz80 core
static struct MemoryReadByte mz80_mem_read[]=
{
  {0x0000,0xffff,mz80_read},
  {(UINT32) -1,(UINT32) -1,NULL}
};
static struct MemoryWriteByte mz80_mem_write[]=
{
  {0x0000,0xffff,mz80_write},
  {(UINT32) -1,(UINT32) -1,NULL}
};
static struct z80PortRead mz80_io_read[] ={
  {(UINT16) -1,(UINT16) -1,NULL}
};
static struct z80PortWrite mz80_io_write[]={
  {(UINT16) -1,(UINT16) -1,NULL}
};

#elif defined(_USE_DRZ80)

static struct DrZ80 drZ80;

static unsigned int DrZ80_rebasePC(unsigned short a)
{
  drZ80.Z80PC_BASE = (unsigned int) Pico.zram;
  return drZ80.Z80PC_BASE + a;
}

static unsigned int DrZ80_rebaseSP(unsigned short a)
{
  drZ80.Z80SP_BASE = (unsigned int) Pico.zram;
  return drZ80.Z80SP_BASE + a;
}

static unsigned char DrZ80_in(unsigned short p)
{
  return 0xff;
}

static void DrZ80_out(unsigned short p,unsigned char d)
{
}

static void DrZ80_irq_callback()
{
  drZ80.Z80_IRQ = 0; // lower irq when accepted
}

#endif

// z80 functionality wrappers
void z80_init()
{
#if defined(_USE_MZ80)
  struct mz80context z80;

  // z80
  mz80init();
  // Modify the default context
  mz80GetContext(&z80);

  // point mz80 stuff
  z80.z80Base=Pico.zram;
  z80.z80MemRead=mz80_mem_read;
  z80.z80MemWrite=mz80_mem_write;
  z80.z80IoRead=mz80_io_read;
  z80.z80IoWrite=mz80_io_write;

  mz80SetContext(&z80);

#elif defined(_USE_DRZ80)

  memset(&drZ80, 0, sizeof(struct DrZ80));
  drZ80.z80_rebasePC=DrZ80_rebasePC;
  drZ80.z80_rebaseSP=DrZ80_rebaseSP;
  drZ80.z80_read8   =z80_read;
  drZ80.z80_read16  =z80_read16;
  drZ80.z80_write8  =z80_write;
  drZ80.z80_write16 =z80_write16;
  drZ80.z80_in      =DrZ80_in;
  drZ80.z80_out     =DrZ80_out;
  drZ80.z80_irq_callback=DrZ80_irq_callback;
#endif
}

void z80_reset()
{
#if defined(_USE_MZ80)
  mz80reset();
#elif defined(_USE_DRZ80)
  memset(&drZ80, 0, 0x54);
  drZ80.Z80F  = (1<<2);  // set ZFlag
  drZ80.Z80F2 = (1<<2);  // set ZFlag
  drZ80.Z80IX = 0xFFFF << 16;
  drZ80.Z80IY = 0xFFFF << 16;
  drZ80.Z80IM = 0; // 1?
  drZ80.Z80PC = drZ80.z80_rebasePC(0);
  drZ80.Z80SP = drZ80.z80_rebaseSP(0x2000); // 0xf000 ?
#endif
  Pico.m.z80_fakeval = 0; // for faking when Z80 is disabled
}

void z80_resetCycles()
{
#if defined(_USE_MZ80)
  mz80GetElapsedTicks(1);
#endif
}

void z80_int()
{
#if defined(_USE_MZ80)
  mz80int(0);
#elif defined(_USE_DRZ80)
  drZ80.z80irqvector = 0xFF; // default IRQ vector RST opcode
  drZ80.Z80_IRQ = 1;
#endif
}

// returns number of cycles actually executed
int z80_run(int cycles)
{
#if defined(_USE_MZ80)
  int ticks_pre = mz80GetElapsedTicks(0);
  mz80exec(cycles);
  return mz80GetElapsedTicks(0) - ticks_pre;
#elif defined(_USE_DRZ80)
  return cycles - DrZ80Run(&drZ80, cycles);
#else
  return cycles;
#endif
}

void z80_pack(unsigned char *data)
{
#if defined(_USE_MZ80)
  struct mz80context mz80;
  *(int *)data = 0x00005A6D; // "mZ"
  mz80GetContext(&mz80);
  memcpy(data+4, &mz80.z80clockticks, sizeof(mz80)-5*4); // don't save base&memhandlers
#elif defined(_USE_DRZ80)
  *(int *)data = 0x015A7244; // "DrZ" v1
  drZ80.Z80PC = drZ80.z80_rebasePC(drZ80.Z80PC-drZ80.Z80PC_BASE);
  drZ80.Z80SP = drZ80.z80_rebaseSP(drZ80.Z80SP-drZ80.Z80SP_BASE);
  memcpy(data+4, &drZ80, 0x54);
#endif
}

void z80_unpack(unsigned char *data)
{
#if defined(_USE_MZ80)
  if(*(int *)data == 0x00005A6D) { // "mZ" save?
    struct mz80context mz80;
    mz80GetContext(&mz80);
    memcpy(&mz80.z80clockticks, data+4, sizeof(mz80)-5*4);
    mz80SetContext(&mz80);
  } else {
    z80_reset();
    z80_int();
  }
#elif defined(_USE_DRZ80)
  if(*(int *)data == 0x015A7244) { // "DrZ" v1 save?
    memcpy(&drZ80, data+4, 0x54);
    // update bases
    drZ80.Z80PC = drZ80.z80_rebasePC(drZ80.Z80PC-drZ80.Z80PC_BASE);
    drZ80.Z80SP = drZ80.z80_rebaseSP(drZ80.Z80SP-drZ80.Z80SP_BASE);
  } else {
    z80_reset();
    drZ80.Z80IM = 1;
    z80_int(); // try to goto int handler, maybe we won't execute trash there?
  }
#endif
}

void z80_exit()
{
#if defined(_USE_MZ80)
  mz80shutdown();
#endif
}

#if defined(__DEBUG_PRINT) || defined(WIN32)
void z80_debug(char *dstr)
{
#if defined(_USE_DRZ80)
  sprintf(dstr, "%sZ80 state: PC: %04x SP: %04x\n", dstr, drZ80.Z80PC-drZ80.Z80PC_BASE, drZ80.Z80SP-drZ80.Z80SP_BASE);
#endif
}
#endif