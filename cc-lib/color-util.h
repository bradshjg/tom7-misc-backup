#ifndef _CC_LIB_COLORUTIL_H
#define _CC_LIB_COLORUTIL_H

#include <tuple>

struct ColorUtil {
  static void HSVToRGB(float hue, float saturation, float value,
                       float *r, float *g, float *b);

  // Convert to CIE L*A*B*.
  // RGB channels are nominally in [0, 1].
  // Here RGB is interpreted as sRGB with a D65 reference white.
  // L* is nominally [0,100]. A* and B* are unbounded but
  // are "commonly clamped to -128 to 127".
  static void RGBToLAB(float r, float g, float b,
                       float *ll, float *aa, float *bb);
  static std::tuple<float, float, float>
  RGBToLAB(float r, float g, float b);

  // CIE1994 distance between sample color Lab2 and reference Lab1.
  // ** Careful: This may not even be symmetric! **
  // Note: This has been superseded by an even more complicated function
  // (CIEDE2000) if you are doing something very sensitive.
  static float DeltaE(float l1, float a1, float b1,
                      float l2, float a2, float b2);
};

#endif
