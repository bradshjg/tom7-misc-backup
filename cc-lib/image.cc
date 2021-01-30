
#include "image.h"

#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <cstring>
#include <tuple>

#include "lines.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "base/logging.h"

using namespace std;
using uint8 = uint8_t;
using uint32 = uint32_t;

namespace {
// Generated by bit7/embedcc.cc.
struct EmbeddedFont {
  static constexpr int CHAR_WIDTH = 9;
  static constexpr int CHAR_HEIGHT = 9;


  // No bounds checking!
  static constexpr bool GetBit(int c, int x, int y) {
    int b = c * (CHAR_WIDTH * CHAR_HEIGHT) + y * CHAR_WIDTH + x;
    return 0 != (words[b >> 6] & (1ULL << (63 - (b & 63))));
  }

  // SetPixel and ClearPixel called like SetPixel(x, y).
  template<class FS, class FC>
  static void Blit(int c, int x, int y,
	           FS SetPixel, FC ClearPixel = [](int, int){}) {
    if (c < 0 || c >= 128) return;
    for (int sy = 0; sy < CHAR_HEIGHT; sy++) {
      for (int sx = 0; sx < CHAR_WIDTH; sx++) {
	if (GetBit(c, sx, sy)) {
	  SetPixel(x + sx, y + sy);
	} else {
	  ClearPixel(x + sx, y + sy);
	}
      }
    }
  }

 private:
  static constexpr uint64_t words[162] = {
  0x0000000000000000ULL, 0x00003f3fdb6ff7fbULL, 0x6dce7e001f9fedb7ULL,
  0xfbfdcee73f000fcfULL, 0xf6dbfdcec36d9f80ULL, 0x000333fdfeff3f0fULL,
  0x030000c0f0fcff7fULL, 0x9f87818000003024ULL, 0x2110848180000000ULL,
  0x000c0f0781800000ULL, 0x007e41a0d068341bULL, 0xfcfc003f20d56935ULL,
  0x5a0dfe7e00020100ULL, 0x8043e01008040201ULL, 0x008040201f080402ULL,
  0x010000000000ff84ULL, 0x0201008040201008ULL, 0x7fc0000000002010ULL,
  0x08043fe100804020ULL, 0x1008040201008040ULL, 0x2010080402010f80ULL,
  0x0000000000000000ULL, 0x007c201008040000ULL, 0x000003e010080402ULL,
  0x01008040201f0000ULL, 0x00000000000000ffULL, 0x8000000007ffffffULL,
  0xffffc00000000000ULL, 0x000000001fffffffULL, 0xfff0f87c3e1f0f87ULL,
  0xc3e1f00783c1e0f0ULL, 0x783c1e0f07ffffffULL, 0xffffffffffffffffULL,
  0xfb6ffffedbffffb6ULL, 0xffedbdb36f6d6ddbULL, 0x5b5b76d491276d24ULL,
  0x495b493246dc9093ULL, 0x2424a48929292249ULL, 0x244992491244924cULL,
  0x9124002480000920ULL, 0x0002480000000000ULL, 0x0000000000000c06ULL,
  0x030180c000301800ULL, 0x198cc66000000000ULL, 0x00000cc667f999feULL,
  0x6633000001b3fb60ULL, 0xfc1b7f3600000702ULL, 0x999c1c1c1ccca070ULL,
  0x01f98c60773599c7ULL, 0xc0000030180c0000ULL, 0x00000000000c0c0cULL,
  0x060300c030000018ULL, 0x060180c060606000ULL, 0x0000030db7f8f0ccULL,
  0x660000000180c1f8ULL, 0xfc180c0000000000ULL, 0x0000180c06010000ULL,
  0x0000007e3f000000ULL, 0x0000000000000006ULL, 0x0300000006070707ULL,
  0x0707030000003c33ULL, 0x33db6f3330f00000ULL, 0x1c1e0b0180c060fcULL,
  0x00000f0cc060f1e0ULL, 0xc07e00000f806030ULL, 0x700c063e00000063ULL,
  0x3198fc0603018000ULL, 0x03f180f80603018fULL, 0x800000f8cc603f18ULL,
  0xcc63e00001fcc603ULL, 0x03030180c000007eULL, 0x61b0cfcc3619f800ULL,
  0x003fb0d867f0180cULL, 0x06000000000180c0ULL, 0x0030180000000000ULL,
  0xc06000180c020000ULL, 0x6060606018060180ULL, 0x00000000fc00001fULL,
  0x8000000180601806ULL, 0x0606060000007c63ULL, 0x3183818000603000ULL,
  0x7e61b6db6ce300f8ULL, 0x00001e0f0cc667fbULL, 0x0d8600003f986c37ULL,
  0xf30d86fe000007e6ULL, 0x36030180631f8000ULL, 0x0fc6330d86c3633fULL,
  0x000007fb0d80f060ULL, 0x30dfe00003fd86c0ULL, 0x7830180c0000007cULL,
  0x636031d86631f800ULL, 0x00c361b0dfec361bULL, 0x0c00003f06030180ULL,
  0xc060fc000007c0c0ULL, 0x6033199878000018ULL, 0xcc6663e19cc76180ULL,
  0x0006030180c06030ULL, 0x9fc000061b9dfedbULL, 0x6db6db6000038de6ULL,
  0xf36db3d9ec700000ULL, 0x786661b0d86661e0ULL, 0x0000fe61b0d86fe6ULL,
  0x030000001e19986cULL, 0x3679987e00003f98ULL, 0x6c361bf998c60000ULL,
  0x0fec3601f806c37fULL, 0x00000ff0c0603018ULL, 0x0c060000061b0d86ULL,
  0xc361b0cfc000030dULL, 0x8666330f07818000ULL, 0x0186c36db6cb47e3ULL,
  0x300000c3739f8307ULL, 0xe73b0c000061b0ccULL, 0xc7e0c0603000003fULL,
  0xc06061e18180ff00ULL, 0x0007c30180c06030ULL, 0x1f0000060380e038ULL,
  0x0e0380c00003e030ULL, 0x180c06030f800000ULL, 0x6078662100000000ULL,
  0x000000000000000fULL, 0xf7f80000380e0380ULL, 0x0000000000000000ULL,
  0x0f8063f318fe0000ULL, 0x180c07e3198cc67eULL, 0x0000000001f180c0ULL,
  0x601f0000003018fcULL, 0xc663318fc0000000ULL, 0x00fcc37f300f8000ULL,
  0x00786633181f0603ULL, 0x00000000001f98ccULL, 0x63f019f80060301bULL,
  0x8e6633198c00000cULL, 0x00070180c060fc00ULL, 0x00018000e030180cULL,
  0x461e000c060331b8ULL, 0xf8663300000380c0ULL, 0x6030180c1f800000ULL,
  0x00016cff6db6d860ULL, 0x00000000dc7b3998ULL, 0xcc60000000003f30ULL,
  0xd86c33f000000000ULL, 0x3f986c37f3018000ULL, 0x00000fcc6631f80cULL,
  0x070000000bc73319ULL, 0x80c00000000003f3ULL, 0x01fc067e00000180ULL,
  0xc1f830180c038000ULL, 0x0000018cc663338fULL, 0x4000000000c36199ULL,
  0x8cc3c00000000061ULL, 0xb6db67e330000000ULL, 0x00198fc183f19800ULL,
  0x0000000c6631f80cULL, 0x863e00000007e070ULL, 0x60e07e000001c180ULL,
  0xc030301807000001ULL, 0x80c06030180c0600ULL, 0x0003806030300c06ULL,
  0x0e0000000000706dULL, 0xb6c1c00000000c1eULL, 0x1d0885cee7000000ULL,
  };
};
}

inline static constexpr std::tuple<uint8, uint8, uint8, uint8>
Unpack32(uint32 color) {
  return {(uint8)((color >> 24) & 255),
	  (uint8)((color >> 16) & 255),
	  (uint8)((color >> 8) & 255),
	  (uint8)(color & 255)};
}

inline static constexpr uint32 Pack32(uint8 r, uint8 g, uint8 b, uint8 a) {
  return
    ((uint32)r << 24) | ((uint32)g << 16) | ((uint32)b << 8) | (uint32)a;
}

// TODO: Duplicate code between the different Load routines..

// static
ImageRGBA *ImageRGBA::Load(const string &filename) {
  vector<uint8> ret;
  int width, height, bpp_unused;
  uint8 *stb_rgba = stbi_load(filename.c_str(),
			      &width, &height, &bpp_unused, 4);
  const int bytes = width * height * 4;
  ret.resize(bytes);
  if (stb_rgba == nullptr) return nullptr;
  // TODO: Is this portable (or even correct) wrt to endianness?
  memcpy(ret.data(), stb_rgba, bytes);
  stbi_image_free(stb_rgba);
  // Does this move image data all the way in, or do we need to
  // write a move constructor manually? Better way?
  return new ImageRGBA(std::move(ret), width, height);
}

ImageRGBA *ImageRGBA::LoadFromMemory(const char *data, size_t size) {
  vector<uint8> ret;
  int width, height, bpp_unused;
  uint8 *stb_rgba = stbi_load_from_memory(
      (const stbi_uc*)data, size,
      &width, &height, &bpp_unused, 4);
  const int bytes = width * height * 4;
  ret.resize(bytes);
  if (stb_rgba == nullptr) return nullptr;
  // TODO: Is this portable (or even correct) wrt to endianness?
  memcpy(ret.data(), stb_rgba, bytes);
  stbi_image_free(stb_rgba);
  // Does this move image data all the way in, or do we need to
  // write a move constructor manually? Better way?
  return new ImageRGBA(std::move(ret), width, height);
}

ImageRGBA *ImageRGBA::LoadFromMemory(const vector<uint8> &filebytes) {
  return LoadFromMemory((const char *)filebytes.data(), filebytes.size());
}

ImageRGBA::ImageRGBA(const vector<uint8> &rgba, int width, int height)
  : width(width), height(height), rgba(rgba) {
  CHECK((int)rgba.size() == width * height * 4);
}

ImageRGBA::ImageRGBA(int width, int height)
  : width(width), height(height), rgba(width * height * 4) {
  Clear(0, 0, 0, 0);
}

bool ImageRGBA::Save(const std::string &filename) const {
  CHECK((int)rgba.size() == width * height * 4);
  return !!stbi_write_png(filename.c_str(),
			  width, height, 4, rgba.data(), 4 * width);
}

vector<uint8> ImageRGBA::SaveToVec() const {
  CHECK((int)rgba.size() == width * height * 4);
  return stbi_make_png_rgba(width, height, rgba.data());
}

string ImageRGBA::SaveToString() const {
  CHECK((int)rgba.size() == width * height * 4);
  const vector<uint8> v = stbi_make_png_rgba(width, height, rgba.data());
  string ret;
  ret.resize(v.size());
  memcpy(ret.data(), v.data(), v.size());
  return ret;
}

bool ImageRGBA::SaveJPG(const std::string &filename, int quality) const {
  CHECK((int)rgba.size() == width * height * 4);
  CHECK(quality >= 0 && quality <= 100) << quality;
  return !!stbi_write_jpg(filename.c_str(),
			  width, height, 4, rgba.data(), quality);
}

ImageRGBA *ImageRGBA::Copy() const {
  return new ImageRGBA(rgba, width, height);
}

ImageRGBA ImageRGBA::Crop32(int x, int y, int w, int h,
			    uint32 fill_color) const {
  CHECK(w > 0 && h > 0) << w << " " << h;
  const auto [r, g, b, a] = Unpack32(fill_color);
  
  ImageRGBA ret{w, h};
  // xx,yy in new image's coordinates
  for (int yy = 0; yy < h; yy++) {
    const int sy = yy + y;
    for (int xx = 0; xx < w; xx++) {
      const int sx = xx + x;
      // Fill color if not in bounds
      uint8 sr = r, sg = g, sb = b, sa = a;
      if (sx >= 0 && sx < width &&
	  sy >= 0 && sy < height) {
	const int sbase = (sy * width + sx) << 2;
	sr = rgba[sbase + 0];
	sg = rgba[sbase + 1];
	sb = rgba[sbase + 2];
	sa = rgba[sbase + 3];	
      }

      const int base = (yy * w + xx) << 2;
      ret.rgba[base + 0] = sr;
      ret.rgba[base + 1] = sg;
      ret.rgba[base + 2] = sb;
      ret.rgba[base + 3] = sa;      
    }
  }
  return ret;
}

ImageRGBA ImageRGBA::ScaleBy(int scale) const {
  // 1 is not useful, but it does work
  CHECK(scale >= 1);
  ImageRGBA ret(width * scale, height * scale);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      const uint32 color = GetPixel32(x, y);
      for (int yy = 0; yy < scale; yy++) {
	for (int xx = 0; xx < scale; xx++) {
	  ret.SetPixel32(x * scale + xx,
			 y * scale + yy,
			 color);
	}
      }
    }
  }
  return ret;
}

void ImageRGBA::Clear32(uint32 color) {
  // PERF: This can be optimized by writing 32 bits at a time,
  // but beware endianness, etc.
  Clear((color >> 24) & 255,
	(color >> 16) & 255,
	(color >> 8) & 255,
	color & 255);
}

void ImageRGBA::Clear(uint8 r, uint8 g, uint8 b, uint8 a) {
  for (int i = 0; i < width * height * 4; i += 4) {
    rgba[i + 0] = r;
    rgba[i + 1] = g;
    rgba[i + 2] = b;
    rgba[i + 3] = a;
  }
}

// PERF: Make inline?
void ImageRGBA::BlendPixel(int x, int y,
			   uint8 r, uint8 g, uint8 b, uint8 a) {
  if (x < 0 || x >= width ||
      y < 0 || y >= height) return;
  int i = (y * width + x) * 4;
  uint32 old_r = rgba[i + 0];
  uint32 old_g = rgba[i + 1];
  uint32 old_b = rgba[i + 2];
  // TODO: Figure out how to blend when dest is also transparent.

  // so a + oma = 255.
  uint32 oma = 0xFF - a;

  // we want (r * a/255) + (oldr * (1-a)/255),
  // which is (r * a)/255 + (oldr * (1-a))/255
  // which is (r * a + oldr * (1-a))/255
  //
  // (Note there are divisionless ways to compute /255, so this is
  // probably OK. Allegedly gcc can do this; might want to verify the
  // assembly.)
  uint32 rr = (((uint32)r * (uint32)a) + (old_r * oma)) / 0xFF;
  // PERF: These might be impossible. If compiler doesn't
  // eliminate them, could perhaps prove their impossibility and
  // remove?
  if (rr > 0xFF) rr = 0xFF;

  uint32 gg = (((uint32)g * (uint32)a) + (old_g * oma)) / 0xFF;
  if (gg > 0xFF) gg = 0xFF;

  uint32 bb = (((uint32)b * (uint32)a) + (old_b * oma)) / 0xFF;
  if (bb > 0xFF) bb = 0xFF;

  rgba[i + 0] = rr;
  rgba[i + 1] = gg;
  rgba[i + 2] = bb;
  rgba[i + 3] = 0xFF;
}

void ImageRGBA::BlendPixel32(int x, int y, uint32 color) {
  const auto [r, g, b, a] = Unpack32(color);
  BlendPixel(x, y, r, g, b, a);
}

void ImageRGBA::BlendRect(int x, int y, int w, int h,
			  uint8 r, uint8 g, uint8 b, uint8 a) {
  // Easy to clip this to the screen. We could call an unclipped
  // version of BlendPixel here.
  if (y < 0) { w += y; y = 0; }
  if (x < 0) { h += x; x = 0; }

  const int yover = (y + h) - height;
  if (yover > 0) h -= yover;
  const int xover = (x + w) - width;
  if (xover > 0) w -= xover;

  if (w <= 0 || h <= 0) return;
  
  for (int yy = y; yy < y + h; yy++) {
    for (int xx = x; xx < x + w; xx++) {
      BlendPixel(xx, yy, r, g, b, a);
    }
  }
}

void ImageRGBA::BlendRect32(int x, int y, int w, int h, uint32 color) {
  const auto [r, g, b, a] = Unpack32(color);
  BlendRect(x, y, w, h, r, g, b, a);
}

void ImageRGBA::BlendBox32(int x, int y, int w, int h,
			   uint32 color, std::optional<uint32> cco) {
  const int x1 = x + w - 1;
  const int y1 = y + h - 1;

  const uint32 corner_color = cco.has_value() ? cco.value() : color;

  // PERF: straight lines can be faster by skipping bresenham
  // XXX this is wrong for 1x1 and 2x2 boxes. fix!
  // Top
  BlendLine32(x + 1, y, x1 - 1, y, color);
  // Left
  BlendLine32(x, y + 1, x, y1 - 1, color);
  // Right
  BlendLine32(x1, y + 1, x1, y1 - 1, color);
  // Bottom
  BlendLine32(x + 1, y1, x1 - 1, y1, color);

  BlendPixel32(x, y, corner_color);
  BlendPixel32(x1, y, corner_color);
  BlendPixel32(x, y1, corner_color);
  BlendPixel32(x1, y1, corner_color);
}

// PERF: For many of these where we call blendpixel in a loop,
// we could probably benefit by doing premultiplied alpha.

void ImageRGBA::BlendText32(int x, int y, uint32 color, const string &s) {
  auto SetPixel = [this, color](int xx, int yy) {
      this->BlendPixel32(xx, yy, color);
    };
  // SetPixel will clip, but exit early if we are totally off-screen.
  if (y >= height || y < -EmbeddedFont::CHAR_HEIGHT) return;
  for (int i = 0; i < (int)s.size(); i++) {
    uint8 c = s[i];
    int xx = x + i * EmbeddedFont::CHAR_WIDTH;
    if (xx >= width) return;
    EmbeddedFont::Blit(c, xx, y, SetPixel, [](int x, int y) {});
  }
}

void ImageRGBA::BlendText(int x, int y,
			  uint8 r, uint8 g, uint8 b, uint8 a,
			  const string &s) {
  BlendText32(x, y, Pack32(r, g, b, a), s);
}

void ImageRGBA::BlendText2x32(int x, int y, uint32 color, const string &s) {
  for (int i = 0; i < (int)s.size(); i++) {
    // Here we draw to "0,0", and then this function scales and translates.
    auto SetPixel = [x, y, i, this, color](int px, int py) {
	const int xx = x + (i * EmbeddedFont::CHAR_WIDTH * 2) + px * 2;
	const int yy = y + py * 2;
	this->BlendPixel32(xx, yy, color);
	this->BlendPixel32(xx + 1, yy, color);
	this->BlendPixel32(xx, yy + 1, color);
	this->BlendPixel32(xx + 1, yy + 1, color);      
      };

    const uint8 c = s[i];
    EmbeddedFont::Blit(c, 0, 0, SetPixel, [](int x, int y) {});
  }
}

void ImageRGBA::BlendText2x(int x, int y,
			    uint8 r, uint8 g, uint8 b, uint8 a,
			    const string &s) {
  BlendText2x32(x, y, Pack32(r, g, b, a), s);
}

void ImageRGBA::BlendLine32(int x1, int y1, int x2, int y2,
			    uint32 color) {
  const auto [r, g, b, a] = Unpack32(color);
  for (const auto [x, y] : Line<int>{x1, y1, x2, y2}) {
    BlendPixel(x, y, r, g, b, a);
  }
}

void ImageRGBA::BlendLine(int x1, int y1, int x2, int y2,
			  uint8 r, uint8 g, uint8 b, uint8 a) {
  for (const auto [x, y] : Line<int>{x1, y1, x2, y2}) {
    BlendPixel(x, y, r, g, b, a);
  }
}

void ImageRGBA::BlendLineAA32(float x1, float y1, float x2, float y2,
			      uint32 color) {
  const auto [r, g, b, a] = Unpack32(color);
  auto Plot = [this, r, g, b, a](int x, int y, float f) {
      uint8 aa = f * a;
      this->BlendPixel(x, y, r, g, b, aa);
    };
  LineAA::Draw<int>(x1, y1, x2, y2, Plot);
}

void ImageRGBA::BlendLineAA(float x1, float y1, float x2, float y2,
                            uint8 r, uint8 g, uint8 b, uint8 a) {
  auto Plot = [this, r, g, b, a](int x, int y, float f) {
      uint8 aa = f * a;
      this->BlendPixel(x, y, r, g, b, aa);
    };
  LineAA::Draw<int>(x1, y1, x2, y2, Plot);
}

void ImageRGBA::BlendImage(int x, int y, const ImageRGBA &other) {
  // PERF can factor out the pixel clipping here, supposing the
  // compiler cannot.
  for (int yy = 0; yy < other.height; yy++) {
    int yyy = y + yy;
    for (int xx = 0; xx < other.width; xx++) {
      int xxx = x + xx;
      BlendPixel32(xxx, yyy, other.GetPixel32(xx, yy));
    }
  }
}

ImageA::ImageA(const vector<uint8> &alpha, int width, int height)
    : width(width), height(height), alpha(alpha) {
  CHECK((int)alpha.size() == width * height);
}

ImageA::ImageA(int width, int height) : width(width), height(height),
					alpha(width * height, 0) {
}

ImageA *ImageA::Copy() const {
  return new ImageA(alpha, width, height);
}

void ImageA::Clear(uint8 value) {
  for (int i = 0; i < (int)alpha.size(); i++) alpha[i] = value;
}

float ImageA::SampleBilinear(float x, float y) const {
  // Truncate to integer pixels.
  int ix = x;
  int iy = y;

  // subpixel values give us the interpolants
  float fx = x - ix;
  float fy = y - iy;

  // Get these four values.
  //         
  //  v00 ----- v10
  //   |   :fy   |
  //   |...*     | 1.0
  //   | fx      |
  //  v01 ----- v11
  //       1.0

  auto BClipPixel = [this](int x, int y) {
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      if (x >= width) x = width - 1;
      if (y >= height) y = height - 1;
      return GetPixel(x, y);
    };
  
  uint8 v00 = BClipPixel(ix, iy);
  uint8 v10 = BClipPixel(ix + 1, iy);
  uint8 v01 = BClipPixel(ix, iy + 1);
  uint8 v11 = BClipPixel(ix + 1, iy + 1);

  // v0 interpolates between v00 and v10 at fx.
  float v0 = (float)v00 + (float)(v10 - v00) * fx;
  float v1 = (float)v01 + (float)(v11 - v01) * fx;

  float v = v0 + (v1 - v0) * fy;
  return v / 255.0f;
}

ImageA ImageA::ResizeBilinear(int nwidth, int nheight) const {
  ImageA ret{nwidth, nheight};
  // XXX Sampling is probably a little off wrt width-1 stuff?
  for (int y = 0; y < nheight; y++) {
    const float fy = y / (float)nheight;
    const float sy = fy * height;
    for (int x = 0; x < nwidth; x++) {
      const float fx = x / (float)nwidth;
      const float sx = fx * width;

      float fv = SampleBilinear(sx, sy);
      uint8 v = fv > 1.0f ? 255 : fv < 0.0f ? 0 : roundf(fv * 255.0f);
      ret.SetPixel(x, y, v);
    }
  }
  return ret;
}

void ImageA::BlendText(int x, int y, uint8 v, const string &s) {
  auto SetPixel = [this, v](int xx, int yy) {
      this->BlendPixel(xx, yy, v);
    };
  // SetPixel will clip, but exit early if we are totally off-screen.
  if (y >= height || y < -EmbeddedFont::CHAR_HEIGHT) return;
  for (int i = 0; i < (int)s.size(); i++) {
    uint8 c = s[i];
    int xx = x + i * EmbeddedFont::CHAR_WIDTH;
    if (xx >= width) return;
    EmbeddedFont::Blit(c, xx, y, SetPixel, [](int x, int y) {});
  }
}


ImageRGBA ImageA::GreyscaleRGBA() const {
  ImageRGBA rgba(width, height);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      const uint8 v = GetPixel(x, y);
      // PERF if compiler can't optimize out the bounds checks here,
      // we can do it
      rgba.SetPixel(x, y, v, v, v, 0xFF);
    }
  }
  return rgba;
}

ImageRGBA ImageA::AlphaMaskRGBA(uint8 r, uint8 g, uint8 b) const {
  ImageRGBA rgba(width, height);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      const uint8 v = GetPixel(x, y);
      // PERF if compiler can't optimize out the bounds checks here,
      // we can do it
      rgba.SetPixel(x, y, r, g, b, v);
    }
  }
  return rgba;
}
