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
  virtual int LIFESTUDIOHEADAPICALL Size(const char *name) = 0;
  virtual bool LIFESTUDIOHEADAPICALL Get(const char *name, char *buffer) = 0;
};

struct ITransformer : public IAnimator
{
  virtual bool LIFESTUDIOHEADAPICALL Load(ITransformerInput *input) = 0;
  virtual void LIFESTUDIOHEADAPICALL OutputAnimator(IAnimator *animator) = 0;
  virtual IAnimator *LIFESTUDIOHEADAPICALL OutputAnimator() const = 0;
  virtual void LIFESTUDIOHEADAPICALL Generate() = 0;

  static LIFESTUDIOHEADAPI_API ITransformer *LIFESTUDIOHEADAPICALL Create();
};

};

#endif
