#include "StdAfx.h"
#include "IWysiwyg.h"
#include "WysiwygMaterial.h"
#include "WysiwygFragmentSel.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "..\Main\DiscretePos.h"
#include "..\Main\BuildingInfo.h"
#include "wEditor.h"
#include "MEUserSettings.h"
#include "..\Misc\BasicShare.h"
#include "WysiwygUndo.h"
#include "..\MapEdit\UserSettingsSetup.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	extern CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetWallMaterialID( const NBuilding::SBuildFragment *pFr, CVec3 ptNormal )
{
	ASSERT( NDb::N_CONSTRUCTION_MATERIALS > 2 );
	SDiscretePos dpos( 0, VNULL3, pFr->nRotationID );
	dpos.InvMoveAndRotate( &ptNormal );
	CVec3 fabsn( fabs( ptNormal.x ), fabs( ptNormal.y ), fabs( ptNormal.z ) );
	if ( fabsn.y > fabsn.x && fabsn.y > fabsn.z )
		return ptNormal.y > 0 ? 1 : 0;
	return 3;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetSolidMaterialID( const NBuilding::SBuildFragment *pFr, CVec3 ptNormal )
{
	ASSERT( NDb::N_CONSTRUCTION_MATERIALS > 2 );
	SDiscretePos dpos( 0, VNULL3, pFr->nRotationID );
	dpos.InvMoveAndRotate( &ptNormal );
	CVec3 fabsn( fabs( ptNormal.x ), fabs( ptNormal.y ), fabs( ptNormal.z ) );
	if ( fabsn.z > fabsn.x && fabsn.z > fabsn.y )
		return ptNormal.z > 0 ? 0 : 1;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool SetFragmentMaterial( NBuilding::SBuildFragment *pFr, int nMatInd, const NBuilding::SRawMaterialApply &m )
{
	if ( m.nTMaterialID <= 0 )
		return false;
	vector<NBuilding::SRawMaterialApply> &layers = pFr->materials[nMatInd].layers;
	if ( layers.size() == 1 && layers[0] == m )
		return false;
	layers.resize( 1 );
	layers[0] = m;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool SetMaterial( NWorld::IBuilding *pObj, int nUserID, const CVec3 &ptCrossNormal, const NBuilding::SRawMaterialApply &m )
{
	NBuilding::SBuildFragment *pFr = GetFragment( pObj, nUserID );
	if ( !pFr )
		return false;
	int nMatInd;
	if ( IsSecondGeometry( nUserID ) )
		nMatInd = NDb::N_FIRST_GEOMMATERIALS;
	else
		nMatInd = IsWall( nUserID ) ? GetWallMaterialID( pFr, ptCrossNormal ) : GetSolidMaterialID( pFr, ptCrossNormal );
	NBuilding::SBuildFragment start = *pFr;
	bool bRet = SetFragmentMaterial( pFr, nMatInd, m );
	if ( bRet )
		NMapEditor::PushUndoCmd( CreateFragmentSelUndo( CWysiwygUndo::UA_CHANGE_POS, pObj->GetInfo().pVariant->GetRecordID(), &start, pFr ) );
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool SetMaterialSet( NWorld::IBuilding *pObj, int nUserID, const CVec3 &ptCrossNormal, const vector<NBuilding::SRawMaterialApply> &m )
{
	if ( m.size() != N_MATERIALSET_SIZE )
		return false;
	NBuilding::SBuildFragment *pFr = GetFragment( pObj, nUserID );
	if ( !pFr )
		return false;
	int nMatInd = IsSecondGeometry( nUserID ) ? NDb::N_FIRST_GEOMMATERIALS : 0;
	bool bUpd = false;
	NBuilding::SBuildFragment start = *pFr;
	for ( int i = 0; i < N_MATERIALSET_SIZE; ++i )
		bUpd = SetFragmentMaterial( pFr, nMatInd + i, m[i] ) || bUpd;
	if ( bUpd )
		NMapEditor::PushUndoCmd( CreateFragmentSelUndo( CWysiwygUndo::UA_CHANGE_POS, pObj->GetInfo().pVariant->GetRecordID(), &start, pFr ) );
	return bUpd;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWysiwygMaterial::CWysiwygMaterial( NWorld::IWorld *pWorld, NGScene::IGameView *pScene, ICamera *pCamera )
{
	bLBDown = false;
	bBuildingUpdated = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CheckModifiers()
{
	return (0x8000 & GetAsyncKeyState( VK_CONTROL )) || (0x8000 & GetAsyncKeyState( VK_MENU ))
		|| (0x8000 & GetAsyncKeyState( VK_SHIFT ));
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygMaterial::Update( CObjectBase *pObj, int nUserID, const CVec3 &ptCrossNormal )
{
	if ( !(0x8000 & GetAsyncKeyState( VK_LBUTTON )) || CheckModifiers() || !bLBDown )
	{
		OnLButtonUp( VNULL2 );
		return;
	}
	NWorld::IBuilding *pWB = dynamic_cast<NWorld::IBuilding*>( pObj );
	if ( pWB )
	{
		vector<NBuilding::SRawMaterialApply> mats;
		GetUserSettings().GetActiveMaterial( &mats );

		if ( mats.empty() )
			return;
		bool bRet;
		if ( mats.size() == N_MATERIALSET_SIZE )
			bRet = SetMaterialSet( pWB, nUserID, ptCrossNormal, mats );
		else
			bRet = SetMaterial( pWB, nUserID, ptCrossNormal, mats.front() );
		//if ( bRet )
			UpdateBuilding( pWB );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygMaterial::OnLButtonDown( const CVec2 &ptPos, CObjectBase *pObj, int nUserID, const CVec3 &ptCrossNormal )
{
	if ( CheckModifiers() )
		return;
	bLBDown = true;
	NWorld::IBuilding *pWB = dynamic_cast<NWorld::IBuilding*>( pObj );
	if ( pWB )
	{
		vector<NBuilding::SRawMaterialApply> mats;
		GetUserSettings().GetActiveMaterial( &mats );

		if ( mats.empty() )
			return;
		bool bRet;
		if ( mats.size() == N_MATERIALSET_SIZE )
			bRet = SetMaterialSet( pWB, nUserID, ptCrossNormal, mats );
		else
			bRet = SetMaterial( pWB, nUserID, ptCrossNormal, mats.front() );
		//if ( bRet )
			UpdateBuilding( pWB );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static NBuilding::SRawMaterialApply* GetRawMaterial( NDb::CMaterial *pM )
{
	if ( !IsValid( pM ) || !IsValid( pM->pTemplate ) )
		return 0;
	static NBuilding::SRawMaterialApply res;
	res.nTMaterialID = pM->pTemplate->GetRecordID();
	res.mapping = NBuilding::SMaterialApply::NORMAL;
	return &res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWysiwygMaterial::SetActiveMaterial( const CVec2 &ptPos, CObjectBase *pObj, int nUserID, const CVec3 &ptCrossNormal )
{
	NWorld::IBuilding *pWB = dynamic_cast<NWorld::IBuilding*>( pObj );
	if ( !pWB )
		return false;
	NBuilding::SBuildFragment *pFr = GetFragment( pWB, nUserID );
	if ( !pFr )
		return false;
	NDb::CTConstructionPart *pTPart = NDb::GetTConstructionPart( pFr->nConstructionPartID );
	if ( !pTPart )
		return false;
	static SRand rand;
	CPtr<NDb::CConstructionPart> pPart = pTPart->CreateConstructionPart( &rand );
	if ( !pPart )
		return false;

	IUserSettingsSetup &settings = GetUserSettingsSetup();
	EMaterialSet eSet = settings.GetActiveMaterialSet();
	vector<NBuilding::SRawMaterialApply> mats;
	if ( eSet == MSET_FIRST )
	{
		int nMatInd;
		if ( IsSecondGeometry( nUserID ) )
			nMatInd = NDb::N_FIRST_GEOMMATERIALS;
		else
			nMatInd = IsWall( nUserID ) ? GetWallMaterialID( pFr, ptCrossNormal ) : GetSolidMaterialID( pFr, ptCrossNormal );
		//
		if ( !pFr->materials[nMatInd].layers.empty() )
			mats.push_back( pFr->materials[nMatInd].layers.front() );
		else if ( NBuilding::SRawMaterialApply *p = GetRawMaterial( pPart->pDefMaterials[nMatInd] ) )
			mats.push_back( *p );
	}
	else
	{
		mats.resize( N_MATERIALSET_SIZE );
		for ( int i = 0; i < N_MATERIALSET_SIZE; ++i )
			if ( !pFr->materials[i].layers.empty() )
				mats[i] = pFr->materials[i].layers.front();
			else if ( NBuilding::SRawMaterialApply *p = GetRawMaterial( pPart->pDefMaterials[i] ) )
				mats[i] = *p;
	}
	if ( !mats.empty() )
	{
		settings.SetMaterial( eSet, mats );
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygMaterial::OnLButtonUp( const CVec2 &ptPos )
{
	bLBDown = false;
	if ( bBuildingUpdated )
	{
		bBuildingUpdated = false;
		UpdateBuildInfo( nBuildingUpdated );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygMaterial::UpdateBuilding( NWorld::IBuilding *pObj )
{
	pObj->Update();
	NDb::CTemplVariant *pVar = pObj->GetInfo().pVariant;
	if ( IsValid( pVar ) )
	{
		bBuildingUpdated = true;
		nBuildingUpdated = pVar->GetRecordID();
		CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( nBuildingUpdated );
		pLoader.Refresh();
		pLoader->Updated();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
