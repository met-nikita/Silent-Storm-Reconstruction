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
  virtual bool Load(const char *file_name) = 0;
  virtual bool Load(const char *buffer, int size) = 0;
  virtual IMacroMuscle *RootMacroMuscle() const = 0;
  virtual IMacroMuscle *FindMacroMuscle(const char *name) = 0;
  virtual void Destroy() = 0;

  static LIFESTUDIOHEADAPI_API IMMTree *__stdcall Create();
};

struct ISequencer;

typedef void *(*MUSCLE_CB)(ISequencer *sa, IMacroMuscle *muscle, int flags, void *user_data);
typedef void *(*MUSCLE_EXPR_CB)(ISequencer *sa, IMacroMuscle *muscle, float expression, int track, int flags, void *user_data);
typedef void *(*MUSCLE_NAME_CB)(ISequencer *sa, const char *clip, int flags, void *user_data);
typedef void *(*MUSCLE_NAME_EXPR_CB)(ISequencer *sa, const char *clip, float expression, int track, int flags, void *user_data);
typedef void *(*SOUND_CB)(ISequencer *sa, const char *file_name, int start_time, int flags, void *user_data);
typedef void *(*SOUND_TIME_CB)(ISequencer *sa, const char *file_name, int start_time, int time_offset, int track, int flags, void *user_data);

struct ISequencer
{
  virtual bool Load(const char *file_name) = 0;
  virtual bool Load(const char *buffer, int size) = 0;
  virtual IMMTree *RegisterMMTree(IMMTree *tree = 0) = 0;
  virtual int SequenceTime() const = 0;
  virtual int TracksCount() const = 0;
  virtual int EnumerateMacroMuscles(MUSCLE_CB cb, void *user_data = 0) = 0;
  virtual int EnumerateMacroMuscles(int time, MUSCLE_EXPR_CB cb, void *user_data = 0) = 0;
  virtual int EnumerateMacroMuscles(MUSCLE_NAME_CB cb, void *user_data = 0) = 0;
  virtual int EnumerateMacroMuscles(int time, MUSCLE_NAME_EXPR_CB cb, void *user_data = 0) = 0;
  virtual int EnumerateSounds(SOUND_CB cb, void *user_data = 0) = 0;
  virtual int EnumerateSounds(int time, SOUND_TIME_CB cb, void *user_data = 0) = 0;
  virtual void RenderMacroMuscles(IAnimator *animator, int time) = 0;
  virtual void Destroy() = 0;

  static LIFESTUDIOHEADAPI_API ISequencer *__stdcall Create();
};

};

#endif
