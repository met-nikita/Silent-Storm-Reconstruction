#include "StdAfx.h"
//
#include "..\DBFormat\DataMap.h"
//
#include "RPGUnitMission.h"
#include "RPGDiplomacy.h"
//
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDiplomacy
////////////////////////////////////////////////////////////////////////////////////////////////////
SDiplomacy::SDiplomacy( DWORD _nDiplomacy ) : nDiplomacy( _nDiplomacy )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::EDiplomacyState SDiplomacy::GetDiplomacyState( int nPlayer ) const
{
	ASSERT( nPlayer >= 0 && nPlayer < 16 );
	return NDb::EDiplomacyState( ( nDiplomacy & ( 3 << ( nPlayer << 1 ) ) ) >> ( nPlayer << 1 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SDiplomacy::SetDiplomacyState( int nPlayer, NDb::EDiplomacyState state )
{
	ASSERT( nPlayer >= 0 && nPlayer < 16 );
	nDiplomacy &= ~( 3 << ( nPlayer << 1 ) );
	nDiplomacy |= ( int( state ) << ( nPlayer << 1 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGlobalDiplomacy 
////////////////////////////////////////////////////////////////////////////////////////////////////
CGlobalDiplomacy::CGlobalDiplomacy()
{
	diplomacy.resize( 16 );
	MakeSelfAFriend();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalDiplomacy::MakeSelfAFriend()
{
	for ( int k = 0; k < diplomacy.size(); ++k )
		diplomacy[k].SetDiplomacyState( k, NDb::DS_ALLY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalDiplomacy::LoadDiplomacy( int nVariantID )
{
	CDBPtr<NDb::CTemplVariant> pVariant = NDb::GetTemplVariant( nVariantID );
	if ( !IsValid( pVariant ) || !IsValid( pVariant->pDiplomacy ) )
		return;
	//
	for ( int i = 0; i < 16; ++i )
		diplomacy[ i ].SetDiplomacy( pVariant->pDiplomacy->diplomasies[ i ] );
	MakeSelfAFriend();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SDiplomacy& CGlobalDiplomacy::GetPlayerDiplomacy( int nPlayer ) const
{ 
	return diplomacy[nPlayer];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalDiplomacy::SetPlayerDiplomacy( int nPlayer, 
	const vector< CPtr<NRPG::IUnitMission> > &playerUnits, DWORD nDiplomacy )
{
	SDiplomacy dip( nDiplomacy );
	dip.SetDiplomacyState( nPlayer, NDb::DS_ALLY );
	diplomacy[nPlayer] = dip;
	//
	for ( int i = 0; i < playerUnits.size(); ++i )
		playerUnits[i]->SetDiplomacy( dip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::EDiplomacyState CGlobalDiplomacy::GetDiplomacyState( int nWho, int nToWhom ) const
{
	NDb::EDiplomacyState res = diplomacy[nWho].GetDiplomacyState( nToWhom );
	ASSERT( nWho != nToWhom || res == NDb::DS_ALLY );
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalDiplomacy::SetDiplomacyState( int nPlayer1, 
	const vector< CPtr<NRPG::IUnitMission> > &player1Units, int nPlayer2, NDb::EDiplomacyState state )
{
	if ( nPlayer1 == nPlayer2 )
	{
		ASSERT(0);
		return;
	}
	diplomacy[nPlayer1].SetDiplomacyState( nPlayer2, state );
	//
	for ( vector< CPtr<NRPG::IUnitMission> >::const_iterator
		i = player1Units.begin(); i != player1Units.end(); ++i )
	{
		NRPG::IUnitMission *pUnit = *i;
		SDiplomacy dip = pUnit->GetDiplomacy();
		dip.SetDiplomacyState( nPlayer2, state );
		pUnit->SetDiplomacy( dip );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CGlobalDiplomacy* CreateGlobalDiplomacy()
{
	return new CGlobalDiplomacy();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NRPG;
REGISTER_SAVELOAD_CLASS( 0x50692161, CGlobalDiplomacy )