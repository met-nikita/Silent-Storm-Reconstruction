#include "StdAfx.h"
#include "..\Main\BuildingInfo.h"
#include "FragmentSelUndo.h"
#include "..\Misc\BasicShare.h"
#include "..\DBFormat\DataMap.h"
#include "MESerialize.h"
#include "weInterface.h"
namespace NGScene
{
	extern CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFragmentPosProcess: public NMapEditor::CPostProcessUndoRedo
{
	OBJECT_BASIC_METHODS(CFragmentPosProcess)
	int nBuildingID;

public:
	CFragmentPosProcess() : nBuildingID(-1) {}

	void ToggleUpdate( int _nBuildingID ) { nBuildingID = _nBuildingID; }

	virtual void OnPostProcess()
	{
		if ( nBuildingID < 0 )
			return;
		CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( nBuildingID );
		pLoader.Refresh();
		NBuilding::CBuildInfo *pInfo = pLoader->GetValue();
		if ( pInfo )
		{
			SerializeBuilding( pInfo, nBuildingID );
			pLoader->Updated();
		}
		NWorld::IEditorWorld *pW = NMapEditor::GetEditorWorld();
		if ( IsValid( pW ) )
			pW->ResetBuilding();
		//
		nBuildingID = -1;
	}
};
static CObj<CFragmentPosProcess> pPosProcess;
////////////////////////////////////////////////////////////////////////////////////////////////////
CFragmentSelUndo::CFragmentSelUndo( int _nBuildingID, const NBuilding::SBuildFragment *pStart, const NBuilding::SBuildFragment *pEnd, CWysiwygUndo::EUndoAction eAction )
: CWysiwygUndo( eAction ), nBuildingID(_nBuildingID)
{
	//
	nID = 0;
	int nPart = -1;
	if ( pStart )
	{
		start = *pStart;
		nID = start.nID;
		nPart = start.nConstructionPartID;
	}
	if ( pEnd )
	{
		end = *pEnd;
		nID = end.nID;
		if ( nPart <= 0 )
			nPart = end.nConstructionPartID;
	}
	ASSERT( nID != 0 && nPart > 0 );
	NDb::CTConstructionPart *pTCP = NDb::GetTConstructionPart( nPart );
	if ( !IsValid( pTCP ) )
	{
		ASSERT(0);
		return;
	}
	static SRand rand;
	CPtr<NDb::CConstructionPart> pCP = pTCP->CreateConstructionPart( &rand );
	if ( !IsValid( pCP ) )
	{
		ASSERT(0);
		return;
	}
	bWall = pCP->nSizeY == 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFragmentSelUndo::DoUndo( NMapEditor::CPostProcessQueue *pQueue )
{
	//
	switch ( GetAction() )
	{
		case CWysiwygUndo::UA_CHANGE_POS:
			{
				if ( start.nID <= 0 ) 
				{	
					ASSERT(0); return false; 
				}
				NBuilding::SBuildFragment *p = FindFragment();
				if ( !p )
					return false;
				*p = start;
			}
			break;
		case CWysiwygUndo::UA_DELETE:
			if ( !InsertFragment() ) 
				return false; 
			break;
		case CWysiwygUndo::UA_INSERT:
			if ( !DeleteFragment() )
				return false;
			break;
	}
	Update( pQueue );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFragmentSelUndo::DoRedo( NMapEditor::CPostProcessQueue *pQueue )
{
	switch ( GetAction() )
	{
		case CWysiwygUndo::UA_CHANGE_POS:
			{
				NBuilding::SBuildFragment *p = FindFragment();
				if ( !p )
					return false;
				//
				if ( end.nID <= 0 )
				{
					ASSERT(0); return false;
				}
				*p = end;
			}
			break;
		case CWysiwygUndo::UA_DELETE:
			if ( !DeleteFragment() )
				return false;
			break;
		case CWysiwygUndo::UA_INSERT:
			if ( !InsertFragment() ) 
				return false; 
			break;
	}
	//
	Update( pQueue );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static NBuilding::SBuildFragment* FindFragment( int nID, vector<NBuilding::SBuildFragment> *pFrags )
{
	for ( int i = 0; i < pFrags->size(); ++i )
		if ( (*pFrags)[i].nID == nID )
			return &(*pFrags)[i];
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool DeleteFragment( int nID, vector<NBuilding::SBuildFragment> *pFrags )
{
	for ( vector<NBuilding::SBuildFragment>::iterator i = pFrags->begin(); i != pFrags->end(); ++i )
		if ( i->nID == nID )
		{
			pFrags->erase( i );
			return true;
		}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NBuilding::CBuildInfo* CFragmentSelUndo::GetInfo()
{
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( nBuildingID );
	pLoader.Refresh();
	return pLoader->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NBuilding::SBuildFragment* CFragmentSelUndo::FindFragment()
{
	if ( nID == 0 )
		return 0;
	NBuilding::CBuildInfo *pInfo = GetInfo();
	if ( !pInfo )
		return 0;
	if ( bWall )
		return ::FindFragment( nID, &pInfo->wallFragments );
	return ::FindFragment( nID, &pInfo->solidFragments );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFragmentSelUndo::InsertFragment()
{
	if ( start.nID <= 0 || start.nID > 1e6 ) 
	{	
		ASSERT(0); return false; 
	}
	NBuilding::CBuildInfo *pInfo = GetInfo();
	if ( !pInfo )
		return false;
	if ( bWall )
		pInfo->wallFragments.push_back( start );
	else
		pInfo->solidFragments.push_back( start );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFragmentSelUndo::DeleteFragment()
{
	if ( nID == 0 )
		return 0;
	NBuilding::CBuildInfo *pInfo = GetInfo();
	if ( !pInfo )
		return 0;
	if ( bWall )
		return ::DeleteFragment( nID, &pInfo->wallFragments );
	return ::DeleteFragment( nID, &pInfo->solidFragments );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFragmentSelUndo::Update( NMapEditor::CPostProcessQueue *pQueue )
{
	if ( !IsValid( pPosProcess ) )
		pPosProcess = new CFragmentPosProcess;
	//
	pPosProcess->ToggleUpdate( nBuildingID );
	pQueue->push_back( pPosProcess.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWysiwygUndo* CreateFragmentSelUndo( CWysiwygUndo::EUndoAction eAction, int nBuildingID, const NBuilding::SBuildFragment *pStart, const NBuilding::SBuildFragment *pEnd )
{
	return new CFragmentSelUndo( nBuildingID, pStart, pEnd, eAction );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
