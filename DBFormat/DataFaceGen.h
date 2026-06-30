#ifndef __DATAFACEGEN_H_
#define __DATAFACEGEN_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Transformable-head / face-customisation DB records, reconstructed from the release Game.exe +
// matched PDB (tables 0x73..0x78). Member layout, serialization tags (operator&) and Import() column
// names are taken verbatim from the decompile so they round-trip game.db identically. These records
// were entirely absent from the dev snapshot - see CHead::pTransformableTextures / CComplexHead::pBodyColor.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTexture;
class CTMaterial;
class CTRndModel;
class CHeadTextures;
class CSequence;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Facial expression kind (table 0x7a FaceExpression2Sequences). Note: the release String2Expression
// maps both "Anger" and "Rage" to FE_ANGER (value 2); FE_RAGE (3) is reserved but never imported.
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EFaceExpression
{
	FE_NORMAL = 0,
	FE_SMILE = 1,
	FE_ANGER = 2,
	FE_RAGE = 3,
	FE_WORRY = 4,
	FE_FEAR = 5,
	FE_SAD = 6,
	FE_HAPPY = 7,
	FE_SMIRK = 8,
	FE_DISGUST = 9,
};
EFaceExpression String2Expression( const string &sz );
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadTexture - base record for the transformable face/eye/eyelash texture variants. Not a table of
// its own; the three concrete variants below are the registered tables, all sharing this layout.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeadTexture: public CDBRecord
{
	OBJECT_BASIC_METHODS( CHeadTexture );
public:
	CPtr<CTexture> pTexture;    // +0x10
	string         szName;      // +0x14
	int            nPriority;   // +0x20

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// The three concrete texture tables. Same layout as CHeadTexture (serialization inherited); each
// imports an extra THMID back-reference that wires it into its CHeadTextures owner's vector.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFaceTexture: public CHeadTexture
{
	OBJECT_BASIC_METHODS( CFaceTexture );
public:
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEyeTexture: public CHeadTexture
{
	OBJECT_BASIC_METHODS( CEyeTexture );
public:
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEyelashTexture: public CHeadTexture
{
	OBJECT_BASIC_METHODS( CEyelashTexture );
public:
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadTextures - groups the transformable textures of a head (table "TransformableHeadMaterials").
// Import() is empty: the vectors are filled by the texture records' Import THMID back-reference.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeadTextures: public CDBRecord
{
	OBJECT_BASIC_METHODS( CHeadTextures );
public:
	vector< CPtr<CHeadTexture> > face;           // +0x10
	vector< CPtr<CHeadTexture> > eye;            // +0x1c
	vector< CPtr<CHeadTexture> > eyelash_teeth;  // +0x28

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRace - per-race body material/skin slider (table "Races"). Referenced by CComplexHead::pBodyColor.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRace: public CDBRecord
{
	OBJECT_BASIC_METHODS( CRace );
public:
	CPtr<CTMaterial> pMaterial;        // +0x10
	float            fSliderPos;       // +0x14
	int              nRaceAttributeID; // +0x18

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Face-gen editor head pieces (tables "TransformableHeadHairs" / "TransformableHeadGlasses").
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFaceGenHeadHair: public CDBRecord
{
	OBJECT_BASIC_METHODS( CFaceGenHeadHair );
public:
	CPtr<CTRndModel> pHair;       // +0x10
	float            fSliderPos;  // +0x14

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFaceGenHeadGlasses: public CDBRecord
{
	OBJECT_BASIC_METHODS( CFaceGenHeadGlasses );
public:
	CPtr<CTRndModel> pGlasses;    // +0x10
	float            fSliderPos;  // +0x14

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFaceExpression - maps a facial-expression kind to its animation sequence (table 0x7a).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFaceExpression: public CDBRecord
{
	OBJECT_BASIC_METHODS( CFaceExpression );
public:
	EFaceExpression  eExpression;  // +0x10
	CPtr<CSequence>  pSequence;    // +0x14

	CFaceExpression() : eExpression( FE_NORMAL ) {}
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DATAFACEGEN_H_
