/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2002 Xodnizel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _SOUND_H_
#define _SOUND_H_

#include "state.h"

struct EXPSOUND {
  void (*Fill)(int Count);	/* Low quality ext sound. */
  
  /* NeoFill is for sound devices that are emulated in a more
     high-level manner(VRC7) in HQ mode.  Interestingly,
     this device has slightly better sound quality(updated more
     often) in lq mode than in high-quality mode.  Maybe that
     should be fixed. :)
  */
  void (*NeoFill)(int32 *Wave, int Count);
  void (*HiFill)();
  void (*HiSync)(int32 ts);

  void (*RChange)();
  void (*Kill)();
};

extern EXPSOUND GameExpSound;

extern int32 nesincsize;

void SetSoundVariables();

int GetSoundBuffer(int32 **W);
int FlushEmulateSound();
extern int32 Wave[2048+512];
extern int32 WaveFinal[2048+512];
extern int32 WaveHi[];
extern uint32 soundtsinc;

extern uint32 soundtsoffs;
#define SOUNDTS (timestamp + soundtsoffs)

void FCEUSND_Power();
void FCEUSND_Reset();
void FCEUSND_SaveState();
void FCEUSND_LoadState(int version);

void FCEU_SoundCPUHook(int);

struct ENVUNIT {
  uint8 Speed;
  uint8 Mode;	/* Fixed volume(1), and loop(2) */
  uint8 DecCountTo1;
  uint8 decvolume;
  int reloaddec;
};

extern const SFORMAT FCEUSND_STATEINFO[];

#endif
