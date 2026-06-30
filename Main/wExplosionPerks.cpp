#include "StdAfx.h"
#include "wExplosionPerks.h"

namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// SPerkMineModifiers::Fill @0x354960 -- collect the unit's explosive-perk modifiers from its
// RPG perks. A null or invalid (being-deleted) unit leaves the struct untouched. Each perk's
// modifier value is returned through HasPerk's first out-param; the flag perk (0x35) has none.
void SPerkMineModifiers::Fill( NRPG::CUnit *pUnit )
{
	if ( !IsValid( pUnit ) )
		return;
	float fModifier = 0;
	if ( pUnit->HasPerk( 0x30, &fModifier ) )
		fStructureDmgModifier = fModifier;
	if ( pUnit->HasPerk( 0x35 ) )
		bAlwaysHumanCritical = true;
	if ( pUnit->HasPerk( 0x5e, &fModifier ) )
		fAEDmgModifier = fModifier;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}  // namespace NWorld
