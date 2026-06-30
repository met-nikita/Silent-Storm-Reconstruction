#include "StdAfx.h"
#include "DataFormat.h"
#include "DataMap.h"
#include "DataRPGTmp.h"
#include "DataAnimation.h"
#include "DataTerrain.h"
#include "..\Main\Grid.h"
#include "..\Misc\StrProc.h"
#include <limits>
#include "DataScenario.h"
#include "DataSound.h"
#include "DataText.h"
#include "DataObject.h"
#include "DataInterface.h"
#include "DataLight.h"
#include "DataAI.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
externA5 CVec3 GetColor( DWORD dwColor );

inline void DebugStringInt( const char *str, int i )
{
	char buf[512];
	sprintf( buf, "%s %d\n", str, i );
	OutputDebugString( buf );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// PushItem<T> hoisted to DataFormat.h so DataFaceGen.cpp (and any other DB-record TU) can share one
// dedup-append template; redefining it here too would be an ODR / function-template redefinition clash.
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef numeric_limits<int> LIM_INT;
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
int SVariantFlags::operator&( CStructureSaver &f )
{
	f.Add( 1, &flags );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void UnpackVariantFlags( const string &str, vector<SVariantFlags> *pFlags )
{
	pFlags->clear();
	if ( str.empty() )
		return;
	
	vector<string> sets;
	// ������ ������ ����� ����� ����������� ';'
	NStr::SplitString( str, sets, ';' );
	pFlags->resize( sets.size() );
	for ( int i = 0; i < sets.size(); ++i )
	{
		if ( sets[i].empty() )
			continue;
		vector<int> &flags = (*pFlags)[i].flags;
		vector<string> attrInds;
		// ������ ������ ����������� ','
		NStr::SplitString( sets[i], attrInds, ',' );
		for ( int j = 0; j < attrInds.size(); ++j )
			flags.push_back( atoi( attrInds[j].c_str() ) );

		std::sort( flags.begin(), flags.end() );
	}	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGlobalMap
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalMap::Import()
{
	NDatabase::ImportField( "Scenario", &pScenario );
	NDatabase::ImportField( "Background", &pBackground );
	NDatabase::ImportField( "BaseZoneID", &pBaseZone );
	NDatabase::ImportField( "StartZoneID", &pStartZone );   // faction intro-mission zone (Axis map3=123 GFirst, Allies map4=122 EFirst)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterMap
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterMap::Import()
{
	for ( int nTemp = 0; nTemp < 4; nTemp++ )
	{
		int nID = 0;
		NDatabase::ImportField( NStr::Format( "CampZone%d", nTemp + 1 ), &nID );

		if ( nID != 0 )
			campZonesSet.push_back( nID );
	}

	NDatabase::ImportField( "Background", &pBackground );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTemplate
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplate::Import()
{
  NDatabase::ImportField( "Width", &nWidth );
  NDatabase::ImportField( "Height", &nHeight );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CTemplate::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CRndPtr<CTemplVariant>*)this );
  f.Add( 3, &nWidth );
  f.Add( 4, &nHeight );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTemplVariant
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplVariant::Import()
{
	NDatabase::ImportField( "Grid", &bGrid );
	string str;
	NDatabase::ImportField( "Flags", &str );
	UnpackVariantFlags( str, &flags );
	NDatabase::ImportField( "TemplateID", &pTemplate );
	NDatabase::ImportField( "DefaultLight", &pLight );
	NDatabase::ImportField( "ColorMap", &pColorMap );
	NDatabase::ImportField( "ScriptID", &pScript );
	NDatabase::ImportField( "Border", &nBorder );
	NDatabase::ImportField( "AmbientMusic", &pAmbientMusic );
	NDatabase::ImportField( "CombatMusic", &pCombatMusic );
	string szBlendType;
	NDatabase::ImportField( "HMBlendType", &szBlendType );
	eHMBlendType = BT_NORMAL;
	if ( szBlendType == "Normal" )
		eHMBlendType = BT_NORMAL;
	else if ( szBlendType == "Add" )
		eHMBlendType = BT_ADD;
	else if ( szBlendType == "Subtract" )
		eHMBlendType = BT_SUBTRACT;
	else
	{
		//ASSERT(0);
	}
	//
	if ( IsValid( pTemplate ) )
	{
		if ( PushItem( &pTemplate->variants, this ) )
		{
			float fRndWeight;
			NDatabase::ImportField( "RndWeight", &fRndWeight );
			pTemplate->roulette.AddSector( fRndWeight );
		}
	}
	else
	{
		int nTemplateID = -1;
		NDatabase::ImportField( "TemplateID", &nTemplateID );
		ASSERT(0);
	}
	NDatabase::ImportField( "DiplomacyID", &pDiplomacy );
	NDatabase::ImportField( "ShowTerrain", &bShowTerrain );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRectangle
////////////////////////////////////////////////////////////////////////////////////////////////////
void ImportRectCoords( CVec2 *pptCenter, float fWidth, float fHeight, float fRotation )
{
	CVec2 &ptCenter = *pptCenter;
	ptCenter.x *= FP_GRID_STEP;
	ptCenter.y *= FP_GRID_STEP;

	CVec2 ptShift( -fWidth * FP_GRID_STEP * 0.5f, -fHeight * FP_GRID_STEP * 0.5f );
	const float fAng = -ToRadian( fRotation );
	float fc = cos( fAng );
	float fs = sin( fAng );
	float x = fc * ptShift.x + fs * ptShift.y;
	ptShift.y = -fs * ptShift.x + fc * ptShift.y;
	ptShift.x = x;
	ptCenter += ptShift;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportRectCoords( CVec2 *pptCenter, float fWidth, float fHeight, float fRotation )
{
	CVec2 &ptCenter = *pptCenter;

	CVec2 ptShift( fWidth * FP_GRID_STEP * 0.5f, fHeight * FP_GRID_STEP * 0.5f );
	const float fAng = -ToRadian( fRotation );
	float fc = cos( fAng );
	float fs = sin( fAng );
	float x = fc * ptShift.x + fs * ptShift.y;
	ptShift.y = -fs * ptShift.x + fc * ptShift.y;
	ptShift.x = x;
	ptCenter += ptShift;

	ptCenter.x *= FP_INV_GRID_STEP;
	ptCenter.y *= FP_INV_GRID_STEP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectangle::Import()
{
  NDatabase::ImportField( "VariantID", &pVariant );
  NDatabase::ImportField( "CenterX", &ptCenter.x );
  NDatabase::ImportField( "CenterY", &ptCenter.y );
  NDatabase::ImportField( "Width", &fWidth );
  NDatabase::ImportField( "Height", &fHeight );
  NDatabase::ImportField( "TemplateLink", &pTemplate );
  NDatabase::ImportField( "Rotation", &fRotation );
	NDatabase::ImportField( "Floor", &nFloor );
	NDatabase::ImportField( "DeltaZ", &fDZ );
	string szParams;
	NDatabase::ImportField( "Params", &szParams );
	NStr::SplitString( szParams, vszParams, ';' );
	//
	ImportRectCoords( &ptCenter, fWidth, fHeight, fRotation );
	ASSERT( IsValid( pTemplate ) );
	if ( IsValid( pVariant ) )
	{
		PushItem( &pVariant->rects, this );
	}
	else
	{
		int nVarID = -1;
		NDatabase::ImportField( "VariantID", &nVarID );
		ASSERT(0);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFinalElement
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFinalElement::Import()
{
  NDatabase::ImportField( "ModelID", &pObject );
#ifdef _DEBUG
	if ( !IsValid( pObject ) )
	{
		char buf[128];
		sprintf( buf, "bad fin element %d\n", GetRecordID() );
		OutputDebugString( buf );
		ASSERT(0);
	}
#endif
  NDatabase::ImportField( "VariantID", &pVariant );
  NDatabase::ImportField( "Rotation", &fRotation );
	NDatabase::ImportField( "PosX", &ptPos.x );
	NDatabase::ImportField( "PosY", &ptPos.y );
	NDatabase::ImportField( "Floor", &nFloor );
	NDatabase::ImportField( "DeltaZ", &fDZ );
	NDatabase::ImportField( "Lightmap", &bLightmap );
	NDatabase::ImportField( "ScaleX", &ptScale.x );
	NDatabase::ImportField( "ScaleY", &ptScale.y );
	NDatabase::ImportField( "ScaleZ", &ptScale.z );
	NDatabase::ImportField( "OpenObject", &bOpen );
	NDatabase::ImportField( "Power", &fPower );
	NDatabase::ImportField( "Radius", &fRadius );
	NDatabase::ImportField( "PassageZoneID", &nPassageZoneID );
	NDatabase::ImportField( "PassageObjectID", &nPassageObjectID );
	NDatabase::ImportField( "APRadius", &nAPRadius );
	NDatabase::ImportField( "Name", &szName );
	int dwColor;
	NDatabase::ImportField( "PointLight", &dwColor );
	vLightCr = GetColor( dwColor );
	NDatabase::ImportField( "LightPosX", &ptLightPos.x );
	NDatabase::ImportField( "LightPosY", &ptLightPos.y );
	NDatabase::ImportField( "LightPosZ", &ptLightPos.z );
	NDatabase::ImportField( "LightRadius", &fLightRadius );
	NDatabase::ImportField( "LightFlareRadius", &fFlareRadius );
	NDatabase::ImportField( "LightFlareTexture", &pFlareTexture );
	NDatabase::ImportField( "LightParam", &szLightParams );
	NDatabase::ImportField( "FlarePosX", &ptFlarePos.x );
	NDatabase::ImportField( "FlarePosY", &ptFlarePos.y );
	NDatabase::ImportField( "FlarePosZ", &ptFlarePos.z );
	NDatabase::ImportField( "ObjectPhase", &nObjectPhase );
	NDatabase::ImportField( "ObjStageDelta", &nObjStageDelta );
	NDatabase::ImportField( "ObjRadius", &fObjRadius );
	NDatabase::ImportField( "Grenade", &pGrenade );
	NDatabase::ImportField( "Armed", &bArmed );
	//
	if ( IsValid( pVariant ) )
	{
		PushItem( &pVariant->pFinalElements, this );
	}
	else
	{
		int nVarID = -1;
		NDatabase::ImportField( "VariantID", &nVarID );
		ASSERT(0);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CParticleSource
////////////////////////////////////////////////////////////////////////////////////////////////////
void CContainer::Import()
{
  NDatabase::ImportField( "ModelID", &pModel );
  NDatabase::ImportField( "VariantID", &pVariant );
  NDatabase::ImportField( "Rotation", &nRotation );
	NDatabase::ImportField( "PosX", &ptPos.x );
	NDatabase::ImportField( "PosY", &ptPos.y );
	NDatabase::ImportField( "Floor", &nFloor );
	NDatabase::ImportField( "DeltaZ", &fDZ );
	NDatabase::ImportField( "RoomID", &nRoomID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CContainer::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
  f.Add( 2, &pModel );
  f.Add( 3, &pVariant ); 
  f.Add( 4, &nRotation ); 
	f.Add( 5, &ptPos.x );
	f.Add( 6, &ptPos.y );
	f.Add( 7, &nFloor );
	f.Add( 8, &fDZ );
	f.Add( 9, &nRoomID );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::Import()
{
  NDatabase::ImportField( "VariantID", &pVariant );
  NDatabase::ImportField( "Rotation", &fRotation );
  NDatabase::ImportField( "MonsterID", &pMonster );
	NDatabase::ImportField( "PosX", &ptPos.x );
	NDatabase::ImportField( "PosY", &ptPos.y );
	NDatabase::ImportField( "Floor", &nFloor );
	NDatabase::ImportField( "ClueSlot", &bClueSlot );
	NDatabase::ImportField( "ClueInventorySlot", &bClueInventorySlot );
	NDatabase::ImportField( "Player", &nPlayer );
	NDatabase::ImportField( "Diplomacy", &nDiplomacy );
	NDatabase::ImportField( "RelativeLevel", &nRelativeLevel );
	NDatabase::ImportField( "Group", &pGroup );
	NDatabase::ImportField( "Name", &szName );
	string szPose;
	NDatabase::ImportField( "Pose", &szPose );
	if ( szPose == "Stand" )
		eInitialPose = UP_STAND;
	else if ( szPose == "Crouch" )
		eInitialPose = UP_CROUCH;
	else if ( szPose == "Crawl" )
		eInitialPose = UP_CRAWL;
	else
	{
		ASSERT(0);
		eInitialPose = UP_STAND;
	}
	string szLogic;
	NDatabase::ImportField( "Logic", &szLogic );
	if ( szLogic == "Guard" )
		eLogic = UL_EMPTY;
	else if ( szLogic == "Sentry" )
		eLogic = UL_DEFAULT;
	else if ( szLogic == "Roaming" )
		eLogic = UL_ROAMING;
	else if ( szLogic == "Fear" )
		eLogic = UL_FEAR;
	else if ( szLogic == "Default" )
		eLogic = UL_DEFAULT;
	else
	{
		ASSERT(0);
		eLogic = UL_DEFAULT;
	}
	NDatabase::ImportField( "RoamingRadius", &nRoamingRadius );
	NDatabase::ImportField( "FearUseToHit", &bFearUseToHit );
	//
	if ( IsValid( pVariant ) )
	{
		PushItem( &pVariant->pUnits, this );
	}
	else
	{
		int nVarID = -1;
		NDatabase::ImportField( "VariantID", &nVarID );
		ASSERT(0);
	}
	NDatabase::ImportField( "GuardAnimation", &pGuardAnimation );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWallModel
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallModel::Import()
{
  NDatabase::ImportField( "ModelID", &pModel );
	NDatabase::ImportField( "Length", &nLength );
	NDatabase::ImportField( "Width", &fWidth );
	NDatabase::ImportField( "Height", &fHeight );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CWallModel::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
  f.Add( 2, &pModel ); 
	f.Add( 3, &nLength );
	f.Add( 4, &fWidth );
	f.Add( 5, &fHeight );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFloorModel
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloorModel::Import()
{
  NDatabase::ImportField( "ModelID", &pModel );
	NDatabase::ImportField( "Width", &nWidth );
	NDatabase::ImportField( "Length", &nLength );		
	NDatabase::ImportField( "Rotation", &nRotation );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CFloorModel::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
  f.Add( 2, &pModel ); 
	f.Add( 3, &nLength );
	f.Add( 4, &nWidth );
	f.Add( 5, &nRotation );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSolidModel
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSolidModel::Import()
{
  NDatabase::ImportField( "ModelID", &pModel );
	NDatabase::ImportField( "Length", &nLength );
	NDatabase::ImportField( "Width", &nWidth );
	NDatabase::ImportField( "Rotation", &nRotation );
	NDatabase::ImportField( "Height", &nHeight );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSolidModel::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
  f.Add( 2, &pModel ); 
	f.Add( 3, &nLength );
	f.Add( 4, &nWidth );
	f.Add( 5, &nRotation );
	f.Add( 6, &nHeight );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWall
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWall::Import()
{
  NDatabase::ImportField( "VariantID", &pVariant );
	NDatabase::ImportField( "WallModelID", &pModel );
	NDatabase::ImportField( "Floor", &nFloor );
	NDatabase::ImportField( "StartX", &ptStart.x );
	NDatabase::ImportField( "StartY", &ptStart.y );
	NDatabase::ImportField( "EndX", &ptEnd.x );
	NDatabase::ImportField( "EndY", &ptEnd.y );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CWall::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
  f.Add( 2, &pVariant );
	f.Add( 3, &pModel );
	f.Add( 4, &nFloor );
	f.Add( 5, &ptStart.x );
	f.Add( 6, &ptStart.y );
	f.Add( 7, &ptEnd.x );
	f.Add( 8, &ptEnd.y );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFloor
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloor::Import()
{
  NDatabase::ImportField( "VariantID", &pVariant );
	NDatabase::ImportField( "ModelID", &pModel );
	NDatabase::ImportField( "Floor", &nFloor );
	NDatabase::ImportField( "Coords", &szCoords );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CFloor::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
  f.Add( 2, &pVariant );
	f.Add( 3, &pModel );
	f.Add( 4, &nFloor );
	f.Add( 5, &szCoords );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSolid
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSolid::Import()
{
  NDatabase::ImportField( "VariantID", &pVariant );
	NDatabase::ImportField( "ModelID", &pModel );
	NDatabase::ImportField( "Floor", &nFloor );
	NDatabase::ImportField( "Coords", &szCoords );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSolid::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
  f.Add( 2, &pVariant );
	f.Add( 3, &pModel );
	f.Add( 4, &nFloor );
	f.Add( 5, &szCoords );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRoom
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRoom::Import()
{
  NDatabase::ImportField( "VariantID", &pVariant );
	NDatabase::ImportField( "ModelID", &nRoomID );
	NDatabase::ImportField( "Floor", &nFloor );
	NDatabase::ImportField( "Coords", &szCoords );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRoom::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
  f.Add( 2, &pVariant );
	f.Add( 3, &nRoomID );
	f.Add( 4, &nFloor );
	f.Add( 5, &szCoords );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAttribute
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAttribute::Import()
{
	NDatabase::ImportField( "UserName", &szName );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAttribute::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
  f.Add( 2, &szName );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExplosion
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplosion::Import()
{
	NDatabase::ImportField( "Floor", &nFloor );
	NDatabase::ImportField( "PosX", &ptPos.x );
	NDatabase::ImportField( "PosY", &ptPos.y );
	NDatabase::ImportField( "DeltaZ", &fDZ );
	NDatabase::ImportField( "Power", &fPower );
	NDatabase::ImportField( "VariantID", &pVariant );
	fRotation = 0;
	if ( IsValid( pVariant ) )
	{
		PushItem( &pVariant->explosions, this );
	}
	else
	{
		int nVarID = -1;
		NDatabase::ImportField( "VariantID", &nVarID );
		ASSERT(0);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExplosion::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
  f.Add( 2, &nFloor );
	f.Add( 3, &ptPos );
	f.Add( 4, &fDZ );
	f.Add( 5, &fPower );
	f.Add( 6, &fRotation );
	f.Add( 7, &pVariant );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CConstructionPart::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pGeometry );
	f.Add( 3, &nSizeX );
	f.Add( 4, &nSizeY );
	f.Add( 5, &nSizeZ );
	f.Add( 6, &fThickness );
	for ( int i = 0; i < N_CONSTRUCTION_MATERIALS; ++i )
		f.Add( 10+i, &pDefMaterials[i] );
	f.Add( 20, &nSubPartsMask );
	f.Add( 21, &pObject );
	f.Add( 22, &p2ndGeometry );
	f.Add( 23, &pArmor );
	f.Add( 24, &nClipGroup );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CConstructionPart::SetPrimaryPart( int *pnMask, int x, int y, bool bPrimary )
{
	const int nBit = y * N_SUBPARTS + x;
	*pnMask = (*pnMask & ~(0x1 << nBit)) | (int( !bPrimary ) << nBit);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRndTerrainSpot
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRndTerrainSpot::Import()
{
	NDatabase::ImportField( "MaterialID", &pSpot );
	NDatabase::ImportField( "PosX", &ptPos.x );
	NDatabase::ImportField( "PosY", &ptPos.y );
	NDatabase::ImportField( "SizeX", &ptSize.x );
	NDatabase::ImportField( "SizeY", &ptSize.y );
	NDatabase::ImportField( "Rotation", &nRotation );
	NDatabase::ImportField( "PlacementID", &pVar );
	NDatabase::ImportField( "Priority", &nPriority );
	NDatabase::ImportField( "Layer", &nLayer );
	ASSERT( IsValid( pSpot ) );
	if ( IsValid( pVar ) )
		PushItem( &pVar->terrainSpots, this );
	else
	{
		ASSERT( 0 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBDiplomacy
////////////////////////////////////////////////////////////////////////////////////////////////////
int CDBDiplomacy::operator&( CStructureSaver &f )
{ 
	f.Add(2,(CDBRecord *)this); 
	for ( int i = 0; i < 16; ++i )
		f.Add( i + 3, &diplomasies[ i ] );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBDiplomacy::Import()
{
	for ( int i = 0; i < 16; ++i )
	{
		char szNum[ 128 ];
		sprintf( szNum, "%d", i + 1 );
		string szFieldName = string( "Diplomacy" ) + string( szNum );
		NDatabase::ImportField( szFieldName.c_str(), &diplomasies[ i ] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRndTerrainSpot::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pSpot );
	f.Add( 3, &ptPos );
	f.Add( 4, &ptSize );
	f.Add( 5, &nRotation );
	f.Add( 6, &pVar );
	f.Add( 7, &nPriority );
	f.Add( 8, &nLayer );

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTerrainSpot* CRndTerrainSpot::CreateTerrainSpot( SRand *pRand, const vector<int> &params ) const
{
	if ( !IsValid( pSpot ) || !IsValid( pSpot->pMaterial ) )
		return 0;
	CTerrainSpot *pRet = new CTerrainSpot;
	pRet->pMaterial = pSpot->pMaterial->GetMaterial( pRand, params );
	pRet->ptPos = ptPos;
	pRet->ptSize = ptSize;
	pRet->nRotation = nRotation;
	pRet->nPriority = nPriority;
	pRet->pArmor = pSpot->pArmor;
	return pRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTerrainSpot* CRndTerrainSpot::CreateTerrainSpot( SRand *pRand ) const
{
	vector<int> params;
	return CreateTerrainSpot( pRand, params );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointName::Import()
{
	NDatabase::ImportField( "UserName", &szName );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypoint::Import()
{
	NDatabase::ImportField( "NameID", &pName );
	NDatabase::ImportField( "VariantID", &pVar );
	if ( IsValid( pVar ) )
		PushItem( &pVar->waypoints, this );
	else
	{
		int nVarID = -1;
		NDatabase::ImportField( "VariantID", &nVarID );
		ASSERT(0);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void UnpackFloorTiles( const string &str, vector<CVec2> *pRes )
{
	const int nTiles = str.length() / 2;
	for ( int j = 0; j < nTiles; ++j )
	{
		CVec2 res;
		res.x = str[j<<1] - 1;
		res.y = str[(j<<1) + 1] - 1;
		pRes->push_back( res );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSameRoom
{
	const CPtr<CRoom> &pRoom;
public:
	CSameRoom( const CPtr<CRoom> &r ) : pRoom( r ) { ASSERT( IsValid( pRoom ) ); }
	bool operator() ( const CPtr<CRoom> &pOp ) 
	{ 
		return pRoom->nRoomID == pOp->nRoomID && pRoom->nFloor == pOp->nFloor; 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
void MinMaxTiles( CTemplVariant *pVar, const vector<CPtr<T> > &fragments )
{
	for ( int i = 0; i < fragments.size(); ++i )
	{
		vector<CVec2> tiles;
		const T &frag = *fragments[i];
		UnpackFloorTiles( frag.szCoords, &tiles );
		for ( int k = 0; k < tiles.size(); ++k )
		{
			pVar->nMaxX = Max( pVar->nMaxX, (int)tiles[k].x );
			pVar->nMaxY = Max( pVar->nMaxY, (int)tiles[k].y );
		}
		pVar->nMinFloor = Min( pVar->nMinFloor, frag.nFloor );
		pVar->nMaxFloor = Max( pVar->nMaxFloor, frag.nFloor );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
void MinMaxTiles4Solids( CTemplVariant *pVar, const vector<CPtr<T> > &fragments )
{
	for ( int i = 0; i < fragments.size(); ++i )
	{
		vector<CVec2> tiles;
		const T &frag = *fragments[i];
		UnpackFloorTiles( frag.szCoords, &tiles );
		for ( int k = 0; k < tiles.size(); ++k )
		{
			pVar->nMaxX = Max( pVar->nMaxX, (int)tiles[k].x );
			pVar->nMaxY = Max( pVar->nMaxY, (int)tiles[k].y );
		}
		pVar->nMinFloor = Min( pVar->nMinFloor, frag.nFloor );
		// �������� ������ ����� �������� �� ���� ������ ��� ������� ��� ������, ������� �� �������� 1
		pVar->nMaxFloor = Max( pVar->nMaxFloor, frag.nFloor + frag.pModel->nHeight );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
externA5 const char *pszDBUniformSlotNames[];
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class L>
void AssignItems( L *p = 0 )
{
	CDBTable<L> *pDTable = NDatabase::GetTable<L>();
	CDBIterator<L> it(*pDTable);
	while ( it.MoveNext() )
	{
		L *pW = it.Get();
		if ( IsValid( pW->pPers ) )
		{
			SItemAssign item;
			item.nQuantity = pW->nQuantity;
			item.pItem = pW->pItem;
			CDynamicCast<CRPGClip4Pers> pClip4Pers(pW);
			if (pClip4Pers)
				item.pAmmo = pClip4Pers->pAmmo;
			pW->pPers->items.push_back(item);
		}
	}
}
void GiveItems()
{
	// give items
	AssignItems<CRPGWeapon4Pers>();
	AssignItems<CRPGClip4Pers>();
	AssignItems<CRPGGrenade4Pers>();
	AssignItems<CRPGFirstAid4Pers>();
	AssignItems<CRPGMeleeWeapon4Pers>();
	AssignItems<CRPGMineDetector4Pers>();
	AssignItems<CRPGMine4Pers>();
	AssignItems<CRPGTool4Pers>();
	AssignItems<CRPGKey4Pers>();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void BuildMapLinks( bool bTranslate )
{
	// animation database: group every animation under its skeleton, keyed by type. (Retail builds this
	// here at db-load; this dev had defined BuildMapLinks but never called it -- it is now invoked from
	// Game/Main.cpp right after the game.db load.)
	{
		CDBTable<CAnimation> *pATable = NDatabase::GetTable<CAnimation>();
		CDBIterator<CAnimation> it( *pATable );
		while ( it.MoveNext() )
		{
			CAnimation *pA = it.Get();
			if ( IsValid( pA->pSkeleton ) )
				pA->pSkeleton->pAnimations[ pA->nType ].anims.push_back( pA );
		}
	}
	// debris materials database
	{
		CDBTable<CDebris> *pDTable = NDatabase::GetTable<CDebris>();
		CDBIterator<CDebris> it( *pDTable );
		while ( it.MoveNext() )
		{
			CDebris *pD = it.Get();
			if ( IsValid( pD->pDebrisMaterial ) )
				pD->pDebrisMaterial->debris.push_back( pD );
		}
	}
	// item models to uniforms correspondence
	{
		CDBTable<CRPGItem2Uniform> *pDTable = NDatabase::GetTable<CRPGItem2Uniform>();
		CDBIterator<CRPGItem2Uniform> it(*pDTable);
		while ( it.MoveNext() )
		{
			CRPGItem2Uniform *pW = it.Get();
			if ( IsValid( pW->pItem ) && IsValid( pW->pUniform ) )
			{
				SUniformItem item;
				item.pUniform = pW->pUniform;
				item.pModelActive = pW->pModelActive;
				item.pModelInactive = pW->pModelInactive;
				pW->pItem->looks.push_back(item);
			}
		}
	}
	//
	if ( bTranslate )
	{
		CDBTable<CTranslatedString> *pTable = NDatabase::GetTable<CTranslatedString>();
		CDBIterator<CTranslatedString> it( *pTable );
		while ( it.MoveNext() )
		{
			CTranslatedString *pTS = it.Get();
			CString *pS = GetString( pTS->GetRecordID() );
			if ( !IsValid( pS ) )
			{
				ASSERT(0);
				continue;
			}
			pS->szStr = pTS->szStr;
		}
	}
	GiveItems();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NDb;
REGISTER_SAVELOAD_CLASS( 0x02511170, CTemplate );
REGISTER_SAVELOAD_CLASS( 0x02511171, CTemplVariant );
REGISTER_SAVELOAD_CLASS( 0x02511172, CRectangle );
REGISTER_SAVELOAD_CLASS( 0x02511173, CFinalElement );
REGISTER_SAVELOAD_CLASS( 0x02511174, CUnit );
REGISTER_SAVELOAD_CLASS( 0x02331161, CWallModel );
REGISTER_SAVELOAD_CLASS( 0x02331162, CFloorModel );
REGISTER_SAVELOAD_CLASS( 0x02331163, CWall );
REGISTER_SAVELOAD_CLASS( 0x02331164, CFloor );
REGISTER_SAVELOAD_CLASS( 0x02151170, CSolidModel );
REGISTER_SAVELOAD_CLASS( 0x02151180, CSolid );
REGISTER_SAVELOAD_CLASS( 0x02561180, CContainer );
REGISTER_SAVELOAD_CLASS( 0x00571131, CRoom );
REGISTER_SAVELOAD_CLASS( 0x01481160, CIntermediateFloor );
REGISTER_SAVELOAD_CLASS( 0x01681150, CAttribute );
REGISTER_SAVELOAD_CLASS( 0x02181160, CIntermediateSolid );
REGISTER_SAVELOAD_CLASS( 0x01991140, CContainerModel );
REGISTER_SAVELOAD_CLASS( 0x02891130, CExplosion );
REGISTER_SAVELOAD_CLASS( 0x010a1180, CTConstructionPart );
REGISTER_SAVELOAD_CLASS( 0x010a1181, CConstructionPart );
REGISTER_SAVELOAD_CLASS( 0x011c1130, CRndTerrainSpot );
REGISTER_SAVELOAD_CLASS( 0x011c1210, CTerrainSpot );
REGISTER_SAVELOAD_CLASS( 0xA2722160, CWaypoint );
REGISTER_SAVELOAD_CLASS( 0xA0532130, CWaypointName );
REGISTER_SAVELOAD_CLASS( 0x52012140, CDBDiplomacy );