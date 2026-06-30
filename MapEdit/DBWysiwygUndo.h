#ifndef __DBWYSIWYGUNDO_H_
#define __DBWYSIWYGUNDO_H_
#include "WysiwygUndo.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef list<CPtr<CWysiwygUndo> > CDBUndoList;
extern CDBUndoList dbundolist;
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class CDBWysiwygUndo: public CWysiwygUndo
{
	int nID;
	void UpdateID( CWysiwygUndo *pSrc, int nOldID, int nNewID )
	{
		if ( (typeid(*this) == typeid(*pSrc)) && (nOldID == nID) )
			nID = nNewID;
	}

protected:
	CObj<T> pStart;
	CObj<T> pEnd;

	virtual bool Insert( T *p ) = 0;
	virtual bool SetPos( T *p, int nID ) = 0;
	virtual bool Delete( int nID ) = 0;
	virtual void Update() = 0;

public:
	CDBWysiwygUndo() {}
	CDBWysiwygUndo( CWysiwygUndo::EUndoAction _eAction, T *_pStart, T *_pEnd, int nDbID ): CWysiwygUndo( _eAction ), nID(-1)
	{
		dbundolist.push_back( this );
		if ( IsValid( _pStart ) )
		{
			nID = _pStart->GetRecordID();
			pStart = new T;
			*pStart = *_pStart;
		}
		if ( IsValid( _pEnd ) )
		{
			nID = _pEnd->GetRecordID();
			pEnd = new T;
			*pEnd = *_pEnd;
		}
		if ( nDbID > 0 )
			nID = nDbID;
		if ( nID < 0 )
		{
			ASSERT(0);
			return;
		}
	}

	int  GetID() const { return nID; }

	void SetID( int nNewID )
	{
		if ( nID == -1 )
		{
			nID = nNewID;
			return;
		}
		//
		for ( CDBUndoList::iterator i = dbundolist.begin(); i != dbundolist.end(); )
		{
			if ( !IsValid( *i ) )
			{
				i = dbundolist.erase( i );
				continue;
			}
			if ( (*i).GetPtr() != this )
			{
				CDynamicCast<CDBWysiwygUndo<T> > p(*i);
				if (p)
					p->UpdateID( this, nID, nNewID );
			}
			++i;
		}
		nID = nNewID;
	}

	virtual bool DoUndo( NMapEditor::CPostProcessQueue *pQueue )
	{
		bool bRet = false;
		//
		switch ( GetAction() )
		{
			case CWysiwygUndo::UA_CHANGE_POS:
				if ( !IsValid( pStart ) )
				{
					ASSERT(0);
					return false;
				}
				bRet = SetPos( pStart, GetID() );
				break;
			case CWysiwygUndo::UA_INSERT:
				bRet = Delete( GetID() );
				if ( bRet )
					SetID( -GetID() );
				break;
			case CWysiwygUndo::UA_DELETE:
				bRet = Insert( pStart );
				break;
		}
		if ( bRet )
			Update();
		return bRet;
	}

	virtual bool DoRedo( NMapEditor::CPostProcessQueue *pQueue )
	{
		bool bRet;
		//
		switch ( GetAction() )
		{
			case CWysiwygUndo::UA_CHANGE_POS:
				if ( !IsValid( pEnd ) )
				{
					ASSERT(0);
					return false;
				}
				bRet = SetPos( pEnd, GetID() );
				break;
			case CWysiwygUndo::UA_DELETE:
				bRet = Delete( GetID() );
				if ( bRet )
					SetID( -GetID() );
				break;
			case CWysiwygUndo::UA_INSERT:
				bRet = Insert( pStart );
				break;
		}
		if ( bRet )
			Update();
		return bRet;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DBWYSIWYGUNDO_H_
