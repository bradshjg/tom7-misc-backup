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

/// \file
/// \brief This file contains all code for coordinating the mapping in of
/// the address space external to the NES.

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "types.h"
#include "fceu.h"
#include "ppu.h"
#include "driver.h"

#include "cart.h"
#include "x6502.h"

#include "file.h"
#include "utils/memory.h"

#include "tracing.h"

#define DEBUG_BANKSWITCH false

Cart::Cart(FC *fc) : fc(fc) {}

void Cart::SetPagePtr(int s, uint32 A, uint8 *p, bool is_ram) {
  const uint32 AB = A >> 11;
  if (DEBUG_BANKSWITCH) printf("setpageptr %d %04x %p %s   AB: %d\n",
			       s, A, p, is_ram ? "RAM" : "not RAM", AB);

  if (p != nullptr) {
    for (int x = 0; x < (s >> 1); x++) {
      PRGIsRAM[AB + x] = is_ram;
      // The subtraction here is so that we can still access the
      // paged memory location by subscripting [A] rather than
      // having to do [A & 2047]. -tom7
      Page[AB + x] = p - A;
    }
  } else {
    for (int x = 0; x < (s >> 1); x++) {
      PRGIsRAM[AB + x] = 0;
      Page[AB + x] = 0;
    }
  }
}

// WTF is this? nothing - x*2048 is a pointer before nothing, like
// into some rando spot in BSS. If it was plus x*2048, then we'd
// need 65k for all the pointers to be in range (32 * 2048). Something
// seems seriously amiss. Are these always just nonsense pointers that
// are overwritten? Or maybe it relies on it pointing to some 0s in
// BSS?
//
// Note that this idiom (-x * 0x400) appears in other code, like at
// the bottom of mappers/6.cc...
//   -tom7
void Cart::ResetCartMapping() {
  for (int x = 0; x < 32; x++) {
    Page[x] = nothing - x * 2048;
    PRGptr[x] = CHRptr[x] = nullptr;
    PRGsize[x] = CHRsize[x] = 0;
  }
  for (int x = 0; x < 8; x++) {
    VPage[x] = nothing - x * 0x400;
  }
}

void Cart::SetupCartPRGMapping(int chip, uint8 *p, uint32 size, bool is_ram) {
  if (DEBUG_BANKSWITCH)
    printf("Setup PRG chip %d -> %p (size %d, %s)\n", chip, p, size,
	   is_ram ? "RAM" : "not RAM");
  PRGptr[chip] = p;
  PRGsize[chip] = size;

  PRGmask2[chip] = (size >> 11) - 1;
  PRGmask4[chip] = (size >> 12) - 1;
  PRGmask8[chip] = (size >> 13) - 1;
  PRGmask16[chip] = (size >> 14) - 1;
  PRGmask32[chip] = (size >> 15) - 1;

  PRGram[chip] = is_ram;
}

void Cart::SetupCartCHRMapping(int chip, uint8 *p, uint32 size, bool is_ram) {
  if (DEBUG_BANKSWITCH)
    printf("CHR chip %d -> %p (size %d, %s)\n", chip, p, size,
	   is_ram ? "RAM" : "not RAM");

  CHRptr[chip] = p;
  CHRsize[chip] = size;

  CHRmask1[chip] = (size >> 10) - 1;
  CHRmask2[chip] = (size >> 11) - 1;
  CHRmask4[chip] = (size >> 12) - 1;
  CHRmask8[chip] = (size >> 13) - 1;

  CHRram[chip] = is_ram;
}

// static
DECLFR_RET Cart::CartBR(DECLFR_ARGS) {
  return fc->cart->CartBR_Direct(DECLFR_FORWARD);
}

DECLFR_RET Cart::CartBR_Direct(DECLFR_ARGS) {
  // printf("Read A=%x so Page %d = %p\n", A, A >> 11, Page[A >> 11]);
  // XXX: A bit disturbing that CartBW and BROB check that Page is
  // non-null, but here we just assume it's good? Maybe this is
  // the point of using BROB (where OB means out of bounds?)?
  return Page[A >> 11][A];
}

// static
DECLFW_RET Cart::CartBW(DECLFW_ARGS) {
  return fc->cart->CartBW_Direct(DECLFW_FORWARD);
}

DECLFW_RET Cart::CartBW_Direct(DECLFW_ARGS) {
  // printf("Ok: %04x:%02x, %d\n",A,V,PRGIsRAM[A>>11]);
  if (PRGIsRAM[A >> 11] && Page[A >> 11]) Page[A >> 11][A] = V;
}

// static
DECLFR_RET Cart::CartBROB(DECLFR_ARGS) {
  return fc->cart->CartBROB_Direct(DECLFR_FORWARD);
}

DECLFR_RET Cart::CartBROB_Direct(DECLFR_ARGS) {
  if (!Page[A >> 11]) return fc->X->DB;
  return Page[A >> 11][A];
}

void Cart::setprg2r(int r, unsigned int A, unsigned int V) {
  V &= PRGmask2[r];
  SetPagePtr(2, A, PRGptr[r] ? &PRGptr[r][V << 11] : 0, PRGram[r]);
}

void Cart::setprg2(uint32 A, uint32 V) {
  setprg2r(0, A, V);
}

void Cart::setprg4r(int r, unsigned int A, unsigned int V) {
  V &= PRGmask4[r];
  SetPagePtr(4, A, PRGptr[r] ? &PRGptr[r][V << 12] : 0, PRGram[r]);
}

void Cart::setprg4(uint32 A, uint32 V) {
  setprg4r(0, A, V);
}

// Ah, these functions may be testing whether the chip is large
// enough to be mapped directly. See how prg8r wants at least an 8k
// chip, or otherwise breaks it into 2k chunks? (Perhaps effectively
// masking off high bits of the address?)
// Similarly 16k and 32k for prg16 and prg32. -tom7
// But why doesn't prg4 not need to do this? Because chips are
// always at least 8k?
void Cart::setprg8r(int r, unsigned int A, unsigned int V) {
  if (DEBUG_BANKSWITCH) printf("setprg8r r=%d A=%x V=%u %s\n", r, A, V,
			       PRGsize[r] >= 8192 ? " (big)" : " (small)");
  if (PRGsize[r] >= 8192) {
    V &= PRGmask8[r];
    SetPagePtr(8, A, PRGptr[r] ? &PRGptr[r][V << 13] : 0, PRGram[r]);
  } else {
    const uint32 VA = V << 2;
    for (int x = 0; x < 4; x++)
      SetPagePtr(2, A + (x << 11),
                 PRGptr[r] ? (&PRGptr[r][((VA + x) & PRGmask2[r]) << 11]) : 0,
                 PRGram[r]);
  }
}

void Cart::setprg8(uint32 A, uint32 V) {
  setprg8r(0, A, V);
}

void Cart::setprg16r(int r, unsigned int A, unsigned int V) {
  if (DEBUG_BANKSWITCH) printf("setprg16r r=%d A=%x V=%u %s\n", r, A, V,
			       PRGsize[r] >= 16384 ? " (big)" : " (small)");
  if (PRGsize[r] >= 16384) {
    V &= PRGmask16[r];
    SetPagePtr(16, A, PRGptr[r] ? &PRGptr[r][V << 14] : 0, PRGram[r]);
  } else {
    const uint32 VA = V << 3;

    for (int x = 0; x < 8; x++)
      SetPagePtr(2, A + (x << 11),
                 PRGptr[r] ? (&PRGptr[r][((VA + x) & PRGmask2[r]) << 11]) : 0,
                 PRGram[r]);
  }
}

void Cart::setprg16(uint32 A, uint32 V) {
  setprg16r(0, A, V);
}

void Cart::setprg32r(int r, unsigned int A, unsigned int V) {
  if (PRGsize[r] >= 32768) {
    V &= PRGmask32[r];
    SetPagePtr(32, A, PRGptr[r] ? (&PRGptr[r][V << 15]) : 0, PRGram[r]);
  } else {
    const uint32 VA = V << 4;

    for (int x = 0; x < 16; x++)
      SetPagePtr(2, A + (x << 11),
                 PRGptr[r] ? (&PRGptr[r][((VA + x) & PRGmask2[r]) << 11]) : 0,
                 PRGram[r]);
  }
}

void Cart::setprg32(uint32 A, uint32 V) {
  setprg32r(0, A, V);
}

void Cart::setchr1r(int r, unsigned int A, unsigned int V) {
  if (!CHRptr[r]) return;
  fc->ppu->LineUpdate();
  V &= CHRmask1[r];
  if (CHRram[r])
    fc->ppu->PPUCHRRAM |= (1 << (A >> 10));
  else
    fc->ppu->PPUCHRRAM &= ~(1 << (A >> 10));
  VPage[A >> 10] = &CHRptr[r][V << 10] - A;
}

void Cart::setchr2r(int r, unsigned int A, unsigned int V) {
  if (!CHRptr[r]) return;
  fc->ppu->LineUpdate();
  V &= CHRmask2[r];
  VPage[A >> 10] = VPage[(A >> 10) + 1] = &CHRptr[r][V << 11] - A;
  if (CHRram[r])
    fc->ppu->PPUCHRRAM |= (3 << (A >> 10));
  else
    fc->ppu->PPUCHRRAM &= ~(3 << (A >> 10));
}

void Cart::setchr4r(int r, unsigned int A, unsigned int V) {
  if (!CHRptr[r]) return;
  fc->ppu->LineUpdate();
  V &= CHRmask4[r];
  VPage[A >> 10] = VPage[(A >> 10) + 1] = VPage[(A >> 10) + 2] =
      VPage[(A >> 10) + 3] = &CHRptr[r][V << 12] - A;
  if (CHRram[r])
    fc->ppu->PPUCHRRAM |= (15 << (A >> 10));
  else
    fc->ppu->PPUCHRRAM &= ~(15 << (A >> 10));
}

void Cart::setchr8r(int r, unsigned int V) {
  if (!CHRptr[r]) return;
  fc->ppu->LineUpdate();
  V &= CHRmask8[r];
  for (int x = 7; x >= 0; x--) VPage[x] = &CHRptr[r][V << 13];
  if (CHRram[r])
    fc->ppu->PPUCHRRAM |= (255);
  else
    fc->ppu->PPUCHRRAM = 0;
}

void Cart::setchr1(unsigned int A, unsigned int V) {
  setchr1r(0, A, V);
}

void Cart::setchr2(unsigned int A, unsigned int V) {
  setchr2r(0, A, V);
}

void Cart::setchr4(unsigned int A, unsigned int V) {
  setchr4r(0, A, V);
}

void Cart::setchr8(unsigned int V) {
  setchr8r(0, V);
}

void Cart::setvram8(uint8 *p) {
  for (int x = 7; x >= 0; x--) VPage[x] = p;
  fc->ppu->PPUCHRRAM |= 255;
}

void Cart::setvram4(uint32 A, uint8 *p) {
  for (int x = 3; x >= 0; x--) VPage[(A >> 10) + x] = p - A;
  fc->ppu->PPUCHRRAM |= (15 << (A >> 10));
}

void Cart::setvramb1(uint8 *p, uint32 A, uint32 b) {
  fc->ppu->LineUpdate();
  VPage[A >> 10] = p - A + (b << 10);
  fc->ppu->PPUCHRRAM |= (1 << (A >> 10));
}

void Cart::setvramb2(uint8 *p, uint32 A, uint32 b) {
  fc->ppu->LineUpdate();
  VPage[(A >> 10)] = VPage[(A >> 10) + 1] = p - A + (b << 11);
  fc->ppu->PPUCHRRAM |= (3 << (A >> 10));
}

void Cart::setvramb4(uint8 *p, uint32 A, uint32 b) {
  fc->ppu->LineUpdate();
  for (int x = 3; x >= 0; x--) VPage[(A >> 10) + x] = p - A + (b << 12);
  fc->ppu->PPUCHRRAM |= (15 << (A >> 10));
}

void Cart::setvramb8(uint8 *p, uint32 b) {
  fc->ppu->LineUpdate();
  for (int x = 7; x >= 0; x--) VPage[x] = p + (b << 13);
  fc->ppu->PPUCHRRAM |= 255;
}

/* This function can be called without calling SetupCartMirroring(). */

void Cart::setntamem(uint8 *p, int ram, uint32 b) {
  fc->ppu->LineUpdate();
  fc->ppu->vnapage[b] = p;
  fc->ppu->PPUNTARAM &= ~(1 << b);
  if (ram) fc->ppu->PPUNTARAM |= 1 << b;
}

void Cart::setmirrorw(int a, int b, int c, int d) {
  fc->ppu->LineUpdate();
  fc->ppu->vnapage[0] = fc->ppu->NTARAM + a * 0x400;
  fc->ppu->vnapage[1] = fc->ppu->NTARAM + b * 0x400;
  fc->ppu->vnapage[2] = fc->ppu->NTARAM + c * 0x400;
  fc->ppu->vnapage[3] = fc->ppu->NTARAM + d * 0x400;
}

void Cart::setmirror(int t) {
  fc->ppu->LineUpdate();
  if (!mirrorhard) {
    switch (t) {
    case MI_H:
      fc->ppu->vnapage[0] = fc->ppu->vnapage[1] = fc->ppu->NTARAM;
      fc->ppu->vnapage[2] = fc->ppu->vnapage[3] = fc->ppu->NTARAM + 0x400;
      break;
    case MI_V:
      fc->ppu->vnapage[0] = fc->ppu->vnapage[2] = fc->ppu->NTARAM;
      fc->ppu->vnapage[1] = fc->ppu->vnapage[3] = fc->ppu->NTARAM + 0x400;
      break;
    case MI_0:
      fc->ppu->vnapage[0] = fc->ppu->vnapage[1] = fc->ppu->vnapage[2] =
        fc->ppu->vnapage[3] = fc->ppu->NTARAM;
      break;
    case MI_1:
      fc->ppu->vnapage[0] = fc->ppu->vnapage[1] = fc->ppu->vnapage[2] =
        fc->ppu->vnapage[3] = fc->ppu->NTARAM + 0x400;
      break;
    }
    fc->ppu->PPUNTARAM = 0xF;
  }
}

void Cart::SetupCartMirroring(int m, int hard, uint8 *extra) {
  if (m < 4) {
    mirrorhard = 0;
    setmirror(m);
  } else {
    fc->ppu->vnapage[0] = fc->ppu->NTARAM;
    fc->ppu->vnapage[1] = fc->ppu->NTARAM + 0x400;
    fc->ppu->vnapage[2] = extra;
    fc->ppu->vnapage[3] = extra + 0x400;
    fc->ppu->PPUNTARAM = 0xF;
  }
  mirrorhard = hard;
}

void Cart::FCEU_SaveGameSave(CartInfo *LocalHWInfo) {
  // XXX TODO: Make this part of the savestate system (if it's not,
  // already). Don't write to disk.
  // fprintf(stderr, "CART Tried to save game state. Blocked.\n");
  return;
  if (LocalHWInfo->battery && LocalHWInfo->SaveGame[0]) {
    FILE *sp;

    std::string f = FCEU_MakeSaveFilename();
    if ((sp = FCEUD_UTF8fopen(f.c_str(), "wb")) == nullptr) {
      FCEU_PrintError("WRAM file \"%s\" cannot be written to.\n", f.c_str());
    } else {
      for (int x = 0; x < 4; x++) {
        if (LocalHWInfo->SaveGame[x]) {
          fwrite(LocalHWInfo->SaveGame[x], 1, LocalHWInfo->SaveGameLen[x], sp);
        }
      }
    }
    // XXX note that it doesn't even close the file? wft?
  }
}

void Cart::FCEU_LoadGameSave(CartInfo *LocalHWInfo) {
  FCEU_ClearGameSave(LocalHWInfo);
  // fprintf(stderr, "Blocked cart from loading save game state.\n");
  return;
  TRACEF("LoadSaveGame");
  if (LocalHWInfo->battery && LocalHWInfo->SaveGame[0] &&
      !disableBatteryLoading) {
    std::string f = FCEU_MakeSaveFilename();
    TRACEF("Save file %s", f.c_str());
    if (FILE *sp = FCEUD_UTF8fopen(f.c_str(), "rb")) {
      for (int x = 0; x < 4; x++) {
        if (LocalHWInfo->SaveGame[x]) {
          TRACEF("Doing it");

		  if (LocalHWInfo->SaveGameLen[x] !=
			  fread(LocalHWInfo->SaveGame[x], 1,
					LocalHWInfo->SaveGameLen[x], sp)) {
			// XXX should handle error...
			return;
		  }
        }
      }
    }
    // XXX note that it doesn't even close the file? wft?
  }
}

// clears all save memory. call this if you want to pretend the
// saveram has been reset (it doesnt touch what is on disk though)
void Cart::FCEU_ClearGameSave(CartInfo *LocalHWInfo) {
  if (LocalHWInfo->battery && LocalHWInfo->SaveGame[0]) {
    for (int x = 0; x < 4; x++) {
      if (LocalHWInfo->SaveGame[x]) {
        memset(LocalHWInfo->SaveGame[x], 0, LocalHWInfo->SaveGameLen[x]);
      }
    }
  }
}
