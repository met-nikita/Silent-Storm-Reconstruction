#ifndef __AIEVENT_H_
#define __AIEVENT_H_
#if _MSC_VER > 1000
#pragma once
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "aiUnitState.h"     // NAI::SAIUnitState
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
class IAIUnit;
////////////////////////////////////////////////////////////////////////////////////////////////////
// IAIEvent (retail-new aiEvent.obj): a notification that mutates a unit's SAIUnitState; vtbl+0x10 =
// Modify. In the release the per-unit AI event system (CAIUnit::Notify) drives these incrementally
// as the unit sees/loses enemies, allies and corpses. The reconstructed CAIEventTracker now drives
// this path, with PrepareEnemies providing retail's begin-turn visibility reconciliation. Each event
// carries at most one CPtr<IAIUnit> payload.
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIEvent : public CObjectBase
{
public:
	virtual void Modify( SAIUnitState *pState ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIBeginTurnEvent : public IAIEvent  { OBJECT_BASIC_METHODS(CAIBeginTurnEvent);  public: virtual void Modify( SAIUnitState *pState ); };
class CAIHelpCalledEvent : public IAIEvent { OBJECT_BASIC_METHODS(CAIHelpCalledEvent); public: virtual void Modify( SAIUnitState *pState ); };
class CAIUpdateEvent : public IAIEvent     { OBJECT_BASIC_METHODS(CAIUpdateEvent);     public: virtual void Modify( SAIUnitState *pState ); };

class CAIEnemyEvent : public IAIEvent              { OBJECT_BASIC_METHODS(CAIEnemyEvent);              public: CPtr<IAIUnit> pEnemy;  virtual void Modify( SAIUnitState *pState ); };
class CAILostEnemyEvent : public IAIEvent          { OBJECT_BASIC_METHODS(CAILostEnemyEvent);          public: CPtr<IAIUnit> pEnemy;  virtual void Modify( SAIUnitState *pState ); };
class CAIPossibleEnemyEvent : public IAIEvent      { OBJECT_BASIC_METHODS(CAIPossibleEnemyEvent);      public: CPtr<IAIUnit> pEnemy;  virtual void Modify( SAIUnitState *pState ); };
class CAILostPossibleEnemyEvent : public IAIEvent  { OBJECT_BASIC_METHODS(CAILostPossibleEnemyEvent);  public: CPtr<IAIUnit> pEnemy;  virtual void Modify( SAIUnitState *pState ); };
class CAIEnemyDiedEvent : public IAIEvent          { OBJECT_BASIC_METHODS(CAIEnemyDiedEvent);          public: CPtr<IAIUnit> pEnemy;  virtual void Modify( SAIUnitState *pState ); };
class CAIAllyNeedHelpEvent : public IAIEvent       { OBJECT_BASIC_METHODS(CAIAllyNeedHelpEvent);       public: CPtr<IAIUnit> pAlly;   virtual void Modify( SAIUnitState *pState ); };
class CAILostAllyEvent : public IAIEvent           { OBJECT_BASIC_METHODS(CAILostAllyEvent);           public: CPtr<IAIUnit> pAlly;   virtual void Modify( SAIUnitState *pState ); };
class CAICorpseEvent : public IAIEvent             { OBJECT_BASIC_METHODS(CAICorpseEvent);             public: CPtr<IAIUnit> pCorpse; virtual void Modify( SAIUnitState *pState ); };
////////////////////////////////////////////////////////////////////////////////////////////////////
// Factories: a live payload unit -> new event, else null. The three payload-less events always
// construct.
IAIEvent* CreateAIEnemyEvent( IAIUnit *pUnit );
IAIEvent* CreateAILostEnemyEvent( IAIUnit *pUnit );
IAIEvent* CreateAIPossibleEnemyEvent( IAIUnit *pUnit );
IAIEvent* CreateAILostPossibleEnemyEvent( IAIUnit *pUnit );
IAIEvent* CreateAIEnemyDiedEvent( IAIUnit *pUnit );
IAIEvent* CreateAIAllyNeedHelpEvent( IAIUnit *pUnit );
IAIEvent* CreateAILostAllyEvent( IAIUnit *pUnit );
IAIEvent* CreateAICorpseEvent( IAIUnit *pUnit );
IAIEvent* CreateAIBeginTurnEvent();
IAIEvent* CreateAIHelpCalledEvent();
IAIEvent* CreateAIUpdateEvent();
bool IsBeginTurnEvent( IAIEvent *pEvent );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif // __AIEVENT_H_
