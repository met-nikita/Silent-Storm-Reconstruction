#include "StdAfx.h"
#include "MapEdit.h"
#include "ObjectBrowser.h"
#include "ItemsMgr.h"
#include "..\Misc\StrProc.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SProperty
{
	vector<string> path;
	CObj<IObjectBrowser> pChild;
};
typedef vector<SProperty> CColumn;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectBrowser: public IObjectBrowser
{
	OBJECT_BASIC_METHODS(CObjectBrowser);
	int nRootTree;
	vector<CColumn> columns;
	CPtr<CProp> pPropHolder;
	CProp* GetProperty( CItemsMgr *pRoot, int nObject, int nVariant, const vector<string> &path, const unordered_map<int, int> &propVariants );

public:
	CObjectBrowser( int nRootObjectTree = 0 ): nRootTree( nRootObjectTree ) {}

	virtual void AddProperty( int nColumn, const string &szPropertyPath, IObjectBrowser *pSubProperties );
	virtual void GetProperties( vector<CPropMap> *pProps, int nObjectID, int nVariantID, const unordered_map<int, int> &propVariants );
	virtual int  GetRootObjectTree() const { return nRootTree; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectBrowser::AddProperty( int nColumn, const string &szPropertyPath, IObjectBrowser *pSubProperties )
{
	if ( columns.size() <= nColumn )
		columns.resize( nColumn + 1 );
	SProperty prop;
	NStr::SplitString( szPropertyPath, prop.path, '\\' );
	prop.pChild = pSubProperties;
	columns[nColumn].push_back( prop );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetVariant( int nPropID, const unordered_map<int, int> &propVariants )
{
	unordered_map<int, int>::const_iterator i = propVariants.find( nPropID );
	if ( i != propVariants.end() )
		return i->second;
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectBrowser::GetProperties( vector<CPropMap> *pProps, int nObjectID, int nVariantID, const unordered_map<int, int> &propVar )
{
	const SResTree *pRootTree = theApp.GetResTree( nRootTree );
	if ( !pRootTree )
		return;
	const CPropMap *pRootProps = pRootTree->pItemsTree->GetPropList( nObjectID, nVariantID );
	if ( !pRootProps )
		return;
	pProps->clear();
	//
	int iColumn = 0;
	for ( vector<CColumn>::const_iterator ic = columns.begin(); ic != columns.end(); ++iColumn, ++ic )
		for ( CColumn::const_iterator i = ic->begin(); i != ic->end(); ++i )
		{
			const SProperty &sp = *i;
			if ( sp.path.empty() )
				continue;
			CProp *p = GetProperty( pRootTree->pItemsTree, nObjectID, nVariantID, sp.path, propVar );
			if ( !p )
				continue;
			//
			if ( iColumn >= pProps->size() )
				pProps->resize( iColumn + 1 );
			//
			(*pProps)[iColumn][sp.path.back()] = p;
			if ( sp.pChild )
			{
				if ( sp.pChild->GetRootObjectTree() != p->GetRelation() )
				{
					ASSERT(0);
					continue;
				}
				vector<CPropMap> subprops;
				sp.pChild->GetProperties( &subprops, p->GetValue(), GetVariant( p->GetID(), propVar ), propVar );
				if ( subprops.size() >= pProps->size() )
					pProps->resize( subprops.size() );
				for ( int j = 0; j < subprops.size(); ++j )
					(*pProps)[j].insert( subprops[j].begin(), subprops[j].end() );
			}			
		}
	//
	pRootTree->pItemsTree->ReleasePropList( pRootProps );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CProp* CObjectBrowser::GetProperty( CItemsMgr *pRoot, int nObject, int nVariant, const vector<string> &path, const unordered_map<int, int> &propVar )
{
	pPropHolder = 0;

	int nCurObj = nObject;
	int nCurVar = nVariant;
	CItemsMgr *pCurMgr = pRoot;

	for ( vector<string>::const_iterator i = path.begin(); i != path.end(); ++i )
	{
		const CPropMap *pProps = pCurMgr->GetPropList( nCurObj, nCurVar );
		if ( !pProps )
			return 0;
		CPropMap::const_iterator ip = pProps->find( *i );
		if ( ip == pProps->end() )
		{
			ASSERT(0);
			return 0;
		}
		pPropHolder = ip->second;
		pCurMgr->ReleasePropList( pProps );
		int nRelation = pPropHolder->GetRelation();
		const SResTree *pTree = theApp.GetResTree( nRelation );
		if ( !pTree )
		{
			if ( *i == path.back() )
				break;
			//
			ASSERT(0);
			return 0;
		}
		pCurMgr = pTree->pItemsTree;
		nCurObj = pPropHolder->GetValue();
		nCurVar = GetVariant( pPropHolder->GetID(), propVar );
	}
	return pPropHolder;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IObjectBrowser* IObjectBrowser::Create( int nRootObjectTree )
{
	return new CObjectBrowser( nRootObjectTree );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CProp );