#ifndef __MINGW32__

struct Timer {
  Timer() {}
  void Start() {}
  double MS() { return 0.0; }
};

#else

#define byte win_byte_override
#  include <windows.h>
#undef byte

// Computes time elapsed in ms. Windows only.
struct Timer {
  Timer() {
    QueryPerformanceFrequency(&freq);
    Start();
  }

  void Start() {
    QueryPerformanceCounter(&starttime);
  }

  // This looks buggy (MS also "stops"); don't call it!
  // void Stop() {
  // QueryPerformanceCounter(&stoptime);
  // }

  double MS() {
    QueryPerformanceCounter(&stoptime);
    // Stop();
    double elapsed_ms = (stoptime.QuadPart - starttime.QuadPart) *
      1000.0 / freq.QuadPart;
    return elapsed_ms;
  }

 private:
  LARGE_INTEGER freq;
  LARGE_INTEGER starttime, stoptime;
};

#endif  // WIN32
