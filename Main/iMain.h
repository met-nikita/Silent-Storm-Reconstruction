#ifndef __IMAIN_H_
#define __IMAIN_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Time.h"
namespace NInput
{
	struct SEvent;
}
namespace NMainLoop
{
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetInterfaceStackDepth();
bool StepApp( bool bActive, bool bSetGamma, bool bInput = true ); // return false on exit state
void DoneInterface();
void ShowLogo();
////////////////////////////////////////////////////////////////////////////////////////////////////
class IInterfaceObject : public CObjectBase
{
protected:
	virtual const STime GetTime();
public:
	virtual void Step() = 0;
	virtual bool ProcessEvent( const NInput::SEvent &eEvent ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IInterfaceBase : public IInterfaceObject
{
protected:
	bool CanRender();

public:
	ZDATA_(IInterfaceObject)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(IInterfaceObject*)this); return 0; }
	virtual void OnGetFocus() = 0;
	// retail IInterfaceBase vtbl+0x24: fired on the interface being COVERED (PushInterface) or
	// REMOVED (PopInterface). CMissionBase pauses its sound scene here (the menu-over-mission pause).
	virtual void OnLostFocus() {}
	// Called on every interface right after a RAW snapshot reload (CICLoadFile, e.g. restart.sav):
	// the graph resumes IN PLACE (no Initialize), so runtime-only caches that Initialize normally
	// derives must be rebuilt here. CMission rebuilds the building shells (SBuildingInfo, which is
	// deliberately never serialized -- retail @0x26e7f0 omits it too) and the camera terrain
	// height source (runtime-only per Camera.cpp).
	virtual void OnSnapshotRestored() {}
	friend class CInterfaceCommand;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInterfaceCommand: public CObjectBase
{
	int operator&( CStructureSaver &f ) { ASSERT(0); return 0; }
	protected:
		void ResetStack();
		void SetInterface( IInterfaceBase *pNewInterface );
		void PushInterface( IInterfaceBase *pNewInterface );
		void PopInterface();
    IInterfaceBase* GetInterface() const;
	public:
		virtual void Exec() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Commands
////////////////////////////////////////////////////////////////////////////////////////////////////
void Command( CInterfaceCommand *pCmd );
bool HaveInterfaceCommand();	// retail @0x1f4e40: is an interface command queued (aborts Step's skip fast-forward)
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICContainer: public CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICContainer);
private:
	vector<CPtr<CInterfaceCommand> > cmdsSet;

public:
	CICContainer() {}
	CICContainer( const vector<CPtr<CInterfaceCommand> > &_cmdsSet ): cmdsSet( _cmdsSet ) {}

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICExitModal: public CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICExitModal);
public:
	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICLoad: public CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICLoad);
private:
	bool bSilent;
	string szName;

public:
	CICLoad() {}
	CICLoad( const string &_szName, bool _bSilent = false ): szName( _szName ), bSilent( _bSilent ) {}

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICSave: public CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICSave);
private:
	bool bSilent;
	string szName;

public:
	CICSave() {}
	CICSave( const string &_szName, bool _bSilent = false ): szName( _szName ), bSilent( _bSilent ) {}

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NMainLoop::CICSaveFile @0x1f6a80 / CICLoadFile @0x1f6830 -- raw fixed-filename FULL-state
// snapshot in the temp slot: no SSaveFileHeader, no screenshot, no save-slot bookkeeping (unlike
// CICSave/CICLoad). Carrier of the mission-start "restart.sav" (CICSaveRestartMission @0x20a830,
// enqueued from CMission::Initialize) and of the pause/lose-menu "Restart mission" button load.
class CICSaveFile: public CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICSaveFile);
private:
	string szName;

public:
	CICSaveFile() {}
	CICSaveFile( const string &_szName ): szName( _szName ) {}

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICLoadFile: public CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICLoadFile);
private:
	string szName;

public:
	CICLoadFile() {}
	CICLoadFile( const string &_szName ): szName( _szName ) {}

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICProfile: public CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICProfile);
private:
	string szProfile;

public:
	CICProfile() {}
	CICProfile( const string &_szProfile ): szProfile( _szProfile ) {}
	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}; // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif