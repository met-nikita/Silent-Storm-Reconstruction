#ifndef _LIFESTUDIOHEADAPIMMTS_H_
#define _LIFESTUDIOHEADAPIMMTS_H_
// ============================================================================
//  LifeStudio:Head API - interoperability declarations (macro-muscle tree /
//  sequencer). 
// ============================================================================

#include "LifeStudioHeadAPI.h"

namespace LifeStudioHeadAPI
{

struct IMMTree
{
  virtual bool LIFESTUDIOHEADAPICALL Load(const char *file_name) = 0;
  virtual bool LIFESTUDIOHEADAPICALL Load(const char *buffer, int size) = 0;
  virtual IMacroMuscle *LIFESTUDIOHEADAPICALL RootMacroMuscle() const = 0;
  virtual IMacroMuscle *LIFESTUDIOHEADAPICALL FindMacroMuscle(const char *name) = 0;
  virtual void LIFESTUDIOHEADAPICALL Destroy() = 0;

  static LIFESTUDIOHEADAPI_API IMMTree *LIFESTUDIOHEADAPICALL Create();
};

struct ISequencer;

typedef void *(LIFESTUDIOHEADAPICALL *MUSCLE_CB)(ISequencer *sa, IMacroMuscle *muscle, int flags, void *user_data);
typedef void *(LIFESTUDIOHEADAPICALL *MUSCLE_EXPR_CB)(ISequencer *sa, IMacroMuscle *muscle, float expression, int track, int flags, void *user_data);
typedef void *(LIFESTUDIOHEADAPICALL *MUSCLE_NAME_CB)(ISequencer *sa, const char *clip, int flags, void *user_data);
typedef void *(LIFESTUDIOHEADAPICALL *MUSCLE_NAME_EXPR_CB)(ISequencer *sa, const char *clip, float expression, int track, int flags, void *user_data);
typedef void *(LIFESTUDIOHEADAPICALL *SOUND_CB)(ISequencer *sa, const char *file_name, int start_time, int flags, void *user_data);
typedef void *(LIFESTUDIOHEADAPICALL *SOUND_TIME_CB)(ISequencer *sa, const char *file_name, int start_time, int time_offset, int track, int flags, void *user_data);

struct ISequencer
{
  virtual bool LIFESTUDIOHEADAPICALL Load(const char *file_name) = 0;
  virtual bool LIFESTUDIOHEADAPICALL Load(const char *buffer, int size) = 0;
  virtual IMMTree *LIFESTUDIOHEADAPICALL RegisterMMTree(IMMTree *tree = 0) = 0;
  virtual int LIFESTUDIOHEADAPICALL SequenceTime() const = 0;
  virtual int LIFESTUDIOHEADAPICALL ChannelsCount() const = 0;
  virtual int LIFESTUDIOHEADAPICALL EnumerateMacroMuscles(MUSCLE_CB cb, void *user_data = 0) = 0;
  virtual int LIFESTUDIOHEADAPICALL EnumerateMacroMuscles(int time, MUSCLE_EXPR_CB cb, void *user_data = 0) = 0;
  virtual int LIFESTUDIOHEADAPICALL EnumerateMacroMuscles(MUSCLE_NAME_CB cb, void *user_data = 0) = 0;
  virtual int LIFESTUDIOHEADAPICALL EnumerateMacroMuscles(int time, MUSCLE_NAME_EXPR_CB cb, void *user_data = 0) = 0;
  virtual int LIFESTUDIOHEADAPICALL EnumerateSounds(SOUND_CB cb, void *user_data = 0) = 0;
  virtual int LIFESTUDIOHEADAPICALL EnumerateSounds(int time, SOUND_TIME_CB cb, void *user_data = 0) = 0;
  virtual void LIFESTUDIOHEADAPICALL RenderMacroMuscles(IAnimator *animator, int time) = 0;
  virtual void LIFESTUDIOHEADAPICALL Destroy() = 0;

  static LIFESTUDIOHEADAPI_API ISequencer *LIFESTUDIOHEADAPICALL Create();
};

};

#endif
