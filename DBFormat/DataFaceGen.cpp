#include "StdAfx.h"
#include "DataFormat.h"
#include "DataFaceGen.h"
//
// Reconstructed transformable-head / face-customisation records. operator& tags and Import column
// names are verbatim from the release decompile (Game.exe + matched PDB). Tables are registered in
// DataFormat.cpp's RegisterDatabaseClasses() alongside the rest of the schema.
//
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadTexture (shared base of the three texture tables)
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadTexture::Import()
{
	NDatabase::ImportField( "TextureID", &pTexture );
	NDatabase::ImportField( "TextureName", &szName );
	NDatabase::ImportField( "Priority", &nPriority );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CHeadTexture::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pTexture );
	f.Add( 3, &szName );
	f.Add( 4, &nPriority );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// The concrete texture tables: base columns + a THMID back-reference that registers the texture into
// its owning CHeadTextures' matching vector (the release wires the relation at Import time).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFaceTexture::Import()
{
	CHeadTexture::Import();
	CPtr<CHeadTextures> pOwner;
	NDatabase::ImportField( "THMID", &pOwner );
	if ( pOwner )
		PushItem( &pOwner->face, (CHeadTexture*)this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEyeTexture::Import()
{
	CHeadTexture::Import();
	CPtr<CHeadTextures> pOwner;
	NDatabase::ImportField( "THMID", &pOwner );
	if ( pOwner )
		PushItem( &pOwner->eye, (CHeadTexture*)this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEyelashTexture::Import()
{
	CHeadTexture::Import();
	CPtr<CHeadTextures> pOwner;
	NDatabase::ImportField( "THMID", &pOwner );
	if ( pOwner )
		PushItem( &pOwner->eyelash_teeth, (CHeadTexture*)this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadTextures (groups a head's transformable textures; vectors populated via the relation above)
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadTextures::Import()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CHeadTextures::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &face );
	f.Add( 3, &eye );
	f.Add( 4, &eyelash_teeth );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRace
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRace::Import()
{
	NDatabase::ImportField( "MaterialID", &pMaterial );
	NDatabase::ImportField( "RaceSliderPos", &fSliderPos );
	NDatabase::ImportField( "RaceAttributeID", &nRaceAttributeID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRace::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pMaterial );
	f.Add( 3, &fSliderPos );
	f.Add( 4, &nRaceAttributeID );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFaceGenHeadHair / CFaceGenHeadGlasses
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFaceGenHeadHair::Import()
{
	NDatabase::ImportField( "HairMeshID", &pHair );
	NDatabase::ImportField( "HairSliderPos", &fSliderPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CFaceGenHeadHair::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pHair );
	f.Add( 3, &fSliderPos );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFaceGenHeadGlasses::Import()
{
	NDatabase::ImportField( "GlassesMeshID", &pGlasses );
	NDatabase::ImportField( "SliderPos", &fSliderPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CFaceGenHeadGlasses::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pGlasses );
	f.Add( 3, &fSliderPos );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFaceExpression - string->enum table verbatim from the release String2Expression decompile.
////////////////////////////////////////////////////////////////////////////////////////////////////
EFaceExpression String2Expression( const string &sz )
{
	static const struct { const char *psz; EFaceExpression e; } table[] =
	{
		{ "Calm",    FE_NORMAL },
		{ "Smile",   FE_SMILE },
		{ "Anger",   FE_ANGER },
		{ "Rage",    FE_ANGER },
		{ "Worry",   FE_WORRY },
		{ "Fear",    FE_FEAR },
		{ "Sad",     FE_SAD },
		{ "Happy",   FE_HAPPY },
		{ "Smirk",   FE_SMIRK },
		{ "Disgust", FE_DISGUST },
	};
	for ( int i = 0; i < sizeof( table ) / sizeof( table[0] ); ++i )
		if ( sz == table[i].psz )
			return table[i].e;
	return FE_NORMAL;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFaceExpression::Import()
{
	string szExpression;
	NDatabase::ImportField( "FaceExpression", &szExpression );
	eExpression = String2Expression( szExpression );
	NDatabase::ImportField( "SequenceID", &pSequence );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CFaceExpression::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &eExpression );
	f.Add( 3, &pSequence );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NDb::GetSequenceByExpression @0x42cf30: linear walk of the FaceExpression2Sequences table;
// first live record with the matching kind wins; null on a miss.
CSequence* GetSequenceByExpression( EFaceExpression eExpression )
{
	CDBTable<CFaceExpression> *pTable = NDatabase::GetTable<CFaceExpression>();
	if ( !pTable )
		return 0;
	CDBIterator<CFaceExpression> i( *pTable );
	while ( i.MoveNext() )
	{
		CDBPtr<CFaceExpression> pRec = i.Get();
		if ( IsValid( pRec ) && pRec->eExpression == eExpression )
			return pRec->pSequence;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
// retail saveload ids (serialization-convergence W1; s2_scratch docs/SERIALIZATION_CONVERGENCE.md)
using namespace NDb;
REGISTER_SAVELOAD_CLASS( 0xA1943140, CFaceTexture )
REGISTER_SAVELOAD_CLASS( 0xA1943141, CHeadTextures )
REGISTER_SAVELOAD_CLASS( 0xA1943142, CEyeTexture )
REGISTER_SAVELOAD_CLASS( 0xA1943143, CEyelashTexture )
REGISTER_SAVELOAD_CLASS( 0xA2243160, CFaceGenHeadHair )
REGISTER_SAVELOAD_CLASS( 0xA2243161, CRace )
REGISTER_SAVELOAD_CLASS( 0xA2443170, CFaceGenHeadGlasses )
REGISTER_SAVELOAD_CLASS( 0xA3053150, CFaceExpression )
