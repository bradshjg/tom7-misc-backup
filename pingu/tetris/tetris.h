
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <cmath>
#include <unordered_set>

#include "../fceulib/emulator.h"
#include "../fceulib/simplefm2.h"
#include "../fceulib/simplefm7.h"
#include "../fceulib/x6502.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "arcfour.h"
#include "image.h"

constexpr int MEM_RNG1 = 0x17;
constexpr int MEM_RNG2 = 0x18;

constexpr int MEM_BOARD_START = 0x400;
constexpr int MEM_CURRENT_PIECE = 0x62;
constexpr int MEM_NEXT_PIECE = 0xBF;
constexpr int MEM_CURRENT_X = 0x40;
// (mod 256)
constexpr int MEM_DROP_COUNT = 0x1A;
// "current" piece in its standard orientation
constexpr int MEM_LAST_DROP = 0x19;

// unrotated piece identity
enum Piece {
  // square
  PIECE_O = 0,
  PIECE_L,
  PIECE_J,
  PIECE_S,
  PIECE_Z,
  PIECE_I,
  PIECE_T,

  NUM_PIECES,
};


// A piece in a specific orientation, using the same byte as
// the NES uses.
enum Shape : uint8 {
  I_VERT = 0x11,
  I_HORIZ = 0x12,
  SQUARE = 0x0a,
  // given as the way the pointy side faces
  T_UP = 0x00,
  T_DOWN = 0x02,
  T_LEFT = 0x03,
  T_RIGHT = 0x01,

  // given as the way the long edge points
  J_UP = 0x04,  // J
  J_LEFT = 0x07, // logical not
  J_DOWN = 0x06, // r
  J_RIGHT = 0x05,

  Z_HORIZ = 0x08,
  Z_VERT = 0x09,

  S_HORIZ = 0x0b,
  S_VERT = 0x0C,

  // as the way the long edge points
  L_UP = 0x0D, // L
  L_LEFT = 0x10, // toboggan
  L_DOWN = 0x0F, // 7
  L_RIGHT = 0x0E,
};

// printable char
static char PieceChar(Piece p) {
  switch (p) {
  case PIECE_O: return 'O';
  case PIECE_L: return 'L';
  case PIECE_J: return 'J';
  case PIECE_S: return 'S';
  case PIECE_Z: return 'Z';
  case PIECE_I: return 'I';
  case PIECE_T: return 'T';
  default: return '?';
  }
}

// 0x17-0x1A
struct State {
  // Initialized to constants at addr 0x80BC.
  uint8_t rng1 = 0x89; // MEM_RNG1
  uint8_t rng2 = 0x88; // MEM_RNG2

  // Cleared when zeroing out memory, then again.
  uint8_t last_drop = 0x00;  // MEM_LAST_DROP
  // Cleared when zeroing out the memory on boot.
  uint8_t drop_count = 0x00;  // MEM_DROP_COUNT
};



// This is the code at 0xAB47. Every JSR to this sets
// X = 17 (M_RNG1) and Y = 2 (its size), so we just
// take those as constants.
//
// I think this is a two-tap LFSR. Its period is 32767.
[[maybe_unused]]
inline State NextRNG(State s) {
  uint8_t x = 17;
  uint8_t y = 2;
  // this is zeropage[x], i.e. rng1
  //      lda     $00,x
  uint8_t a = s.rng1;
  // # means the immediate value 02
  //      and     #$02
  a &= 0x02;
  // (flags?)

  //      sta     $00
  uint8_t zp0 = a;

  // this is zeropage[x + 1], i.e. rng2
  //      lda     $01,x
  a = s.rng2;

  //      and     #$02
  a &= 0x02;

  //      eor     $00
  a ^= zp0;
  bool zero_flag = (a == 0x00);

  //      clc
  bool carry_flag = false;

  //      beq     $AB57
  if (zero_flag)
    goto _AB57;

  //      sec
  carry_flag = true;

  // Looks like this just boils down to a 16-bit
  // rotate, once we have the carry flag set.
 _AB57:
  //      ror     $00,x
  bool ctmp1 = (s.rng1 & 0x1) == 0x1;
  s.rng1 = (s.rng1 >> 1) | (carry_flag ? 128 : 0);
  carry_flag = ctmp1;

  // these do not affect carry flag.
  //      inx
  x++;
  //      dey
  y--;
  zero_flag = (y == 0x00);

  //      bne     $AB57
  // assuming y is always 2, runs one more time.
  CHECK(!zero_flag);
  bool ctmp2 = (s.rng2 & 0x1) == 0x1;
  s.rng2 = (s.rng2 >> 1) | (carry_flag ? 128 : 0);
  carry_flag = ctmp2;

  x++;
  y--;
  zero_flag = (y == 0x00);
  //      bne     $AB57
  CHECK(zero_flag);

  //      rts
  return s;
}

// Generate next piece.
// This is the code at $9907.
[[maybe_unused]]
inline State NextPiece(State s) {
  constexpr std::array<uint8_t, 8> PIECES = {
    0x02, 0x07, 0x08, 0x0A, 0x0B, 0x0E, 0x12,
    // not used ?
    0x02,
  };

  uint8_t x = 0;

  //  inc $001a  (MEM_DROP_COUNT)
  s.drop_count++;
  //  lda $0017  (MEM_RNG_1);
  uint8_t a = s.rng1;
  //  clc
  bool carry_flag = false;
  //  adc $001a = #$0a
  // (recall NES does not even implement decimal flag)
  a += s.drop_count;

  //  and #$07
  a &= 0x7;
  //  cmp #$07
  //  beq $991c
  if (a == 0x7)
    goto _991C;

  //  tax
  x = a;

  // 994e is a (constant?) table of pieces in their
  // original orientations
  //  lda $994e,x
  a = PIECES[x];

  // If this is a new piece, succeed.
  //  cmp $0019  (MEM_LAST_DROP)
  //  bne $9938
  if (a != s.last_drop)
    goto _9938;

 _991C:
  // We get here if the first random number was out
  // of bounds, or the drop was the same as the
  // previous.

  //  ldx #$17
  //  ldy #$02
  //  jsr $ab47  (NextRNG)
  s = NextRNG(s);

  //  lda $0017
  a = s.rng1;
  //  and #$07
  a &= 0x7;
  //  clc
  carry_flag = false;
  //  adc $0019 = #$0e
  a += s.drop_count;

 _992A:
  // This code is (a %= 7) implemented as a loop,
  // which is not really necessary since a is
  // known to be in [0,7] due to the AND above.
  // We could simply check for == 7 and replace
  // with zero in that case, or just use the fact
  // that the table at position 7 contains the
  // same entry as at zero!

  //  cmp #$07
  carry_flag = (a >= 0x07);
  //  bcc $9934
  if (!carry_flag)
    goto _9934;

  //  sec
  carry_flag = true;
  //  sbc #$07
  a -= 7;

  goto _992A;

 _9934:
  //  tax
  x = a;
  //  lda $994e,x
  a = PIECES[x];
  // In this situation we fall through to allowing
  // the drop to be equal.

 _9938:
  // Success!
  //  sta $0019  (MEM_LAST_DROP)
  s.last_drop = a;

  //  rts
  return s;
}

// Square types:
// EF = empty
// 7B = white square w/shine, like in T, square, line
// 7D = blue, like in J
// 7C = cyan, like Z

// mems
// 1a: counts up with each block
// 0x40, 0x60: dropping block's x pos.
//   this depends on rotation somehow; you can't put it
//   in x=0 in some rotations.
// 0xb1: frame counter, goes when paused
// 0x17, 0x18: probably RNG? goes when paused

// next piece: 0x19, 0xBF (seem equal). BF can be modified
//   and overwrites 19.
//
// 0x0A - square
// 0x0E - L shape
// 0x0B - S
// 0x02 - T ?
// 0x07 - J
// 0x08 - Z
// 0x12 - bar

// These all hold the current piece: 0x42, 0x62, 0xAC.
// overwriting 0x62 immediately changes the current piece, cool.
//
// Note that these have rotations:
// 0x11 - rotated bar (vertical)
// 0x7, 0x6, 0x5, 0x4 - rotated J
// 0x8, 0x9 - z
// 0x0a - square
// 0x0, 0x1, 0x2, 0x3 - T
// 0x0D, 0x0E, 0x0F, 0x10 - L
// 0xB, 0xC - S
// this all makes sense!

inline Piece DecodePiece(uint8_t p) {
  switch (p) {
  case 0x11:
  case 0x12: return PIECE_I;
  case 0x7:
  case 0x6:
  case 0x5:
  case 0x4: return PIECE_J;
  case 0x8:
  case 0x9: return PIECE_Z;
  case 0x0A: return PIECE_O;
  case 0x0:
  case 0x1:
  case 0x2:
  case 0x3: return PIECE_T;
  case 0x0D:
  case 0x0E:
  case 0x0F:
  case 0x10: return PIECE_L;
  case 0x0B:
  case 0x0C: return PIECE_S;
  default:
    LOG(FATAL) << "Can't decode byte: " << (int)p;
    return PIECE_I;
  }
}


// 10x20, row major.
inline std::vector<uint8_t> GetBoard(const Emulator &emu) {
  std::vector<uint8_t> ret(10 * 20);
  for (int i = 0; i < 10 * 20; i++) {
    ret[i] = emu.ReadRAM(MEM_BOARD_START + i);
  }
  return ret;
}

inline void SaveScreenshot(const string &filename, Emulator *emu) {
  std::vector<uint8_t> save = emu->SaveUncompressed();
  emu->StepFull(0, 0);

  ImageRGBA img(emu->GetImage(), 256, 256);
  img.Save(filename);
  printf("Wrote %s\n", filename.c_str());

  emu->LoadUncompressed(save);
}
