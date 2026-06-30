#ifndef __HISTORY_H__
#define __HISTORY_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE> class CHistory
{
	vector<TYPE> history;
	typedef typename vector<TYPE>::iterator iterator;
	iterator activeIt;
public:
	CHistory();

	void Push( const TYPE &event );
	bool CanForward() const;
	bool CanBackward() const;
	TYPE Forward();
	TYPE Back();
	TYPE GetBack();
	bool LastEvent( TYPE *pEvent );
	void EraseBeginning();
	void EraseTail();
	void Clear();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE> CHistory<TYPE>::CHistory()
{
	activeIt = history.end();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE> inline bool CHistory<TYPE>::CanForward() const
{
	if ( history.empty() || history.end() == activeIt )
		return false;
	vector<TYPE>::const_iterator it = activeIt;
	if ( history.end() != ++it )
		return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE> inline bool CHistory<TYPE>::CanBackward() const
{
	if ( !history.empty() && history.begin() != activeIt )
		return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE> inline void CHistory<TYPE>::Push( const TYPE &event )
{
	if ( activeIt != history.end() )
	{
		vector<TYPE>::iterator it = activeIt;
		++it;
		history.erase( it, history.end() );
	}
	history.push_back( event );
	activeIt = history.end();
	--activeIt;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE> inline TYPE CHistory<TYPE>::Forward()
{
	if ( CanForward() )
	{
		++activeIt;
		return *activeIt;
	}
	return TYPE();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE> inline TYPE CHistory<TYPE>::GetBack()
{
	if ( CanBackward() )
	{
		TYPE ret = *activeIt;
		--activeIt;
		return ret;
	}
	return TYPE();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE> inline TYPE CHistory<TYPE>::Back()
{
	if ( CanBackward() )
	{
		--activeIt;
		return *activeIt;
	}
	return TYPE();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ≈сли в списке еще нет событий, возвращает false
template<class TYPE> inline bool CHistory<TYPE>::LastEvent( TYPE *pEvent )
{
	if ( !history.empty() )
	{
		*pEvent = history.back();
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE> inline void CHistory<TYPE>::EraseBeginning()
{
	if ( !history.empty() )
		history.erase( history.begin(), activeIt );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE> inline void CHistory<TYPE>::EraseTail()
{
	if ( activeIt != history.end() )
	{
		vector<TYPE>::iterator it = activeIt;
		++it;
		history.erase( it, history.end() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE> inline void CHistory<TYPE>::Clear()
{
	history.clear();
	activeIt = history.end();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __HISTORY_H__
