#include "StdAfx.h"
#include "MEUndoRedo.h"
#include "..\MapEdit\history.h"
#include "..\MapEdit\MapEdit.h"
#include "weInterface.h"
#include "..\Input\Bind.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NMapEditor
{
class CUndoRedoList;
////////////////////////////////////////////////////////////////////////////////////////////////////
static CHistory<CObj<CUndoRedo> > undoQueue;
static CPtr<NWorld::IEditorWorld> pEditorWorld;
static vector<CObj<CUndoRedoList> > undoListStack;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUndoRedoList: public CUndoRedo
{
	OBJECT_BASIC_METHODS(CUndoRedoList)
	list<CObj<CUndoRedo> > info;
	bool bSkip;
public:
	CUndoRedoList( bool _bSkip = false ): bSkip(_bSkip) {}

	bool IsEmpty() const { return info.empty(); }
	bool NeedSkip() const { return bSkip; }
	void Push( CUndoRedo *pCmd ) { info.push_back( pCmd ); }
	virtual bool DoUndo( CPostProcessQueue *pQueue )
	{
		for ( list<CObj<CUndoRedo> >::reverse_iterator i = info.rbegin(); i != info.rend(); ++i )
			if ( !(*i)->DoUndo( pQueue ) )
				return false;
		return true;
	}
	virtual bool DoRedo( CPostProcessQueue *pQueue )
	{
		for ( list<CObj<CUndoRedo> >::iterator i = info.begin(); i != info.end(); ++i )
			if ( !(*i)->DoRedo( pQueue ) )
				return false;
		return true;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetNewWorld( NWorld::IEditorWorld *pWorld )
{
	pEditorWorld = pWorld;
	undoQueue.Clear();
	undoQueue.Push( 0 );
}
NWorld::IEditorWorld* GetEditorWorld()
{
	if ( IsValid( pEditorWorld ) )
		return pEditorWorld;
	ASSERT( 0 );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void BeginUndoList( bool bSkip )
{
	undoListStack.push_back( new CUndoRedoList( bSkip ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void EndUndoList()
{
	if ( undoListStack.empty() )
	{
		ASSERT(0);
		return;
	}
	CObj<CUndoRedoList> pHold = undoListStack.back();
	undoListStack.pop_back();
	if ( !pHold->NeedSkip() && !pHold->IsEmpty() )
		PushUndoCmd( pHold );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void PushUndoCmd( CUndoRedo *pCmd )
{
	if ( !undoListStack.empty() )
		undoListStack.back()->Push( pCmd );
	else
		undoQueue.Push( pCmd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void PosProcess( CPostProcessQueue &queue )
{
	for ( CPostProcessQueue::iterator i = queue.begin(); i !=	queue.end(); ++i )
		if ( IsValid( *i ) )
			(*i)->OnPostProcess();
}
void DoUndo()
{
	CUndoRedo *p = undoQueue.GetBack();
	if ( IsValid( p ) )
	{
		static bool bProcess = false;
		if ( bProcess )
			return;
		bProcess = true;
		theApp.BeginWaitCursor();
		CPostProcessQueue queue;
		if ( !p->DoUndo( &queue ) )
			undoQueue.EraseBeginning();
		PosProcess( queue );
		NInput::PostEvent( "deselect" );
		theApp.EndWaitCursor();
		bProcess = false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void DoRedo()
{
	CUndoRedo *p = undoQueue.Forward();
	if ( IsValid( p ) )
	{
		static bool bProcess = false;
		if ( bProcess )
			return;
		bProcess = true;
		theApp.BeginWaitCursor();
		CPostProcessQueue queue;
		if ( !p->DoRedo( &queue ) )
			undoQueue.EraseTail();
		PosProcess( queue );
		NInput::PostEvent( "deselect" );
		theApp.EndWaitCursor();
		bProcess = false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
