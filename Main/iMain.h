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