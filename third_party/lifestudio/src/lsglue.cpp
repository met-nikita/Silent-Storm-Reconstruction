// ============================================================================
//  LifeStudio:Head - host-side init glue (reconstructed)
// ============================================================================

#include "LifeStudioHeadAPIInit.h"
#include <cstdint>

namespace LifeStudioHeadAPI
{

// IOptions::Create is a real export of LifeStudioHeadAPI.dll. We only ever call
// it from Init() (and discard the returned pointer), so this minimal import
// declaration is all that is needed; the import library synthesized from
// def/LifeStudioHeadAPI.def resolves it to the DLL at link time.
struct IOptions
{
  static __declspec(dllimport) IOptions *__stdcall Create(void *p, unsigned int a, unsigned int b);
};

// The host curve-evaluation callback the DLL invokes during morphing.
//   ctrl layout:  [0] int  keyCount
//                 [1] const Key*    keys    (Key = { float x; float _; }, stride 8)
//                 [2] const float*  coeffs  (4 floats per segment, stride 16)
// For the segment i with keys[i].x <= x <= keys[i+1].x:
//   result = c0*dl + c1*dr + c2*dl*dl*dr + c3*dl*dr*dr   (dl = x-keys[i].x, dr = x-keys[i+1].x)
float __stdcall Compute(void *ctrl, float x)
{
  struct Key { float x; float _pad; };
  const int n = *reinterpret_cast<const int *>(ctrl);
  const Key *keys = *reinterpret_cast<const Key *const *>(reinterpret_cast<const char *>(ctrl) + 4);
  const float *coeffs = *reinterpret_cast<const float *const *>(reinterpret_cast<const char *>(ctrl) + 8);
  for (int i = 0; i < n - 1; ++i)
  {
    const float kl = keys[i].x;
    const float kr = keys[i + 1].x;
    if (kl <= x && x <= kr)
    {
      const float *c = coeffs + i * 4;
      const float dl = x - kl;
      const float dr = x - kr;
      return c[0] * dl + c[1] * dr + c[2] * dl * dl * dr + c[3] * dl * dr * dr;
    }
  }
  return 0.0f;
}

// Scratch slot the DLL stores for its own use (its meaning is internal to the DLL).
static unsigned int tmp = 0;
// Second Init-lib static; retail Init calls TemporarySignFunction() and discards
// the result (@0x3cc940 just returns this slot).
static unsigned int sign = 0;

unsigned int __stdcall TemporarySignFunction()
{
  return sign;
}

void __stdcall Init()
{
  // Hand the DLL the address of our Compute() callback, encoded exactly the way
  // the DLL expects to decode it (it recovers the full 32-bit pointer regardless
  // of where Compute() happens to load).
  const std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(&Compute);
  IOptions::Create(&tmp,
                   static_cast<unsigned int>(addr) & 0x1EF4FFFFu,
                   (static_cast<unsigned int>(addr) >> 16) + 0x33E50000u);
  TemporarySignFunction();
}

};
