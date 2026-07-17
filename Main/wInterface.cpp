#include "StdAfx.h"
#include "wInterface.h"
#include "wMain.h"
#include "aiPath.h"
#include "RPGUnitInfo.h"
#include "wUnitCommands.h"
// CRAP{ for smooth path visualization
#include "GAnimPath.h"
#include "scriptCallLUA.h"
// CRPA}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPathViewer
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddSegment( vector<CVec3> *pRes, NAI::CPath *pPath )
{
	NAI::SPosition pos;
	pos.SetNetwork( pPath->pNet );
	pRes->resize( pPath->points.size() );
	for ( int i = 0; i < pRes->size(); ++i )
	{
		pos.p = pPath->points[i];
		(*pRes)[i] = pos.GetCP();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddSmoothedSegment( vector<CVec3> *pRes, NAI::CPath *pSegment )
{
	float fT = 0;
	vector<CVec3> tmp;
	AddSegment( &tmp, pSegment );
	CObj<NAnimation::CPathInterpolator> pInt( new NAnimation::CPathInterpolator( tmp ) );
	pRes->clear();
	pRes->push_back( pInt->GetPosition(0) );
	while ( fT < pInt->GetDistance() )
	{
		fT += 0.1f;
		pRes->push_back( pInt->GetPosition( fT ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::EPose CUnit::GetPose() const 
{ 
	return GetPosition().GetPose(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCmdSetCommand
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCmdSetCommand::IsSkippable() const 
{ 
	return pCmd->IsSkippable(); 
}
/*
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathViewer::GetPoints( vector<CVec3> *pRes )
{
	pRes->resize( 0 );
	AddSegment( pRes, pPath );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathViewer::GetSmoothed( vector<CVec3> *pRes )
{
	pRes->resize( 0 );
	AddSmoothedSegment( pRes, pPath );
}
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCmdCallScriptFunction
////////////////////////////////////////////////////////////////////////////////////////////////////
CCmdCallScriptFunction::CCmdCallScriptFunction( string _szFuncName, char *szParams, ...  ):
	szFuncName( _szFuncName )
{
	va_list l;
	va_start( l, szParams );
	NScript::luaMakeCallParamsVector( szParams, &l, &params );
	va_end( l );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
IWorld* CreateWorld( NRPG::CGlobalGame *_pGlobalGame )
{
	CWorld *pRes = new NWorld::CWorld( _pGlobalGame );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NWorld;
BASIC_REGISTER_CLASS( CUnit );
REGISTER_SAVELOAD_CLASS( 0x02731140, CCmdEndOfTurn )
REGISTER_SAVELOAD_CLASS( 0x02731130, CCommander )
// (0x02741156 CCmdQuitGame REMOVED -- W5: never-constructed stub, id absent from retail)
REGISTER_SAVELOAD_CLASS( 0xB13b1141, CAckEvent )
REGISTER_SAVELOAD_CLASS( 0xB13b1142, CHitLocator )
REGISTER_SAVELOAD_CLASS( 0x013b1140, CActionCounter )
REGISTER_SAVELOAD_CLASS( 0x004C1140, CCmdCancel )
REGISTER_SAVELOAD_CLASS( 0x01122120, CCmdSetCommand )
REGISTER_SAVELOAD_CLASS( 0x23075380, CCmdPlayAck )		// retail id (oracle s2_ccmdplayack.h)
REGISTER_SAVELOAD_CLASS( 0x00812150, CWorldSyncSrc )
REGISTER_SAVELOAD_TEMPL_CLASS( 0x01412160, CSetSyncSrc<IVisObj>, CSetSyncSrc )
typedef CBoolSyncSrc<IVisObj,CIntersectionFunc> TIntersectFunc;
REGISTER_SAVELOAD_TEMPL_CLASS( 0x01412161, TIntersectFunc, CBoolSyncSrc )
typedef CBoolSyncSrc<IVisObj,CUnionFunc> TUnionFunc;
REGISTER_SAVELOAD_TEMPL_CLASS( 0x01412162, TUnionFunc, CBoolSyncSrc )
// retail carries a registered CBoolSyncSrc<IVisObj,CSubtractFunc> (NewCBoolSyncSrc @0x35f660);
// used by CRenderGame::UpdateVisible's no-viewer branch (heard-marker subtraction under show-all)
typedef CBoolSyncSrc<IVisObj,CSubtractFunc> TSubtractFunc;
REGISTER_SAVELOAD_TEMPL_CLASS( 0x01412163, TSubtractFunc, CBoolSyncSrc )
REGISTER_SAVELOAD_CLASS( 0x51402141, CCmdInterfaceEvent )
BASIC_REGISTER_CLASS( IItem )
BASIC_REGISTER_CLASS( IObject )
REGISTER_SAVELOAD_CLASS( 0x51822130, CCmdCallScriptFunction )
REGISTER_SAVELOAD_CLASS( 0xB3625120, CEarthQuakeEvent )   // retail id (grenade-blast camera shake)