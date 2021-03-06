/* FCE Ultra - NES/Famicom Emulator
*
* Copyright notice for this file:
*  Copyright (C) 1998 BERO
*  Copyright (C) 2003 Xodnizel
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

// Tom 7's notes. See this page for an ok explanation:
// http://wiki.nesdev.com/w/index.php/PPU_OAM
// Note sprites are drawn from the end of the array to the beginning.

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <tuple>
#include <utility>
#include <string>
#include <typeinfo>  // Only necessary for CHECKs
#include <typeindex> // Ditto.

#include "types.h"
#include "x6502.h"
#include "fceu.h"
#include "ppu.h"
#include "sound.h"
#include "file.h"
#include "utils/endian.h"
#include "utils/memory.h"
#include "boards/mmc5.h"

#include "cart.h"
#include "palette.h"
#include "state.h"
#include "input.h"
#include "driver.h"
#include "fsettings.h"

// XXX better way of doing this...
#if ENABLE_AOT
// XXX get from some generated header...
extern void contra_Run(FC *fc, int32 cycles);
#define Run6502(c) contra_Run(fc, c)
#else
#define Run6502(c) fc->X->Run(c)
#endif

#define DEBUGF if (0) fprintf
// #define DCHECK if (0)

#define VBlankON  (PPU_values[0]&0x80)   //Generate VBlank NMI
#define Sprite16  (PPU_values[0]&0x20)   //Sprites 8x16/8x8
// #define BGAdrHI   (PPU_values[0]&0x10)   //BG pattern adr $0000/$1000
// #define SpAdrHI   (PPU_values[0]&0x08)   //Sprite pattern adr $0000/$1000
#define INC32     (PPU_values[0]&0x04)   //auto increment 1/32

#define SpriteON  (PPU_values[1]&0x10)   //Show Sprite
#define ScreenON  (PPU_values[1]&0x08)   //Show screen
#define PPUON    (PPU_values[1]&0x18)    //PPU should operate
#define GRAYSCALE (PPU_values[1]&0x01)   //Grayscale (AND palette entries with 0x30)

#define SpriteLeft8 (PPU_values[1]&0x04)
#define BGLeft8 (PPU_values[1]&0x02)

#define PPU_status (PPU_values[2])

// When BG is turned off, this is the fill color.
// 0xFF shall indicate to use palette[0]
static constexpr uint8 gNoBGFillColor = 0xFF;

// These used to be options that could be controlled through the UI
// but that's probably not a good idea for general purposes. Saved as
// constants in case it becomes useful to make them settable in the
// future (e.g. automapping).
static constexpr bool rendersprites = true;
static constexpr bool renderbg = true;

std::string attrbits(uint8 b) {
  std::string buf = "01234567";
  for (int i = 0; i < 8; i ++) {
    buf[7 - i] = (b & (1 << i))? "VHB???11"[7 - i] : "__F___00"[7 - i];
  }
  return buf;
}

static constexpr uint32 ppulut1[256] = {
0x00000000, 0x10000000, 0x01000000, 0x11000000, 0x00100000, 0x10100000,
0x01100000, 0x11100000, 0x00010000, 0x10010000, 0x01010000, 0x11010000,
0x00110000, 0x10110000, 0x01110000, 0x11110000, 0x00001000, 0x10001000,
0x01001000, 0x11001000, 0x00101000, 0x10101000, 0x01101000, 0x11101000,
0x00011000, 0x10011000, 0x01011000, 0x11011000, 0x00111000, 0x10111000,
0x01111000, 0x11111000, 0x00000100, 0x10000100, 0x01000100, 0x11000100,
0x00100100, 0x10100100, 0x01100100, 0x11100100, 0x00010100, 0x10010100,
0x01010100, 0x11010100, 0x00110100, 0x10110100, 0x01110100, 0x11110100,
0x00001100, 0x10001100, 0x01001100, 0x11001100, 0x00101100, 0x10101100,
0x01101100, 0x11101100, 0x00011100, 0x10011100, 0x01011100, 0x11011100,
0x00111100, 0x10111100, 0x01111100, 0x11111100, 0x00000010, 0x10000010,
0x01000010, 0x11000010, 0x00100010, 0x10100010, 0x01100010, 0x11100010,
0x00010010, 0x10010010, 0x01010010, 0x11010010, 0x00110010, 0x10110010,
0x01110010, 0x11110010, 0x00001010, 0x10001010, 0x01001010, 0x11001010,
0x00101010, 0x10101010, 0x01101010, 0x11101010, 0x00011010, 0x10011010,
0x01011010, 0x11011010, 0x00111010, 0x10111010, 0x01111010, 0x11111010,
0x00000110, 0x10000110, 0x01000110, 0x11000110, 0x00100110, 0x10100110,
0x01100110, 0x11100110, 0x00010110, 0x10010110, 0x01010110, 0x11010110,
0x00110110, 0x10110110, 0x01110110, 0x11110110, 0x00001110, 0x10001110,
0x01001110, 0x11001110, 0x00101110, 0x10101110, 0x01101110, 0x11101110,
0x00011110, 0x10011110, 0x01011110, 0x11011110, 0x00111110, 0x10111110,
0x01111110, 0x11111110, 0x00000001, 0x10000001, 0x01000001, 0x11000001,
0x00100001, 0x10100001, 0x01100001, 0x11100001, 0x00010001, 0x10010001,
0x01010001, 0x11010001, 0x00110001, 0x10110001, 0x01110001, 0x11110001,
0x00001001, 0x10001001, 0x01001001, 0x11001001, 0x00101001, 0x10101001,
0x01101001, 0x11101001, 0x00011001, 0x10011001, 0x01011001, 0x11011001,
0x00111001, 0x10111001, 0x01111001, 0x11111001, 0x00000101, 0x10000101,
0x01000101, 0x11000101, 0x00100101, 0x10100101, 0x01100101, 0x11100101,
0x00010101, 0x10010101, 0x01010101, 0x11010101, 0x00110101, 0x10110101,
0x01110101, 0x11110101, 0x00001101, 0x10001101, 0x01001101, 0x11001101,
0x00101101, 0x10101101, 0x01101101, 0x11101101, 0x00011101, 0x10011101,
0x01011101, 0x11011101, 0x00111101, 0x10111101, 0x01111101, 0x11111101,
0x00000011, 0x10000011, 0x01000011, 0x11000011, 0x00100011, 0x10100011,
0x01100011, 0x11100011, 0x00010011, 0x10010011, 0x01010011, 0x11010011,
0x00110011, 0x10110011, 0x01110011, 0x11110011, 0x00001011, 0x10001011,
0x01001011, 0x11001011, 0x00101011, 0x10101011, 0x01101011, 0x11101011,
0x00011011, 0x10011011, 0x01011011, 0x11011011, 0x00111011, 0x10111011,
0x01111011, 0x11111011, 0x00000111, 0x10000111, 0x01000111, 0x11000111,
0x00100111, 0x10100111, 0x01100111, 0x11100111, 0x00010111, 0x10010111,
0x01010111, 0x11010111, 0x00110111, 0x10110111, 0x01110111, 0x11110111,
0x00001111, 0x10001111, 0x01001111, 0x11001111, 0x00101111, 0x10101111,
0x01101111, 0x11101111, 0x00011111, 0x10011111, 0x01011111, 0x11011111,
0x00111111, 0x10111111, 0x01111111, 0x11111111, };

// PERF: This is just the values in ppulut1 shifted up by one bit.
// Is it faster to just shift wherever this is used, and reduce
// the cache load?
static constexpr uint32 ppulut2[256] = {
0x00000000, 0x20000000, 0x02000000, 0x22000000, 0x00200000, 0x20200000,
0x02200000, 0x22200000, 0x00020000, 0x20020000, 0x02020000, 0x22020000,
0x00220000, 0x20220000, 0x02220000, 0x22220000, 0x00002000, 0x20002000,
0x02002000, 0x22002000, 0x00202000, 0x20202000, 0x02202000, 0x22202000,
0x00022000, 0x20022000, 0x02022000, 0x22022000, 0x00222000, 0x20222000,
0x02222000, 0x22222000, 0x00000200, 0x20000200, 0x02000200, 0x22000200,
0x00200200, 0x20200200, 0x02200200, 0x22200200, 0x00020200, 0x20020200,
0x02020200, 0x22020200, 0x00220200, 0x20220200, 0x02220200, 0x22220200,
0x00002200, 0x20002200, 0x02002200, 0x22002200, 0x00202200, 0x20202200,
0x02202200, 0x22202200, 0x00022200, 0x20022200, 0x02022200, 0x22022200,
0x00222200, 0x20222200, 0x02222200, 0x22222200, 0x00000020, 0x20000020,
0x02000020, 0x22000020, 0x00200020, 0x20200020, 0x02200020, 0x22200020,
0x00020020, 0x20020020, 0x02020020, 0x22020020, 0x00220020, 0x20220020,
0x02220020, 0x22220020, 0x00002020, 0x20002020, 0x02002020, 0x22002020,
0x00202020, 0x20202020, 0x02202020, 0x22202020, 0x00022020, 0x20022020,
0x02022020, 0x22022020, 0x00222020, 0x20222020, 0x02222020, 0x22222020,
0x00000220, 0x20000220, 0x02000220, 0x22000220, 0x00200220, 0x20200220,
0x02200220, 0x22200220, 0x00020220, 0x20020220, 0x02020220, 0x22020220,
0x00220220, 0x20220220, 0x02220220, 0x22220220, 0x00002220, 0x20002220,
0x02002220, 0x22002220, 0x00202220, 0x20202220, 0x02202220, 0x22202220,
0x00022220, 0x20022220, 0x02022220, 0x22022220, 0x00222220, 0x20222220,
0x02222220, 0x22222220, 0x00000002, 0x20000002, 0x02000002, 0x22000002,
0x00200002, 0x20200002, 0x02200002, 0x22200002, 0x00020002, 0x20020002,
0x02020002, 0x22020002, 0x00220002, 0x20220002, 0x02220002, 0x22220002,
0x00002002, 0x20002002, 0x02002002, 0x22002002, 0x00202002, 0x20202002,
0x02202002, 0x22202002, 0x00022002, 0x20022002, 0x02022002, 0x22022002,
0x00222002, 0x20222002, 0x02222002, 0x22222002, 0x00000202, 0x20000202,
0x02000202, 0x22000202, 0x00200202, 0x20200202, 0x02200202, 0x22200202,
0x00020202, 0x20020202, 0x02020202, 0x22020202, 0x00220202, 0x20220202,
0x02220202, 0x22220202, 0x00002202, 0x20002202, 0x02002202, 0x22002202,
0x00202202, 0x20202202, 0x02202202, 0x22202202, 0x00022202, 0x20022202,
0x02022202, 0x22022202, 0x00222202, 0x20222202, 0x02222202, 0x22222202,
0x00000022, 0x20000022, 0x02000022, 0x22000022, 0x00200022, 0x20200022,
0x02200022, 0x22200022, 0x00020022, 0x20020022, 0x02020022, 0x22020022,
0x00220022, 0x20220022, 0x02220022, 0x22220022, 0x00002022, 0x20002022,
0x02002022, 0x22002022, 0x00202022, 0x20202022, 0x02202022, 0x22202022,
0x00022022, 0x20022022, 0x02022022, 0x22022022, 0x00222022, 0x20222022,
0x02222022, 0x22222022, 0x00000222, 0x20000222, 0x02000222, 0x22000222,
0x00200222, 0x20200222, 0x02200222, 0x22200222, 0x00020222, 0x20020222,
0x02020222, 0x22020222, 0x00220222, 0x20220222, 0x02220222, 0x22220222,
0x00002222, 0x20002222, 0x02002222, 0x22002222, 0x00202222, 0x20202222,
0x02202222, 0x22202222, 0x00022222, 0x20022222, 0x02022222, 0x22022222,
0x00222222, 0x20222222, 0x02222222, 0x22222222, };

// Note: Used inside pputile.inc.
static constexpr uint32 ppulut3[128] = {
0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
0x00000000, 0x00000000, 0x44444444, 0x04444444, 0x00444444, 0x00044444,
0x00004444, 0x00000444, 0x00000044, 0x00000004, 0x88888888, 0x08888888,
0x00888888, 0x00088888, 0x00008888, 0x00000888, 0x00000088, 0x00000008,
0xcccccccc, 0x0ccccccc, 0x00cccccc, 0x000ccccc, 0x0000cccc, 0x00000ccc,
0x000000cc, 0x0000000c, 0x00000000, 0x40000000, 0x44000000, 0x44400000,
0x44440000, 0x44444000, 0x44444400, 0x44444440, 0x44444444, 0x44444444,
0x44444444, 0x44444444, 0x44444444, 0x44444444, 0x44444444, 0x44444444,
0x88888888, 0x48888888, 0x44888888, 0x44488888, 0x44448888, 0x44444888,
0x44444488, 0x44444448, 0xcccccccc, 0x4ccccccc, 0x44cccccc, 0x444ccccc,
0x4444cccc, 0x44444ccc, 0x444444cc, 0x4444444c, 0x00000000, 0x80000000,
0x88000000, 0x88800000, 0x88880000, 0x88888000, 0x88888800, 0x88888880,
0x44444444, 0x84444444, 0x88444444, 0x88844444, 0x88884444, 0x88888444,
0x88888844, 0x88888884, 0x88888888, 0x88888888, 0x88888888, 0x88888888,
0x88888888, 0x88888888, 0x88888888, 0x88888888, 0xcccccccc, 0x8ccccccc,
0x88cccccc, 0x888ccccc, 0x8888cccc, 0x88888ccc, 0x888888cc, 0x8888888c,
0x00000000, 0xc0000000, 0xcc000000, 0xccc00000, 0xcccc0000, 0xccccc000,
0xcccccc00, 0xccccccc0, 0x44444444, 0xc4444444, 0xcc444444, 0xccc44444,
0xcccc4444, 0xccccc444, 0xcccccc44, 0xccccccc4, 0x88888888, 0xc8888888,
0xcc888888, 0xccc88888, 0xcccc8888, 0xccccc888, 0xcccccc88, 0xccccccc8,
0xcccccccc, 0xcccccccc, 0xcccccccc, 0xcccccccc, 0xcccccccc, 0xcccccccc,
0xcccccccc, 0xcccccccc, };

static inline const uint8 *MMC5SPRVRAMADR(FC *fc, uint32 v) {
  DCHECK(std::type_index(typeid(*fc->fceu->cartiface)) ==
	std::type_index(typeid(MMC5))) << "\n" <<
    typeid(fc->fceu->cartiface).name() << "\n vs \n" <<
    typeid(MMC5).name();
  MMC5 *mmc5 = static_cast<MMC5*>(fc->fceu->cartiface);
  return mmc5->SPRVPagePtr(v);
}
static inline const uint8 *VRAMADR(FC *fc, uint32 A) {
  return fc->cart->VPagePointer(A);
}

// mbg 8/6/08 - fix a bug relating to
// "When in 8x8 sprite mode, only one set is used for both BG and sprites."
// in mmc5 docs
const uint8 *PPU::MMC5BGVRAMADR(uint32 V) {
  DCHECK(std::type_index(typeid(*fc->fceu->cartiface)) ==
	std::type_index(typeid(MMC5))) << "\n" <<
    typeid(fc->fceu->cartiface).name() << "\n vs \n" <<
    typeid(MMC5).name();
  MMC5 *mmc5 = static_cast<MMC5*>(fc->fceu->cartiface);
  if (!Sprite16) {
    if (mmc5ABMode == 0)
      return mmc5->SPRVPagePtr(V);
    else
      return mmc5->BGVPagePtr(V);
  } else return mmc5->BGVPagePtr(V);
}

// static
DECLFR_RET PPU::A2002(DECLFR_ARGS) {
  return fc->ppu->A2002_Direct(DECLFR_FORWARD);
}

DECLFR_RET PPU::A2002_Direct(DECLFR_ARGS) {
  TRACEF("A2002 %d", PPU_status);

  LineUpdate();
  uint8 ret = PPU_status;
  TRACEN(ret);

  ret|=PPUGenLatch&0x1F;
  TRACEN(ret);

  vtoggle=0;
  PPU_status&=0x7F;
  PPUGenLatch=ret;

  return ret;
}

DECLFR_RET PPU::A2004(DECLFR_ARGS) {
  return fc->ppu->A2004_Direct(DECLFR_FORWARD);
}
// static
DECLFR_RET PPU::A2004_Direct(DECLFR_ARGS) {
  LineUpdate();
  return PPUGenLatch;
}

/* Not correct for $2004 reads. */
DECLFR_RET PPU::A200x(DECLFR_ARGS) {
  return fc->ppu->A200x_Direct(DECLFR_FORWARD);
}
// static
DECLFR_RET PPU::A200x_Direct(DECLFR_ARGS) {
  LineUpdate();
  return PPUGenLatch;
}

DECLFR_RET PPU::A2007(DECLFR_ARGS) {
  return fc->ppu->A2007_Direct(DECLFR_FORWARD);
}
// static
DECLFR_RET PPU::A2007_Direct(DECLFR_ARGS) {
  uint32 tmp = RefreshAddr & 0x3FFF;

  LineUpdate();

  uint8 ret = VRAMBuffer;

  if (PPU_hook) PPU_hook(fc, tmp);
  PPUGenLatch=VRAMBuffer;
  if (tmp < 0x2000) {
    VRAMBuffer = fc->cart->ReadVPage(tmp);
  } else if (tmp < 0x3F00) {
    VRAMBuffer = vnapage[(tmp>>10)&0x3][tmp&0x3FF];
  }


  if ((ScreenON || SpriteON) && scanline < 240) {
    uint32 rad=RefreshAddr;

    if ((rad&0x7000)==0x7000) {
      rad^=0x7000;
      if ((rad&0x3E0)==0x3A0)
        rad^=0xBA0;
      else if ((rad&0x3E0)==0x3e0)
        rad^=0x3e0;
      else
        rad+=0x20;
    } else {
      rad+=0x1000;
    }
    RefreshAddr=rad;
  } else {
    if (INC32)
      RefreshAddr+=32;
    else
      RefreshAddr++;
  }
  if (PPU_hook) PPU_hook(fc, RefreshAddr & 0x3fff);

  return ret;
}



//static DECLFW(Write_PSG) {
//  return fc->sound->Write_PSG_Direct(DECLFW_FORWARD);
//}
//void Sound::Write_PSG_Direct(DECLFW_ARGS) {

static DECLFW(B2000) {
  fc->ppu->B2000_Direct(DECLFW_FORWARD);
}
// static
void PPU::B2000_Direct(DECLFW_ARGS) {
  //    FCEU_printf("%04x:%02x, (%d) %02x, %02x\n",A,V,scanline,PPU_values[0],PPU_status);

  LineUpdate();
  PPUGenLatch=V;
  if (!(PPU_values[0]&0x80) && (V&0x80) && (PPU_status&0x80)) {
    //     FCEU_printf("Trigger NMI, %d, %d\n",timestamp,ppudead);
    fc->X->TriggerNMI2();
  }
  PPU_values[0]=V;
  TempAddr&=0xF3FF;
  TempAddr|=(V&3)<<10;
}

static DECLFW(B2001) {
  fc->ppu->B2001_Direct(DECLFW_FORWARD);
}
// static
void PPU::B2001_Direct(DECLFW_ARGS) {
  //printf("%04x:$%02x, %d\n",A,V,scanline);
  LineUpdate();
  PPUGenLatch=V;
  PPU_values[1]=V;
  if (V&0xE0)
    deemp=V>>5;
}

static DECLFW(B2002) {
  fc->ppu->B2002_Direct(DECLFW_FORWARD);
}
// static
void PPU::B2002_Direct(DECLFW_ARGS) {
  PPUGenLatch=V;
}

static DECLFW(B2003) {
  fc->ppu->B2003_Direct(DECLFW_FORWARD);
}
// static
void PPU::B2003_Direct(DECLFW_ARGS) {
  //printf("$%04x:$%02x, %d, %d\n",A,V,timestamp,scanline);
  PPUGenLatch = V;
  PPU_values[3] = V;
  PPUSPL = V & 0x7;
}

static DECLFW(B2004) {
  fc->ppu->B2004_Direct(DECLFW_FORWARD);
}
// static
void PPU::B2004_Direct(DECLFW_ARGS) {
  //printf("Wr: %04x:$%02x\n",A,V);
  PPUGenLatch = V;
  if (PPUSPL >= 8) {
    if (PPU_values[3] >= 8)
      SPRAM[PPU_values[3]]=V;
  } else {
    //printf("$%02x:$%02x\n",PPUSPL,V);
    SPRAM[PPUSPL] = V;
  }
  PPU_values[3]++;
  PPUSPL++;
}

static DECLFW(B2005) {
  fc->ppu->B2005_Direct(DECLFW_FORWARD);
}
// static
void PPU::B2005_Direct(DECLFW_ARGS) {
  // http://wiki.nesdev.com/w/index.php/PPU_registers#PPUSCROLL
  // 2005 is 'PPUSCROLL', which controls the scrolling of the
  // background. There are two scroll bytes, x, and y, and two
  // consecutive writes set them (first x, then y).
  // (vtoggle keeps track of this state).
  //
  // Note that both 2005 and 2006 modify the TempAddr; they
  // just map the inputs differently.

  // This page is great for understanding all this weird stuff:
  // http://wiki.nesdev.com/w/index.php/PPU_scrolling
  uint32 tmp = TempAddr;
  LineUpdate();
  PPUGenLatch = V;
  if (!vtoggle) {
    // x scroll offset
    tmp &= 0xFFE0;
    tmp |= V >> 3;
    // XOffset is the x position within a tile.
    XOffset = V & 7;
  } else {
    // y scroll offset
    tmp &= 0x8C1F;
    tmp |= (V & ~7) << 2;
    // Weirdly, the y position within a tile (0-7) is
    // stored as the top bits
    tmp |= (V & 7) << 12;
  }
  TempAddr = tmp;
  vtoggle ^= 1;
}


static DECLFW(B2006) {
  fc->ppu->B2006_Direct(DECLFW_FORWARD);
}
// static
void PPU::B2006_Direct(DECLFW_ARGS) {
  // http://wiki.nesdev.com/w/index.php/PPU_registers#PPUADDR
  // 2006 is 'PPUADDR', which gives the memory address that
  // reads/writes to PPUDATA (2007) shall be directed to.
  // Since this is a 16-bit address, two consecutive writes
  // are used to set the upper byte and then lower byte.
  // (vtoggle keeps track of this state).
  //
  // Note that both 2005 and 2006 modify the TempAddr; they
  // just map the inputs differently.
  LineUpdate();

  PPUGenLatch = V;
  if (!vtoggle) {
    TempAddr &= 0x00FF;
    TempAddr |= (V & 0x3f) << 8;
  } else {
    TempAddr &= 0xFF00;
    TempAddr |= V;

    RefreshAddr = TempAddr;
    if (PPU_hook)
      PPU_hook(fc, RefreshAddr);
    // printf("%d, %04x\n",scanline,RefreshAddr);
  }

  vtoggle ^= 1;
}

static DECLFW(B2007) {
  fc->ppu->B2007_Direct(DECLFW_FORWARD);
}

// static
void PPU::B2007_Direct(DECLFW_ARGS) {
  // This is the PPUDATA register.
  const uint32 tmp = RefreshAddr & 0x3FFF;

  PPUGenLatch=V;
  if (tmp>=0x3F00) {
    // hmmm....
    if (!(tmp&0xf))
      PALRAM[0x00]=PALRAM[0x04]=PALRAM[0x08]=PALRAM[0x0C]=V&0x3F;
    else if (tmp&3) PALRAM[(tmp&0x1f)]=V&0x3f;
  } else if (tmp<0x2000) {
    if (PPUCHRRAM&(1<<(tmp>>10)))
      fc->cart->WriteVPage(tmp, V);
  } else {
    if (PPUNTARAM&(1<<((tmp&0xF00)>>10)))
      vnapage[((tmp&0xF00)>>10)][tmp&0x3FF]=V;
  }
  //      FCEU_printf("ppu (%04x) %04x:%04x %d, %d\n",fc->X->PC,RefreshAddr,PPUGenLatch,scanline,timestamp);
  if (INC32) RefreshAddr+=32;
  else RefreshAddr++;
  if (PPU_hook) PPU_hook(fc, RefreshAddr & 0x3fff);
}

static DECLFW(B4014) {
  fc->ppu->B4014_Direct(DECLFW_FORWARD);
}
// static
void PPU::B4014_Direct(DECLFW_ARGS) {
  // This is the OAMDMA register. Writing here initiates a copy of
  // 256 bytes to the PPU OAM (sprite) memory.
  const uint32 src_address = V << 8;
  // n.b. according to NESDEV, there is one or two (if we are on an
  // odd cpu cycle) idle cycles (so we should do ADDCYC or the
  // equivalent) before this DMA loop. -tom7
  // http://wiki.nesdev.com/w/index.php/PPU_OAM#DMA
  for (int x = 0; x < 256; x++) {
    fc->X->DMW(0x2004, fc->X->DMR(src_address + x));
  }
}

void PPU::ResetRL(uint8 *target) {
  FCEU_dwmemset(target, 0xFFFFFFFF, 256);
  fc->input->InputScanlineHook(0, 0, 0, 0);
  Plinef = target;
  Pline = target;
  firsttile = 0;
  linestartts = fc->X->timestamp * 48 + fc->X->count;
  tofix = 0;
  LineUpdate();
  tofix = 1;
}

void PPU::LineUpdate() {
  if (Pline) {
    const int l = (fc->fceu->PAL ?
                   ((fc->X->timestamp*48-linestartts)/15) :
                   ((fc->X->timestamp*48-linestartts)>>4) );
    RefreshLine(l);
  }
}

void PPU::CheckSpriteHit(int p) {
  TRACEF("CheckSpriteHit %d %d %02x", p, sprite_hit_x, sprite_hit_mask);
  if (sprite_hit_x == 0x100) return;

  // Port note: This used to check x < 255 in the loop, but why loop
  // beyond that? The loop only does anything if the condition is
  // true. -tom7 PERF: A similar trick may apply to x < sprite_hit_x + 8.
  const int l = std::min(p - 16, 255);
  
  for (int x = sprite_hit_x; x < sprite_hit_x + 8 && x < l; x++) {
    // Basically, check whether the sprite pixel is not background
    // (sprite_hit_mask is a bitmask where 0 means palette entry 0,
    // background) and the pixel in the scanline (Pline) does not have
    // bit 6 set. (Now, who is setting this bit? -tom7)
    if ((sprite_hit_mask & (0x80 >> (x - sprite_hit_x))) &&
	!(Plinef[x] & 0x40)) {
      TRACELOC();
      PPU_status |= 0x40;
      //printf("Ha:  %d, %d, Hita: %d, %d, %d, %d, %d\n",
      //       p,p&~7,scanline,GETLASTPIXEL-16,&Plinef[x],Pline,Pline-Plinef);
      //printf("%d\n",GETLASTPIXEL-16);
      //if (Plinef[x] == 0xFF)
      //printf("PL: %d, %02x\n",scanline, Plinef[x]);
      sprite_hit_x = 0x100;
      break;
    }
  }
}

void PPU::EndRL() {
  RefreshLine(272);
  if (tofix)
    Fixit1();
  CheckSpriteHit(272);
  Pline = nullptr;
}

// Used to be an include hack, replaced with a templated function.
// Returns {refreshaddr_local, P}, which are both modified.
template<bool PPUT_MMC5, bool PPUT_MMC5SP, bool PPUT_HOOK, bool PPUT_MMC5CHR1>
inline std::pair<uint32, uint8 *> PPU::PPUTile(const int X1, uint8 *P,
                                               const uint32 vofs,
                                               uint32 refreshaddr_local) {
  const uint8 *C;
  uint8 cc;
  uint32 vadr;
  uint8 zz;

  uint8 xs, ys;
  if (PPUT_MMC5SP) {
    xs = X1;
    ys = ((scanline>>3) + MMC5HackSPScroll) & 0x1F;
    if (ys >= 0x1E) ys -= 0x1E;
  }

  if (X1 >= 2) {
    const uint8 *S = PALRAM;
    uint32 pixdata;

    pixdata = ppulut1[(pshift[0]>>(8-XOffset))&0xFF] |
              ppulut2[(pshift[1]>>(8-XOffset))&0xFF];

    pixdata |= ppulut3[XOffset|(atlatch<<3)];
    // printf("%02x ",ppulut3[XOffset|(atlatch<<3)]);

    P[0]=S[pixdata&0xF];
    pixdata>>=4;
    P[1]=S[pixdata&0xF];
    pixdata>>=4;
    P[2]=S[pixdata&0xF];
    pixdata>>=4;
    P[3]=S[pixdata&0xF];
    pixdata>>=4;
    P[4]=S[pixdata&0xF];
    pixdata>>=4;
    P[5]=S[pixdata&0xF];
    pixdata>>=4;
    P[6]=S[pixdata&0xF];
    pixdata>>=4;
    P[7]=S[pixdata&0xF];
    P+=8;
  }

  if (PPUT_MMC5SP) {
    vadr = (MMC5HackExNTARAMPtr[xs|(ys<<5)]<<4)+(vofs&7);
  } else {
    zz = refreshaddr_local&0x1F;
    C = vnapage[(refreshaddr_local>>10)&3];
    /* Fetch name table byte. */
    vadr = (C[refreshaddr_local&0x3ff]<<4)+vofs;
  }

  if (PPUT_HOOK) {
    PPU_hook(fc, 0x2000 | (refreshaddr_local & 0xfff));
  }

  if (PPUT_MMC5SP) {
    cc = MMC5HackExNTARAMPtr[0x3c0+(xs>>2)+((ys&0x1C)<<1)];
    cc = (cc >> ((xs&2) + ((ys&0x2)<<1))) & 3;
  } else {
    if (PPUT_MMC5CHR1) {
      cc = (MMC5HackExNTARAMPtr[refreshaddr_local & 0x3ff] & 0xC0) >> 6;
    } else {
      /* Fetch attribute table byte. */
      cc = C[0x3c0+(zz>>2)+((refreshaddr_local&0x380)>>4)];
      cc = (cc >> ((zz&2) + ((refreshaddr_local&0x40)>>4))) & 3;
    }
  }

  atlatch >>= 2;
  atlatch |= cc << 2;

  pshift[0] <<= 8;
  pshift[1] <<= 8;

  if (PPUT_MMC5SP) {
    C = MMC5HackVROMPTR + vadr;
    C += ((MMC5HackSPPage & 0x3f & MMC5HackVROMMask) << 12);
  } else {
    if (PPUT_MMC5CHR1) {
      C = MMC5HackVROMPTR;
      C += (((MMC5HackExNTARAMPtr[refreshaddr_local & 0x3ff]) & 0x3f &
             MMC5HackVROMMask) << 12) + (vadr & 0xfff);
      //11-jun-2009 for kuja_killer
      C += (MMC50x5130 & 0x3) << 18;
    } else if (PPUT_MMC5) {
      C = MMC5BGVRAMADR(vadr);
    } else {
      C = VRAMADR(fc, vadr);
    }
  }

  if (PPUT_HOOK) {
    PPU_hook(fc, vadr);
  }

  pshift[0] |= C[0];
  pshift[1] |= C[8];

  if ((refreshaddr_local&0x1f) == 0x1f) {
    refreshaddr_local ^= 0x41F;
  } else {
    refreshaddr_local++;
  }

  if (PPUT_HOOK) {
    PPU_hook(fc, 0x2000 | (refreshaddr_local & 0xfff));
  }

  return std::make_pair(refreshaddr_local, P);
}

// lasttile is really "second to last tile."
void PPU::RefreshLine(int lastpixel) {
  // Not clear why we make a backup copy of this -- probably so that
  // hooks don't see the updated version until we're done. Modified
  // wihin PPUTile. (However note that RefreshAddr is only mentioned
  // inside ppu.cc.)
  uint32 refreshaddr_local = RefreshAddr;

  /* Yeah, recursion would be bad. PPU_hook() functions can call
     mirroring/chr bank switching functions, which call
     LineUpdate, which call this function. */
  if (norecurse) return;

  TRACEF("RefreshLine %d %u %u %u %u %d",
         lastpixel, pshift[0], pshift[1], atlatch, refreshaddr_local,
         norecurse);

  int lasttile = lastpixel >> 3;

  if (sprite_hit_x != 0x100 && !(PPU_status & 0x40)) {
    if ((sprite_hit_x < lastpixel - 16) &&
	!(sprite_hit_x < (lasttile - 2) * 8)) {
      //printf("OK: %d\n",scanline);
      lasttile++;
    }
  }

  if (lasttile > 34) lasttile = 34;
  int numtiles = lasttile - firsttile;

  if (numtiles <= 0) return;

  uint8 *P = Pline;

  uint32 vofs = ((PPU_values[0]&0x10)<<8) | ((refreshaddr_local>>12)&7);

  static constexpr int TOFIXNUM = 272 - 0x4;
  if (!ScreenON && !SpriteON) {
    const uint32 tmp =
      PALRAM[0] | (PALRAM[0] << 8) | (PALRAM[0] << 16) | (PALRAM[0] << 24) |
      0x40404040;
    FCEU_dwmemset(Pline, tmp, numtiles * 8);
    P += numtiles * 8;
    Pline = P;

    firsttile = lasttile;

    if (lastpixel >= TOFIXNUM && tofix) {
      Fixit1();
      tofix=0;
    }

    if (lastpixel - 16 >= 0) {
      fc->input->InputScanlineHook(Plinef,
				   any_sprites_on_line ? sprlinebuf : 0,
                                   linestartts,
				   lasttile * 8 - 16);
    }
    return;
  }

  // Priority bits, needed for sprite emulation.
  //
  // I think these are the 0 entries for each palette, so this is a
  // trick to make sure we set bit 0x40 in XBuf when we've rendered
  // background, which is used for the sprite 0 hit, among other
  // things. Note that we OR PALRAM[0] with 0x40 anyway in several
  // places. Could maybe move this up and take advantage. I'd prefer
  // not temporarily modifying this array, though. (It may also work
  // to just keep these bits always set as a representation
  // invariant? 6502 CPU may be able to read from PALRAM, though.) -tom7
  PALRAM[0]|=64;
  PALRAM[4]|=64;
  PALRAM[8]|=64;
  PALRAM[0xC]|=64;

  // This high-level graphics MMC5 emulation code was written for MMC5
  // carts in "CL" mode. It's probably not totally correct for carts in
  // "SL" mode.

  if (MMC5Hack) {
    if (MMC5HackCHRMode == 0 && (MMC5HackSPMode & 0x80)) {
      int tochange=MMC5HackSPMode&0x1F;
      tochange-=firsttile;
      for (int X1 = firsttile; X1 < lasttile; X1++) {
        if ((tochange<=0 && MMC5HackSPMode&0x40) ||
            (tochange>0 && !(MMC5HackSPMode&0x40))) {
          TRACELOC();
          // MMC5 and MMC5SP
          std::tie(refreshaddr_local, P) =
            PPUTile<true, true, false, false>(X1, P, vofs, refreshaddr_local);
        } else {
          TRACELOC();
          // MMC5 only
          std::tie(refreshaddr_local, P) =
            PPUTile<true, false, false, false>(X1, P, vofs, refreshaddr_local);
        }
        tochange--;
      }
    } else if (MMC5HackCHRMode == 1 && (MMC5HackSPMode & 0x80)) {
      int tochange=MMC5HackSPMode&0x1F;
      tochange-=firsttile;

      for (int X1 = firsttile; X1 < lasttile; X1++) {
        TRACELOC();
        // MMC5, MMC5SP, MMC5CHR1
        std::tie(refreshaddr_local, P) =
          PPUTile<true, true, false, true>(X1, P, vofs, refreshaddr_local);
      }
    } else if (MMC5HackCHRMode == 1) {
      for (int X1 = firsttile; X1 < lasttile; X1++) {
        TRACELOC();
        // MMC5, MMC5CHR1
        std::tie(refreshaddr_local, P) =
          PPUTile<true, false, false, true>(X1, P, vofs, refreshaddr_local);
      }
    } else {
      for (int X1 = firsttile; X1 < lasttile; X1++) {
        TRACELOC();
        // MMC5 only
        std::tie(refreshaddr_local, P) =
          PPUTile<true, false, false, false>(X1, P, vofs, refreshaddr_local);
      }
    }
  } else if (PPU_hook) {
    norecurse = 1;
    for (int X1 = firsttile; X1 < lasttile; X1++) {
      TRACELOC();
      // HOOK
      std::tie(refreshaddr_local, P) =
        PPUTile<false, false, true, false>(X1, P, vofs, refreshaddr_local);
    }
    norecurse=0;
  } else {
    for (int X1 = firsttile; X1 < lasttile; X1++) {
      TRACELOC();
      // Nothing.
      std::tie(refreshaddr_local, P) =
        PPUTile<false, false, false, false>(X1, P, vofs, refreshaddr_local);
    }
  }

  TRACEF("After PPU: %d %d", fc->X->reg_PC, refreshaddr_local);
  TRACEF("Moreover: %d %u %u %u %u %d",
         lastpixel, pshift[0], pshift[1], atlatch, refreshaddr_local,
         norecurse);
  TRACEA(PPU_values, 4);

  // Reverse changes made before.
  // (Note this also clears 7th bit -tom7)
  PALRAM[0] &= 63;
  PALRAM[4] &= 63;
  PALRAM[8] &= 63;
  PALRAM[0xC] &= 63;

  RefreshAddr = refreshaddr_local;
  if (firsttile <= 2 && 2 < lasttile && !(PPU_values[1] & 2)) {
    const uint32 tmp = 
      PALRAM[0] | (PALRAM[0] << 8) | (PALRAM[0] << 16) | (PALRAM[0] << 24) |
      0x40404040;
    *(uint32 *)Plinef = *(uint32 *)(Plinef + 4) = tmp;
  }

  if (!ScreenON) {
    const uint32 tmp =
      PALRAM[0] | (PALRAM[0] << 8) | (PALRAM[0] << 16) | (PALRAM[0] << 24) |
      0x40404040;
    
    int t_start = firsttile - 2;
    int t_count = lasttile - firsttile;
    if (t_start < 0) {
      t_count += t_start;
      t_start = 0;
    }
    if (t_count > 0) {
      FCEU_dwmemset(Plinef + t_start * 8, tmp, t_count * 8);
    }
  }

  if (lastpixel >= TOFIXNUM && tofix) {
    //puts("Fixed");
    Fixit1();
    tofix = 0;
  }

  // This only works right because of a hack earlier in this function.
  CheckSpriteHit(lastpixel);

  if (lastpixel - 16 >= 0) {
    fc->input->InputScanlineHook(Plinef, any_sprites_on_line ? sprlinebuf : 0,
				 linestartts, lasttile * 8 - 16);
  }
  Pline = P;
  firsttile = lasttile;
}

void PPU::Fixit2() {
  if (ScreenON || SpriteON) {
    uint32 rad=RefreshAddr;
    rad&=0xFBE0;
    rad|=TempAddr&0x041f;
    RefreshAddr=rad;
    //PPU_hook(fc, RefreshAddr);
    //PPU_hook(fc, RefreshAddr,-1);
  }
}

void PPU::Fixit1() {
  if (ScreenON || SpriteON) {
    uint32 rad = RefreshAddr;

    if ((rad & 0x7000) == 0x7000) {
      rad ^= 0x7000;
      if ((rad & 0x3E0) == 0x3A0)
        rad ^= 0xBA0;
      else if ((rad & 0x3E0) == 0x3e0)
        rad ^= 0x3e0;
      else
        rad += 0x20;
    } else {
      rad += 0x1000;
    }
    RefreshAddr = rad;
  }
}

void PPU::DoLine() {
  #ifdef TRACK_INTERFRAME_SCROLL
  {
    interframe_x[scanline] = GetXScroll8();
    interframe_y[scanline] = GetYScroll8();
  }
  #endif

  uint8 *target = fc->fceu->XBuf + (scanline << 8);

  if (MMC5Hack && (ScreenON || SpriteON)) {
    DCHECK(std::type_index(typeid(*fc->fceu->cartiface)) ==
	  std::type_index(typeid(MMC5))) << "\n" <<
      typeid(fc->fceu->cartiface).name() << "\n vs \n" <<
      typeid(MMC5).name();
    MMC5 *mmc5 = static_cast<MMC5*>(fc->fceu->cartiface);
    mmc5->MMC5HackHB(scanline);
  }

  Run6502(256);
  EndRL();

  if (!renderbg) {
    // User asked to not display background data.
    const uint8 col = (gNoBGFillColor == 0xFF) ? PALRAM[0] : gNoBGFillColor;
    const uint32 tmp =
      col | (col << 8) | (col << 16) | (col << 24) | 0x40404040;
    FCEU_dwmemset(target, tmp, 256);
  }

  if (SpriteON)
    CopySprites(target);


  // What is this?? ANDs every byte in the buffer with 0x30 if PPU_values[1]
  // has its lowest bit set (this indicates monochrome mode -tom7).

  if (ScreenON || SpriteON) {
    // Yes, very el-cheapo.
    if (PPU_values[1] & 0x01) {
      for (int x = 63; x >= 0; x--)
        *(uint32 *)&target[x << 2] = (*(uint32*)&target[x << 2]) & 0x30303030;
    }
  }

  // This might be NTSC emphasis? Document. -tom7
  if ((PPU_values[1] >> 5) == 0x7) {
    for (int x = 63; x >= 0; x--)
      *(uint32 *)&target[x << 2] =
        ((*(uint32*)&target[x << 2]) & 0x3f3f3f3f) | 0xc0c0c0c0;
  } else if (PPU_values[1] & 0xE0) {
    for (int x = 63; x >= 0; x--)
      *(uint32 *)&target[x << 2] = (*(uint32*)&target[x << 2]) | 0x40404040;
  } else {
    for (int x = 63; x >= 0; x--)
      *(uint32 *)&target[x << 2] =
        ((*(uint32*)&target[x << 2]) & 0x3f3f3f3f) | 0x80808080;
  }

  sprite_hit_x = 0x100;

  if (ScreenON || SpriteON)
    FetchSpriteData();

  if (GameHBIRQHook && (ScreenON || SpriteON) &&
      ((PPU_values[0] & 0x38) != 0x18)) {
    Run6502(6);
    Fixit2();
    Run6502(4);
    GameHBIRQHook(fc);
    Run6502(85 - 16 - 10);
  } else {
    Run6502(6);  // Tried 65, caused problems with Slalom (maybe others)
    Fixit2();
    Run6502(85 - 6 - 16);

    // A semi-hack for Star Trek: 25th Anniversary
    if (GameHBIRQHook && (ScreenON || SpriteON) &&
	((PPU_values[0] & 0x38) != 0x18)) {
      GameHBIRQHook(fc);
    }
  }

  if (SpriteON)
    RefreshSprites();
  if (GameHBIRQHook2 && (ScreenON || SpriteON))
    GameHBIRQHook2(fc);
  scanline++;
  if (scanline < 240) {
    ResetRL(fc->fceu->XBuf + (scanline << 8));
  }
  Run6502(16);
}

#define V_FLIP  0x80
#define H_FLIP  0x40
#define SP_BACK 0x20

namespace {

// Four bytes per sprite in SPRAM.
struct SPR {
  // y coordinate - 1 (upper-left corner of sprite)
  uint8 y;
  // Tile index in pattern table.
  uint8 no;
  // Sprite attributes:
  //  vhp000cc | Attributes                           |
  //           |   v = Vertical Flip   (1=Flip)       |
  //           |   h = Horizontal Flip (1=Flip)       |
  //           |   p = Background Priority            |
  //           |         0 = In front                 |
  //           |         1 = Behind                   |
  //           |   c = Upper two (2) bits of colour   |
  uint8 atr;
  // x coordinate.
  uint8 x;
};

struct SPRB {
  // I think ca is the actual character data, but separated into
  // two planes. They together make the 2-bit color information,
  // which is done through the lookup tables ppulut1 and 2.
  // They have to be the actual data (not addresses) because
  // ppulut is a fixed transformation.
  uint8 ca[2], atr, x;
};
}  // namespace

#define STATIC_ASSERT( condition, name ) \
  static_assert( condition, #condition " " #name)

STATIC_ASSERT(sizeof (SPR) == 4, spr_size);
STATIC_ASSERT(sizeof (SPRB) == 4, sprb_size);
STATIC_ASSERT(sizeof (uint32) == 4, uint32_size);

void PPU::DisableSpriteLimitation(int a) {
  maxsprites = a ? 64 : 8;
}

// I believe this corresponds to the "internal operation" section of
// http://wiki.nesdev.com/w/index.php/PPU_OAM
// where the PPU is looking for sprites for the NEXT scanline.
// Fills in SPRBUF, numsprites, sprite_0_in_sprbuf.
void PPU::FetchSpriteData() {
  const uint8 P0 = PPU_values[0];

  const SPR *spr = (SPR *)SPRAM;
  // Sprite height; either 8 or 16. -tom7
  const uint8 sprite_height = 8 + ((P0 & 0x20) >> 2);

  // Number of sprites on this line (so far).
  uint8 ns = 0;

  // port note: This used to be called SpriteBlurp and was a bit
  // unnecessarily weird (the test was in the loop, but can only
  // be satisfied in this specific circumstance). The purpose of
  // this is to test whether sprite 0 is going into the sprite
  // buffer, so that we can do the sprite 0 hit test only in
  // that case.
  sprite_0_in_sprbuf = (unsigned int)(scanline - spr->y) < sprite_height;
  
  const int vofs = (unsigned int)(P0 & 0x8 & (((P0 & 0x20) ^ 0x20) >> 2)) << 9;

  DEBUGF(stderr, "FetchSprites @%d\n", scanline);
  if (!PPU_hook) {
    for (int n = 63; n >= 0; n--, spr++) {
      // This tests whether the sprite is on the scanline, being a
      // bit sneaky and using unsigned ints to capture the case where
      // sprite y > scanline.
      if ((unsigned int)(scanline - spr->y) >= sprite_height) continue;
      //printf("%d, %u\n",scanline,(unsigned int)(scanline-spr->y));
      if (ns < maxsprites) {
        DEBUGF(stderr, "   sp %2d: %d,%d #%d attr %s\n",
               n, spr->x, spr->y, spr->no, attrbits(spr->atr).c_str());

	SPRB dst;
	const int t = (int)scanline - spr->y;
	// made uint32 from uint -tom7
	uint32 vadr =
	  Sprite16 ?
	  ((spr->no & 1) << 12) + ((spr->no & 0xFE) << 4) :
	  (spr->no << 4) + vofs;

	if (spr->atr & V_FLIP) {
	  vadr += 7;
	  vadr -= t;
	  vadr += (P0 & 0x20) >> 1;
	  vadr -= t & 8;
	} else {
	  vadr += t;
	  vadr += t & 8;
	}

	const uint8 *C = MMC5Hack ?
	  MMC5SPRVRAMADR(fc, vadr) : VRAMADR(fc, vadr);

	dst.ca[0] = C[0];
	dst.ca[1] = C[8];
	dst.x = spr->x;
	dst.atr = spr->atr;

	{
	  uint32 *dest32 = (uint32 *)&dst;
	  uint32 *sprbuf32 = (uint32 *)&SPRBUF[ns<<2];
	  *sprbuf32=*dest32;
	}

        ns++;
      } else {
        TRACELOC();
        PPU_status|=0x20;
        break;
      }
    }
  } else {
    for (int n = 63; n >= 0; n--,spr++) {
      if ((unsigned int)(scanline - spr->y) >= sprite_height) continue;

      if (ns < maxsprites) {
	SPRB dst;

	const int t = (int)scanline - spr->y;

	unsigned int vadr =
	  Sprite16 ?
	  ((spr->no&1)<<12) + ((spr->no&0xFE)<<4) :
	  (spr->no<<4) + vofs;

	if (spr->atr & V_FLIP) {
	  vadr+=7;
	  vadr-=t;
	  vadr+=(P0&0x20)>>1;
	  vadr-=t&8;
	} else {
	  vadr+=t;
	  vadr+=t&8;
	}

	const uint8 *C = MMC5Hack ?
	  MMC5SPRVRAMADR(fc, vadr) : VRAMADR(fc, vadr);
	dst.ca[0] = C[0];
	if (ns < 8) {
	  PPU_hook(fc, 0x2000);
	  PPU_hook(fc, vadr);
	}
	dst.ca[1] = C[8];
	dst.x = spr->x;
	dst.atr = spr->atr;

	{
	  // XXX why bother with this? Just copy the struct,
	  // or initialize in place.
	  uint32 *dst32 = (uint32 *)&dst;
	  uint32 *sprbuf32 = (uint32 *)&SPRBUF[ns << 2];
	  *sprbuf32 = *dst32;
	}

        ns++;
      } else {
        TRACELOC();
        PPU_status |= 0x20;
        break;
      }
    }
  }
  //if (ns>=7)
  //printf("%d %d\n",scanline,ns);

  //Handle case when >8 sprites per scanline option is enabled.
  if (ns > 8) {
    TRACELOC();
    PPU_status |= 0x20;
  } else if (PPU_hook) {
    for (int n = 0; n < (8 - ns); n++) {
      PPU_hook(fc, 0x2000);
      PPU_hook(fc, vofs);
    }
  }
  numsprites = ns;
}

// As I understand, this takes the sprites on this line (there are
// numsprites of them, which have already been prepared and put into
// SPRBUF -- I during the previous scanline) and writes actual pixel
// data (plus some flags) to sprlinebuf. It also sets up for the
// sprite 0 hit test.
void PPU::RefreshSprites() {
  any_sprites_on_line = 0;
  if (!numsprites) return;

  // Initialize the line buffer to 0x80, meaning "no pixel here."
  FCEU_dwmemset(sprlinebuf, 0x80808080, 256);
  // XXX It's weird to modify numsprites here; can we just use
  // numsprites - 1 in the expressions below? -tom7
  numsprites--;
  SPRB *spr = (SPRB*)SPRBUF + numsprites;

  DEBUGF(stderr, "RefreshSprites @%d with numsprites = %d\n",
         scanline, numsprites);
  for (int n = numsprites; n >= 0; n--,spr--) {
    const int x = spr->x;

    // I think the lookup table basically gets the 4 bytes of sprite
    // data for this scanline. Since ppulut2 is ppulut1 shifted up a
    // bit, I think we're getting 2-bit color data from the two bytes
    // ca[0] and ca[1], and that's why this is an OR. 32 bits is 16
    // pixels, as expected. -tom7
    uint32 pixdata = ppulut1[spr->ca[0]] | ppulut2[spr->ca[1]];

    // So then J is like the 1-bit mask of non-zero pixels.
    // This is for three things: If this line of the sprite is just background,
    // there's nothing to do. We also save this mask as sprite_hit_mask
    // (reversed if the sprite is horizontally flipped) to do the sprite
    // zero hit test, if applicable. Finally, we use the bitmask to
    // guard whether we draw the individual pixels, since background
    // must actually be transparent.
      
    const uint8 J = spr->ca[0] | spr->ca[1];

    const uint8 atr = spr->atr;

    DEBUGF(stderr, "   sp %2d: x=%d ca[%d,%d] attr %s\n",
           n, spr->x, spr->ca[0], spr->ca[1], attrbits(spr->atr).c_str());

    if (J) {
      // We only check sprite hit for sprite 0. If it's in the sprite
      // buffer, it will be at index n == 0.
      if (n == 0 && sprite_0_in_sprbuf && !(PPU_status & 0x40)) {
        sprite_hit_x = x;
	// bitmask J, maybe reversed if the sprite is horizontally flipped.
        sprite_hit_mask =
	  (atr & H_FLIP) ?
	  ((J << 7) & 0x80) |
	  ((J << 5) & 0x40) |
	  ((J << 3) & 0x20) |
	  ((J << 1) & 0x10) |
	  ((J >> 1) & 0x08) |
	  ((J >> 3) & 0x04) |
	  ((J >> 5) & 0x02) |
	  ((J >> 7) & 0x01) :
	  J;
      }

      // PERF If we are in DISABLE_VIDEO we can probably skip
      // drawing the sprite pixels themselves, since we already
      // have enough info for the sprite 0 hit test.
      
      // C is destination for the 8 pixels we'll write
      // on this scanline.
      // C is an array of bytes, each corresponding to
      // a pixel. The bit 0x40 is set if the pixel should
      // show behind the background. The rest of the pixels
      // come from VB (probably just the lowest two?)
      uint8 *C = sprlinebuf + x;
      // pixdata is abstract color values 0,1,2,3.
      // VB gives us an index into the palette data
      // based on the palette selector in this sprite's
      // attributes.
      const uint8 *VB = (PALRAM + 0x10) + ((atr&3)<<2);

      // In back or in front of background?
      if (atr & SP_BACK) {
        // back...

        if (atr & H_FLIP) {
          if (J & 0x80) C[7] = VB[pixdata & 3] | 0x40;
          pixdata >>= 4;
          if (J & 0x40) C[6] = VB[pixdata & 3] | 0x40;
          pixdata >>= 4;
          if (J & 0x20) C[5] = VB[pixdata & 3] | 0x40;
          pixdata >>= 4;
          if (J & 0x10) C[4] = VB[pixdata & 3] | 0x40;
          pixdata >>= 4;
          if (J & 0x08) C[3] = VB[pixdata & 3] | 0x40;
          pixdata >>= 4;
          if (J & 0x04) C[2] = VB[pixdata & 3] | 0x40;
          pixdata >>= 4;
          if (J & 0x02) C[1] = VB[pixdata & 3] | 0x40;
          pixdata >>= 4;
          if (J & 0x01) C[0] = VB[pixdata] | 0x40;
        } else {
          if (J & 0x80) C[0] = VB[pixdata & 3] | 0x40;
          pixdata >>= 4;
          if (J & 0x40) C[1] = VB[pixdata & 3] | 0x40;
          pixdata >>= 4;
          if (J & 0x20) C[2] = VB[pixdata & 3] | 0x40;
          pixdata >>= 4;
          if (J & 0x10) C[3] = VB[pixdata & 3] | 0x40;
          pixdata >>= 4;
          if (J & 0x08) C[4] = VB[pixdata & 3] | 0x40;
          pixdata >>= 4;
          if (J & 0x04) C[5] = VB[pixdata & 3] | 0x40;
          pixdata >>= 4;
          if (J & 0x02) C[6] = VB[pixdata & 3] | 0x40;
          pixdata >>= 4;
          if (J & 0x01) C[7] = VB[pixdata] | 0x40;
        }
      } else {
        if (atr & H_FLIP) {
          if (J & 0x80) C[7] = VB[pixdata & 3];
          pixdata >>= 4;
          if (J & 0x40) C[6] = VB[pixdata & 3];
          pixdata >>= 4;
          if (J & 0x20) C[5] = VB[pixdata & 3];
          pixdata >>= 4;
          if (J & 0x10) C[4] = VB[pixdata & 3];
          pixdata >>= 4;
          if (J & 0x08) C[3] = VB[pixdata & 3];
          pixdata >>= 4;
          if (J & 0x04) C[2] = VB[pixdata & 3];
          pixdata >>= 4;
          if (J & 0x02) C[1] = VB[pixdata & 3];
          pixdata >>= 4;
          if (J & 0x01) C[0] = VB[pixdata];
        } else {
          if (J & 0x80) C[0] = VB[pixdata & 3];
          pixdata >>= 4;
          if (J & 0x40) C[1] = VB[pixdata & 3];
          pixdata >>= 4;
          if (J & 0x20) C[2] = VB[pixdata & 3];
          pixdata >>= 4;
          if (J & 0x10) C[3] = VB[pixdata & 3];
          pixdata >>= 4;
          if (J & 0x08) C[4] = VB[pixdata & 3];
          pixdata >>= 4;
          if (J & 0x04) C[5] = VB[pixdata & 3];
          pixdata >>= 4;
          if (J & 0x02) C[6] = VB[pixdata & 3];
          pixdata >>= 4;
          if (J & 0x01) C[7] = VB[pixdata];
        }
      }
    }
  }
  sprite_0_in_sprbuf = false;
  any_sprites_on_line = 1;
}

// Actually writes sprites to the pixel buffer for a particular scanline.
// target is the beginning of the scanline.
void PPU::CopySprites(uint8 *target) {
  // ends up either 8 or zero. But why?
  uint8 n = ((PPU_values[1] & 4) ^ 4) << 1;
  uint8 *P = target;

  if (!any_sprites_on_line) return;
  any_sprites_on_line = 0;

  // User asked to not display sprites.
  if (!rendersprites) return;

  // looping until n overflows to 0. This is the whole scanline, I think,
  // 4 pixels at a time.
  do {
    uint32 *linebuf32 = (uint32 *)(sprlinebuf + n);
    uint32 t = *linebuf32;

    // I think we're testing to see if the pixel should not be drawn
    // because because there's already a sprite drawn there. (bit 0x80).
    // But how does that bit get set?
    // Might come from the VB array above. If there is one there, then
    // we don't copy. If there isn't one, then we look to see if
    // there's a transparent background pixel (has bit 0x40 set)
    if (t != 0x80808080) {

      // t is 4 bytes of pixel data; we do the same thing
      // for each of them.

#if 1 // was ifdef LSB_FIRST!

      if (!(t&0x80)) {
	// Normal sprite || behind bg sprite
        if (!(t&0x40) || (P[n]&0x40))
          P[n]=sprlinebuf[n];
      }

      if (!(t&0x8000)) {
        if (!(t&0x4000) || (P[n+1]&0x40))
          P[n+1]=(sprlinebuf+1)[n];
      }

      if (!(t&0x800000)) {
        if (!(t&0x400000) || (P[n+2]&0x40))
          P[n+2]=(sprlinebuf+2)[n];
      }

      if (!(t&0x80000000)) {
        if (!(t&0x40000000) || (P[n+3]&0x40))
          P[n+3]=(sprlinebuf+3)[n];
      }
#else
# error LSB_FIRST is assumed, because endianness detection is wrong in this compile, sorry

      /* TODO:  Simplify */
      if (!(t&0x80000000)) {
        if (!(t&0x40000000))       // Normal sprite
          P[n]=sprlinebuf[n];
        else if (P[n]&64)  // behind bg sprite
          P[n]=sprlinebuf[n];
      }

      if (!(t&0x800000)) {
        if (!(t&0x400000))       // Normal sprite
          P[n+1]=(sprlinebuf+1)[n];
        else if (P[n+1]&64)  // behind bg sprite
          P[n+1]=(sprlinebuf+1)[n];
      }

      if (!(t&0x8000)) {
        if (!(t&0x4000))       // Normal sprite
          P[n+2]=(sprlinebuf+2)[n];
        else if (P[n+2]&64)  // behind bg sprite
          P[n+2]=(sprlinebuf+2)[n];
      }

      if (!(t&0x80)) {
        if (!(t&0x40))       // Normal sprite
          P[n+3]=(sprlinebuf+3)[n];
        else if (P[n+3]&64)  // behind bg sprite
          P[n+3]=(sprlinebuf+3)[n];
      }
#endif
    }
    n +=4;
  } while (n);
}

void PPU::SetVideoSystem(int is_pal) {
  scanlines_per_frame = is_pal ? 312 : 262;
}


void PPU::FCEUPPU_Reset() {
  VRAMBuffer = PPU_values[0] = PPU_values[1] = PPU_status = PPU_values[3] = 0;
  PPUSPL = 0;
  PPUGenLatch = 0;
  RefreshAddr = TempAddr = 0;
  vtoggle = 0;
  ppudead = 2;
  cycle_parity = 0;
  // Used to not be saved. -tom7.
  RefreshAddrT = TempAddrT = 0;
  sprite_hit_x = 0;
  sprite_hit_mask = 0;
  // Used to not be reset. Should be handled at initialization time now,
  // but want to prevent divergence from the object version, which
  // always initializes to 0 at creation time. -tom7
  pshift[0] = pshift[1] = 0;
  XOffset = 0;

  #ifdef TRACK_INTERFRAME_SCROLL
  for (int i = 0; i < 256; i++) {
    interframe_x[i] = interframe_y[i] = 0;
  }
  #endif
}

void PPU::FCEUPPU_Power() {
  memset(NTARAM,0x00,0x800);
  memset(PALRAM,0x00,0x20);
  memset(SPRAM,0x00,0x100);
  FCEUPPU_Reset();

  for (int x = 0x2000; x < 0x4000; x += 8) {
    fc->fceu->ARead[x] = A200x;
    fc->fceu->BWrite[x] = B2000;
    fc->fceu->ARead[x + 1] = A200x;
    fc->fceu->BWrite[x + 1] = B2001;
    fc->fceu->ARead[x + 2] = A2002;
    fc->fceu->BWrite[x + 2] = B2002;
    fc->fceu->ARead[x + 3] = A200x;
    fc->fceu->BWrite[x + 3] = B2003;
    fc->fceu->ARead[x + 4] = A2004;
    fc->fceu->BWrite[x + 4] = B2004;
    fc->fceu->ARead[x + 5] = A200x;
    fc->fceu->BWrite[x + 5] = B2005;
    fc->fceu->ARead[x + 6] = A200x;
    fc->fceu->BWrite[x + 6] = B2006;
    fc->fceu->ARead[x + 7] = A2007;
    fc->fceu->BWrite[x + 7] = B2007;
  }
  fc->fceu->BWrite[0x4014] = B4014;
}

void PPU::FrameLoop() {
  if (true) {
    TRACE_SCOPED_ENABLE_IF(true);
    TRACEFUN();
    TRACEF("%d %d %d | "
           "%d %d %d | "
           "%d %d %d | "
           "%d %d %d | "
           "%d",
           cycle_parity, ppudead, PPUSPL,
           RefreshAddrT, TempAddrT, VRAMBuffer,
           PPUGenLatch, pshift[0], pshift[1],
           XOffset, vtoggle, sprite_hit_x,
           sprite_hit_mask);
  }

  // Needed for Knight Rider, possibly others.
  if (ppudead) {
    memset(fc->fceu->XBuf, 0x80, 256 * 240);
    Run6502(scanlines_per_frame * (256 + 85));
    ppudead--;
  } else {
    TRACELOC();
    Run6502(256 + 85);
    TRACEA(fc->fceu->RAM, 0x800);

    PPU_status |= 0x80;

    // Not sure if this is correct.
    // According to Matt Conte and my own tests, it is.
    // Timing is probably off, though.
    // NOTE:  Not having this here breaks a Super Donkey Kong game.
    PPU_values[3] = PPUSPL = 0;

    // I need to figure out the true nature and length of this delay.
    Run6502(12);

    if (VBlankON)
      fc->X->TriggerNMI();

    Run6502((scanlines_per_frame - 242) * (256 + 85) - 12);
    PPU_status &= 0x1f;
    Run6502(256);

    if (ScreenON || SpriteON) {
      if (GameHBIRQHook && (PPU_values[0] & 0x38) != 0x18)
        GameHBIRQHook(fc);
      if (PPU_hook) {
        for (int x = 0; x < 42; x++) {
	  PPU_hook(fc, 0x2000);
	  PPU_hook(fc, 0);
	}
      }
      if (GameHBIRQHook2)
        GameHBIRQHook2(fc);
    }
    Run6502(85 - 16);
    if (ScreenON || SpriteON) {
      RefreshAddr = TempAddr;
      if (PPU_hook) PPU_hook(fc, RefreshAddr & 0x3fff);
    }

    // Clean this stuff up later.
    any_sprites_on_line = numsprites = 0;
    ResetRL(fc->fceu->XBuf);

    Run6502(16 - cycle_parity);
    cycle_parity ^= 1;

    deemp = PPU_values[1] >> 5;
    for (scanline = 0; scanline < 240; ) {
      // scanline is incremented in DoLine.  Evil. :/
      deempcnt[deemp]++;
      DoLine();
    }

    // Triggers MMC5-specific interrupts, etc.
    if (MMC5Hack && (ScreenON || SpriteON)) {
      DCHECK(std::type_index(typeid(*fc->fceu->cartiface)) ==
	    std::type_index(typeid(MMC5))) << "\n" <<
	typeid(fc->fceu->cartiface).name() << "\n vs \n" <<
	typeid(MMC5).name();
      MMC5 *mmc5 = static_cast<MMC5*>(fc->fceu->cartiface);
      mmc5->MMC5HackHB(scanline);
    }

    int max = 0, maxref = 0;
    for (int x = 0; x < 7; x++) {
      if (deempcnt[x] > max) {
	max = deempcnt[x];
	maxref = x;
      }
      deempcnt[x]=0;
    }
    // FCEU_DispMessage("%2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x %d",
    //                  0,deempcnt[0],deempcnt[1],deempcnt[2],
    //                  deempcnt[3],deempcnt[4],deempcnt[5],
    //                  deempcnt[6],deempcnt[7],maxref);
    // memset(deempcnt,0,sizeof(deempcnt));
    fc->palette->SetNESDeemph(maxref, 0);
  } //else... to if (ppudead)
}

// Why do we need to do this? -tom7
// Note that weirdly, the T versions are 16-bit.
void PPU::FCEUPPU_LoadState(int version) {
  TempAddr = TempAddrT;
  RefreshAddr = RefreshAddrT;
}

PPU::PPU(FC *fc) :
  stateinfo {
    { NTARAM, 0x800, "NTAR" },
    { PALRAM, 0x20, "PRAM" },
    { SPRAM, 0x100, "SPRA" },
    { PPU_values, 0x4, "PPUR" },
    { &cycle_parity, 1, "PARI" },
    { &ppudead, 1, "DEAD" },
    { &PPUSPL, 1, "PSPL" },
    { &XOffset, 1, "XOFF" },
    { &vtoggle, 1, "VTOG" },
    { &RefreshAddrT, 2|FCEUSTATE_RLSB, "RADD" },
    { &TempAddrT, 2|FCEUSTATE_RLSB, "TADD" },
    { &VRAMBuffer, 1, "VBUF" },
    { &PPUGenLatch, 1, "PGEN" },
    { &pshift[0], 4|FCEUSTATE_RLSB, "PSH1" },
    { &pshift[1], 4|FCEUSTATE_RLSB, "PSH2" },
    // tom7 added this; needed in Ultimate Basketball.
    { &sprite_hit_x, 4|FCEUSTATE_RLSB, "Psph" },
    { &sprite_hit_mask, 1, "Pspd" },
  }, fc(fc) {
  // Constructor, empty.
}

void PPU::FCEUPPU_SaveState() {
  TempAddrT = TempAddr;
  RefreshAddrT = RefreshAddr;
}
