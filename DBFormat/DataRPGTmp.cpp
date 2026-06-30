#include "StdAfx.h"
#include "DataFormat.h"
#include "DataRPGTmp.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
template <class L>
void ImportItem( L *pLink )
{
	NDatabase::ImportField( "RPGItemID", &pLink->pItem );
	NDatabase::ImportField( "RPGPersID", &pLink->pPers );
	NDatabase::ImportField( "Quantity", &pLink->nQuantity );
}
/*void CRPGWeapon4Pers::Import()
{
	ImportItem(this);
}*/
void CRPGClip4Pers::Import()
{
	ImportItem(this);
	NDatabase::ImportField( "RPGAmmoID", &pAmmo );
}
/*void CRPGGrenade4Pers::Import()
{
	ImportItem(this);
}
void CRPGFirstAid4Pers::Import()
{
	ImportItem(this);
}
void CRPGMeleeWeapon4Pers::Import()
{
	ImportItem(this);
}
void CRPGMineDetector4Pers::Import()
{
	ImportItem(this);
}
void CRPGMine4Pers::Import()
{
	ImportItem(this);
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGItem2Uniform::Import()
{
	NDatabase::ImportField( "ItemID", &pItem );
	NDatabase::ImportField( "UniformID", &pUniform );
	NDatabase::ImportField( "ModelActive1", &pModelActive );
	NDatabase::ImportField( "Model1", &pModelInactive );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NDb;
REGISTER_SAVELOAD_TEMPL_CLASS( 0xE10A1160, CRPGWeapon4Pers, CRPGSomethingForPers )
REGISTER_SAVELOAD_CLASS( 0xE10A1161, CRPGClip4Pers )
REGISTER_SAVELOAD_TEMPL_CLASS( 0xE10A1162, CRPGGrenade4Pers, CRPGSomethingForPers )
REGISTER_SAVELOAD_CLASS( 0xE01B1160, CRPGItem2Uniform )
REGISTER_SAVELOAD_TEMPL_CLASS( 0xA1112152, CRPGFirstAid4Pers, CRPGSomethingForPers )
REGISTER_SAVELOAD_TEMPL_CLASS( 0xA1512130, CRPGMeleeWeapon4Pers, CRPGSomethingForPers )
REGISTER_SAVELOAD_TEMPL_CLASS( 0x11462171, CRPGMineDetector4Pers, CRPGSomethingForPers )
REGISTER_SAVELOAD_TEMPL_CLASS( 0x018c2113, CRPGMine4Pers, CRPGSomethingForPers )
REGISTER_SAVELOAD_TEMPL_CLASS( 0x024c2143, CRPGTool4Pers, CRPGSomethingForPers )
REGISTER_SAVELOAD_TEMPL_CLASS( 0x024c2144, CRPGKey4Pers, CRPGSomethingForPers )
REGISTER_SAVELOAD_TEMPL_CLASS( 0x70122180, CRPGPicklock4Pers, CRPGSomethingForPers )
REGISTER_SAVELOAD_TEMPL_CLASS( 0xa2763110, CRPGEngGrenade4Pers, CRPGSomethingForPers )
