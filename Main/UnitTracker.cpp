#include "StdAfx.h"
#include "GView.h"
#include "Grid.h"
#include "Transform.h"
#include "GSceneUtils.h"
#include "iMain.h"
#include "wInterface.h"
#include "RPGUnitInfo.h"
#include "RWGame.h"
#include "RWSound.h"
#include "aiPath.h"
#include "Interface.h"
#include "UIWrap.h"
#include "iMission.h"
#include "iMissionUI.h"
#include "iCommonUI.h"
#include "MemObject.h"
#include "UnitTracker.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataGeometry.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CProjectedText: public CWindow
{
	OBJECT_BASIC_METHODS(CProjectedText)
private:
	ZDATA_(CWindow)
	CVec3 vPos;
	CPtr<NGame::IMission> pMission;
	////
	CObj<CImageNumber> pText;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&vPos); f.Add(3,&pMission); f.Add(4,&pText); return 0; }

public:
	CProjectedText() {}
	CProjectedText( const SWindowInfo &sInfo, int nAP, const CVec3 &vPos, NGame::IMission *pMission );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CProjectedText::CProjectedText( const SWindowInfo &sInfo, int nAP, const CVec3 &_vPos, NGame::IMission *_pMission ):
	CWindow( sInfo ), vPos( _vPos ), pMission( _pMission )
{
	pText = new CImageNumber( SWindowInfo( this, SPoint( 0, 0 ), SPoint( 0, 0 ), "ap", STYLE_VISIBLE | STYLE_ENABLED | STYLE_TOPMOST ), CImageNumber::TYPE_UNITINFOPANEL );

	// crap
	pText->Set( nAP );
	pText->SetSize( pText->GetRealSize() );
	SetSize( pText->GetRealSize() );
	pText->Set( 0 );
	pText->Set( nAP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CProjectedText::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	CVec2 vScreenPoint;
	CVec2 vScreenRect = pMission->GetScene()->GetScreenRect();
	CTransformStack sTS = pMission->GetCameraTransform();

	pText->SetStyle( STYLE_VISIBLE, false );
	if ( TestRayInFrustrum( vPos, &sTS, vScreenRect, &vScreenPoint ) )
	{
		vScreenPoint.x = vScreenPoint.x * 1024 / vScreenRect.x;
		vScreenPoint.y = vScreenPoint.y * 768 / vScreenRect.y;
		SPoint sPosition;
		GetParent()->ScreenToClient( NUI::SPoint( vScreenPoint.x - sSize.x / 2, vScreenPoint.y - sSize.y / 2 ), &sPosition );
		SetPosition( sPosition );

		pText->SetStyle( STYLE_VISIBLE, true );
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const float 
	F_PATHPOINT_DIST = 0.5f;
const CVec4
	V_SELECTIONCOLOR_SELECT = CVec4( 0.0431f, 0.2823f, 0, 1 ),
	V_SELECTIONCOLOR_HILIGHT = CVec4( 0.0215f, 0.1411f, 0, 1 );
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsSamePlace( const NAI::SPathPlace &s1, const NAI::SPathPlace &s2 )
{
	if ( s1.GetX() != s2.GetX() )
		return false;
	if ( s1.GetY() != s2.GetY() )
		return false;
	if ( s1.GetLayer() != s2.GetLayer() )
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitTracker::CUnitTracker( IMission *_pMission, NWorld::CUnit *_pUnit ): 
	pMission( _pMission ), pUnit( _pUnit ), nFloor( 0 ), bSelected( false ), bHilighted( false ), bPathVisible( false ), bHilightTarget( false ), bPathDigitsVisible( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::SetTargetPosition( const NAI::SPosition &sPos, bool bInstantly )
{
	CObj<NAI::CPath> pCurrentPath( pUnit->GetCurrentPath() );

	if ( pMission->IsRealTime() || bInstantly )
	{
		sTarget = sPos;
		pMission->Command( pUnit, new NWorld::CCmdPath( sPos, NAI::PF_DEFAULT ) );
	}
	else
	{
		if ( IsValid( pCurrentPath ) && IsSamePlace( sTarget.p, sPos.p ) )
			pMission->Command( pUnit, new NWorld::CCmdContinue, false );
		else
			pMission->Command( pUnit, new NWorld::CCmdPath( sPos, NAI::PF_DEFAULT ), false );

		sTarget = sPos;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::SPosition CUnitTracker::GetTargetPosition() const
{
	CObj<NAI::CPath> pCurrentPath( pUnit->GetCurrentPath() );

	if ( IsValid( pCurrentPath ) )
	{
		NAI::SPosition sPos;
		sPos.p = pCurrentPath->points.back();
		sPos.SetNetwork( pMission->GetWorld()->GetPathNetwork() );
		return sPos;
	}

	return pUnit->GetPosition().pos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitTracker::IsPathComplete() const
{
	return pUnit->GetCurrentPath() == 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::CancelPath()
{
	pMission->Command( new NWorld::CCmdCancel( pUnit ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::ShowPath()
{
	bool bNeedUpdate = true;
	CObj<NAI::CPath> pNewPath = pUnit->GetCurrentPath();
	if ( pPath && pNewPath )
	{
		if ( pPath->IsEqual( (*pNewPath) ) )
			bNeedUpdate = false;
	}
	else
	{
		if ( !pPath && !pNewPath )
			bNeedUpdate = false;
	}

	if ( pNewPath )
	{
		bool bNewHilightTarget = false;
		NAI::SPosition sPosition;
		if ( pMission->GetTracePosition( &sPosition ) && ( sPosition == sTarget ) )
			bNewHilightTarget = true;

		if ( bNewHilightTarget != bHilightTarget )
		{
			bNeedUpdate = true;
			bHilightTarget = bNewHilightTarget;
		}
	}

	if ( bNeedUpdate )
	{
		pPath = pNewPath;
		HidePath();
		HidePathDigits();
	}

	if ( pPath && !bPathVisible )
	{
		CObj<NWorld::IPathViewer> pPathViewer = pUnit->CreatePathViewer();
		vector<NWorld::SPathPoint> points;
		pPathViewer->SetPath( pPath );
		pPathViewer->GetPoints( &points );

		if ( points.empty() )
			return;

		nTargetAP = points.back().nAP;
		vTargetPos = points.back().vPoint;
		sTarget.p = pPath->points.back();
		sTarget.SetNetwork( pMission->GetWorld()->GetPathNetwork() );

//		if ( !pMission->IsRealTime() )
//			pPathAPText = new NUI::CProjectedText( NUI::SWindowInfo( pMission->GetMissionUI()->GetClientWindow(), NUI::SPoint( 0, 0 ), NUI::SPoint( 0, 0 ), "", NUI::STYLE_ENABLED | NUI::STYLE_TOPMOST | NUI::STYLE_TRANSPARENT | NUI::STYLE_VISIBLE ), points.back().nAP, vTargetPos, pMission );

		NGScene::IGameView *pScene = pMission->GetScene();
		CPtr<NRPG::IUnitMissionInfo> pRPG = pUnit->GetRPG();

		vector<NWorld::SPathPoint> resPoints;
		SmoothPathLine( points, &resPoints );

		int nAttackAP = 0;
		SActionInfo sAction;
		pMission->GetActionInfo( UA_ATTACK, &sAction );
		if ( sAction.bAvailable )
			nAttackAP = sAction.nActionAP;

		if ( !resPoints.empty() )
		{
			for ( int nTemp = 1; nTemp < resPoints.size() - 1; nTemp++ )
			{
				NWorld::SPathPoint sPoint = resPoints[nTemp];
				sPoint.vPoint.z += 0.1f;

				SFBTransform sTransform;
				MakeMatrix( &sTransform, 0, 0, 0, sPoint.vPoint );

				CPtr<NDb::CModel> pPoint = NDb::GetModel( 1558 );
				if ( !pMission->IsRealTime() )
				{
					if ( !pRPG->CanSpendAP( sPoint.nAP ) )
						pPoint = NDb::GetModel( 1570 );
					else if ( !pRPG->CanSpendAP( sPoint.nAP + nAttackAP ) )
						pPoint = NDb::GetModel( 1569 );
				}

				NGScene::CLightGroup *pGroup = pScene->CreateLightGroup();
				groupsSet.push_back( pGroup );
				nodesSet.push_back( pScene->CreateMesh( pPoint, sTransform, NGScene::SFullRoomInfo( pGroup, sPoint.nFloor ) ) );
			}
		}

		SFBTransform sTransform;
		MakeMatrix( &sTransform, 0, 0, 0, vTargetPos + CVec3( 0, 0, 0.1f ) );

		CPtr<NDb::CModel> pCross = NDb::GetModel( 1578 );
		if ( !pMission->IsRealTime() )
		{
			if ( bHilightTarget )
				pCross = NDb::GetModel( 1559 );
			else if ( !pRPG->CanSpendAP( points.back().nAP ) )
				pCross = NDb::GetModel( 1634 );
			else if ( !pRPG->CanSpendAP( points.back().nAP + nAttackAP ) )
				pCross = NDb::GetModel( 1635 );
		}

		NGScene::CLightGroup *pGroup = pScene->CreateLightGroup();
		groupsSet.push_back( pGroup );
		nodesSet.push_back( pScene->CreateMesh( pCross, sTransform, NGScene::SFullRoomInfo( pGroup ) ) );

		bPathVisible = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::HidePath()
{
	bPathVisible = false;
	pPathAPText = 0;
	groupsSet.clear();
	nodesSet.clear();

	HidePathDigits();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::ShowPathDigits()
{
	if ( !bPathVisible )
		return;
	if ( bPathDigitsVisible )
		return;

	HidePathDigits();

	NGScene::IGameView *pScene = pMission->GetScene();

	CObj<NWorld::IPathViewer> pPathViewer = pUnit->CreatePathViewer();
	vector<NWorld::SPathPoint> points;
	pPathViewer->SetPath( pPath );
	pPathViewer->GetPoints( &points );

	if ( points.empty() )
		return;

	CPtr<NRPG::IUnitMissionInfo> pRPG = pUnit->GetRPG();

	int nAttackAP = 0;
	SActionInfo sAction;
	pMission->GetActionInfo( UA_ATTACK, &sAction );
	if ( sAction.bAvailable )
		nAttackAP = sAction.nActionAP;

	bPathDigitsVisible = true;

	CVec3 vDir = pMission->GetCamera()->GetForwardDir();
	vDir.z = 0;
	Normalize( &vDir );

	CVec3 vAPDir( 0, 0, 1 );
	if ( points.size() > 1 )
		vAPDir = vTargetPos - points[points.size() - 2].vPoint;
	else
		vAPDir = vTargetPos - pUnit->GetPosition().GetCP();

	vAPDir.z = 0;
	Normalize( &vAPDir );

	int nValue = nTargetAP;
	CVec2 vSize( 0, 0 );
	do
	{
		int nDigit = nValue % 10;
		nValue /= 10;

		CPtr<NDb::CModel> pDigit = NDb::GetModel( 1585 + nDigit );
		if ( !IsValid( pDigit->pGeometry ) )
			continue;

		vSize.x += pDigit->pGeometry->boundSize.x;
		vSize.y = Max( vSize.y, pDigit->pGeometry->boundSize.y );
	} while( nValue > 0 );

	nValue = nTargetAP;
	CVec2 vShift = vSize / 2;
	do
	{
		int nDigit = nValue % 10;
		nValue /= 10;

		CPtr<NDb::CModel> pDigit = NDb::GetModel( 1585 + nDigit );
		if ( !pMission->IsRealTime() )
		{
			if ( !pRPG->CanSpendAP( points.back().nAP ) )
				pDigit = NDb::GetModel( 3454 + nDigit );
			else if ( !pRPG->CanSpendAP( points.back().nAP + nAttackAP ) )
				pDigit = NDb::GetModel( 3444 + nDigit );
		}

		if ( !IsValid( pDigit->pGeometry ) )
			continue;

		const CVec3 &vBounds = pDigit->pGeometry->boundSize;

		SHMatrix sTranslate, sRotate;
		Identity( &sTranslate );
		sTranslate._14 = vShift.x - vBounds.x;
		sTranslate._24 = vShift.y;
		sTranslate._34 = 0.20f;
		MakeMatrix( &sRotate, CVec3( 0, 0, 0 ), vDir );

		SFBTransform sTransform;
		Multiply( &sTransform.forward, sRotate, sTranslate );
		sTransform.forward._14 += vTargetPos.x;
		sTransform.forward._24 += vTargetPos.y;
		sTransform.forward._34 += vTargetPos.z;
		InvertMatrix( &sTransform.backward, sTransform.forward );

		NGScene::CLightGroup *pGroup = pScene->CreateLightGroup();
		digidGroupsSet.push_back( pGroup );
		digidNodesSet.push_back( pScene->CreateMesh( pDigit, sTransform, NGScene::SFullRoomInfo( pGroup ) ) );

		vShift.x -= vBounds.x;
	} while( nValue > 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::HidePathDigits()
{
	bPathDigitsVisible = false;
	digidGroupsSet.clear();
	digidNodesSet.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitTracker::IsActive() const
{
	return bActive;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::SetActive( bool bState )
{
	bActive = bState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitTracker::IsSelected() const
{
	return bSelected;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::SetSelected( bool bState )
{
	bSelected = bState;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitTracker::IsHilighted() const
{
	return bHilighted;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::SetHilighted( bool bState )
{
	bHilighted = bState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::ShowSelection()
{
	if ( bSelected )
		pSelection = pMission->GetRenderGame()->Select( pUnit, V_SELECTIONCOLOR_SELECT );
	else if ( bHilighted )
		pSelection = pMission->GetRenderGame()->Select( pUnit, V_SELECTIONCOLOR_HILIGHT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::HideSelection()
{
	pSelection = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::Update()
{
	if ( pMission->IsInterfaceHidden() || ( !IsSelected() && !IsHilighted() ) || !pMission->IsRealTime() && ( pUnit->GetPlayer() != pMission->GetActivePlayer()->GetPlayer() ) )
	{
		HidePath();
		HideSelection();
		HidePathDigits();
	}
	else
	{
		ShowPath();
		ShowSelection();

		if ( !pMission->IsRealTime() )
			ShowPathDigits();
		else
			HidePathDigits();
	}

	enemiesList.clear();

	vector<CPtr<NWorld::CUnit> > visibleUnits;
	pUnit->GetVisible( &visibleUnits );
	for ( int nTemp = 0; nTemp < visibleUnits.size(); nTemp++ )
	{
		if ( GetUnitDiplomacy( visibleUnits[nTemp] ) != NDb::DS_ENEMY )
			continue;

		enemiesList.push_back( visibleUnits[nTemp] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::EDiplomacyState CUnitTracker::GetUnitDiplomacy( NWorld::CUnit *pTestUnit ) const
{
	return pMission->GetWorld()->GetDiplomacyState( pTestUnit, pUnit->GetPlayer() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::GetVisibleEnemiesList( list<CPtr<NWorld::CUnit> > *pEnemies ) const
{
	*pEnemies = enemiesList;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CUnit* CUnitTracker::GetNextVisibleEnemy()
{
	if ( enemiesList.empty() )
	{
		pLastEnemyUnit = 0;
		return 0;
	}

	list<CPtr<NWorld::CUnit> >::iterator iTemp = enemiesList.begin();
	if ( IsValid( pLastEnemyUnit ) )
	{
		list<CPtr<NWorld::CUnit> >::iterator iSearch = find( enemiesList.begin(), enemiesList.end(), pLastEnemyUnit );
		if ( iSearch != enemiesList.end() )
		{
			iTemp = iSearch;
			iTemp++;
		}
	}

	for ( int nTemp = 0; nTemp < enemiesList.size(); nTemp++ )
	{
		if ( iTemp == enemiesList.end() )
			iTemp = enemiesList.begin();

		if ( !(*iTemp)->IsDead() )
		{
			pLastEnemyUnit = (*iTemp);
			return (*iTemp);
		}

		iTemp++;
	}

	pLastEnemyUnit = 0;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::SmoothPathLine( const vector<NWorld::SPathPoint> &points, vector<NWorld::SPathPoint> *pRes )
{
	if ( points.size() < 2 )
		return;

	vector<NWorld::SPathPoint> smoothedPoints( points.size() );
	for ( int nTemp = 0; nTemp < points.size(); nTemp++ )
		smoothedPoints[nTemp] = points[points.size() - 1 - nTemp];

	while ( SmoothIteration( &smoothedPoints ) > 0.05f );

	float fDistance = 0;
	pRes->push_back( smoothedPoints.front() );
	for( int nTemp = 0; nTemp < smoothedPoints.size() - 1; nTemp++ )
	{
		const NWorld::SPathPoint &sTemp = smoothedPoints[nTemp];
		const NWorld::SPathPoint &sNext = smoothedPoints[nTemp + 1];

		fDistance += fabs( sNext.vPoint - sTemp.vPoint );
		if ( fDistance > F_PATHPOINT_DIST )
		{
			pRes->push_back( sNext );
			fDistance = 0;
		}
	}
	pRes->push_back( smoothedPoints.back() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitTracker::SmoothIteration( vector<NWorld::SPathPoint> *pRes )
{
	vector<NWorld::SPathPoint> &points = (*pRes);

	vector<NWorld::SPathPoint> newPoints;
	newPoints.reserve( points.size() * 2 + 1 );
	for ( int nTemp = 0; nTemp < points.size() - 1; ++nTemp )
	{
		const NWorld::SPathPoint &sTemp = points[nTemp];
		const NWorld::SPathPoint &sNext = points[nTemp + 1];

		NWorld::SPathPoint sPoint;
		sPoint.nAP = Max( sTemp.nAP, sNext.nAP );
		sPoint.nFloor = Min( sTemp.nFloor, sNext.nFloor );
		sPoint.vPoint.x = ( sTemp.vPoint.x + sNext.vPoint.x) / 2;
		sPoint.vPoint.y = ( sTemp.vPoint.y + sNext.vPoint.y) / 2;
		sPoint.vPoint.z = ( sTemp.vPoint.z + sNext.vPoint.z) / 2;
		newPoints.push_back( sTemp );
		newPoints.push_back( sPoint );
	}
	newPoints.push_back( points.back() );

	for ( int nTemp = 1; nTemp < points.size() - 1; ++nTemp )
	{
		const NWorld::SPathPoint &sTemp = points[nTemp];
		const NWorld::SPathPoint &sNext = points[nTemp + 1];
		const NWorld::SPathPoint &sPrev = points[nTemp - 1];

		newPoints[nTemp * 2].vPoint.x = ( 4.0f * sTemp.vPoint.x + sPrev.vPoint.x + sNext.vPoint.x) / 6.0f;
		newPoints[nTemp * 2].vPoint.y = ( 4.0f * sTemp.vPoint.y + sPrev.vPoint.y + sNext.vPoint.y) / 6.0f;
		newPoints[nTemp * 2].vPoint.z = ( 4.0f * sTemp.vPoint.z + sPrev.vPoint.z + sNext.vPoint.z) / 6.0f;
	}
	points = newPoints;

	float fMax = 0;
	for ( int nTemp = 0; nTemp < points.size() - 1; ++nTemp )
		fMax = Max( fMax, fabs( points[nTemp + 1].vPoint - points[nTemp].vPoint ) );

	return fMax;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0x12221220, CUnitTracker )
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0x12221221, CProjectedText )
