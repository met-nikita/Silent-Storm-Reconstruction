#ifndef _LIFESTUDIOHEADAPI_H_
#define _LIFESTUDIOHEADAPI_H_
// ============================================================================
//  LifeStudio:Head API - interoperability declarations
//
// ============================================================================

#ifdef LIFESTUDIOHEADAPI_EXPORTS_LIB
#define LIFESTUDIOHEADAPI_API
#else
#ifdef LIFESTUDIOHEADAPI_EXPORTS
#define LIFESTUDIOHEADAPI_API __declspec(dllexport)
#else
#define LIFESTUDIOHEADAPI_API __declspec(dllimport)
#endif
#endif

#ifndef LIFESTUDIOHEADAPICALL
#define LIFESTUDIOHEADAPICALL __stdcall
#endif

namespace LifeStudioHeadAPI
{

typedef int UserID;

struct IMuscle;
struct IBone;
struct IMacroMuscle;

struct IAnimator
{
  virtual bool LIFESTUDIOHEADAPICALL Load(const char *buffer, int sizeOfBuffer) = 0;
  virtual bool LIFESTUDIOHEADAPICALL Load(const char *buffer, int sizeOfBuffer, float threshold, unsigned int maxNMuscles) = 0;
  virtual int LIFESTUDIOHEADAPICALL SaveBufferSize() = 0;
  virtual bool LIFESTUDIOHEADAPICALL Save(char *buffer) = 0;
  virtual IMuscle *LIFESTUDIOHEADAPICALL MuscleByName(const char *name) = 0;
  virtual IMuscle *LIFESTUDIOHEADAPICALL Muscle(int number) = 0;
  virtual int LIFESTUDIOHEADAPICALL MusclesCount() const = 0;
  virtual IBone *LIFESTUDIOHEADAPICALL BoneByName(const char *name) = 0;
  virtual IBone *LIFESTUDIOHEADAPICALL Bone(int number) = 0;
  virtual IBone *LIFESTUDIOHEADAPICALL BoneByType(unsigned long type, IBone *prev = 0) = 0;
  virtual int LIFESTUDIOHEADAPICALL BonesCount() const = 0;
  virtual void LIFESTUDIOHEADAPICALL FillUnused(bool fill) = 0;
  virtual bool LIFESTUDIOHEADAPICALL FillUnused() const = 0;
  virtual bool LIFESTUDIOHEADAPICALL Process(float *vertexArray, int step) = 0;
  virtual int LIFESTUDIOHEADAPICALL VerticesCount() const = 0;
  virtual void LIFESTUDIOHEADAPICALL ClearAllMacroMuscles() = 0;
  virtual void LIFESTUDIOHEADAPICALL AddMacroMuscle(IMacroMuscle *muscle, float expression) = 0;
  virtual void LIFESTUDIOHEADAPICALL MultMacroMuscle(IMacroMuscle *muscle, float expression) = 0;
  virtual void LIFESTUDIOHEADAPICALL ComputePhysics() = 0;
  virtual void LIFESTUDIOHEADAPICALL RegisterMacroMuscle(IMacroMuscle *muscle) = 0;
  virtual void LIFESTUDIOHEADAPICALL UnregisterMacroMuscle(IMacroMuscle *muscle) = 0;
  virtual void LIFESTUDIOHEADAPICALL ClearAllRegistration() = 0;
  virtual void LIFESTUDIOHEADAPICALL CollectUserItems(bool use) = 0;
  virtual bool LIFESTUDIOHEADAPICALL CollectUserItems() const = 0;
  virtual UserID LIFESTUDIOHEADAPICALL UserItem(const char *itemName) = 0;
  virtual int LIFESTUDIOHEADAPICALL UserValuesCount(UserID id) = 0;
  virtual float LIFESTUDIOHEADAPICALL UserValue(UserID id, int number) = 0;
  virtual void LIFESTUDIOHEADAPICALL ClearUserItems() = 0;
  virtual void LIFESTUDIOHEADAPICALL ComputeBonesHierarchy() = 0;
  virtual bool LIFESTUDIOHEADAPICALL HasNeck() const = 0;
  virtual void LIFESTUDIOHEADAPICALL Multiplier(float value) = 0;
  virtual float LIFESTUDIOHEADAPICALL Multiplier() const = 0;
  virtual unsigned int LIFESTUDIOHEADAPICALL Flags() const = 0;
  virtual void LIFESTUDIOHEADAPICALL Destroy() = 0;

  static LIFESTUDIOHEADAPI_API IAnimator *LIFESTUDIOHEADAPICALL Create();
  static LIFESTUDIOHEADAPI_API IAnimator *LIFESTUDIOHEADAPICALL CreateBlending();
};

};

#endif
