#ifndef _LIFESTUDIOHEADAPITRANSFORM_H_
#define _LIFESTUDIOHEADAPITRANSFORM_H_
// ============================================================================
//  LifeStudio:Head API - interoperability declarations (transformer).
// ============================================================================

#include "LifeStudioHeadAPI.h"

namespace LifeStudioHeadAPI
{

struct ITransformerInput
{
  virtual int Size(const char *name) = 0;
  virtual bool Get(const char *name, char *buffer) = 0;
};

struct ITransformer : public IAnimator
{
  virtual bool Load(ITransformerInput *input) = 0;
  virtual void OutputAnimator(IAnimator *animator) = 0;
  virtual IAnimator *OutputAnimator() const = 0;
  virtual void Generate() = 0;

  static LIFESTUDIOHEADAPI_API ITransformer *__stdcall Create();
};

};

#endif
