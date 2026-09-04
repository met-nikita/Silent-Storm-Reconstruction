#include "StdAfx.h"
#include "DataFormat.h"
#include "DataRPG.h"
#include "DataAI.h"
#include "DataAnimation.h"
#include "DataSound.h"
#include "DataObject.h"
#include "..\Misc\RandomGen.h"
#include "..\Main\AIPosition.h"
#include "DataPerk.h"
#include "DataInterface.h"
#include "DataAck.h"
#include "DataChest.h"
#include "DataMisc.h"	// complete CUIHint for ImportField<CUIHint>("HintID") in CRPGItem::Import (typeid needs the full type)

namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const char *pszSubTypes[N_SUBTYPES] = 
{
	"",
	"2HWeapon",
	"Pistol",
	"MeleeKnife",
	"SmallGrenade",
	"BigGrenade",
	"ThrowKnife",
	"MedicalItem",
	"Engineering",
	"MineDetector",
	"PistolAmmo",
	"SMGAmmo",
	"RifleAmmo",
	"MGAmmo",
	"HeavyAmmo",
};
const char *pszUniformPlaces[N_ITEM_PLACES] = 
{
	"BeltL1",
	"BeltR1",
	"BeltM1",
	"BeltMediumL1",
	"BeltMediumR1",
	"BeltMediumL2",
	"BeltMediumR2",
	"WaistBeltL1",
	"WaistBeltR1",
};
EItemSubType GetSubTypeByName( const string &szName )
{
	for ( int i = 0; i < N_SUBTYPES; ++i )
		if ( szName == pszSubTypes[i] )
			return (EItemSubType)i;
	return SUBTYPE_NONE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGItem
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGItem::Import()
{
	NDatabase::ImportField( "Weight", &nWeight );
	NDatabase::ImportField( "SizeX",	&sSize.x );
	NDatabase::ImportField( "SizeY",	&sSize.y );
	NDatabase::ImportField( "TooltipID", &pToolTip );
	NDatabase::ImportField( "ModelID", &pModel );
	NDatabase::ImportField( "ModelActive1", &pModelActive );
	NDatabase::ImportField( "Model1", &pModelInactive );
	string szSubType;
	NDatabase::ImportField( "SubType", &szSubType );
	subType = GetSubTypeByName( szSubType );
	NDatabase::ImportField( "SubTypePriority", &nSubTypePriority );

	NDatabase::ImportField( "NameID", &pName );
	NDatabase::ImportField( "DescrID", &pDescription );

	SCameraParams &sCamera = sCameras[CAMERA_NORMAL];
	NDatabase::ImportField( "CameraAnchorX",	&sCamera.vAnchor.x );
	NDatabase::ImportField( "CameraAnchorY",	&sCamera.vAnchor.y );
	NDatabase::ImportField( "CameraAnchorZ",	&sCamera.vAnchor.z );
	NDatabase::ImportField( "CameraYaw",	&sCamera.fYaw );
	NDatabase::ImportField( "CameraPitch",	&sCamera.fPitch );
	NDatabase::ImportField( "CameraRoll",	&sCamera.fRoll );
	NDatabase::ImportField( "CameraDistance",	&sCamera.fDistance );

	SCameraParams &sSlotCamera = sCameras[CAMERA_SLOT];
	NDatabase::ImportField( "SlotCameraAnchorX",	&sSlotCamera.vAnchor.x );
	NDatabase::ImportField( "SlotCameraAnchorY",	&sSlotCamera.vAnchor.y );
	NDatabase::ImportField( "SlotCameraAnchorZ",	&sSlotCamera.vAnchor.z );
	NDatabase::ImportField( "SlotCameraYaw",	&sSlotCamera.fYaw );
	NDatabase::ImportField( "SlotCameraPitch",	&sSlotCamera.fPitch );
	NDatabase::ImportField( "SlotCameraRoll",	&sSlotCamera.fRoll );
	NDatabase::ImportField( "SlotCameraDistance",	&sSlotCamera.fDistance );

	SCameraParams &sReloadCamera = sCameras[CAMERA_RELOADBUTTON];
	NDatabase::ImportField( "AmmoCameraAnchorX",	&sReloadCamera.vAnchor.x );
	NDatabase::ImportField( "AmmoCameraAnchorY",	&sReloadCamera.vAnchor.y );
	NDatabase::ImportField( "AmmoCameraAnchorZ",	&sReloadCamera.vAnchor.z );
	NDatabase::ImportField( "AmmoCameraYaw",	&sReloadCamera.fYaw );
	NDatabase::ImportField( "AmmoCameraPitch",	&sReloadCamera.fPitch );
	NDatabase::ImportField( "AmmoCameraRoll",	&sReloadCamera.fRoll );
	NDatabase::ImportField( "AmmoCameraDistance",	&sReloadCamera.fDistance );

	NDatabase::ImportField( "PlacableID", &pPlaceObj );
	if ( IsValid( pPlaceObj ) )
		pPlaceObj->pRPGItem = this;
	else
	{
		ASSERT(0);
	}

	NDatabase::ImportField( "ArmorID", &pRPGArmor );
	NDatabase::ImportField( "DestructionEffectID", &pDestructionEffect );
	// retail CRPGItem::Import tail (oracle s2_dbimport.h:2727-2729): HintID / PlaceInHand / Cost.
	// pStoreItem is NOT column-fed -- CRPGStoreItem::Import @0x428570 installs itself on it.
	NDatabase::ImportField( "HintID", &pHint );
	NDatabase::ImportField( "PlaceInHand", &bPlaceInHand );
	NDatabase::ImportField( "Cost", &nCost );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTRndModel* CRPGItem::GetItemModel( bool bActive, CRPGUniform *pUniform )
{
	CTRndModel *pResModel = 0;
	//
	CTRndModel *pItemModel = bActive ? pModelActive : pModelInactive;
	if ( pItemModel )
		pResModel = pItemModel;
	//
	CTRndModel *pUniformModel = 0;
	if ( pUniform )
	{
		SUniformItem *pLook = 0;
		for ( int i = 0; i < looks.size(); ++i )
			if ( pUniform == looks[i].pUniform )
			{
				pLook = &looks[i];
				break;
			}
		if ( pLook )
			pUniformModel = bActive ? pLook->pModelActive : pLook->pModelInactive;
	}
	if ( pUniformModel )
		pResModel = pUniformModel;
	//
	return pResModel;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRPGItem::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &nWeight );
	f.Add( 3, &nSubTypePriority );
	f.Add( 4, &subType );
	f.Add( 5, &sSize );
	f.Add( 6, &pToolTip );
	f.Add( 7, &pModel );
	f.Add( 8, &pModelActive );
	f.Add( 9, &pModelActive );
	f.Add( 10, &pModelInactive );
	f.Add( 11, &looks );
	f.Add( 12, &pName );
	f.Add( 13, &pDescription );
	f.Add( 14, &pPlaceObj );
	f.Add( 15, &pSuccessor );
	f.Add( 16, &pRPGArmor );
	f.Add( 17, &pDestructionEffect );
	f.Add( 18, &pHint );          // retail tag 0x12 (pHint) -- the item's UI hint record
	f.Add( 19, &pStoreItem );     // retail tag 0x13 (pStoreItem) -- the store join record
	f.Add( 20, &bPlaceInHand );   // retail tag 20 (bPlaceInHand @+0xc0); tag-tolerant append reads it from the retail game.db
	f.Add( 21, &nCost );          // retail tag 0x15 (nCost) -- the item's store price

	for ( int i = 0; i < CAMERA_MAX_VALUE; ++i )
		f.Add( 24, &sCameras[i], i + 1 );

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGStoreItem
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGStoreItem::Import()
{
	NDatabase::ImportField( "Rating", &nRating );
	NDatabase::ImportField( "ItemID",	&pItem );
	NDatabase::ImportField( "SideID",	&pSide );
	NDatabase::ImportField( "Quantity", &fQuantity );
	// retail CRPGStoreItem::Import @0x428570 tail: install this store row on its item's pStoreItem.
	if ( IsValid( pItem ) )
		pItem->pStoreItem = this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGDmgToArmor
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGDmgToArmor::Import()
{
	NDatabase::ImportField( "Glass", &armors[0] );
	NDatabase::ImportField( "Wood", &armors[1] );
	NDatabase::ImportField( "Forest", &armors[2] );
	NDatabase::ImportField( "Brick", &armors[3] );
	NDatabase::ImportField( "Stone", &armors[4] );
	NDatabase::ImportField( "Concrete", &armors[5] );
	NDatabase::ImportField( "Steel", &armors[6] );
	NDatabase::ImportField( "LightArmor", &armors[7] );
	NDatabase::ImportField( "MediumArmor", &armors[8] );
	NDatabase::ImportField( "HeavyArmor", &armors[9] );
	NDatabase::ImportField( "Leaves", &armors[10] );
	NDatabase::ImportField( "Item", &armors[11] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGMaterial
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGMaterial::Import()
{
	NDatabase::ImportField( "Threshold", &nThreshold );
	NDatabase::ImportField( "Density", &fDensity );
	NDatabase::ImportField( "VP", &nVP );
	NDatabase::ImportField( "DR", &nDR );
	NDatabase::ImportField( "Transparency", &fTransparency );
	NDatabase::ImportField( "UltimateMoment", &fUltimateMoment );
	NDatabase::ImportField( "UltimatePressure", &fUltimatePresure );
	NDatabase::ImportField( "Weight", &fWeight );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGArmor
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGArmor::Import()
{
	NDatabase::ImportField( "SoundStepID", &pSoundStep );
	NDatabase::ImportField( "SoundShotID", &pSoundShot );
	//NDatabase::ImportField( "SoundGrenadeID", &pSoundGrenade );
	NDatabase::ImportField( "ShotEffectID", &pShotEffect );
	NDatabase::ImportField( "AISoundType", &nAISoundType );
	NDatabase::ImportField( "RPGMaterialID", &pMaterial );
	NDatabase::ImportField( "GrenadeSoundType", &nGrenadeSoundType );
	NDatabase::ImportField( "GrenadeExplosionType", &nGrenadeExplostionType);
	NDatabase::ImportField( "ShotMaterial", &pShotMaterial );
	NDatabase::ImportField( "ShotRadius", &fShotRadius );
	// Steam game.db CRPGArmor int column "ItemCanBreakIt" (retail CRPGArmor::Import @0x4285e0 loads this as its
	// 10th ImportField -- a bool right after ShotRadius). It gates CheckItemsBreakGlass @0x347bd0, the HP-BYPASSING
	// SetDestroyStage(+1) trigger that breaks explodable/breakable objects (gas tanks, fuel barrels, glass). Those
	// are metal/high-threshold so their structural-HP damage is ~0 and they can ONLY advance a destroy stage via
	// this flag; ordinary objects (walls/crates) break via the HP path and don't need it. Never importing this
	// column left bBreakableByGrenade permanently false, so explodables never broke and never reached the destroy
	// stage that arms their attached-grenade explosion. (Ricochet/ShotEffect1-3 parity deferred -- orthogonal to
	// this bug and would perturb bullet behaviour.)
	NDatabase::ImportField( "ItemCanBreakIt", &bBreakableByGrenade );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGWeaponType
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGWeaponType::Import()
{
	NDatabase::ImportField( "SkillIndex", &nSkillIndex );
	NDatabase::ImportField( "MovePenalty", &fMovePenalty );
	NDatabase::ImportField( "CrawlBonus", &fCrawlBonus );
	NDatabase::ImportField( "CrouchBonus", &fCrouchBonus );
	NDatabase::ImportField( "TwoHanded", &bTwoHanded );
	NDatabase::ImportField( "AISoundID", &pAISound );
	NDatabase::ImportField( "BurstAISoundID", &pBurstAISound );
	NDatabase::ImportField( "ID", &nWeaponTypeID );
	NDatabase::ImportField( "NameID", &pName );

	string szStoreType;
	NDatabase::ImportField( "StoreType", &szStoreType );

	eStoreWeaponType = SWT_OTHER;
	if ( szStoreType == "Pistol" )
		eStoreWeaponType = SWT_PISTOL;
	else if ( szStoreType == "Rifle" )
		eStoreWeaponType = SWT_RIFLE;
	else if ( szStoreType == "SubMachineGun" )
		eStoreWeaponType = SWT_SUB_MACHINE_GUN;
	else if ( szStoreType == "HeavyWeapon" )
		eStoreWeaponType = SWT_HEAVY_WEAPON;
	else if ( szStoreType == "ColdSteel" )
		eStoreWeaponType = SWT_COLD_STEEL;
	else if ( szStoreType == "Grenade" )
		eStoreWeaponType = SWT_GRENADE;
	else if ( szStoreType == "PKWeapon" )
		eStoreWeaponType = SWT_PK_WEAPON;
	else if ( szStoreType == "Other" )
		eStoreWeaponType = SWT_OTHER;
	else
		ASSERT( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGAmmo
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGAmmo::Import()
{
	NDatabase::ImportField( "BulletType", &nBulletType );
	NDatabase::ImportField( "RangeMod", &fRangeMod );
	NDatabase::ImportField( "DmgMin", &nDmgMin );
	NDatabase::ImportField( "DmgMax", &nDmgMax );
	NDatabase::ImportField( "UnitWeight", &fUnitWeight );
	NDatabase::ImportField( "NameID", &pName );
	NDatabase::ImportField( "GrenadeID", &pExplosiveBullet );
	NDatabase::ImportField( "AmmoGroup", &nAmmoGroup );
	int nColor;
	NDatabase::ImportField( "AmmoColor", &nColor );
	color = ( EAmmoColor ) nColor;
	NDatabase::ImportField( "UnconsciousProbability", &nUnconsciousProbability );
	NDatabase::ImportField( "Calibr", &fCalibr );   // retail Import @0x428900
	NDatabase::ImportField( "Weight", &fWeight );   // retail Import @0x428900
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRPGAmmo::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &nBulletType );
	f.Add( 3, &fRangeMod );
	f.Add( 4, &nDmgMin );
	f.Add( 5, &nDmgMax );
	f.Add( 6, &fUnitWeight );
	f.Add( 7, &pName );
	f.Add( 8, &pExplosiveBullet );
	f.Add( 9, &nAmmoGroup );
	f.Add( 10, &color );
	f.Add( 11, &nUnconsciousProbability );
	f.Add( 12, &fCalibr );   // retail operator& @0x429ad0 tag 12
	f.Add( 13, &fWeight );   // retail operator& @0x429ad0 tag 13
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAnimWeaponType
////////////////////////////////////////////////////////////////////////////////////////////////////
EWeaponType GetAnimationType( const string &szAWT )
{
	if ( szAWT == "Default" )
		return WT_DEFAULT;
	else if ( szAWT == "Pistol" )
		return WT_PISTOL;
	else if ( szAWT == "Rifle" )
		return WT_RIFLE;
	else if ( szAWT == "SubMachineGun" )
		return WT_SUB_MACHINE_GUN;
	else if ( szAWT == "Knife" )
		return WT_KNIFE;
	else if ( szAWT == "Katana" )
		return WT_KATANA;
	else if ( szAWT == "Machete" )
		return WT_MACHETE;
	else if ( szAWT == "MachineGun" )
		return WT_MACHINE_GUN;
	else if ( szAWT == "RLauncher" )
		return WT_RLAUNCHER;
	else if ( szAWT == "Plazmagun" )
		return WT_PLAZMAGUN;
	else if ( szAWT == "PK_Plazmagun" )
		return WT_PK_PLAZMAGUN;
	else
		return WT_DEFAULT;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimWeaponType::Import()
{
	NDatabase::ImportField( "CrawlX", &crawl.x );
	NDatabase::ImportField( "CrawlY", &crawl.y );
	NDatabase::ImportField( "CrawlZ", &crawl.z );
	NDatabase::ImportField( "CrouchX", &crouch.x );
	NDatabase::ImportField( "CrouchY", &crouch.y );
	NDatabase::ImportField( "CrouchZ", &crouch.z );
	NDatabase::ImportField( "StandX", &stand.x );
	NDatabase::ImportField( "StandY", &stand.y );
	NDatabase::ImportField( "StandZ", &stand.z );
	NDatabase::ImportField( "MinDistance", &fMinDistance );
	string szAWT;
	NDatabase::ImportField( "AnimationWeaponType", &szAWT );
	type = GetAnimationType( szAWT );
	NDatabase::ImportField( "AimedStrafe", &bAimedStrafe );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGWeapon
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRPGWeapon::operator&( CStructureSaver &f ) 
{ 
	f.Add(1,(CDBRecord*)this); 
	f.Add(2,&nInitialVelocity); 
	f.Add(3,&nReloadAP); 
	f.Add(4,&nWeight); 
	f.Add(5,&nShotAP); 
	f.Add(6,&nTargetingAP); 
	f.Add(7,&nRecoil); 
	f.Add(8,&nRoF); 
	f.Add(9,&nDamageMod); 
	f.Add(10,&pWeaponType);
	// retail @0x429bd0 skips tag 11 (goes 10 -> 0xc); pAmmo stays a DB-Import-fed member, off the wire (W5)
	f.Add(12,&pItem);
	f.Add(13,&pSound); 
	f.Add(14,&pSoundBurst); 
	f.Add(15,&pSoundReload); 
	f.Add(16,&pSoundStartBurst); 
	f.Add(17,&pSoundFinishBurst); 
	f.Add(18,&pSoundCycleBurst); 
	f.Add(19,&bScope); 
	f.Add(20,&szAnimName); 
	f.Add(21,&nQuality); 
	f.Add(22,&pShotEffect); 
	f.Add(23,&pAnimWeaponType); 
	f.Add(24,&fTrailSpeed); 
	f.Add(25,&pTrailEffect); 
	f.Add(26,&pTrailParticle); 
	f.Add(27,&bBazookaLogic); 
	f.Add(28,&pInnerClip); 
	f.Add(29,&nInnerClipAmmoQuantity);
	f.Add(30,&nPanzerkleinType);
	f.Add(31,&eWeaponType);
	f.Add(32,&nMinRange);
	f.Add(33,&nMaxRange);
	for ( int i = 0; i < SM_MAXVALUE; ++i )
		f.Add( 34 + i, &shootModes[i] );
	// retail v1.1 tail tags 50..54 (disasm @0x429bd0), v1.2 appends ShotsInOne as tag 55
	// (disasm-confirmed in the Steam v1.2 exe: push 0x37 with lea [rec+0xB0])
	f.Add( 50, &nBPM );
	f.Add( 51, &nDamageModMax );
	f.Add( 52, &fSilencer );
	f.Add( 53, &nAIRating );
	f.Add( 54, &nShotEffectType );
	f.Add( 55, &nShotsInOne );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGWeapon::Import()
{
	NDatabase::ImportField( "InitialVelocity", &nInitialVelocity );
	NDatabase::ImportField( "ReloadAP", &nReloadAP );
	NDatabase::ImportField( "ShotAP", &nShotAP );
	NDatabase::ImportField( "Targeting", &nTargetingAP );
	NDatabase::ImportField( "Recoil", &nRecoil );
	NDatabase::ImportField( "RoF", &nRoF );
	NDatabase::ImportField( "DamageMod", &nDamageMod );
	NDatabase::ImportField( "WeaponTypeID", &pWeaponType );
	NDatabase::ImportField( "AmmoTypeID", &pAmmo );
	NDatabase::ImportField( "ItemID", &pItem );
	NDatabase::ImportField( "MinRange", &nMinRange );
	NDatabase::ImportField( "MaxRange", &nMaxRange );
	if ( IsValid( pItem ) )
		pItem->pSuccessor = this;
	else
		ASSERT(0);
	NDatabase::ImportField( "SoundID", &pSound );
	NDatabase::ImportField( "BurstSoundID", &pSoundBurst );
	NDatabase::ImportField( "ReloadSoundID", &pSoundReload );
	//NDatabase::ImportField( "BurstStartSoundID", &pSoundStartBurst );
	NDatabase::ImportField( "BurstEndSoundID", &pSoundFinishBurst );
	NDatabase::ImportField( "LongBurstSoundID", &pSoundCycleBurst );	
	//
	NDatabase::ImportField( "Scope", &bScope );
	NDatabase::ImportField( "AnimationName", &szAnimName );
	NDatabase::ImportField( "Quality", &nQuality );
	NDatabase::ImportField( "ShotEffectID", &pShotEffect );
	NDatabase::ImportField( "AnimWeaponID", &pAnimWeaponType );

	NDatabase::ImportField( "TrailSpeed", &fTrailSpeed );
	NDatabase::ImportField( "TrailEffectID", &pTrailEffect );
	NDatabase::ImportField( "TrailParticleID", &pTrailParticle );

	NDatabase::ImportField( "SnapShot", &shootModes[SM_Snap] );
	NDatabase::ImportField( "AimedShot", &shootModes[SM_Aimed] );
	NDatabase::ImportField( "CarefulShot", &shootModes[SM_Careful] );
	NDatabase::ImportField( "ShortBurst", &shootModes[SM_ShortBurst] );
	NDatabase::ImportField( "LongBurst", &shootModes[SM_LongBurst] );
	NDatabase::ImportField( "SnipeShot", &shootModes[SM_Snipe] );
	// CRAP - must somehow else detemine it's a bazooka?
	bBazookaLogic = 
		//( pAmmo->pExplosiveBullet != 0 ); 
		//( pAnimWeaponType->type == WT_RLAUNCHER );
		( pWeaponType->nWeaponTypeID == 5 );
	//
	NDatabase::ImportField( "InnerClip", &pInnerClip );
	NDatabase::ImportField( "InnerClipAmmoQuantity", &nInnerClipAmmoQuantity );
	NDatabase::ImportField( "PanzerkleinWeaponType", &nPanzerkleinType );

	string szAWT;
	NDatabase::ImportField( "WeaponRPGLogics", &szAWT );
	eWeaponType = GetAnimationType( szAWT );

	// v1.1 retail tail (retail Import @0x4289b0 reads these right after WeaponRPGLogics);
	// nBPM is the inter-bullet timing divisor -- missing column keeps the 0 default
	// (GetNextBulletTime then leaves the timestamp unchanged, like retail's invalid-record path)
	NDatabase::ImportField( "BPM", &nBPM );
	NDatabase::ImportField( "DamageModMax", &nDamageModMax );
	NDatabase::ImportField( "Silencer", &fSilencer );
	NDatabase::ImportField( "AIRating", &nAIRating );
	NDatabase::ImportField( "ShotEffectType", &nShotEffectType );

	// v1.2: NEW ShotsInOne column (bullets per trigger pull, retail rec+0xb0); guarded so a
	// mod db missing the column keeps single-bullet behaviour
	if ( !NDatabase::ImportField( "ShotsInOne", &nShotsInOne ) )
		nShotsInOne = 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CModel* CRPGWeapon::GetModel()
{
	if ( !IsValid( pRollModel ) )
	{
		SRand rand;
		pRollModel = pItem->pModel->CreateModel( &rand );
	}
	return pRollModel;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGUniform
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGUniform::Import()
{
	NDatabase::ImportField( "CapModelID", &pCapModel );
	NDatabase::ImportField( "BackpackModelID", &pBackpackModel );
	fixedModels.resize( N_ITEM_PLACES );
	for ( int i = 0; i < N_ITEM_PLACES; ++i )
	{
		string szTmp, szSubType;
		szTmp = "ST";
		szTmp += pszUniformPlaces[i];
		NDatabase::ImportField( szTmp.c_str(), &szSubType );
		subTypes[i] = GetSubTypeByName( szSubType );
		szTmp = "Model";
		szTmp += pszUniformPlaces[i];
		NDatabase::ImportField( szTmp.c_str(), &fixedModels[i] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRPGUniform::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 3, &pCapModel );
	f.Add( 4, &pBackpackModel );
	f.Add( 5, &subTypes );
	f.Add( 6, &fixedModels );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGGrenade
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGGrenade::Import()
{
	NDatabase::ImportField( "ItemID", &pItem );
	if ( IsValid( pItem ) )
		pItem->pSuccessor = this;
	else
		ASSERT(0);
	NDatabase::ImportField( "WaveNumber", &nWaveNumber );
	NDatabase::ImportField( "WaveDmgMin", &fWaveDmgMin );
	NDatabase::ImportField( "WaveDmgMax", &fWaveDmgMax );
	NDatabase::ImportField( "CriticalProbability", &nCriticalProbability );
	NDatabase::ImportField( "CriticalDifficulty", &nCriticalDifficulty );
	NDatabase::ImportField( "StructureDamageCoeff", &fStructureDamageCoeff );
	NDatabase::ImportField( "FragmentNumber", &nFragmentNumber );
	NDatabase::ImportField( "FragmentAPA", &nFragmentAPA );
	NDatabase::ImportField( "FragmentDmgMin", &nFragmentDmgMin );
	NDatabase::ImportField( "FragmentDmgMax", &nFragmentDmgMax );
	NDatabase::ImportField( "MaxDelay", &nMaxDelay );
	NDatabase::ImportField( "Quality", &nQuality );
	NDatabase::ImportField( "WeaponTypeID", &pWeaponType );
	NDatabase::ImportField( "WaveRadius", &fWaveRadius );
	NDatabase::ImportField( "PanzerkleinWeapon", &nPanzerkleinWeapon );
	NDatabase::ImportField( "Sound1ID", &pSound.p[0] );
	NDatabase::ImportField( "Sound2ID", &pSound.p[1] );
	NDatabase::ImportField( "Sound3ID", &pSound.p[2] );
	NDatabase::ImportField( "Sound4ID", &pSound.p[3] );
	NDatabase::ImportField( "Effect1ID", &pEffect.p[0] );
	NDatabase::ImportField( "Effect2ID", &pEffect.p[1] );
	NDatabase::ImportField( "Effect3ID", &pEffect.p[2] );
	NDatabase::ImportField( "Effect4ID", &pEffect.p[3] );
	NDatabase::ImportField( "DecalRadius", &fDecalRadius );
	NDatabase::ImportField( "FragmentRange", &fFragmentRange );   // retail column @0x8d38d8
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGEngGrenade
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGEngGrenade::Import()
{
	NDatabase::ImportField( "ItemID", &pItem );
	if ( IsValid( pItem ) )
		pItem->pSuccessor = this;
	else
		ASSERT(0);
	NDatabase::ImportField( "Sound1ID", &pSound.p[0] );
	NDatabase::ImportField( "Sound2ID", &pSound.p[1] );
	NDatabase::ImportField( "Sound3ID", &pSound.p[2] );
	NDatabase::ImportField( "Sound4ID", &pSound.p[3] );
	NDatabase::ImportField( "Effect1ID", &pEffect.p[0] );
	NDatabase::ImportField( "Effect2ID", &pEffect.p[1] );
	NDatabase::ImportField( "Effect3ID", &pEffect.p[2] );
	NDatabase::ImportField( "Effect4ID", &pEffect.p[3] );
	NDatabase::ImportField( "SkillReq", &nSkillReq );
	NDatabase::ImportField( "StartNWave", &nStartNWave );
	NDatabase::ImportField( "DeltaWave", &nDeltaWave );
	NDatabase::ImportField( "WaveRadius", &fWaveRadius );
	NDatabase::ImportField( "DeltaRadius", &fDeltaRadius );
	NDatabase::ImportField( "DamageModifier", &fDamageModifier );
	NDatabase::ImportField( "FragDmgModifier", &fFragDmgModifier );
	NDatabase::ImportField( "APAModifier", &nAPAModifier );
	NDatabase::ImportField( "WeaponTypeID", &pWeaponType );
	// release sets these to fixed defaults (not DB-driven)
	nCriticalProbability = 30;
	nCriticalDifficulty = 30;
	fStructureDamageCoeff = 1.0f;
	nMaxDelay = 0;
	nQuality = 0;
	nPanzerkleinWeapon = 0;
	fDecalRadius = 3.0f;
	fFragmentRange = 5.0f;
	NDatabase::ImportField( "PerkID", &nRequiredPerkID );
	NDatabase::ImportField( "StartWaveDamage", &fStartWaveDamage );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGFirstAid
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGFirstAid::Import()
{
	NDatabase::ImportField( "ItemID", &pItem );
	if ( IsValid( pItem ) )
		pItem->pSuccessor = this;
	else
		ASSERT(0);
	NDatabase::ImportField( "Quantity", &nQuantity );
	string szEffect;
	NDatabase::ImportField( "Effect", &szEffect );
	if ( szEffect == "Normal" )
		effect = FAE_NORMAL;
	else if ( szEffect == "CriticalOnly" )
		effect = FAE_CRITICAL_ONLY;
	else if ( szEffect == "TempRemovePenalties" )
		effect = FAE_TEMP_REMOVE_PENALTIES;
	else if ( szEffect == "BoostVP" )
		effect = FAE_BOOST_VP;
	else if ( szEffect == "TempStopBleeding" )
		effect = FAE_TEMP_STOP_BLEEDING;
	else if ( szEffect == "RemoveBleeding" )
		effect = FAE_REMOVE_BLEEDING;
	NDatabase::ImportField( "RequiredSkill", &nRequiedSkill );
	NDatabase::ImportField( "SkillModifier", &nSkillModifier );
	NDatabase::ImportField( "Duration", &nDuration );
	NDatabase::ImportField( "Power", &fPower );
	NDatabase::ImportField( "TotalHealVP", &nTotalHealVP );
	NDatabase::ImportField( "APToUse", &nAPToUse );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGMeleeWeapon
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGMeleeWeapon::Import()
{
	NDatabase::ImportField( "ItemID", &pItem );
	if ( IsValid( pItem ) )
		pItem->pSuccessor = this;
//	else
//		ASSERT(0);
	NDatabase::ImportField( "DmgMin", &nDmgMin );
	NDatabase::ImportField( "DmgMax", &nDmgMax );
	NDatabase::ImportField( "ToHitBonus", &nToHitBonus );
	NDatabase::ImportField( "CriticalBonus", &nCriticalBonus );
	NDatabase::ImportField( "MinAP", &nMinAP );
	NDatabase::ImportField( "MaxAP", &nMaxAP );
	NDatabase::ImportField( "WeaponTypeID", &pWeaponType );
	if ( !IsValid( pWeaponType ) )
		ASSERT(0);
	NDatabase::ImportField( "AnimWeaponID", &pAnimWeaponType );
	NDatabase::ImportField( "Throwing", &bThrowing );
	NDatabase::ImportField( "UnconsciousProbability", &nUnconsciousProbability );
	NDatabase::ImportField( "PanzerkleinWeaponType", &nPanzerkleinType );

	string szAWT;
	NDatabase::ImportField( "WeaponRPGLogics", &szAWT );
	eWeaponType = GetAnimationType( szAWT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGMineDetector
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGMineDetector::Import()
{
	NDatabase::ImportField( "ItemID", &pItem );
	if ( IsValid( pItem ) )
		pItem->pSuccessor = this;
	else
		ASSERT(0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGMine
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGMine::Import()
{
	NDatabase::ImportField( "ItemID", &pItem );
	if ( IsValid( pItem ) )
		pItem->pSuccessor = this;
	else
		ASSERT(0);
	NDatabase::ImportField( "APToSet", &nAPToSet );
	NDatabase::ImportField( "Explosion", &pExplosion );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGTool
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGTool::Import()
{
	NDatabase::ImportField( "ItemID", &pItem );
	if ( IsValid( pItem ) )
		pItem->pSuccessor = this;
	else
		ASSERT(0);

	NDatabase::ImportField( "Charges", &nCharges );
	NDatabase::ImportField( "NeededEngSkill", &nNeededEngSkill );
	NDatabase::ImportField( "NeededPerkID", &pNeededPerk );
	NDatabase::ImportField( "CanUseForMineCleaning", &bCanUseForMineCleaning );
	NDatabase::ImportField( "SkillModifForMineCleaning", &nSkillModifForMineCleaning );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGKey
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGKey::Import()
{
	NDatabase::ImportField( "ItemID", &pItem );
	if ( IsValid( pItem ) )
		pItem->pSuccessor = this;
	else
		ASSERT(0);
	NDatabase::ImportField( "KeyID", &nKeyID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGClass
////////////////////////////////////////////////////////////////////////////////////////////////////
static void ImportSkills( int skills[SKILL_TYPE_NUMBERS] )
{
	NDatabase::ImportField( "Melee",	&skills[ST_MELEE] );
	NDatabase::ImportField( "Shooting",	&skills[ST_SHOOTING] );
	NDatabase::ImportField( "Throwing", &skills[ST_THROWING] );
	NDatabase::ImportField( "Burst", &skills[ST_BURST] );
	NDatabase::ImportField( "Snipe", &skills[ST_SNIPE] );

	NDatabase::ImportField( "Stealth", &skills[ST_STEALTH] );
	NDatabase::ImportField( "Spot", &skills[ST_SPOT] );
	NDatabase::ImportField( "Medicine", &skills[ST_MEDICINE] );
	NDatabase::ImportField( "Engineering",&skills[ST_ENGINEERING] );
	
	NDatabase::ImportField( "VP", &skills[ST_VP] );
	NDatabase::ImportField( "AP", &skills[ST_AP] );
	NDatabase::ImportField( "IC", &skills[ST_IC] );
	NDatabase::ImportField( "Interrupt", &skills[ST_INTERRUPT] );
	NDatabase::ImportField( "Lvl", &skills[ST_LEVEL] );

	NDatabase::ImportField( "Str", &skills[ST_STR] );
	NDatabase::ImportField( "Dex", &skills[ST_DEX] );
	NDatabase::ImportField( "Int", &skills[ST_INT] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGClass::Import()
{
	ImportSkills( skills.skills );
	NDatabase::ImportField( "PerkTreeID", &nPerkTreeID );
	////
	NDatabase::ImportField( "ToolTip", &pToolTip );
	NDatabase::ImportField( "Icon", &pIcon );
	NDatabase::ImportField( "IconDisabled", &pIconDisabled );
	NDatabase::ImportField( "PerksPanel", &pPerksPanel );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGBaseValue
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGBaseValue::Import()
{
	ImportSkills( skills.skills );
	// Exclusive!!!
	NDatabase::ImportField( "XP", &nBaseXP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRPGBaseValue::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );

	for ( int i = ST_MELEE; i < SKILL_TYPE_NUMBERS; ++i )
		f.Add( 2 + i, &skills[i] );

	// Exclusive!!!
	f.Add( 30, &nBaseXP );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGPers::Import()
{
	NDatabase::ImportField( "ID", &nRPGPersID );
	NDatabase::ImportField( "UserName", &szUserName );
	////
	NDatabase::ImportField( "FaceID", &pHead );
	NDatabase::ImportField( "ModelID", &pModel );
	NDatabase::ImportField( "HitSoundID", &pSoundHit );
	NDatabase::ImportField( "DisplayName", &pName );
	NDatabase::ImportField( "DeathSoundID", &pSoundDeath );
	////
	NDatabase::ImportField( "SideID", &pSide );
	NDatabase::ImportField( "ClassID", &pClass );
	NDatabase::ImportField( "WeaponID", &pWeapon );
	NDatabase::ImportField( "UniformID", &pUniform );
	NDatabase::ImportField( "BaseValueID", &pBaseValue );
	NDatabase::ImportField( "NationalityID", &pNationality );
	////
	NDatabase::ImportField( "FaceGenCameraAnchorX",	&sFaceGenCamera.vAnchor.x );
	NDatabase::ImportField( "FaceGenCameraAnchorY",	&sFaceGenCamera.vAnchor.y );
	NDatabase::ImportField( "FaceGenCameraAnchorZ",	&sFaceGenCamera.vAnchor.z );
	NDatabase::ImportField( "FaceGenCameraYaw",	&sFaceGenCamera.fYaw );
	NDatabase::ImportField( "FaceGenCameraPitch",	&sFaceGenCamera.fPitch );
	NDatabase::ImportField( "FaceGenCameraDistance",	&sFaceGenCamera.fDistance );
	////
	NDatabase::ImportField( "CameraAnchorX",	&sPortraitCamera.vAnchor.x );
	NDatabase::ImportField( "CameraAnchorY",	&sPortraitCamera.vAnchor.y );
	NDatabase::ImportField( "CameraAnchorZ",	&sPortraitCamera.vAnchor.z );
	NDatabase::ImportField( "CameraYaw",	&sPortraitCamera.fYaw );
	NDatabase::ImportField( "CameraPitch",	&sPortraitCamera.fPitch );
	NDatabase::ImportField( "CameraDistance",	&sPortraitCamera.fDistance );
	////
	NDatabase::ImportField( "WearingPanzerkleinID", &pDefaultWearsPanzerklein );
	// retail @0x429540 (+0x38): non-null = this pers IS a PK suit. IsEmptyPK @0x34eca0 reads it;
	// without this import every PK spawn at CWorld::AddUnit took the manned-unit path (null-player crash).
	NDatabase::ImportField( "IsPanzerkleinID", &pPanzerklein );
	////
	NDatabase::ImportField( "IsFemale", &bIsFemale );
	NDatabase::ImportField( "Voice", &nVoice );
	// retail CRPGPers::Import @0x429540 -- recruit-menu / biography columns (between Voice and CanBeListed):
	NDatabase::ImportField( "PhotoID", &pPhoto );					// biography-panel portrait photo
	NDatabase::ImportField( "BiographyID", &pBiography );			// biography prose CString
	NDatabase::ImportField( "CharacteristicsID", &pCharacteristics );	// bio "characteristics" CString
	NDatabase::ImportField( "CanHired", &bCanHired );				// THE recruit-roster filter (AddTeamMngPerses @0x29adb0)
	NDatabase::ImportField( "CanBeListed", &bCanBeListed );   // release CharGen model-override roll filter
	NDatabase::ImportField( "WeaponInHand", &pHandWeapon );			// retail CRPGPers::Import @0x429540 -- in-hand loot chest
	NDatabase::ImportField( "WeaponInBackpack", &pBackpackWeapon );	// retail CRPGPers::Import @0x429540 -- backpack loot chest
	NDatabase::ImportField( "LongNameID", &pLongName );				// retail CRPGPers::Import @0x429540 -- long display name (between WeaponInBackpack and AckUnit)
	NDatabase::ImportField( "AckUnit", &pAcksHolder );				// retail CRPGPers::Import @0x429540 -- voice-holder pers whose Acks rows this persona barks with
	NDatabase::ImportField( "LongBurstSnd", &pLongBurstSnd );		// retail CRPGPers::Import @0x429540 -- long-burst voice sound (tail column)
	////
	NDatabase::ImportRelation( this, &scripts );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGClip::Import()
{
	NDatabase::ImportField( "AmmoGroup", &nAmmoGroup );
	NDatabase::ImportField( "Quantity",	&nQuantity );
	NDatabase::ImportField( "ItemID",	&pItem );
	if ( IsValid( pItem ) )
		pItem->pSuccessor = this;
	else
		ASSERT(0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScript::Import()
{
	NDatabase::ImportField( "CodeText",	&strCode );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGCritical::Import()
{
	string szHL, szType;
	NDatabase::ImportField( "HitLocation",	&szHL );
	NDatabase::ImportField( "Type",	&szType );
	if ( "Head" == szHL )
		hl = CL_HEAD;
	else if ( "Torso" == szHL )
		hl = CL_TORSO;
	else if ( "Arms" == szHL )
		hl = CL_ARMS;
	else if ( "Legs" == szHL )
		hl = CL_LEGS;
	else if ( "Any" == szHL )
		hl = CL_ANY;
	else
		ASSERT(0);
	//
	if ( "Death" == szType )
		type = C_DEATH;
	else if ( "AP reduction" == szType )
		type = C_AP_REDUCTION;
	else if ( "Blind" == szType )
		type = C_BLIND;
	else if ( "Weapon skill reduction" == szType )
		type = C_WEAPONSKILL_REDUCTION;
	else if ( "Motionless" == szType )
		type = C_MOTIONLESS;
	else if ( "Encumbrance" == szType )
		type = C_ENCUMBRANCE;
	else if ( "Accidental shot" == szType )
		type = C_ACCIDENTAL_SHOT;
	else if ( "Stun" == szType )
		type = C_STUN;
	else if ( "Lost weapon" == szType )
		type = C_LOST_WEAPON;
	else if ( "Idle hand" == szType )
		type = C_IDLE_HAND;
	else if ( "Damage weapon" == szType )
		type = C_DAMAGE_WEAPON;
	else if ( "Patient" == szType )
		type = C_PATIENT;
	else if ( "Deaf" == szType )
		type = C_DEAF;
	else if ( "Bleeding" == szType )
		type = C_BLEEDING;
	else
		ASSERT(0);
	//
	NDatabase::ImportField( "QueueIndex", &nWeight );
	NDatabase::ImportField( "Range", &nRange );
	NDatabase::ImportField( "MinDuration", &nMinDuration );
	NDatabase::ImportField( "MaxDuration", &nMaxDuration );
	if ( nMinDuration >= 0 && nMinDuration > nMaxDuration )
	{
		ASSERT(0);
		nMaxDuration = nMinDuration;
	}
	NDatabase::ImportField( "Value", &fValue );
	NDatabase::ImportField( "UserName", &szName );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGToHit::Import()
{
	NDatabase::ImportField( "MaxShotsRepeat",	&constants.nMaxShotsRepeat );
	NDatabase::ImportField( "MaxMoveBonus",	&constants.nMaxMoveBonus );
	NDatabase::ImportField( "FirstRoundCoeff",	&constants.fFirstRoundCoeff );
	NDatabase::ImportField( "MaxBurstStabilize",	&constants.nMaxBurstStabilize );
	NDatabase::ImportField( "SnipingCoeff",	&constants.nSnipingCoeff );
	NDatabase::ImportField( "Area&CoverCoeff",	&constants.nAreaCoverCoeff );
	NDatabase::ImportField( "SMaxMove",	&constants.nSMaxMove );
	NDatabase::ImportField( "GreandeBaseCoeff", &constants.fGrenadeBaseCoeff );
	NDatabase::ImportField( "GrenadeSTRCoeff", &constants.fGrenadeSTRCoeff );
	NDatabase::ImportField( "Gravity", &constants.fGravity );
	NDatabase::ImportField( "GrenadeBaseWeight", &constants.fGrenadeBaseWeight );
	NDatabase::ImportField( "MaxWeightRelation", &constants.fMaxWeightRelation );
	NDatabase::ImportField( "ICModifier", &constants.fICModifier );
	NDatabase::ImportField( "StrikeAddition", &constants.nStrikeAddition );
	NDatabase::ImportField( "MaxAdditionalStrikes", &constants.nMaxAdditionalStrikes );
	NDatabase::ImportField( "MaxAPonStrike", &constants.nMaxAPonStrike );
	NDatabase::ImportField( "BackSkillMultiplyer", &constants.fBackSkillMult );
	NDatabase::ImportField( "StanceMultiplyer", &constants.fStanceMult );
	NDatabase::ImportField( "BaseMeleeCritChance", &constants.nBaseMeleeCritChance );
	NDatabase::ImportField( "BaseDamage", &constants.nBaseDamage );
	NDatabase::ImportField( "StrengthNormalize", &constants.nStrengthNormalize );
	NDatabase::ImportField( "BackCriticalMult", &constants.fBackCriticalMult );
	NDatabase::ImportField( "AttackerMultiplyer", &constants.fAttackerMult );
	NDatabase::ImportField( "DefenderMultiplyer", &constants.fDefenderMult );
	NDatabase::ImportField( "MeleeToHitScaling", &constants.nMeleeToHitScaling );
	NDatabase::ImportField( "BaseMeleeToHit", &constants.nBaseMeleeToHit );
	NDatabase::ImportField( "GrenadeWeightScaling", &constants.nGrenadeWeightScaling );
	NDatabase::ImportField( "GrenadeWeightScalingBase", &constants.nGrenadeWeightScalingBase );
	NDatabase::ImportField( "MaxGrenadeWeightDifference", &constants.nMaxGrenadeWeightDifference );
	// retail Import @0x826f10 tail: the weapon-adaptation constants (column names from the retail .rdata)
	NDatabase::ImportField( "MaxWeaponAdaptation", &constants.nMaxWeaponAdaptation );
	NDatabase::ImportField( "WeaponAdaptationMultiplyer", &constants.fWeaponAdaptationMult );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGAISoundConstants::Import()
{
	NDatabase::ImportField( "PrecisePositionRadius", &constants.nPrecisePositionRadius );
	NDatabase::ImportField( "ExitRadius", &constants.nExitRadius );
	NDatabase::ImportField( "BaseProbability", &constants.nBaseProbability );
	NDatabase::ImportField( "DistanceCoeff", &constants.nDistanceCoeff );
	NDatabase::ImportField( "LoudSound", &constants.nLoudSound );
	NDatabase::ImportField( "HideCoeff", &constants.fHideCoeff );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGInterruptsConstants::Import()
{
	NDatabase::ImportField( "BackInterruptsBase",	&constants.nBackInterruptsBase );
	NDatabase::ImportField( "InterruptsBase",	&constants.nInterruptsBase );
	NDatabase::ImportField( "MissedShotInterruptsBase",	&constants.nMissedShotInterruptsBase );
	NDatabase::ImportField( "APInterruptReduction",	&constants.fAPInterruptReduction );
	NDatabase::ImportField( "MinInterruptAP",	&constants.nMinInterruptAP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSound* GetSound( CTSound *p )
{
	static SRand rnd;
	if ( !p )
		return 0;
	CSoundVariant *pV = p->GetSound( &rnd );
	if ( !pV )
		return 0;
	return pV->pSound;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAnimWeaponType* GetAnimWeaponType( int nAnimFlags )
{
	EWeaponType type = WT_DEFAULT;
	if ( nAnimFlags & CAnimation::WEAPON_PISTOL )
		type = WT_PISTOL;
	else if ( nAnimFlags & CAnimation::WEAPON_RIFLE )
		type = WT_RIFLE;
	else if ( nAnimFlags & CAnimation::WEAPON_SUB_MACHINE_GUN )
		type = WT_SUB_MACHINE_GUN;
	else if ( nAnimFlags & CAnimation::WEAPON_KNIFE )
		type = WT_KNIFE;
	else if ( nAnimFlags & CAnimation::WEAPON_KATANA )
		type = WT_KATANA;
	else if ( nAnimFlags & CAnimation::WEAPON_MACHINE_GUN )
		type = WT_MACHINE_GUN;
	else if ( nAnimFlags & CAnimation::WEAPON_RLAUNCHER )
		type = WT_RLAUNCHER;
	else if ( nAnimFlags & CAnimation::WEAPON_PLAZMAGUN )
		type = WT_PLAZMAGUN;
	else if ( nAnimFlags & CAnimation::PK_WEAPON_PLAZMAGUN )
		type = WT_PK_PLAZMAGUN;

	CDBTable<CAnimWeaponType> *pDTable = NDatabase::GetTable<CAnimWeaponType>();
	CDBIterator<CAnimWeaponType> it(*pDTable);
	while ( it.MoveNext() )
	{
		CAnimWeaponType *pT = it.Get();
		if ( pT->type == type )
			return pT;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x427160 jump tables; unknown types return 0 (retail has no assert path)
int WeaponTypeToAnimFlags( EWeaponType type, bool bActive, bool bPK )
{
	if ( !bActive )
		return CAnimation::WEAPON_NONE;
	if ( !bPK )
	{
		switch ( type )
		{
			case NDb::WT_DEFAULT:
				return CAnimation::WEAPON_ITEM;
			case NDb::WT_PISTOL:
				return CAnimation::WEAPON_PISTOL;
			case NDb::WT_RIFLE:
				return CAnimation::WEAPON_RIFLE;
			case NDb::WT_SUB_MACHINE_GUN:
				return CAnimation::WEAPON_SUB_MACHINE_GUN;
			case NDb::WT_KNIFE:
				return CAnimation::WEAPON_KNIFE;
			case NDb::WT_KATANA:
				return CAnimation::WEAPON_KATANA;
			case NDb::WT_MACHETE:
				return CAnimation::WEAPON_MACHETE;
			case NDb::WT_MACHINE_GUN:
				return CAnimation::WEAPON_MACHINE_GUN;
			case NDb::WT_RLAUNCHER:
				return CAnimation::WEAPON_RLAUNCHER;
			case NDb::WT_MINE_DETECTOR:
				return CAnimation::WEAPON_MINE_DETECTOR;
			case NDb::WT_PLAZMAGUN:
				return CAnimation::WEAPON_PLAZMAGUN;
		}
	}
	else
	{
		switch ( type )
		{
			case NDb::WT_DEFAULT:
				return CAnimation::WEAPON_ITEM;
			case NDb::WT_PISTOL:
			case NDb::WT_RIFLE:
			case NDb::WT_SUB_MACHINE_GUN:
			case NDb::WT_MACHINE_GUN:
			case NDb::WT_RLAUNCHER:
			case NDb::WT_TERROR_SHOOTER:
				return CAnimation::PK_WEAPON_SHOOTER;
			case NDb::WT_KNIFE:
			case NDb::WT_KATANA:
			case NDb::WT_MACHETE:
				return CAnimation::PK_WEAPON_SLASHER;
			case NDb::WT_PK_PLAZMAGUN:
			case NDb::WT_BOSS_PLAZMAGUN:
				return CAnimation::PK_WEAPON_PLAZMAGUN;
			case NDb::WT_TERROR_SPECIAL_GUN:
				return CAnimation::PK_WEAPON_TERROR_GUN;
		}
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPanzerklein
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPanzerklein::Import()
{
	// no "PersID" column in the retail db (chunk-verified): retail @0x4297e0 derives pPers by
	// scanning RPGPers for pers->pPanzerklein == this; done in BuildMapLinks (import order there).
	NDatabase::ImportField( "RicochetProb", &nRicochetProb );
	NDatabase::ImportField( "MaxVP", &nMaxVP );
	NDatabase::ImportField( "AddMoveAP", &nAddMoveAP );
	NDatabase::ImportField( "AddCoverIgnore", &fAddCoverIgnore );
	NDatabase::ImportField( "SensorRange", &fSensorRange );
	NDatabase::ImportField( "RegenerationValue", &fRegenerationValue );
	NDatabase::ImportField( "CriticalResist", &fCriticalResist );
	NDatabase::ImportField( "ArmorID", &pArmor );
	NDatabase::ImportField( "EncumbranceKoeff", &fEncumbranceKoeff );
  NDatabase::ImportField( "SelfExplosionID", &pSelfExplosion );
  NDatabase::ImportField( "LeftHandItemID", &pLeftHandItem );
  NDatabase::ImportField( "StrengthForGrenades", &nGrenadeStrength );
  NDatabase::ImportField( "HasNoHead", &bHasNoHead );
	char buf[ 32 ];
	for ( int i = 1; i <= N_PK_SPECIAL_WEAPONS; ++i )
	{
		sprintf( buf, "AllowWeaponType%d", i );
		NDatabase::ImportField( buf, &bAllowWeaponType[ i - 1 ] );
	}
	// pPers/pChangeValues resolved in BuildMapLinks (retail @0x4297e0 tail; needs RPGPers imported)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CNationality
////////////////////////////////////////////////////////////////////////////////////////////////////
void CNationality::Import()
{
  NDatabase::ImportField( "ToolTip", &pToolTip );
  NDatabase::ImportField( "MaleHead", &pMaleHead );
  NDatabase::ImportField( "FemaleHead", &pFemaleHead );
  NDatabase::ImportField( "FlagTexture", &pFlag );
  NDatabase::ImportField( "IconNormalTexture", &pIconNormal );
  NDatabase::ImportField( "IconDisabledTexture", &pIconDisabled );
	//// Retail schema (the customMale/FemaleHead vectors were dropped). The runtime 36MB game.db is COLUMNAR and is loaded
	//// through Import()/ImportField BY COLUMN NAME, so these names must match the game.db columns EXACTLY.
	//// Verified vs the runtime Nationalities table (tid 96): the 4 ref columns carry an "ID" suffix.
  NDatabase::ImportField( "FaceGenTemplate", &nFaceGenTemplate );
  NDatabase::ImportField( "MaleBiographyID", &pMaleBiography );
  NDatabase::ImportField( "FemaleBiographyID", &pFemaleBiography );
  NDatabase::ImportField( "CharGenBackgroundID", &pCharGenBackground );
  NDatabase::ImportField( "CustomHeadBackgroundID", &pCustomHeadBackground );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSide
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSide::Import()
{
  NDatabase::ImportField( "GlobalMap", &nGlobalMapID );
  NDatabase::ImportField( "HeroSelectTemplate", &nHeroSelectTemplate );
  NDatabase::ImportField( "StringID", &pName );
  NDatabase::ImportField( "Nationality1", &pNationality1 );
  NDatabase::ImportField( "Nationality2", &pNationality2 );
  NDatabase::ImportField( "Nationality3", &pNationality3 );

	malePersesSet.resize( CLASS_MAXVALUE );
  NDatabase::ImportField( "MaleMedic", &malePersesSet[MEDIC] );
  NDatabase::ImportField( "MaleScout", &malePersesSet[SCOUT] );
  NDatabase::ImportField( "MaleSniper", &malePersesSet[SNIPER] );
  NDatabase::ImportField( "MaleSoldier", &malePersesSet[SOLDIER] );
  NDatabase::ImportField( "MaleEngineer", &malePersesSet[ENGINEER] );
  NDatabase::ImportField( "MaleGrenadier", &malePersesSet[GRENADIER] );

	femalePersesSet.resize( CLASS_MAXVALUE );
  NDatabase::ImportField( "FemaleMedic", &femalePersesSet[MEDIC] );
  NDatabase::ImportField( "FemaleScout", &femalePersesSet[SCOUT] );
  NDatabase::ImportField( "FemaleSniper", &femalePersesSet[SNIPER] );
  NDatabase::ImportField( "FemaleSoldier", &femalePersesSet[SOLDIER] );
  NDatabase::ImportField( "FemaleEngineer", &femalePersesSet[ENGINEER] );
  NDatabase::ImportField( "FemaleGrenadier", &femalePersesSet[GRENADIER] );

	defaultPersesSet.resize( 6 );
  NDatabase::ImportField( "Nationality1Male", &defaultPersesSet[0] );
  NDatabase::ImportField( "Nationality1Female", &defaultPersesSet[1] );
  NDatabase::ImportField( "Nationality2Male", &defaultPersesSet[2] );
  NDatabase::ImportField( "Nationality2Female", &defaultPersesSet[3] );
  NDatabase::ImportField( "Nationality3Male", &defaultPersesSet[4] );
  NDatabase::ImportField( "Nationality3Female", &defaultPersesSet[5] );

	// The 6 per-preset class-description tooltips shown on the HeroMenu nationality portraits. The Steam
	// game.db CSide carries these as named CString-id columns (verified: Axis=19237..19242, Allies=
	// 19231..19236; Test/Demo sides leave them 0 -> null CPtr -> empty tooltip). Same name-keyed load path
	// as defaultPersesSet above; THIS is what populates the field for the v1 columnar game.db (not operator&).
	defaultPersToolTipsSet.resize( 6 );
  NDatabase::ImportField( "Nationality1MaleToolTip", &defaultPersToolTipsSet[0] );
  NDatabase::ImportField( "Nationality1FemaleToolTip", &defaultPersToolTipsSet[1] );
  NDatabase::ImportField( "Nationality2MaleToolTip", &defaultPersToolTipsSet[2] );
  NDatabase::ImportField( "Nationality2FemaleToolTip", &defaultPersToolTipsSet[3] );
  NDatabase::ImportField( "Nationality3MaleToolTip", &defaultPersToolTipsSet[4] );
  NDatabase::ImportField( "Nationality3FemaleToolTip", &defaultPersToolTipsSet[5] );
	//// Interface
	// retail CSide::Import @0x42a240 tail order (oracle s2_dbimport.h:2891-2897): ESCMenuBackground,
	// UICMInfoBackgroundID, UICluePaperBackgroundID, UIMedalPaperContainerID, UIBaseFlag,
	// UIBaseFlagActive, HeroDialogPersID. medals is NOT imported here -- CMedal records install
	// themselves (CMedal::Import @0x42a110); pKIAPaper is chunk-stream-only (no column) in retail too.
  NDatabase::ImportField( "ESCMenuBackground", &pESCMenuBackground );
  NDatabase::ImportField( "UICMInfoBackgroundID", &pChapterMapInfoBackground );
  NDatabase::ImportField( "UICluePaperBackgroundID", &pCluePaperBackground );
  NDatabase::ImportField( "UIMedalPaperContainerID", &pMedalPaperContainer );
  NDatabase::ImportField( "UIBaseFlag", &pBaseFlag );
  NDatabase::ImportField( "UIBaseFlagActive", &pBaseFlagActive );
  NDatabase::ImportField( "HeroDialogPersID", &pDialogHero );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NDb;
// Format 0x[Person]DDMYHHN
REGISTER_SAVELOAD_CLASS( 0xE0502151, CRPGArmor )
REGISTER_SAVELOAD_CLASS( 0xE0502152, CRPGAmmo )
REGISTER_SAVELOAD_CLASS( 0xE0502130, CRPGBaseValue )
REGISTER_SAVELOAD_CLASS( 0x00231160, CRPGClass )
REGISTER_SAVELOAD_CLASS( 0x00231161, CRPGWeaponType )
REGISTER_SAVELOAD_CLASS( 0x00231162, CRPGWeapon )
REGISTER_SAVELOAD_CLASS( 0x00231165, CRPGPers )
REGISTER_SAVELOAD_CLASS( 0xE0591180, CRPGClip )
REGISTER_SAVELOAD_CLASS( 0xE0891160, CRPGItem )
REGISTER_SAVELOAD_CLASS( 0xE1991120, CScript )
REGISTER_SAVELOAD_CLASS( 0x106B1130, CRPGUniform )
REGISTER_SAVELOAD_CLASS( 0x106B1131, CRPGGrenade )
REGISTER_SAVELOAD_CLASS( 0xA26B1190, CRPGCritical )
REGISTER_SAVELOAD_CLASS( 0xA1112140, CRPGFirstAid )
REGISTER_SAVELOAD_CLASS( 0xA1512131, CRPGMeleeWeapon )
REGISTER_SAVELOAD_CLASS( 0xA2222130, CRPGToHit )
REGISTER_SAVELOAD_CLASS( 0x12532170, CAnimWeaponType )
REGISTER_SAVELOAD_CLASS( 0x52632170, CRPGAISoundConstants )
REGISTER_SAVELOAD_CLASS( 0x51542120, CRPGInterruptsConstants )
REGISTER_SAVELOAD_CLASS( 0xE0562170, CRPGDmgToArmor )
REGISTER_SAVELOAD_CLASS( 0x11262130, CRPGMineDetector )
REGISTER_SAVELOAD_CLASS( 0x70902130, CPanzerklein )
REGISTER_SAVELOAD_CLASS( 0xA15A2160, CRPGMaterial )
REGISTER_SAVELOAD_CLASS( 0xB1028140, CSide )
REGISTER_SAVELOAD_CLASS( 0xB1028141, CNationality )
REGISTER_SAVELOAD_CLASS( 0xB1028142, CRPGStoreItem )
REGISTER_SAVELOAD_CLASS( 0x018c2111, CRPGMine )
REGISTER_SAVELOAD_CLASS( 0x024c2145, CRPGTool )
REGISTER_SAVELOAD_CLASS( 0x024c2146, CRPGKey )
REGISTER_SAVELOAD_CLASS( 0x71963210, CRPGEngGrenade )	// retail saveload id (from initCRPGEngGrenade0x71963210)
