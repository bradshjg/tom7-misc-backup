/* Hooks and glue for using SDL through SML. Portable to win32, linux, osx.
   This is incomplete and a little messy. But we try to make it better.

    - Tom                   Last update: 1 Nov 2015
*/
#include <SDL.h>

// This actually includes all of the definitions within
// this compilation unit.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


#ifdef USE_DISPLAY_FORMAT
# if USE_DISPLAY_FORMAT == 0
#  error "sdlml.c no longer supports explicitly setting USE_DISPLAY_FORMAT. If you want console mode, use -DCONSOLE_MODE."
# endif
#endif

#ifdef CONSOLE_MODE
// Use the display format for surfaces, unless
// in console mode, where there is no display!
# define USE_DISPLAY_FORMAT 0
#else
# define USE_DISPLAY_FORMAT 1
#endif

/* XXX It seems that byte order doesn't affect the
   ordering of the colors within a Uint32. */
#if 0 /* SDL_BYTEORDER == SDL_BIG_ENDIAN */
  const Uint32 rmask = 0xff000000;
  const Uint32 gmask = 0x00ff0000;
  const Uint32 bmask = 0x0000ff00;
  const Uint32 amask = 0x000000ff;
  const Uint32 rshift = 24;
  const Uint32 gshift = 16;
  const Uint32 bshift = 8;
  const Uint32 ashift = 0;
#else
  const Uint32 rmask = 0x000000ff;
  const Uint32 gmask = 0x0000ff00;
  const Uint32 bmask = 0x00ff0000;
  const Uint32 amask = 0xff000000;
  const Uint32 ashift = 24;
  const Uint32 bshift = 16;
  const Uint32 gshift = 8;
  const Uint32 rshift = 0;
#endif

int ml_init() {
#ifdef CONSOLE_MODE

  if (SDL_Init(SDL_INIT_NOPARACHUTE) < 0) {
    printf("Unable to initialize SDL. (%s)\n", SDL_GetError());
    return 0;
  } else {
    return 1;
  }

#else

  if (SDL_Init(SDL_INIT_VIDEO |
               SDL_INIT_TIMER |
               SDL_INIT_JOYSTICK |
               SDL_INIT_AUDIO |
               /* for debugging */
               SDL_INIT_NOPARACHUTE |
               SDL_INIT_AUDIO) < 0) {

    printf("Unable to initialize SDL. (%s)\n", SDL_GetError());

    return 0;
  } else {
    SDL_EnableUNICODE(1);
    return 1;
  }

#endif
}

int ml_platform() {
#if defined(WIN32)
  return 1;
#elif defined(OSX)
  return 2;
#elif defined(LINUX)
  return 3;
#else
#  error "You must define (-D) WIN32, OSX, or LINUX when compiling sdlml.o. This is used to determine the platform."
  return -1;
#endif
}

int ml_consolemode() {
#ifdef CONSOLE_MODE
  return 1;
#else
  return 0;
#endif
}

void slock(SDL_Surface *surf) {
  if(SDL_MUSTLOCK(surf))
    SDL_LockSurface(surf);
}

void sulock(SDL_Surface *surf) {
  if (SDL_MUSTLOCK(surf))
    SDL_UnlockSurface(surf);
}

/* out has room for 512 chars */
void ml_joystickname (int i, char *out) {
  const char *p = SDL_JoystickName(i);
  if (p) {
    strncpy(out, p, 512);
  }
}

void ml_setjoystate(int i) {
  SDL_JoystickEventState(i ? SDL_ENABLE : SDL_IGNORE);
}

// This is a macro, so eta-expand for linking.
SDL_Surface *ml_loadbmp(const char *s) {
  return SDL_LoadBMP(s);
}

/* try to make a hardware surface, and, failing that,
   make a software surface */
SDL_Surface *ml_makesurface(int w, int h, int alpha) {

  /* PERF need to investigate relative performance
     of sw/hw surfaces */
  SDL_Surface *ss = 0;
#if 0
  SDL_CreateRGBSurface(SDL_HWSURFACE |
                       (alpha ? SDL_SRCALPHA : 0),
                       w, h, 32,
                       rmask, gmask, bmask,
                       amask);
#endif

  if (!ss) ss = SDL_CreateRGBSurface(SDL_SWSURFACE |
                                     (alpha ? SDL_SRCALPHA : 0),
                                     w, h, 32,
                                     rmask, gmask, bmask,
                                     amask);

  if (ss && !alpha) SDL_SetAlpha(ss, 0, 255);

  /* then convert to the display format. */
# if USE_DISPLAY_FORMAT
  if (ss) {
    SDL_Surface *rr;
    if (alpha) rr = SDL_DisplayFormatAlpha(ss);
    else rr = SDL_DisplayFormat(ss);
    SDL_FreeSurface(ss);
    return rr;
  } else return 0;
# else
  return ss;
# endif
}

SDL_Surface *ml_makescreen(int w, int h) {
  /* Can't use HWSURFACE here, because not handling this SDL_BlitSurface
     case mentioned in the documentation:

     "If either of the surfaces were in video memory, and the blit returns -2,
     the video memory was lost, so it should be reloaded with artwork
     and re-blitted."

     Plus, on Windows, the only time you get HWSURFACE is with FULLSCREEN.

     -- Adam
  */

  /* SDL_ANYFORMAT
     "Normally, if a video surface of the requested bits-per-pixel (bpp)
     is not available, SDL will emulate one with a shadow surface.
     Passing SDL_ANYFORMAT prevents this and causes SDL to use the
     video surface, regardless of its pixel depth."

     Probably should not pass this.

     -- Adam
  */

  /* SDL_DOUBLEBUF only valid with SDL_HWSURFACE! */
  SDL_Surface *ret = SDL_SetVideoMode(w, h, 32,
                                      SDL_SWSURFACE |
                                      SDL_RESIZABLE);
  return ret;
}

SDL_Surface *ml_makefullscreen(int w, int h) {
  /* Can't use HWSURFACE here, because not handling this SDL_BlitSurface
     case mentioned in the documentation:

     "If either of the surfaces were in video memory, and the blit returns -2,
     the video memory was lost, so it should be reloaded with artwork
     and re-blitted."

     Plus, on Windows, the only time you get HWSURFACE is with FULLSCREEN.

     -- Adam
  */

  /* SDL_ANYFORMAT
     "Normally, if a video surface of the requested bits-per-pixel (bpp)
     is not available, SDL will emulate one with a shadow surface.
     Passing SDL_ANYFORMAT prevents this and causes SDL to use the
     video surface, regardless of its pixel depth."

     Probably should not pass this.

     -- Adam
  */

  /* SDL_DOUBLEBUF only valid with SDL_HWSURFACE! */
  SDL_Surface *ret = SDL_SetVideoMode(w, h, 32,
                                      SDL_FULLSCREEN |
                                      SDL_DOUBLEBUF |
                                      SDL_HWSURFACE);
  return ret;
}

void ml_blitall(SDL_Surface *src, SDL_Surface *dst, int x, int y) {
  SDL_Rect r;
  r.x = x;
  r.y = y;
  SDL_BlitSurface(src, 0, dst, &r);
}

void ml_blit(SDL_Surface *src, int srcx, int srcy, int srcw, int srch,
             SDL_Surface *dst, int dstx, int dsty) {
  SDL_Rect sr;
  SDL_Rect dr;
  sr.x = srcx;
  sr.y = srcy;
  sr.w = srcw;
  sr.h = srch;
  dr.x = dstx;
  dr.y = dsty;
  SDL_BlitSurface(src, &sr, dst, &dr);
}

int ml_surfacewidth(SDL_Surface *src) {
  return src->w;
}

int ml_surfaceheight(SDL_Surface *src) {
  return src->h;
}

void ml_clearsurface(SDL_Surface *s, Uint32 color) {
  SDL_FillRect(s, 0, color);
}

SDL_Event *ml_newevent() {
  return (SDL_Event*) malloc (sizeof (SDL_Event));
}

int ml_eventtag(SDL_Event *e) {
  return e->type;
}

int ml_event8_2nd(SDL_Event *e) {
  return (((SDL_KeyboardEvent*)e)->which);
}

int ml_event8_3rd(SDL_Event *e) {
  return (((SDL_KeyboardEvent*)e)->state);
}

int ml_event8_4th(SDL_Event *e) {
  return (((SDL_JoyHatEvent*)e)->value);
}

int ml_event_joyaxis_value(SDL_JoyAxisEvent *e) {
  return e->value;
}

int ml_event_keyboard_sym(SDL_KeyboardEvent* e) {
  return e->keysym.sym;
}

int ml_event_keyboard_mod(SDL_KeyboardEvent* e) {
  return e->keysym.mod;
}

int ml_event_keyboard_unicode(SDL_KeyboardEvent* e) {
  return e->keysym.unicode;
}

int ml_event_mmotion_x(SDL_MouseMotionEvent* e) {
  return e->x;
}
int ml_event_mmotion_y(SDL_MouseMotionEvent* e) {
  return e->y;
}
int ml_event_mmotion_xrel(SDL_MouseMotionEvent *e) {
  return e->xrel;
}
int ml_event_mmotion_yrel(SDL_MouseMotionEvent *e) {
  return e->yrel;
}

int ml_event_mbutton_x(SDL_MouseButtonEvent *e) { return e->x; }
int ml_event_mbutton_y(SDL_MouseButtonEvent *e) { return e->y; }
int ml_event_mbutton_button(SDL_MouseButtonEvent *e) { return e->button; }


/* XXX should lock before calling (for certain modes)... */
void ml_drawpixel(SDL_Surface *surf, int x, int y,
                  int R, int G, int B) {
  // printf("RGB %d %d %d\n", R, G, B);
  Uint32 color = SDL_MapRGB(surf->format, R, G, B);
  switch (surf->format->BytesPerPixel) {
    case 1: // Assuming 8-bpp
      {
        Uint8 *bufp;
        bufp = (Uint8 *)surf->pixels + y*surf->pitch + x;
        *bufp = color;
      }
      break;
    case 2: // Probably 15-bpp or 16-bpp
      {
        Uint16 *bufp;
        bufp = (Uint16 *)surf->pixels + y*surf->pitch/2 + x;
        *bufp = color;
      }
      break;
    case 3: // Slow 24-bpp mode, usually not used
      {
        Uint8 *bufp;
        bufp = (Uint8 *)surf->pixels + y*surf->pitch + x * 3;
        if(SDL_BYTEORDER == SDL_LIL_ENDIAN) {
            bufp[0] = color;
            bufp[1] = color >> 8;
            bufp[2] = color >> 16;
          } else {
            bufp[2] = color;
            bufp[1] = color >> 8;
            bufp[0] = color >> 16;
          }
      }
      break;
    case 4: // Probably 32-bpp
      {
        Uint32 *bufp;
        bufp = (Uint32 *)surf->pixels + y*surf->pitch/4 + x;
        *bufp = color;
      }
      break;
    }
}

void ml_getpixel(SDL_Surface *surf, int x, int y,
                 Uint8 *R, Uint8 *G, Uint8 *B) {
  switch (surf->format->BytesPerPixel) {
    case 4: // Probably 32-bpp
      {
        Uint32 *bufp;
        bufp = (Uint32 *)surf->pixels + y*surf->pitch/4 + x;
        SDL_GetRGB(*bufp, surf->format, R, G, B);
      }
      break;
  default:
    printf("want 32bpp\n");
    abort();
  }
}

void ml_getpixela(SDL_Surface *surf, int x, int y,
                  Uint8 *R, Uint8 *G, Uint8 *B, Uint8 *A) {
  switch (surf->format->BytesPerPixel) {
    case 4: // Probably 32-bpp
      {
        Uint32 *bufp;
        bufp = (Uint32 *)surf->pixels + y*surf->pitch/4 + x;
        SDL_GetRGBA(*bufp, surf->format, R, G, B, A);
      }
      break;
  default:
    printf("want 32bpp\n");
    abort();
  }
}

void ml_unpixels(SDL_Surface *surf, unsigned char *RGBA) {
  int y, x, offset = 0;
  switch (surf->format->BytesPerPixel) {
    case 4: // Probably 32-bpp
      for (y = 0; y < surf->h; y++) {
        for (x = 0; x < surf->w; x++) {
          /* PERF this can probably be a lot more efficient by
             unrolling SDL_MapRGBA; we unpack from the word and
             then effectively repack. */
          Uint32 *bufp = (Uint32 *)surf->pixels + y*surf->pitch/4 + x;
          Uint32 color = SDL_MapRGBA(surf->format,
                                     RGBA[offset],
                                     RGBA[offset + 1],
                                     RGBA[offset + 2],
                                     RGBA[offset + 3]);
          *bufp = color;
          offset += 4;
        }
      }
      break;
    case 3: // Slow 24-bpp mode, usually not used
      for (y = 0; y < surf->h; y++) {
        for (x = 0; x < surf->w; x++) {
          Uint8 *bufp = (Uint8 *)surf->pixels + y*surf->pitch + x * 3;
          bufp[0] = RGBA[offset];
          bufp[1] = RGBA[offset + 1];
          bufp[2] = RGBA[offset + 2];
          offset += 4;
        }
      }
      break;
  default:
    printf("unsupported pixel format in ml_unpixels (want 24 or 32 bpp)\n");
    abort();
  }
}

// There may be byte order issues in the 24bpp case?
void ml_pixels(SDL_Surface *surf, unsigned char *RGBA) {
  int y, x, offset = 0;
  switch (surf->format->BytesPerPixel) {
    case 4: // Probably 32-bpp
      for (y = 0; y < surf->h; y++) {
        for (x = 0; x < surf->w; x++) {
          /* PERF this can probably be a lot more efficient by
             unrolling SDL_GetRGBA based on endianness; we unpack from
             the word and then effectively repack. */
          Uint32 *bufp;
          bufp = (Uint32 *)surf->pixels + y*surf->pitch/4 + x;
          SDL_GetRGBA(*bufp, surf->format,
                      &RGBA[offset],
                      &RGBA[offset + 1],
                      &RGBA[offset + 2],
                      &RGBA[offset + 3]);
          offset += 4;
        }
      }
      break;
    case 3: // Slow 24-bpp mode, usually not used
      for (y = 0; y < surf->h; y++) {
        for (x = 0; x < surf->w; x++) {
          Uint8 *bufp;
          bufp = (Uint8 *)surf->pixels + y*surf->pitch + x * 3;
          // assume (SDL_BYTEORDER == SDL_LIL_ENDIAN) ?
          RGBA[offset] = bufp[0];
          RGBA[offset + 1] = bufp[1];
          RGBA[offset + 2] = bufp[2];
          // Assume 100% alpha.
          RGBA[offset + 3] = 0xFF;
          offset += 4;
        }
      }
      break;
  default:
    printf("unsupported pixel format in ml_pixels (want 24 or 32 bpp)\n");
    abort();
  }
}

// Note that this always packs words as 0xRRGGBBAA, regardless
// of native byte order.
void ml_pixels32(SDL_Surface *surf, Uint32 *RGBA) {
  int y, x, offset = 0;
  switch (surf->format->BytesPerPixel) {
    case 4: // Probably 32-bpp
      for (y = 0; y < surf->h; y++) {
        for (x = 0; x < surf->w; x++) {
          /* PERF this can probably be a lot more efficient by
             unrolling SDL_GetRGBA based on endianness; we unpack from
             the word and then effectively repack. */
          Uint32 *bufp;
          bufp = (Uint32 *)surf->pixels + y*surf->pitch/4 + x;
          Uint8 r, g, b, a;
          SDL_GetRGBA(*bufp, surf->format,
                      &r, &g, &b, &a);
          RGBA[offset] = (r << 24) | (g << 16) | (b << 8) | a;
          offset++;
        }
      }
      break;
    case 3: // Slow 24-bpp mode, usually not used
      for (y = 0; y < surf->h; y++) {
        for (x = 0; x < surf->w; x++) {
          Uint8 *bufp;
          bufp = (Uint8 *)surf->pixels + y*surf->pitch + x * 3;
          // assume (SDL_BYTEORDER == SDL_LIL_ENDIAN) ?
          Uint8 r = bufp[0];
          Uint8 g = bufp[1];
          Uint8 b = bufp[2];
          // Assume 100% alpha.
          Uint8 a = 0xFF;
          RGBA[offset] = (r << 24) | (g << 16) | (b << 8) | a;

          offset++;
        }
      }
      break;
  default:
    printf("unsupported pixel format in ml_pixels32 (want 24 or 32 bpp)\n");
    abort();
  }
}

void ml_fillrect(SDL_Surface *dst, int x, int y, int w, int h,
                 int r, int g, int b) {
  Uint32 c = SDL_MapRGB(dst->format, r, g, b);
  SDL_Rect rect;
  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;
  SDL_FillRect(dst, &rect, c);
}

void ml_fillrecta(SDL_Surface *dst, int x, int y, int w, int h,
                  int r, int g, int b, int a) {
  Uint32 c = SDL_MapRGBA(dst->format, r, g, b, a);
  SDL_Rect rect;
  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;
  SDL_FillRect(dst, &rect, c);
}

SDL_Surface *ml_alphadim(SDL_Surface *src) {
  /* must be 32 bpp */
  if (src->format->BytesPerPixel != 4) return 0;

  int ww = src->w, hh = src->h;

  SDL_Surface *ret = ml_makesurface(ww, hh, 1);

  if (!ret) return 0;

  slock(ret);
  slock(src);

  int y, x;

  for(y = 0; y < hh; y ++) {
    for(x = 0; x < ww; x ++) {
      Uint32 color = *((Uint32 *)src->pixels + y*src->pitch/4 + x);

      /* divide alpha channel by 2 */
      /* XXX this seems to be wrong on powerpc (dims some other channel
         instead). but if the masks are right, how could this go wrong? */
      color =
        (color & (rmask | gmask | bmask)) |
        (((color & amask) >> 1) & amask);

      *((Uint32 *)ret->pixels + y*ret->pitch/4 + x) = color;

    }
  }

  sulock(src);
  sulock(ret);

  return ret;
}

SDL_Surface *ml_loadimage(const char *filename) {
  int width = 0, height = 0, bpp_unused = 0;
  Uint8 *stb_rgba = stbi_load(filename,
                              &width, &height, &bpp_unused, 4);
  if (stb_rgba == 0) return 0;
  const int bytes = width * height * 4;
  SDL_Surface *ret = ml_makesurface(width, height, 1);
  ml_unpixels(ret, stb_rgba);
  free(stb_rgba);
  return ret;
}

int ml_version_packed() {
  const SDL_version *v = SDL_Linked_Version();
  fprintf(stderr, "Version: %u.%u.%u\n", v->major, v->minor, v->patch);
  return (v->major << 16) | (v->minor << 8) | v->patch;
}
