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

namespace LifeStudioHeadAPI
{

typedef int UserID;

struct IMuscle;
struct IBone;
struct IMacroMuscle;

struct IAnimator
{
  virtual bool Load(const char *buffer, int sizeOfBuffer) = 0;
  virtual int SaveBufferSize() = 0;
  virtual bool Save(char *buffer) = 0;
  virtual IMuscle *MuscleByName(const char *name) = 0;
  virtual IMuscle *Muscle(int number) = 0;
  virtual int MusclesCount() const = 0;
  virtual IBone *BoneByName(const char *name) = 0;
  virtual IBone *Bone(int number) = 0;
  virtual IBone *BoneByType(unsigned long type, IBone *prev = 0) = 0;
  virtual int BonesCount() const = 0;
  virtual void FillUnused(bool fill) = 0;
  virtual bool FillUnused() const = 0;
  virtual bool Process(float *vertexArray, int step) = 0;
  virtual int VerticesCount() const = 0;
  virtual void ClearAllMacroMuscles() = 0;
  virtual void AddMacroMuscle(IMacroMuscle *muscle, float expression) = 0;
  virtual void MultMacroMuscle(IMacroMuscle *muscle, float expression) = 0;
  virtual void ComputePhysics() = 0;
  virtual void RegisterMacroMuscle(IMacroMuscle *muscle) = 0;
  virtual void UnregisterMacroMuscle(IMacroMuscle *muscle) = 0;
  virtual void ClearAllRegistration() = 0;
  virtual void CollectUserItems(bool use) = 0;
  virtual bool CollectUserItems() const = 0;
  virtual UserID UserItem(const char *itemName) = 0;
  virtual int UserValuesCount(UserID id) = 0;
  virtual float UserValue(UserID id, int number) = 0;
  virtual void ClearUserItems() = 0;
  virtual void ComputeBonesHierarchy() = 0;
  virtual bool HasNeck() const = 0;
  virtual void NeckProcessing2(bool use) = 0;
  virtual bool NeckProcessing2() const = 0;
  virtual IAnimator *Clone() = 0;
  virtual void Destroy() = 0;

  static LIFESTUDIOHEADAPI_API IAnimator *__stdcall Create();
};

};

#endif
