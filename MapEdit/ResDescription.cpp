#include "StdAfx.h"
#include "MapEdit.h"
#include "dbDefs.h"
#include "ItemsMgr.h"
#include "ItemsDBCmd.h"
#include "TreeDBCmd.h"
#include "Export.h"
#include "CtrlObjectInspector.h"
#include "TreeSelItemDlg.h"
#include "PlacableItemsDlg.h"
#include "..\dbFormat\DataConst.h"
#include "ExportScenario.h"
#include "ExpImpDialogs.h"

extern SDBConnection dbConnection;
const bool bHideResources = false;
void SetupObjects();
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
  CItemsDBCmd itemsDB;
  CTreeDBCmd  treeDB;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetTemplateView( int nTemplID, int nVarID )
{
  theApp.SetTemplate( nTemplID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetModelView( int nModelID, int nVarID )
{
  theApp.SetModel( nModelID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetAIModelView( int nModelID, int nVarID )
{
  theApp.SetAIModel( nModelID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetGeometryView( int nGeomID, int nVarID )
{
  theApp.SetGeometry( nGeomID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetMaterialView( int nTemplID, int nVarID )
{
  theApp.SetMaterial( nTemplID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetTextureView( int nTexID, int nVarID )
{
  theApp.SetTexture( nTexID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetActiveWall( int nWallModelID, int nVarID )
{
  theApp.SetActiveBrushModel( IDC_WALLS_TREE, nWallModelID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetActiveFloor( int nFloorModelID, int nVarID )
{
  theApp.SetActiveBrushModel( IDC_CONSTRUCTIONPARTS_TREE, nFloorModelID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetActiveSolid( int nSolidModelID, int nVarID )
{
  theApp.SetActiveBrushModel( IDC_SOLIDMODELS_TREE, nSolidModelID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetEmptyView( int nWallModelID, int nVarID )
{
  theApp.SetEmptyView();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetAnimationView( int nAnimationID, int nVarID )
{
  theApp.SetAnimation( nAnimationID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetParticleView( int nParticleID, int nVarID )
{
  theApp.SetParticle( nParticleID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetContainerView( int nContainerID, int nVarID )
{
  theApp.SetContainer( nContainerID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetObjectView( int nContainerID, int nVarID )
{
  theApp.SetObject( nContainerID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetSoundView( int nSoundID, int nVarID )
{
  theApp.SetSound( nSoundID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetUIView( int nUIID, int nVarID )
{
  theApp.SetUI( nUIID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetConstructionPartView( int nID, int nVarID )
{
  theApp.SetConstructionPart( nID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetAmbientLightView( int nID, int nVarID )
{
	theApp.SetAmbientLight( nID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetRndSoundView( int nID, int nVarID )
{
	theApp.SetRndSound( nID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetSoundEffectView( int nID, int nVarID )
{
	theApp.SetDiplomacy( nID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetDiplomacyView( int nID, int nVarID )
{
	theApp.SetDiplomacy( nID, nVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetGreandeView( int nID, int nVarID )
{
	const SResTree *pGrenadeTree = theApp.GetResTree( IDC_RPG_GRENADES_TREE );
	const SResTree *pItemsTree = theApp.GetResTree( IDC_RPG_ITEMS_TREE );
	const SResTree *pModelsTree = theApp.GetResTree( IDC_MODELS_TREE );
	if ( !pGrenadeTree || !pItemsTree || !pModelsTree )
		return;
	const CPropMap *pGProps = pGrenadeTree->pItemsTree->GetPropList( nID );
	if ( pGProps )
	{
		CPropMap::const_iterator it = pGProps->find( "ItemID" );
		if ( it != pGProps->end() )
		{
			const CPropMap *pIProps = pItemsTree->pItemsTree->GetPropList( it->second->GetValue() );
			if ( pIProps )
			{
				it = pIProps->find( "ModelID" );
				if ( it != pIProps->end() )
				{
					int nModel = it->second->GetValue();
					vector<int> vars;
					if ( pModelsTree->pItemsTree->GetItemVariants( nModel, &vars ) && !vars.empty() )
						theApp.SetModel( nModel, vars[0] );
				}
				pItemsTree->pItemsTree->ReleasePropList( pIProps );
			}
		}
		pGrenadeTree->pItemsTree->ReleasePropList( pGProps );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetPersView( int nID, int nVarID ) { theApp.SetPers( nID, nVarID ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetWeaponView( int nID, int nVarID ) { theApp.SetWeapon( nID, nVarID ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetChapterView( int nID, int nVarID ) { theApp.SetChapter( nID, nVarID ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetGlobalMapView( int nID, int nVarID ) { theApp.SetGlobalMap( nID, nVarID ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetRPGItemView( int nID, int nVarID ) { theApp.SetRPGItem( nID, nVarID ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetHeadView( int nID, int nVarID ) { theApp.SetHead( nID, nVarID ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetScenarioView( int nID, int nVarID ) { theApp.SetScenario( nID, nVarID ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
string GetResString( UINT nID )
{
  static char buf[1024];
  LoadString( theApp.m_hInstance, nID, buf, sizeof(buf) );

  return buf;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// «агрузка ресурсных деревьев из базы данных
bool LoadResources()
{
	itemsDB.SetConnection( &dbConnection );
  treeDB.SetConnection( &dbConnection );
  puts( "DB->OpenConnection()" );

  {
    SResTree res;
    res.nTreeID    = IDC_TEMPLATE_TREE;
    res.szTabName  = "Templates";
    res.szRootName = "All Templates";
    res.pItemsTree = dbgnew CItemsMgr( IDC_TEMPLATE_TREE, &itemsDB, &treeDB, TYPE_TBL, TEMPL_TBL, "TemplVariants" );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Grid", false );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "RndWeight", 1.f );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "DefaultLight", 102 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ScriptID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Border" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "AmbientMusic" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "CombatMusic" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "HMBlendType" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "DiplomacyID" );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "ShowTerrain", true );
		res.pItemsTree->SetRelation( "DefaultLight", IDC_AMBIENTLIGHTS_TREE );
		res.pItemsTree->SetRelation( "ScriptID", IDC_SCRIPTS_TREE );
		res.pItemsTree->SetRelation( "AmbientMusic", IDC_MUSIC_TREE );
		res.pItemsTree->SetRelation( "CombatMusic", IDC_MUSIC_TREE );
		res.pItemsTree->SetRelation( "DiplomacyID", IDC_DIPLOMACY_TREE );
		res.pItemsTree->AddString( "HMBlendType", "Normal" );
		res.pItemsTree->AddString( "HMBlendType", "Add" );
		res.pItemsTree->AddString( "HMBlendType", "Subtract" );
		res.pItemsTree->SetTip( "RndWeight", IDS_TIP_RNDWEIGHT );
    res.pSelectCb  = &SetTemplateView;
		res.bVisible   = true;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_MODELS_TREE;
    res.szTabName  = "Models";
    res.szRootName = "All Models";
    res.pItemsTree = dbgnew CItemsMgr( IDC_MODELS_TREE, &itemsDB, &treeDB, MODELSTREE_TBL, "ModelTemplates", MODELS_TBL );
    res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Material0" );
    res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Material1" );
    res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Material2" );
    res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Material3" );
    res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "GeometryID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SkeletonID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "RPGArmorID" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "RndWeight", 1.f );
    res.pItemsTree->SetRelation( "GeometryID", IDC_GEOMETRIES_TREE );
    res.pItemsTree->SetRelation( "Material0", IDC_MATERIALS_TREE );
    res.pItemsTree->SetRelation( "Material1", IDC_MATERIALS_TREE );
    res.pItemsTree->SetRelation( "Material2", IDC_MATERIALS_TREE );
    res.pItemsTree->SetRelation( "Material3", IDC_MATERIALS_TREE );
		res.pItemsTree->SetRelation( "SkeletonID", IDC_SKELETONS_TREE );
		res.pItemsTree->SetRelation( "RPGArmorID", IDC_RPG_ARMORS_TREE );
		res.pItemsTree->SetTip( "RndWeight", IDS_TIP_RNDWEIGHT );
    res.pSelectCb  = &SetModelView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_GEOMETRIES_TREE;
    res.szTabName  = "Geometries";
    res.szRootName = "All Geometries";
    res.pItemsTree = dbgnew CItemsMgr( IDC_GEOMETRIES_TREE, &itemsDB, &treeDB, GEOMTREE_TBL, GEOMETRIES_TBL );
    res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcName" );
    res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "RootJoint" );
    res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "AIGeometryID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "AIGeometry2ID" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SizeX", 0, true );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SizeY", 0, true );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SizeZ", 0, true );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CenterX", 0, true );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CenterY", 0, true );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CenterZ", 0, true );
    res.pItemsTree->SetPrefix( "SrcName", "Models\\" );
    res.pItemsTree->SetRelation( "AIGeometryID", IDC_AIGEOMETRIES_TREE );
		res.pItemsTree->SetRelation( "AIGeometry2ID", IDC_AIGEOMETRIES_TREE );
    res.pSelectCb  = &SetGeometryView;
    res.pDoExport  = &ExportGeometry;
		res.pDoUpdateDB = &GeometryUpdateDBData;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_AIGEOMETRIES_TREE;
    res.szTabName  = "AIGeometries";
    res.szRootName = "All AIGeometries";
    res.pItemsTree = dbgnew CItemsMgr( IDC_AIGEOMETRIES_TREE, &itemsDB, &treeDB, "AIGeometriesTree", "AIGeometries" );
    res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcName" );
    res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "RootJoint" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Volume", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SolidPart", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Damage", true );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Cover", true );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Vision", true );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Passability", true );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "ItemBlocker", true );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_GEOMETRYPIECES, "PiecesInfo" );
    res.pItemsTree->SetPrefix( "SrcName", "Models\\" );
    res.pSelectCb  = &SetAIModelView;
    res.pDoExport  = &ExportAIGeometry;
		res.pDoUpdateDB = &AIGeometryUpdateDBData;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_MATERIALS_TREE;
    res.szTabName  = "Materials";
    res.szRootName = "All Materials";
    res.pItemsTree = dbgnew CItemsMgr( IDC_MATERIALS_TREE, &itemsDB, &treeDB, MATTREE_TBL, "MaterialTemplates", MATERIALS_TBL );
    res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "TextureID" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "AddressMode", "Clamp" );
    res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "BRDFID" );
    res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "Alpha", "opaque" );
    res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "BumpID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "GlossID" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SpecFactor", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_COLOR, "SpecColor", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "MetalMirror", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "DielMirror", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "MirrorID" );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "CastShadow", true );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "RndWeight", 1.f );
    res.pItemsTree->SetRelation( "BRDFID", IDC_BRDF_TREE );
    res.pItemsTree->SetRelation( "TextureID", IDC_TEXTURES_TREE );
    res.pItemsTree->SetRelation( "BumpID", IDC_TEXTURES_TREE );
		res.pItemsTree->SetRelation( "GlossID", IDC_TEXTURES_TREE );
		res.pItemsTree->SetRelation( "MirrorID", IDC_TEXTURES_TREE );
    res.pItemsTree->AddString( "Alpha", "opaque" );
    res.pItemsTree->AddString( "Alpha", "alpha_test" );
    res.pItemsTree->AddString( "Alpha", "transparent" );
		res.pItemsTree->AddString( "Alpha", "transparent_2sided" );
		res.pItemsTree->AddString( "Alpha", "self_illum" );
		res.pItemsTree->AddString( "Alpha", "self_illum_alpha_test" );
		res.pItemsTree->AddString( "Alpha", "overlay" );
		res.pItemsTree->AddString( "Alpha", "predator" );
		res.pItemsTree->AddString( "Alpha", "explosion_decal" );
		res.pItemsTree->AddString( "AddressMode", "Wrap" );
		res.pItemsTree->AddString( "AddressMode", "Clamp" );
		res.pItemsTree->SetTip( "RndWeight", IDS_TIP_RNDWEIGHT );
    res.pSelectCb  = &SetMaterialView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_TEXTURES_TREE;
    res.szTabName  = "Textures";
    res.szRootName = "All Textures";
    res.pItemsTree = dbgnew CItemsMgr( IDC_TEXTURES_TREE, &itemsDB, &treeDB, TEXTREE_TBL, TEXTURES_TBL );
    res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcName" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "Type", "Ordinary" );
    res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "Format", "8888" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "AddrType", "Clamp" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "NMips", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "MappingSize", 2 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "BumpGain", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Width", 0, true );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Height", 0, true );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "AverageColor", 0, true );
    res.pItemsTree->SetPrefix( "SrcName", "Textures\\" );
    res.pItemsTree->AddString( "Format", "8888" );
    res.pItemsTree->AddString( "Format", "565" );
		res.pItemsTree->AddString( "Format", "dxt1" );
		res.pItemsTree->AddString( "Format", "dxt2" );
		res.pItemsTree->AddString( "Format", "dxt3" );
		res.pItemsTree->AddString( "Format", "dxt4" );
		res.pItemsTree->AddString( "Format", "dxt5" );
		res.pItemsTree->AddString( "Type", "Ordinary" );
		res.pItemsTree->AddString( "Type", "2D" );
		res.pItemsTree->AddString( "Type", "Bump" );
		res.pItemsTree->AddString( "Type", "Transparent" );
		res.pItemsTree->AddString( "Type", "TransparentAdd" );
		res.pItemsTree->AddString( "Type", "LinearPicture" );
		res.pItemsTree->AddString( "AddrType", "Wrap" );
		res.pItemsTree->AddString( "AddrType", "Clamp" );
    res.pSelectCb  = &SetTextureView;
    res.pDoExport  = &ExportTextures;
		res.pDoUpdateDB = &TextureUpdateDBData;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_BRDF_TREE;
    res.szTabName  = "BRDFs";
    res.szRootName = "BRDFs";
    res.pItemsTree = dbgnew CItemsMgr( IDC_BRDF_TREE, &itemsDB, &treeDB, "BRDFsTree", "BRDFs" );
    res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "fake", 1 );
    res.pSelectCb  = &SetEmptyView;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_SKELETONS_TREE;
    res.szTabName  = "Skeletons";
    res.szRootName = "All Skeletons";
    res.pItemsTree = dbgnew CItemsMgr( IDC_SKELETONS_TREE, &itemsDB, &treeDB, "SkeletonsTree", "Skeletons" );
    res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcName" );
    res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "RootJoint" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "AIMeshStay" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "AIMeshCrouch" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "AIMeshLie" );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "MSRFormat", false );
		res.pItemsTree->SetRelation( "AIMeshStay", IDC_AIGEOMETRIES_TREE );
		res.pItemsTree->SetRelation( "AIMeshCrouch", IDC_AIGEOMETRIES_TREE );
		res.pItemsTree->SetRelation( "AIMeshLie", IDC_AIGEOMETRIES_TREE );
    res.pItemsTree->SetPrefix( "SrcName", "Models\\" );
    res.pSelectCb  = &SetEmptyView;
    res.pDoExport  = &ExportSkeletons;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_ANIMATIONS_TREE;
    res.szTabName  = "Animations";
    res.szRootName = "All Animations";
    res.pItemsTree = dbgnew CItemsMgr( IDC_ANIMATIONS_TREE, &itemsDB, &treeDB, "AnimationsTree", "Animations" );
    res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcName" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "PersName" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcName2" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "PersName2" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SkeletonID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "StartFrame", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "EndFrame", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Speed", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "RndWeight", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "Type", "Pose" );
		// Pose
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Stand", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Crouch", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Crawl", false );
		//Weapon
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "NoWeapon", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Item", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Pistol", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Rifle", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "SubMachineGun", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "MachineGun", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "RLauncher", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Plazmagun", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Knife", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Machete", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Katana", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "MDetector", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "PKShooter", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "PKPlazmagun", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "PKSlasher", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "PKRepairer", false );
		//Sex
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Male", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Female", false );
		//Combat
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Combat", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Realtime", false );
		//Class
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Engineer", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Grenadier", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Medic", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Scout", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Sniper", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Soldier", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Enemy", false );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SideID" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "Params" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Time1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "StepTime1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "StepTime2" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SoundTime1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Sound1" );
    res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Angle" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FallHeight" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FPS", 30 );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "DefaultFrames", true );
		
    res.pItemsTree->SetRelation( "SkeletonID", IDC_SKELETONS_TREE );
		res.pItemsTree->SetRelation( "Sound1", IDC_SOUNDS_TREE );
		res.pItemsTree->SetRelation( "SideID", IDC_SIDES_TREE );
    res.pItemsTree->SetPrefix( "SrcName", "Models\\" );
		res.pItemsTree->SetPrefix( "SrcName2", "Models\\" );
		//
		res.pItemsTree->AddString( "Type", "Pose" );         
		res.pItemsTree->AddString( "Type", "StartMove" );		
		res.pItemsTree->AddString( "Type", "Move" );				
		res.pItemsTree->AddString( "Type", "StartRun" );		
		res.pItemsTree->AddString( "Type", "Run" );					
		res.pItemsTree->AddString( "Type", "StartAttack" );	
		res.pItemsTree->AddString( "Type", "Attack" );			
		res.pItemsTree->AddString( "Type", "AttackUp" );		
		res.pItemsTree->AddString( "Type", "AttackDown" );	
		res.pItemsTree->AddString( "Type", "TurnLeft" );		
		res.pItemsTree->AddString( "Type", "TurnRight" );		
		res.pItemsTree->AddString( "Type", "PoseStrafe" );	
		res.pItemsTree->AddString( "Type", "StartStrafeF" );
		res.pItemsTree->AddString( "Type", "StrafeF" );			
		res.pItemsTree->AddString( "Type", "StartStrafeL" );
		res.pItemsTree->AddString( "Type", "StrafeL" );			
		res.pItemsTree->AddString( "Type", "StartStrafeR" );
		res.pItemsTree->AddString( "Type", "StrafeR" );			
		res.pItemsTree->AddString( "Type", "StartStrafeB" );
		res.pItemsTree->AddString( "Type", "StrafeB" );			
		res.pItemsTree->AddString( "Type", "ClimbLow" );		
		res.pItemsTree->AddString( "Type", "ClimbHigh" );		
		res.pItemsTree->AddString( "Type", "JumpLow" );			
		res.pItemsTree->AddString( "Type", "JumpHigh" );		
		res.pItemsTree->AddString( "Type", "JumpStart" );		
		res.pItemsTree->AddString( "Type", "ClimbFinish" );	
		res.pItemsTree->AddString( "Type", "Activate" );		
		res.pItemsTree->AddString( "Type", "Deactivate" );	
		res.pItemsTree->AddString( "Type", "PoseItem" );		
		res.pItemsTree->AddString( "Type", "Use" );					
		res.pItemsTree->AddString( "Type", "ChangePose" );	
		res.pItemsTree->AddString( "Type", "Death" );
		res.pItemsTree->AddString( "Type", "PoseCorpse" );
		res.pItemsTree->AddString( "Type", "TakeCorpse" );
		res.pItemsTree->AddString( "Type", "DropCorpse" );
		res.pItemsTree->AddString( "Type", "StartMoveCorpse" );
		res.pItemsTree->AddString( "Type", "MoveCorpse" );
		res.pItemsTree->AddString( "Type", "AttackLeftDown" );
		res.pItemsTree->AddString( "Type", "AttackRightDown" );
		res.pItemsTree->AddString( "Type", "AttackLeftUp" );
		res.pItemsTree->AddString( "Type", "AttackRightUp" );
		res.pItemsTree->AddString( "Type", "Open" );
		res.pItemsTree->AddString( "Type", "Reload" );
		res.pItemsTree->AddString( "Type", "Idle" );
		res.pItemsTree->AddString( "Type", "PoseHeal" );
		res.pItemsTree->AddString( "Type", "StartHeal" );
		res.pItemsTree->AddString( "Type", "Heal" );
		res.pItemsTree->AddString( "Type", "AttackCeiling" );
		res.pItemsTree->AddString( "Type", "AttackFloor" );
		res.pItemsTree->AddString( "Type", "EnterLadderUp" );
		res.pItemsTree->AddString( "Type", "EnterLadderDown" );
		res.pItemsTree->AddString( "Type", "LeaveLadderUp" );
		res.pItemsTree->AddString( "Type", "LeaveLadderDown" );
		res.pItemsTree->AddString( "Type", "MoveLadderUp" );
		res.pItemsTree->AddString( "Type", "MoveLadderDown" );
		res.pItemsTree->AddString( "Type", "JumpLadder" );
		res.pItemsTree->AddString( "Type", "PutBackpack" );
		res.pItemsTree->AddString( "Type", "GetBackpack" );
		res.pItemsTree->AddString( "Type", "ThrowKnife" );
		res.pItemsTree->AddString( "Type", "Fall" );
		res.pItemsTree->AddString( "Type", "Destruct1" );
		res.pItemsTree->AddString( "Type", "Destruct2" );
		res.pItemsTree->AddString( "Type", "Destruct3" );
		res.pItemsTree->AddString( "Type", "Destruct4" );
		//
    res.pSelectCb  = &SetAnimationView;
    res.pDoExport  = &ExportAnimations;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_FONTS_TREE;
    res.szTabName  = "Fonts";
    res.szRootName = "All Fonts";
    res.pItemsTree = dbgnew CItemsMgr( IDC_FONTS_TREE, &itemsDB, &treeDB, "FontsTree", "Fonts" );
    res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "TextureID" );
    res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Height", 20 );
    res.pItemsTree->AddProperty( CVariant::VT_INT, DT_COMBO, "Thickness", "400" );
    res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Italic", false );
    res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Antialiased", true );
    res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "Pitch", "default" );
    res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "Charset", "ansi" );
    res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "FaceName", "Courier" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "Name" );
    res.pItemsTree->SetRelation( "TextureID", IDC_TEXTURES_TREE );
    //
    res.pItemsTree->AddString( "Thickness", "400" );
    res.pItemsTree->AddString( "Thickness", "700" );
    //
    res.pItemsTree->AddString( "Pitch", "default" );
    res.pItemsTree->AddString( "Pitch", "variable" );
    res.pItemsTree->AddString( "Pitch", "fixed" );
    res.pSelectCb  = &SetEmptyView;
    res.pDoExport  = &ExportFonts;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
/*
  {
    SResTree res;
    res.nTreeID    = 7777;
    res.szTabName  = "Test";
    res.szRootName = "All Tests";
    res.pItemsTree = dbgnew CItemsMgr( &itemsDB, &treeDB, "TestTree", "Test" );
    res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "Str" );
    res.pSelectCb  = &SetEmptyView;
    theApp.resTreesHash[res.nTreeID] = res;
  }
*/
  {
    SResTree res;
    res.nTreeID    = IDC_RPG_CLASSES_TREE;
    res.szTabName  = "Classes";
    res.szRootName = "All Classes";
    res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_CLASSES_TREE, &itemsDB, &treeDB, "RPGClassesTree", "RPGClasses" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Icon" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "IconDisabled" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ToolTip" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "PerksPanel" );
		res.pItemsTree->SetRelation( "Icon", IDC_UITEXTURES_TREE );
		res.pItemsTree->SetRelation( "IconDisabled", IDC_UITEXTURES_TREE );
		res.pItemsTree->SetRelation( "PerksPanel", IDC_INTERFACES_TREE );
		res.pItemsTree->SetRelation( "ToolTip", IDC_STRINGS_TREE );
    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_RPG_PERS_TREE;
    res.szTabName  = "Pers";
    res.szRootName = "All Pers";
    res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_PERS_TREE, &itemsDB, &treeDB, "RPGPersTree", "RPGPers" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ModelID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ClassID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "WeaponID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "BaseValueID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "FaceID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "HitSoundID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "DeathSoundID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "NationalityID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "DisplayName" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "Side" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "UniformID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "MaxAck", CVariant(), true );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SideID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Voice" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcAcksFile" );
		//
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraAnchorX", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraAnchorY", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraAnchorZ", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraDistance", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraYaw", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraPitch", -1.55 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraRoll", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraFOV", 35 );
		//
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FaceGenCameraAnchorX", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FaceGenCameraAnchorY", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FaceGenCameraAnchorZ", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FaceGenCameraDistance", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FaceGenCameraYaw", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FaceGenCameraPitch", -1.55 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FaceGenCameraRoll", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FaceGenCameraFOV", 35 );

    res.pItemsTree->SetRelation( "ModelID", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "ClassID", IDC_RPG_CLASSES_TREE );
		res.pItemsTree->SetRelation( "WeaponID", IDC_RPG_WEAPONS_TREE );
		res.pItemsTree->SetRelation( "BaseValueID", IDC_RPG_BASEVALUES );
		res.pItemsTree->SetRelation( "FaceID", IDC_COMPLEXHEADS );
		res.pItemsTree->SetRelation( "HitSoundID", IDC_RNDSOUNDS_TREE );
		res.pItemsTree->SetRelation( "DeathSoundID", IDC_RNDSOUNDS_TREE );
		res.pItemsTree->SetRelation( "NationalityID", IDC_NATIONALITY_TREE );
		res.pItemsTree->SetRelation( "DisplayName", IDC_STRINGS_TREE );
		res.pItemsTree->SetRelation( "UniformID", IDC_UNIFORMS_TREE );
		res.pItemsTree->SetRelation( "SideID", IDC_SIDES_TREE );
		res.pSelectCb  = &SetPersView;
		res.bVisible   = true;
		res.bHideTree  = bHideResources;
		res.pDoExport  = ExportAcksTexts;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_RPG_WEAPONS_TREE;
    res.szTabName  = "RPG Weapons";
      res.szRootName = "RPG Weapons";
    res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_WEAPONS_TREE, &itemsDB, &treeDB, "RPGWeaponsTree", "RPGWeapons" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ItemID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SoundID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "BurstSoundID" );
		//res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "CycleBurstSoundID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "LongBurstSoundID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "BurstEndSoundID" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "AnimationName" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ReloadSoundID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ShotEffectID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "TrailEffectID" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "TrailSpeed", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "InnerClip" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "InnerClipAmmoQuantity" );
		res.pItemsTree->SetRelation( "SoundID", IDC_SOUNDS_TREE );
		res.pItemsTree->SetRelation( "BurstSoundID", IDC_SOUNDS_TREE );
		res.pItemsTree->SetRelation( "LongBurstSoundID", IDC_SOUNDS_TREE );
		res.pItemsTree->SetRelation( "BurstEndSoundID", IDC_SOUNDS_TREE );
		res.pItemsTree->SetRelation( "ReloadSoundID", IDC_SOUNDS_TREE );
		res.pItemsTree->SetRelation( "ItemID", IDC_RPG_ITEMS_TREE );
		res.pItemsTree->SetRelation( "ShotEffectID", IDC_EFFECTS_TREE );
		res.pItemsTree->SetRelation( "TrailEffectID", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "InnerClip", IDC_RPG_CLIPS_TREE );
    res.pSelectCb  = &SetWeaponView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_RPG_GRENADES_TREE;
    res.szTabName  = "Grenades";
    res.szRootName = "Grenades";
    res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_GRENADES_TREE, &itemsDB, &treeDB, "RPGGrenadesTree", "RPGGrenades" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ItemID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Sound1ID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Sound2ID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Sound3ID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Sound4ID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Effect1ID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Effect2ID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Effect3ID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Effect4ID" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "DecalRadius", 3 );
		res.pItemsTree->SetRelation( "Sound1ID", IDC_RNDSOUNDS_TREE );
		res.pItemsTree->SetRelation( "Sound2ID", IDC_RNDSOUNDS_TREE );
		res.pItemsTree->SetRelation( "Sound3ID", IDC_RNDSOUNDS_TREE );
		res.pItemsTree->SetRelation( "Sound4ID", IDC_RNDSOUNDS_TREE );
		res.pItemsTree->SetRelation( "ItemID", IDC_RPG_ITEMS_TREE );
		res.pItemsTree->SetRelation( "Effect1ID", IDC_EFFECTS_TREE );
		res.pItemsTree->SetRelation( "Effect2ID", IDC_EFFECTS_TREE );
		res.pItemsTree->SetRelation( "Effect3ID", IDC_EFFECTS_TREE );
		res.pItemsTree->SetRelation( "Effect4ID", IDC_EFFECTS_TREE );
    res.pSelectCb  = &SetGreandeView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_RPG_BASEVALUES;
    res.szTabName  = "BaseValues";
		res.szRootName = "BaseValues";
    res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_BASEVALUES, &itemsDB, &treeDB, "RPGBaseValuesTree", "RPGBaseValues" );
    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_PARTICLES_TREE;
    res.szTabName  = "Particles";
		res.szRootName = "Particles";
    res.pItemsTree = dbgnew CItemsMgr( IDC_PARTICLES_TREE, &itemsDB, &treeDB, "ParticlesTree", "Particles" );
    res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcName" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "AIGeometryID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "RPGArmorID" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "ExportPrefix", "PFX_" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CenterX", 0, true );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CenterY", 0, true );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CenterZ", 0, true );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "HalfBoxX", 0, true );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "HalfBoxY", 0, true );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "HalfBoxZ", 0, true );
		res.pItemsTree->SetRelation( "AIGeometryID", IDC_AIGEOMETRIES_TREE );
		res.pItemsTree->SetRelation( "RPGArmorID", IDC_RPG_ARMORS_TREE );
		res.pItemsTree->SetPrefix( "SrcName", "\\" );
    res.pSelectCb  = &SetEmptyView;
		res.pDoExport  = &ExportParticles;
		res.pDoUpdateDB = &ParticlesUpdateDBData;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_PARTICLEINSTANCES_TREE;
    res.szTabName  = "ParticleInstances";
		res.szRootName = "ParticleInstances";
    res.pItemsTree = dbgnew CItemsMgr( IDC_PARTICLEINSTANCES_TREE, &itemsDB, &treeDB, "ParticleInstancesTree", "ParticleInstances" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ParticleID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "EffectID", CVariant(), true );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Scale", 1.0f );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Speed", 1.0f );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "Light", "Normal" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PositionX" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PositionY" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PositionZ" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "RotationX" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "RotationY" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "RotationZ" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Offset", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "EndCycle", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "CycleCount", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PivotX", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PivotY", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "AlphaBlending", "Normal" );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "IsCrown", false );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "Static", "Dynamic" );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "CastShadow", false );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "GlueToEffectBone", 0 );
		res.pItemsTree->AddString( "Light", "Normal" );
		res.pItemsTree->AddString( "Light", "Lit" );
		res.pItemsTree->AddString( "AlphaBlending", "Normal" );
		res.pItemsTree->AddString( "AlphaBlending", "Additive" );
		res.pItemsTree->AddString( "Static", "Static" );
		res.pItemsTree->AddString( "Static", "Dynamic" );
		res.pItemsTree->SetRelation( "ParticleID", IDC_PARTICLES_TREE );

		for ( int i = 0; i < NDb::N_PARTICLE_TEXTURES; ++i )
		{
			string szName = "Texture" + IToA( i );
			res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, szName.c_str() );
	    res.pItemsTree->SetRelation( szName.c_str(), IDC_TEXTURES_TREE );
		}
    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = true;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_EFFECTS_TREE;
    res.szTabName  = "Effects";
		res.szRootName = "Effects";
    res.pItemsTree = dbgnew CItemsMgr( IDC_EFFECTS_TREE, &itemsDB, &treeDB, "EffectsTree", "EffectTemplates", "Effects" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "RndWeight", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_RELLIST, "ParticleList" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_RELLIST, "LightList" );
		res.pItemsTree->SetRelation( "ParticleList", IDC_PARTICLEINSTANCES_TREE );
		res.pItemsTree->SetRelation( "LightList", IDC_LIGHTINSTANCES_TREE );
    res.pSelectCb  = &SetParticleView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_TERRAINTILES_TREE;
    res.szTabName  = "Terr Tiles";
    res.szRootName = "Terrain tiles";
    res.pItemsTree = dbgnew CItemsMgr( IDC_TERRAINTILES_TREE, &itemsDB, &treeDB, "TerrainTilesTree", "TerrainTiles" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_COLOR, "UserColor" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "TextureID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "BumpID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "MaskID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Priority", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "TextureVariants", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "MaskVariants", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "RPGArmorID" );
		res.pItemsTree->SetRelation( "TextureID", IDC_TEXTURES_TREE );
		res.pItemsTree->SetRelation( "BumpID", IDC_TEXTURES_TREE );
		res.pItemsTree->SetRelation( "MaskID", IDC_TEXTURES_TREE );
		res.pItemsTree->SetRelation( "RPGArmorID", IDC_RPG_ARMORS_TREE );
    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_AMBIENTLIGHTS_TREE;
    res.szTabName  = "Light";
    res.szRootName = "Ambient Lights";
    res.pItemsTree = dbgnew CItemsMgr( IDC_AMBIENTLIGHTS_TREE, &itemsDB, &treeDB, "AmbientLightsTree", "AmbientLightTemplates", "AmbientLights" );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "UseInGame", true );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_COLOR, "LightColor", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_COLOR, "AmbientColor", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_COLOR, "GlossColor", (int)0xffffff );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_COLOR, "FogColor", (int)0xffffff );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_COLOR, "VapourColor", (int)0xffffff );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FogStartDistance", 15 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FogDistance", 100 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "VapourHeight", 5 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "VapourDensity", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "VapourNoiseParam", 0.8f );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "VapourSpeed", 0.05f );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "VapourSwitchTime", 5 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Pitch", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Yaw", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SkyID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_COLOR, "ShadowColor", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_COLOR, "BackLightColor", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "GForce2LightID" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "VapourStartHeight", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "BlurStrength", 1.5f );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_COLOR, "GroundAmbientColor", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "RndWeight", 1 );
		res.pItemsTree->SetRelation( "SkyID", IDC_CUBEMAPS_TREE );
		res.pItemsTree->SetRelation( "GForce2LightID", IDC_AMBIENTLIGHTS_TREE );
    res.pSelectCb  = &SetAmbientLightView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
  {
    SResTree res;
    res.nTreeID    = IDC_RPG_ARMORS_TREE;
    res.szTabName  = "AI Materials";
    res.szRootName = "AIMaterials";
    res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_ARMORS_TREE, &itemsDB, &treeDB, "RPGArmorsTree", "RPGArmors" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SoundStepID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SoundShotID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ShotEffectID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "RPGMaterialID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "GrenadeSoundType" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "GrenadeExplosionType" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ShotMaterial" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "ShotRadius", 0.2f );
		res.pItemsTree->SetRelation( "SoundStepID", IDC_RNDSOUNDS_TREE );
		res.pItemsTree->SetRelation( "SoundShotID", IDC_RNDSOUNDS_TREE );
		res.pItemsTree->SetRelation( "SoundGrenadeID", IDC_RNDSOUNDS_TREE );
		res.pItemsTree->SetRelation( "ShotEffectID", IDC_EFFECTS_TREE );
		res.pItemsTree->SetRelation( "RPGMaterialID", IDC_RPGMATERIALS_TREE );
		res.pItemsTree->SetRelation( "ShotMaterial", IDC_MATERIALS_TREE );
    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
	{
		SResTree res;
		res.nTreeID    = IDC_RPGMATERIALS_TREE;
		res.szTabName  = "RPG Materials";
		res.szRootName = "RPG Materials";
		res.pItemsTree = dbgnew CItemsMgr( IDC_RPGMATERIALS_TREE, &itemsDB, &treeDB, "RPGMaterialsTree", "RPGMaterials" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "UltimateMoment", 140 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "UltimatePressure", 40 );
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
  {
    SResTree res;
    res.nTreeID    = IDC_CONTAINERS_TREE;
    res.szTabName  = "Containers";
    res.szRootName = "Containers";
    res.pItemsTree = dbgnew CItemsMgr( IDC_CONTAINERS_TREE, &itemsDB, &treeDB, "ContainerModelsTree", "ContainerModels" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ModelID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ParticleID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SoundID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "DestructionSoundID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SoundEffectID", CVariant(), false, IDC_RNDSOUNDS_TREE );
		const int PLIGHT_GRP = 2105;
		const int SLIGHT_GRP = 2106;
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_COLOR, "PointLight", CVariant(), false, PLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_COLOR, "SpotLight", CVariant(), false, SLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "ParticlePosX", 0, false, IDC_EFFECTS_TREE );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "ParticlePosY", 0, false, IDC_EFFECTS_TREE );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "ParticlePosZ", 0, false, IDC_EFFECTS_TREE );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "SoundType", "Permanent", false, IDC_RNDSOUNDS_TREE );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SoundAvgInterval", 10, false, IDC_RNDSOUNDS_TREE );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SoundPosX", 0, false, IDC_RNDSOUNDS_TREE );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SoundPosY", 0, false, IDC_RNDSOUNDS_TREE );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SoundPosZ", 0, false, IDC_RNDSOUNDS_TREE );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PLightPosX", 0, false, PLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PLightPosY", 0, false, PLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PLightPosZ", 0, false, PLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PLightRadius", 10, false, PLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PLightFlareRadius", 0, false, PLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "PLightFlareTexture", 0, false, PLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PLightFlarePosX", 0, false, PLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PLightFlarePosY", 0, false, PLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PLightFlarePosZ", 0, false, PLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SLightMaskID", CVariant(), false, SLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SLightRadius", 10, false, SLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SLightPosX", 0, false, SLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SLightPosY", 0, false, SLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SLightPosZ", 0, false, SLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SLightDirX", 0, false, SLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SLightDirY", 0, false, SLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SLightDirZ", 1, false, SLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SLightFOV", 90, false, SLIGHT_GRP );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_COLOR, "AmbientColor", 0 );

		res.pItemsTree->SetRelation( "ModelID", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "ParticleID", IDC_EFFECTS_TREE );
		res.pItemsTree->SetRelation( "SoundID", IDC_RNDSOUNDS_TREE );
		res.pItemsTree->SetRelation( "DestructionSoundID", IDC_RNDSOUNDS_TREE );
		res.pItemsTree->SetRelation( "SoundEffectID", IDC_SOUNDEFFECTS_TREE );
		res.pItemsTree->SetRelation( "PLightFlareTexture", IDC_TEXTURES_TREE );
		res.pItemsTree->SetRelation( "SLightMaskID", IDC_TEXTURES_TREE );
		res.pItemsTree->AddString( "SoundType", "Permanent" );
		res.pItemsTree->AddString( "SoundType", "Random" );
		res.pItemsTree->AddString( "SoundType", "Realtime" );
		res.pItemsTree->AddString( "SoundType", "Wind" );
    res.pSelectCb  = &SetContainerView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_SOUNDS_TREE;
    res.szTabName  = "Sound";
    res.szRootName = "Sound samples";
    res.pItemsTree = dbgnew CItemsMgr( IDC_SOUNDS_TREE, &itemsDB, &treeDB, "SoundsTree", "Sounds" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcName" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "MinDistance", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "MaxDistance", 100 );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Loop", false );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Priority", 1 );
    res.pSelectCb  = &SetSoundView;
		res.pDoExport  = &ExportSounds;
		res.bVisible   = true;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_RNDSOUNDS_TREE;
    res.szTabName  = "Rnd sound";
    res.szRootName = "Random sounds";
		res.pItemsTree = dbgnew CItemsMgr( IDC_RNDSOUNDS_TREE, &itemsDB, &treeDB, "SoundTemplatesTree", "SoundTemplates", "SoundVariants" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SoundID" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "RndWeight", 1 );
		res.pItemsTree->SetRelation( "SoundID", IDC_SOUNDS_TREE );
    res.pSelectCb  = &SetRndSoundView;
		res.bVisible   = true;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_DEBRIS_TREE;
    res.szTabName  = "Debris";
    res.szRootName = "Debris";
    res.pItemsTree = dbgnew CItemsMgr( IDC_DEBRIS_TREE, &itemsDB, &treeDB, "DebrisTree", "Debris" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ModelID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "DebrisMaterialID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Volume", 0 );
		res.pItemsTree->SetRelation( "ModelID", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "DebrisMaterialID", IDC_DEBRISMATERIALS_TREE );
    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_DEBRISMATERIALS_TREE;
    res.szTabName  = "DebrisMaterials";
    res.szRootName = "Debris materials";
    res.pItemsTree = dbgnew CItemsMgr( IDC_DEBRISMATERIALS_TREE, &itemsDB, &treeDB, "DebrisMaterialsTree", "DebrisMaterials" );
    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_OBJECTS_TREE;
    res.szTabName  = "Objects";
    res.szRootName = "Objects";
    res.pItemsTree = dbgnew CItemsMgr( IDC_OBJECTS_TREE, &itemsDB, &treeDB, "ObjectsTree", "ObjectTemplates", "Objects" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Model0" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Model1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Model2" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Model3" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Model4" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "DebrisMaterialID" );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Targetable", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "IsDeployPoint", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "KeepDecals", true );
		//res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "InteractiveType", "Default" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "RndWeight", 1.f );
//		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "RPGWeaponID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ChildID" );
		res.pItemsTree->SetRelation( "Model0", IDC_CONTAINERS_TREE );
		res.pItemsTree->SetRelation( "Model1", IDC_CONTAINERS_TREE );
		res.pItemsTree->SetRelation( "Model2", IDC_CONTAINERS_TREE );
		res.pItemsTree->SetRelation( "Model3", IDC_CONTAINERS_TREE );
		res.pItemsTree->SetRelation( "Model4", IDC_CONTAINERS_TREE );
		res.pItemsTree->SetRelation( "DebrisMaterialID", IDC_DEBRISMATERIALS_TREE );
		res.pItemsTree->SetRelation( "RPGWeaponID", IDC_RPG_WEAPONS_TREE );
		res.pItemsTree->SetRelation( "ChildID", IDC_OBJECTS_TREE );
		res.pItemsTree->SetTip( "RndWeight", IDS_TIP_RNDWEIGHT );
//		res.pItemsTree->AddString( "InteractiveType", "Default" );
//		res.pItemsTree->AddString( "InteractiveType", "WindowDoor" );
//		res.pItemsTree->AddString( "InteractiveType", "Cannon" );
    res.pSelectCb  = &SetObjectView;
		res.bVisible = true;
    theApp.resTreesHash[res.nTreeID] = res;
  }
	{
    SResTree res;
    res.nTreeID    = IDC_STRINGS_TREE;
    res.szTabName  = "Strings";
    res.szRootName = "Strings";
    res.pItemsTree = dbgnew CItemsMgr( IDC_STRINGS_TREE, &itemsDB, &treeDB, "StringsTree", "Strings" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "String" );
    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_INTERFACES_TREE;
    res.szTabName  = "Interfaces";
    res.szRootName = "Interfaces";
    res.pItemsTree = dbgnew CItemsMgr( IDC_INTERFACES_TREE, &itemsDB, &treeDB, "UIContainersTree", "UIContainers" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Width", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Height", 0 );
    res.pSelectCb  = &SetUIView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_CONSTRUCTIONPARTS_TREE;
    res.szTabName  = "Constructor";
    res.szRootName = "Construction Kit";
    res.pItemsTree = dbgnew CItemsMgr( IDC_CONSTRUCTIONPARTS_TREE, &itemsDB, &treeDB, "ConstructionPartsTree", "ConstructionPartTemplates", "ConstructionParts" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "FirstGeometryID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SecondGeometryID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_COLOR, "UserColor", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SizeX", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SizeY", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SizeZ", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_COMBO, "Thickness", 0.1f );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "FirstGeomDefMaterial0" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "FirstGeomDefMaterial1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "FirstGeomDefMaterial2" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "FirstGeomDefCutMaterial" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SecondGeomDefMaterial" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SecondGeomDefMaterial1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SecondGeomDefMaterial2" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SecondGeomDefCutMaterial" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_SUBPARTS, "SubPartMask", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Object" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "RPGArmorID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ClipGroupID", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "RndWeight", 1.f );
		res.pItemsTree->AddString( "Thickness", "0.1" );
		res.pItemsTree->AddString( "Thickness", "0.2" );
		res.pItemsTree->AddString( "Thickness", "0.3" );
		res.pItemsTree->SetRelation( "FirstGeometryID", IDC_GEOMETRIES_TREE );
		res.pItemsTree->SetRelation( "SecondGeometryID", IDC_GEOMETRIES_TREE );
		res.pItemsTree->SetRelation( "FirstGeomDefMaterial0", IDC_MATERIALS_TREE );
		res.pItemsTree->SetRelation( "FirstGeomDefMaterial1", IDC_MATERIALS_TREE );
		res.pItemsTree->SetRelation( "FirstGeomDefMaterial2", IDC_MATERIALS_TREE );
		res.pItemsTree->SetRelation( "FirstGeomDefCutMaterial", IDC_MATERIALS_TREE );
		res.pItemsTree->SetRelation( "SecondGeomDefMaterial", IDC_MATERIALS_TREE );
		res.pItemsTree->SetRelation( "SecondGeomDefMaterial1", IDC_MATERIALS_TREE );
		res.pItemsTree->SetRelation( "SecondGeomDefMaterial2", IDC_MATERIALS_TREE );
		res.pItemsTree->SetRelation( "SecondGeomDefCutMaterial", IDC_MATERIALS_TREE );
		res.pItemsTree->SetRelation( "Object", IDC_OBJECTS_TREE );
		res.pItemsTree->SetRelation( "RPGArmorID", IDC_RPG_ARMORS_TREE );
		res.pItemsTree->SetTip( "SizeX", IDS_TIP_CONSTRUCTION_SIZEX );
		res.pItemsTree->SetTip( "SizeY", IDS_TIP_CONSTRUCTION_SIZEX );
		res.pItemsTree->SetTip( "SizeZ", IDS_TIP_CONSTRUCTION_SIZEZ );
		res.pItemsTree->SetTip( "Thickness", IDS_TIP_CONSTRUCTION_THICKNESS );
		res.pItemsTree->SetTip( "FirstGeomDefMaterial0", IDS_TIP_CONSTRUCTION_DEFM );
		res.pItemsTree->SetTip( "FirstGeomDefMaterial1", IDS_TIP_CONSTRUCTION_DEFM );
		res.pItemsTree->SetTip( "FirstGeomDefMaterial2", IDS_TIP_CONSTRUCTION_DEFM );
		res.pItemsTree->SetTip( "FirstGeomDefCutMaterial", IDS_TIP_CONSTRUCTION_CUTM );
		res.pItemsTree->SetTip( "SecondGeomDefMaterial", IDS_TIP_CONSTRUCTION_DEFM2 );
		res.pItemsTree->SetTip( "SecondGeomDefCutMaterial", IDS_TIP_CONSTRUCTION_CUTM2 );
		res.pItemsTree->SetTip( "RndWeight", IDS_TIP_RNDWEIGHT );
    res.pSelectCb  = &SetConstructionPartView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_GRASS_TREE;
    res.szTabName  = "Grass";
    res.szRootName = "Grass";
    res.pItemsTree = dbgnew CItemsMgr( IDC_GRASS_TREE, &itemsDB, &treeDB, "GrassTree", "Grass" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "TextureID" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "ScaleX", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "ScaleY", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PivotX", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PivotY", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SpotMaterialID" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SpotScale", 0.5f );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SideSize", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "ScaleRange", 0 );
		res.pItemsTree->SetRelation( "TextureID", IDC_TEXTURES_TREE );
		res.pItemsTree->SetRelation( "SpotMaterialID", IDC_MATERIALS_TREE );

    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_UITEXTURES_TREE;
    res.szTabName  = "UITextures";
    res.szRootName = "UITextures";
    res.pItemsTree = dbgnew CItemsMgr( IDC_UITEXTURES_TREE, &itemsDB, &treeDB, "UITexturesTree", "UITextures" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "R_800x600" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "R_1024x768" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "R_1280x960" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "R_1600x1200" );
		res.pItemsTree->SetRelation( "R_800x600", IDC_TEXTURES_TREE );
		res.pItemsTree->SetRelation( "R_1024x768", IDC_TEXTURES_TREE );
		res.pItemsTree->SetRelation( "R_1280x960", IDC_TEXTURES_TREE );
		res.pItemsTree->SetRelation( "R_1600x1200", IDC_TEXTURES_TREE );

    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_RPG_ITEMS_TREE;
    res.szTabName  = "RPG Items";
    res.szRootName = "RPG Items";
    res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_ITEMS_TREE, &itemsDB, &treeDB, "RPGItemsTree", "RPGItems" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SizeX", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SizeY", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Weight", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ModelID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "NameID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "DescrID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ModelActive1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Model1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ModelActive2" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Model2" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "TooltipID" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "SubType" );
		//
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraAnchorX", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraAnchorY", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraAnchorZ", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraDistance", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraYaw", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraPitch", -1.55 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraRoll", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CameraFOV", 35 );
		//
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SlotCameraAnchorX", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SlotCameraAnchorY", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SlotCameraAnchorZ", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SlotCameraDistance", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SlotCameraYaw", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SlotCameraPitch", -1.55 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SlotCameraRoll", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SlotCameraFOV", 35 );
		//
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "AmmoCameraAnchorX", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "AmmoCameraAnchorY", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "AmmoCameraAnchorZ", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "AmmoCameraDistance", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "AmmoCameraYaw", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "AmmoCameraPitch", -1.55 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "AmmoCameraRoll", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "AmmoCameraFOV", 35 );
		//
		res.pItemsTree->SetRelation( "ModelID", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "ModelActive1", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "Model1", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "ModelActive2", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "Model2", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "TooltipID", IDC_STRINGS_TREE );
		res.pItemsTree->SetRelation( "NameID", IDC_STRINGS_TREE );
		res.pItemsTree->SetRelation( "DescrID", IDC_STRINGS_TREE );

    res.pSelectCb  = &SetRPGItemView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_RPG_CRITICALS_TREE;
    res.szTabName  = "Criticals";
    res.szRootName = "Criticals";
    res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_CRITICALS_TREE, &itemsDB, &treeDB, "RPGCriticalsTree", "RPGCriticals" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "HitLocation", "Torso" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "QueueIndex", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "MinDuration", -1 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "MaxDuration", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Range", 10 );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "Type", "Death" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Value", 1.0f );
		res.pItemsTree->AddString( "HitLocation", "Head" );
		res.pItemsTree->AddString( "HitLocation", "Torso" );
		res.pItemsTree->AddString( "HitLocation", "Arms" );
		res.pItemsTree->AddString( "HitLocation", "Legs" );
		res.pItemsTree->AddString( "HitLocation", "Any" );
		res.pItemsTree->AddString( "Type", "Death" );
		res.pItemsTree->AddString( "Type", "AP reduction" );
		res.pItemsTree->AddString( "Type", "Blind" );
		res.pItemsTree->AddString( "Type", "Weapon skill reduction" );
		res.pItemsTree->AddString( "Type", "Motionless" );
		res.pItemsTree->AddString( "Type", "Encumbrance" );
		res.pItemsTree->AddString( "Type", "Accidental shot" );
		res.pItemsTree->AddString( "Type", "Stun" );
		res.pItemsTree->AddString( "Type", "Lost weapon" );
		res.pItemsTree->AddString( "Type", "Idle hand" );
		res.pItemsTree->AddString( "Type", "Damage weapon" );
		res.pItemsTree->AddString( "Type", "Patient" );
		res.pItemsTree->AddString( "Type", "Deaf" );
		res.pItemsTree->AddString( "Type", "Bleeding" );

    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_CUBEMAPS_TREE;
    res.szTabName  = "Cubemaps";
    res.szRootName = "Cubemaps";
    res.pItemsTree = dbgnew CItemsMgr( IDC_CUBEMAPS_TREE, &itemsDB, &treeDB, "CubeTexturesTree", "CubeTextures" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "PositiveX" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "PositiveY" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "PositiveZ" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "NegativeX" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "NegativeY" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "NegativeZ" );
		res.pItemsTree->SetRelation( "PositiveX", IDC_TEXTURES_TREE );
		res.pItemsTree->SetRelation( "PositiveY", IDC_TEXTURES_TREE );
		res.pItemsTree->SetRelation( "PositiveZ", IDC_TEXTURES_TREE );
		res.pItemsTree->SetRelation( "NegativeX", IDC_TEXTURES_TREE );
		res.pItemsTree->SetRelation( "NegativeY", IDC_TEXTURES_TREE );
		res.pItemsTree->SetRelation( "NegativeZ", IDC_TEXTURES_TREE );

    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
	{
    SResTree res;
    res.nTreeID    = IDC_CHAPTERS_TREE;
    res.szTabName  = "Chapters";
    res.szRootName = "Chapters";
    res.pItemsTree = dbgnew CItemsMgr( IDC_CHAPTERS_TREE, &itemsDB, &treeDB, "ChapterMapsTree", "ChapterMaps" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Background" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "CampZone1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "CampZone2" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "CampZone3" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "CampZone4" );
		res.pItemsTree->SetRelation( "CampZone1", IDC_TEMPLATE_TREE );
		res.pItemsTree->SetRelation( "CampZone2", IDC_TEMPLATE_TREE );
		res.pItemsTree->SetRelation( "CampZone3", IDC_TEMPLATE_TREE );
		res.pItemsTree->SetRelation( "CampZone4", IDC_TEMPLATE_TREE );
		res.pItemsTree->SetRelation( "Background", IDC_UITEXTURES_TREE );
    res.pSelectCb  = &SetChapterView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_WAYPOINTNAMES_TREE;
    res.szTabName  = "WaypointNames";
    res.szRootName = "WaypointNames";
    res.pItemsTree = dbgnew CItemsMgr( IDC_WAYPOINTNAMES_TREE, &itemsDB, &treeDB, "WaypointNamesTree", "WaypointNames" );
		
    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_WAYPOINTS_TREE;
    res.szTabName  = "Waypoints";
    res.szRootName = "Waypoints";
    res.pItemsTree = dbgnew CItemsMgr( IDC_WAYPOINTS_TREE, &itemsDB, &treeDB, "WaypointsTree", "Waypoints" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "VariantID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "NameID" );
		res.pItemsTree->SetRelation( "NameID", IDC_WAYPOINTNAMES_TREE );
		
    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = true;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_DOORS_TREE;
    res.szTabName  = "Doors";
    res.szRootName = "Doors and Windows";
    res.pItemsTree = dbgnew CItemsMgr( IDC_DOORS_TREE, &itemsDB, &treeDB, "DoorsTree", "Doors" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ObjectID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "OpenSoundID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "CloseSoundID" );
		res.pItemsTree->SetRelation( "ObjectID", IDC_OBJECTS_TREE );
		res.pItemsTree->SetRelation( "OpenSoundID", IDC_RNDSOUNDS_TREE );
		res.pItemsTree->SetRelation( "CloseSoundID", IDC_RNDSOUNDS_TREE );

		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		res.pDoUpdateDB = &DoorsUpdateDBData;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
    SResTree res;
    res.nTreeID    = IDC_GUNS_TREE;
    res.szTabName  = "Guns";
    res.szRootName = "Guns";
    res.pItemsTree = dbgnew CItemsMgr( IDC_GUNS_TREE, &itemsDB, &treeDB, "GunsTree", "Guns" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ObjectID" );
		res.pItemsTree->SetRelation( "ObjectID", IDC_OBJECTS_TREE );

    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }	
	{
		SResTree res;
		res.nTreeID    = IDC_NATIONALITY_TREE;
		res.szTabName  = "Nationality";
		res.szRootName = "Nationality";
		res.pItemsTree = dbgnew CItemsMgr( IDC_NATIONALITY_TREE, &itemsDB, &treeDB, "NationalityTree", "Nationalities" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "IGNationality" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "Shortcut" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "MaleHead" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "FemaleHead" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "IconNormalTexture" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "IconDisabledTexture" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "FlagTexture" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ToolTip" );
		res.pItemsTree->SetRelation( "MaleHead", IDC_COMPLEXHEADS );
		res.pItemsTree->SetRelation( "FemaleHead", IDC_COMPLEXHEADS );
		res.pItemsTree->SetRelation( "IconNormalTexture", IDC_UITEXTURES_TREE );
		res.pItemsTree->SetRelation( "IconDisabledTexture", IDC_UITEXTURES_TREE );
		res.pItemsTree->SetRelation( "FlagTexture", IDC_UITEXTURES_TREE );
		res.pItemsTree->SetRelation( "ToolTip", IDC_STRINGS_TREE );
		for ( int i = 1; i < 7; ++i )
		{
			string szField = string( "MaleCustomHead" ) + IToA(i);
			res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, szField.c_str() );
			res.pItemsTree->SetRelation( szField.c_str(), IDC_COMPLEXHEADS );
		}
		for ( int i = 1; i < 7; ++i )
		{
			string szField = string( "FemaleCustomHead" ) + IToA(i);
			res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, szField.c_str() );
			res.pItemsTree->SetRelation( szField.c_str(), IDC_COMPLEXHEADS );
		}
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}	
	{
		SResTree res;
		res.nTreeID    = IDC_HEADS_TREE;
		res.szTabName  = "Heads";
		res.szRootName = "Heads";
		res.pItemsTree = dbgnew CItemsMgr( IDC_HEADS_TREE, &itemsDB, &treeDB, "HeadsTree", "Heads" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcName" );
		for ( int i = 0; i < 8; ++i )
		{
			string szMat = "Material";
			szMat += IToA( i );
			res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, szMat.c_str() );
			res.pItemsTree->SetRelation( szMat.c_str(), IDC_MATERIALS_TREE );
		}
		res.pSelectCb = &SetEmptyView;
		res.pDoExport = &ExportHeads;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}	
	{
		SResTree res;
		res.nTreeID    = IDC_HEADSEQS_TREE;
		res.szTabName  = "HeadSeqs";
		res.szRootName = "HeadSeqs";
		res.pItemsTree = dbgnew CItemsMgr( IDC_HEADSEQS_TREE, &itemsDB, &treeDB, "HeadSeqsTree", "HeadSeqs" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcName" );
		res.pSelectCb  = &SetEmptyView;
		res.pDoExport  = &ExportHeadSeqs;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}	
	{
		SResTree res;
		res.nTreeID    = IDC_ANIMWEAPONTYPES_TREE;
		res.szTabName  = "AnimWeapon";
		res.szRootName = "AnimWeaponTypes";
		res.pItemsTree = dbgnew CItemsMgr( IDC_ANIMWEAPONTYPES_TREE, &itemsDB, &treeDB, "AnimWeaponTypesTree", "AnimWeaponTypes" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CrawlX", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CrawlY", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CrawlZ", 0.3f );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CrouchX", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CrouchY", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "CrouchZ", 1.1f );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "StandX", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "StandY", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "StandZ", 1.5f );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "MinDistance" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "AnimationWeaponType", "Default" );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "AimedStrafe", false );
		res.pItemsTree->AddString( "AnimationWeaponType", "Default" );
		res.pItemsTree->AddString( "AnimationWeaponType", "Pistol" );
		res.pItemsTree->AddString( "AnimationWeaponType", "Rifle" );
		res.pItemsTree->AddString( "AnimationWeaponType", "SubMachineGun" );
		res.pItemsTree->AddString( "AnimationWeaponType", "Knife" );
		res.pItemsTree->AddString( "AnimationWeaponType", "MachineGun" );
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}	
	{
		SResTree res;
		res.nTreeID    = IDC_ACKINFOS;
		res.szTabName  = "AckInfo";
		res.szRootName = "AckInfo";
		res.pItemsTree = dbgnew CItemsMgr( IDC_ACKINFOS, &itemsDB, &treeDB, "AckInfosTree", "AckInfos" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "WhoID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "HeadSequenceID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "StringID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SoundID" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "IntonationID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SoundID1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SoundID2" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SoundID3" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SoundID4" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SoundID5" );
		res.pItemsTree->SetRelation( "WhoID", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "HeadSequenceID", IDC_HEADSEQS_TREE );
		res.pItemsTree->SetRelation( "StringID", IDC_STRINGS_TREE );
		res.pItemsTree->SetRelation( "SoundID", IDC_SOUNDS_TREE );
		res.pItemsTree->SetRelation( "SoundID1", IDC_SOUNDS_TREE );
		res.pItemsTree->SetRelation( "SoundID2", IDC_SOUNDS_TREE );
		res.pItemsTree->SetRelation( "SoundID3", IDC_SOUNDS_TREE );
		res.pItemsTree->SetRelation( "SoundID4", IDC_SOUNDS_TREE );
		res.pItemsTree->SetRelation( "SoundID5", IDC_SOUNDS_TREE );
		res.pSelectCb  = &SetEmptyView;
		res.pDoExport  = &ExportAckInfos;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}	
	{
		SResTree res;
		res.nTreeID    = IDC_LIGHTS_TREE;
		res.szTabName  = "Lights";
		res.szRootName = "Lights";
		res.pItemsTree = dbgnew CItemsMgr( IDC_LIGHTS_TREE, &itemsDB, &treeDB, "LightsTree", "Lights" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcName" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "SelectNode" );
		res.pSelectCb  = &SetEmptyView;
		res.pDoExport  = &ExportLights;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}	
  {
    SResTree res;
    res.nTreeID    = IDC_LIGHTINSTANCES_TREE;
    res.szTabName  = "LightInstances";
		res.szRootName = "LightInstances";
    res.pItemsTree = dbgnew CItemsMgr( IDC_LIGHTINSTANCES_TREE, &itemsDB, &treeDB, "LightInstancesTree", "LightInstances" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "EffectID", CVariant(), true );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Scale", 1.0f );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Speed", 1.0f );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "LightID" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PositionX" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PositionY" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PositionZ" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "RotationX" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "RotationY" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "RotationZ" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Offset", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "EndCycle", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "CycleCount", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "GlueToEffectBone", 0 );
		res.pItemsTree->SetRelation( "LightID", IDC_LIGHTS_TREE );
    res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
    theApp.resTreesHash[res.nTreeID] = res;
  }
	{
		SResTree res;
		res.nTreeID    = IDC_MUSIC_TREE;
		res.szTabName  = "Music";
		res.szRootName = "Music";
		res.pItemsTree = dbgnew CItemsMgr( IDC_MUSIC_TREE, &itemsDB, &treeDB, "MusicTree", "Musics" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcName" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "Type", "Ambient" );
		res.pItemsTree->AddString( "Type", "Ambient" );
		res.pItemsTree->AddString( "Type", "PreCombat" );
		res.pItemsTree->AddString( "Type", "Combat" );
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_COMPLEXHEADS;
		res.szTabName  = "ComplexHeads";
		res.szRootName = "ComplexHeads";
		res.pItemsTree = dbgnew CItemsMgr( IDC_COMPLEXHEADS, &itemsDB, &treeDB, "ComplexHeadsTree", "ComplexHeads" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "HeadID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Hair" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "HairInCap" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Mesh0" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Mesh1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Mesh2" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Mesh3" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "IFMesh0" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "IFMesh1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "IFMesh2" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "IFMesh3" );
		res.pItemsTree->SetRelation( "HeadID", IDC_HEADS_TREE );
		res.pItemsTree->SetRelation( "Hair", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "HairInCap", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "Mesh0", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "Mesh1", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "Mesh2", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "Mesh3", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "IFMesh0", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "IFMesh1", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "IFMesh2", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "IFMesh3", IDC_MODELS_TREE );
		res.pSelectCb = &SetHeadView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_RPGWEAPONTYPES_TREE;
		res.szTabName  = "WeaponType";
		res.szRootName = "WeaponType";
		res.pItemsTree = dbgnew CItemsMgr( IDC_RPGWEAPONTYPES_TREE, &itemsDB, &treeDB, "RPGWeaponTypesTree", "RPGWeaponTypes" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "NameID" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "StoreType", "Other" );
		res.pItemsTree->SetRelation( "NameID", IDC_STRINGS_TREE );
		res.pItemsTree->AddString( "StoreType", "Pistol" );
		res.pItemsTree->AddString( "StoreType", "Rifle" );
		res.pItemsTree->AddString( "StoreType", "SubMachineGun" );
		res.pItemsTree->AddString( "StoreType", "HeavyWeapon" );
		res.pItemsTree->AddString( "StoreType", "ColdSteel" );
		res.pItemsTree->AddString( "StoreType", "Grenade" );
		res.pItemsTree->AddString( "StoreType", "Other" );
		res.pItemsTree->AddString( "StoreType", "PKWeapon" );
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_RPGAMMOTYPES_TREE;
		res.szTabName  = "AmmoType";
		res.szRootName = "AmmoType";
		res.pItemsTree = dbgnew CItemsMgr( IDC_RPGAMMOTYPES_TREE, &itemsDB, &treeDB, "RPGAmmosTree", "RPGAmmos" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "NameID" );
		res.pItemsTree->SetRelation( "NameID", IDC_STRINGS_TREE );
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_SCRIPTS_TREE;
		res.szTabName  = "Script";
		res.szRootName = "Scripts";
		res.pItemsTree = dbgnew CItemsMgr( IDC_SCRIPTS_TREE, &itemsDB, &treeDB, "ScriptsTree", "Scripts" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "CodeText" );
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_UNIFORMS_TREE;
		res.szTabName  = "Uniforms";
		res.szRootName = "Uniforms";
		res.pItemsTree = dbgnew CItemsMgr( IDC_UNIFORMS_TREE, &itemsDB, &treeDB, "RPGUniformsTree", "RPGUniforms" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "CapModelID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "BackpackModelID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ModelBeltL1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ModelBeltR1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ModelBeltM1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ModelBeltMediumL1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ModelBeltMediumR1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ModelBeltMediumL2" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ModelBeltMediumR2" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ModelWaistBeltL1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ModelWaistBeltR1" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "STBeltL1" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "STBeltR1" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "STBeltM1" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "STBeltMediumL1" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "STBeltMediumR1" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "STBeltMediumL2" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "STBeltMediumR2" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "STWaistBeltL1" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "STWaistBeltR1" );
		res.pItemsTree->SetRelation( "CapModelID", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "BackpackModelID", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "ModelBeltL1", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "ModelBeltR1", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "ModelBeltM1", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "ModelBeltMediumL1", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "ModelBeltMediumR1", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "ModelBeltMediumL2", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "ModelBeltMediumR2", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "ModelWaistBeltL1", IDC_MODELS_TREE );
		res.pItemsTree->SetRelation( "ModelWaistBeltR1", IDC_MODELS_TREE );
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_RPG_CLIPS_TREE;
		res.szTabName  = "Clips";
		res.szRootName = "Clips";
		res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_CLIPS_TREE, &itemsDB, &treeDB, "RPGClipsTree", "RPGClips" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Quantity", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "AmmoGroup", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ItemID" );
		res.pItemsTree->SetRelation( "ItemID", IDC_RPG_ITEMS_TREE );
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_RPG_FIRSTAID_TREE;
		res.szTabName  = "First aid";
		res.szRootName = "First Aid";
		res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_FIRSTAID_TREE, &itemsDB, &treeDB, "RPGFirstAidsTree", "RPGFirstAids" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ItemID" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "Effect", "Normal" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "RequiredSkill", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SkillModifier", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Duration", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Power", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "TotalHealVP", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "APToUse", 0 );
		res.pItemsTree->SetRelation( "ItemID", IDC_RPG_ITEMS_TREE );
		res.pItemsTree->AddString( "Effect", "Normal" );
		res.pItemsTree->AddString( "Effect", "CriticalOnly" );
		res.pItemsTree->AddString( "Effect", "TempRemovePenalties" );
		res.pItemsTree->AddString( "Effect", "BoostVP" );
		res.pItemsTree->AddString( "Effect", "TempStopBleeding" );
		res.pItemsTree->AddString( "Effect", "RemoveBleeding" );
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_RPG_MINEDETECTORS_TREE;
		res.szTabName  = "MineDetectors";
		res.szRootName = "MineDetectors";
		res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_MINEDETECTORS_TREE, &itemsDB, &treeDB, "RPGMineDetectorsTree", "RPGMineDetectors" );
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = true;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_RPG_MELEEWEAPONS_TREE;
		res.szTabName  = "MeleeWeapons";
		res.szRootName = "MeleeWeapons";
		res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_MELEEWEAPONS_TREE, &itemsDB, &treeDB, "RPGMeleeWeaponsTree", "RPGMeleeWeapons" );
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = true;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_SPOTS_TREE;
		res.szTabName  = "Spots";
		res.szRootName = "Spots";
		res.pItemsTree = dbgnew CItemsMgr( IDC_SPOTS_TREE, &itemsDB, &treeDB, "SpotsTree", "Spots" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "MaterialID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "RPGArmorID" );
		res.pItemsTree->SetRelation( "MaterialID", IDC_MATERIALS_TREE );
		res.pItemsTree->SetRelation( "RPGArmorID", IDC_RPG_ARMORS_TREE );
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_SCENARIOS_TREE;
		res.szTabName  = "Scenario";
		res.szRootName = "Scenario";
		res.pItemsTree = dbgnew CItemsMgr( IDC_SCENARIOS_TREE, &itemsDB, &treeDB, "ScenariosTree", "Scenarios" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "Name" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "Description" );
		//res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcName" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcXLSName" );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "FullGraph" );
		res.pSelectCb  = &SetScenarioView;
		res.pDoExport  = &ReadScenarioXLS;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_GLOBALMAPS_TREE;
		res.szTabName  = "GlobalMaps";
		res.szRootName = "Global Maps";
		res.pItemsTree = dbgnew CItemsMgr( IDC_GLOBALMAPS_TREE, &itemsDB, &treeDB, "GlobalMapsTree", "GlobalMaps" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "BaseZoneID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Background" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Scenario" );
		res.pItemsTree->SetRelation( "BaseZoneID", IDC_SCENARIO_ZONES );
		res.pItemsTree->SetRelation( "Background", IDC_UITEXTURES_TREE );
		res.pItemsTree->SetRelation( "Scenario", IDC_SCENARIOS_TREE );

		res.pSelectCb  = &SetGlobalMapView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_SCENARIO_ZONES;
		res.szTabName  = "ScenarioZones";
		res.szRootName = "Scenario Zones";
		res.pItemsTree = dbgnew CItemsMgr( IDC_SCENARIO_ZONES, &itemsDB, &treeDB, "ScenarioZonesTree", "ScenarioZones" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "SmallDescription" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "Description" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ItemSlots" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "PersonSlots" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Scenario" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "TemplateID1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "TemplateID2" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "TemplateID3" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "CluesMaxNumber" );
		res.pItemsTree->SetRelation( "Scenario", IDC_SCENARIOS_TREE );
		res.pItemsTree->SetRelation( "TemplateID1", IDC_TEMPLATE_TREE );
		res.pItemsTree->SetRelation( "TemplateID2", IDC_TEMPLATE_TREE );
		res.pItemsTree->SetRelation( "TemplateID3", IDC_TEMPLATE_TREE );
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_PASSAGE_OBJECTS_TREE;
		res.szTabName  = "PassageObj";
		res.szRootName = "Passage Objects";
		res.pItemsTree = dbgnew CItemsMgr( IDC_PASSAGE_OBJECTS_TREE, &itemsDB, &treeDB, "PassageObjectsTree", "PassageObjects" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ObjectID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "UseSoundID" );
		res.pItemsTree->SetRelation( "ObjectID", IDC_OBJECTS_TREE );
		res.pItemsTree->SetRelation( "UseSoundID", IDC_SOUNDS_TREE );
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_PANZERKLEINS_TREE;
		res.szTabName  = "Panzerkleins";
		res.szRootName = "Panzerkleins";
		res.pItemsTree = dbgnew CItemsMgr( IDC_PANZERKLEINS_TREE, &itemsDB, &treeDB, "PanzerkleinsTree", "Panzerkleins" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "PersID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "UnitSkeleton" );
		res.pItemsTree->SetRelation( "PersID", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "UnitSkeleton", IDC_SKELETONS_TREE );

		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}	
	{
		SResTree res;
		res.nTreeID    = IDC_CAMERAS_TREE;
		res.szTabName  = "Cameras";
		res.szRootName = "Cameras";
		res.pItemsTree = dbgnew CItemsMgr( IDC_CAMERAS_TREE, &itemsDB, &treeDB, "CamerasTree", "Cameras" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "AnchorX" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "AnchorY" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "AnchorZ" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Yaw" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Pitch" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Distance" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Roll", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FOV", 35 );

		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = true;
		theApp.resTreesHash[res.nTreeID] = res;
	}	
	{
		SResTree res;
		res.nTreeID    = IDC_UNITGROUPS_TREE;
		res.szTabName  = "UnitGroups";
		res.szRootName = "Unit groups";
		res.pItemsTree = dbgnew CItemsMgr( IDC_UNITGROUPS_TREE, &itemsDB, &treeDB, "UnitGroupsTree", "UnitGroups" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "Formation" );

		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = true;
		theApp.resTreesHash[res.nTreeID] = res;
	}	
	{
		SResTree res;
		res.nTreeID    = IDC_SOUNDINSTANCES_TREE;
		res.szTabName  = "Sound Instances";
		res.szRootName = "Sound Instances";
		res.pItemsTree = dbgnew CItemsMgr( IDC_SOUNDINSTANCES_TREE, &itemsDB, &treeDB, "SoundInstancesTree", "SoundInstances" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SoundID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "StartTime", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "CycleCount", 1 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Volume", 255 );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "FadeIn", false );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "FadeOut", false );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "FadeSamples", 0 );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_COMBO, "SoundType", "Permanent" );
		res.pItemsTree->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SoundAvgInterval", 10 );
		res.pItemsTree->SetRelation( "SoundID", IDC_RNDSOUNDS_TREE );
		res.pItemsTree->AddString( "SoundType", "Permanent" );
		res.pItemsTree->AddString( "SoundType", "Random" );
		res.pItemsTree->AddString( "SoundType", "Realtime" );
		res.pItemsTree->AddString( "SoundType", "Wind" );

		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = true;
		theApp.resTreesHash[res.nTreeID] = res;
	}	
	{
		SResTree res;
		res.nTreeID    = IDC_SOUNDEFFECTS_TREE;
		res.szTabName  = "Sound Effects";
		res.szRootName = "Sound Effects";
		res.pItemsTree = dbgnew CItemsMgr( IDC_SOUNDEFFECTS_TREE, &itemsDB, &treeDB, "SoundEffectsTree", "SoundEffects" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_RELLIST, "SoundList" );
		res.pItemsTree->SetRelation( "SoundList", IDC_SOUNDINSTANCES_TREE );

		res.pSelectCb  = &SetSoundEffectView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}	
	{
		SResTree res;
		res.nTreeID    = IDC_SIDES_TREE;
		res.szTabName  = "Sides";
		res.szRootName = "Sides";
		res.pItemsTree = dbgnew CItemsMgr( IDC_SIDES_TREE, &itemsDB, &treeDB, "SidesTree", "Sides" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Nationality1" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Nationality2" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Nationality3" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "StringID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "GlobalMap" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "MaleMedic" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "MaleScout" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "MaleSniper" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "MaleSoldier" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "MaleEngineer" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "MaleGrenadier" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "FemaleMedic" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "FemaleScout" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "FemaleSniper" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "FemaleSoldier" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "FemaleEngineer" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "FemaleGrenadier" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Nationality1Male" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Nationality1Female" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Nationality2Male" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Nationality2Female" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Nationality3Male" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Nationality3Female" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "HeroSelectTemplate" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ESCMenuBackground" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "UIBaseFlag" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "UIBaseFlagActive" );
		res.pItemsTree->SetRelation( "Nationality1", IDC_NATIONALITY_TREE );
		res.pItemsTree->SetRelation( "Nationality2", IDC_NATIONALITY_TREE );
		res.pItemsTree->SetRelation( "Nationality3", IDC_NATIONALITY_TREE );
		res.pItemsTree->SetRelation( "StringID", IDC_STRINGS_TREE );
		res.pItemsTree->SetRelation( "GlobalMap", IDC_GLOBALMAPS_TREE );
		res.pItemsTree->SetRelation( "UIBaseFlag", IDC_UITEXTURES_TREE );
		res.pItemsTree->SetRelation( "UIBaseFlagActive", IDC_UITEXTURES_TREE );
		res.pItemsTree->SetRelation( "ESCMenuBackground", IDC_UITEXTURES_TREE );

		res.pItemsTree->SetRelation( "MaleMedic", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "MaleScout", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "MaleSniper", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "MaleSoldier", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "MaleEngineer", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "MaleGrenadier", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "FemaleMedic", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "FemaleScout", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "FemaleSniper", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "FemaleSoldier", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "FemaleEngineer", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "FemaleGrenadier", IDC_RPG_PERS_TREE );

		res.pItemsTree->SetRelation( "Nationality1Male", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "Nationality1Female", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "Nationality2Male", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "Nationality2Female", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "Nationality3Male", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "Nationality3Female", IDC_RPG_PERS_TREE );
		res.pItemsTree->SetRelation( "HeroSelectTemplate", IDC_TEMPLATE_TREE );

		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}	
	{
		SResTree res;
		res.nTreeID    = IDC_RPG_STOREITEMS_TREE;
		res.szTabName  = "Store Items";
		res.szRootName = "Store Items";
		res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_STOREITEMS_TREE, &itemsDB, &treeDB, "RPGStoreItemsTree", "RPGStoreItems" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Rating" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ItemID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "SideID" );
		res.pItemsTree->SetRelation( "ItemID", IDC_RPG_ITEMS_TREE );
		res.pItemsTree->SetRelation( "SideID", IDC_SIDES_TREE );

		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}	
	{
		SResTree res;
		res.nTreeID    = IDC_DIPLOMACY_TREE;
		res.szTabName  = "Diplomacies";
		res.szRootName = "Diplomacies";
		res.pItemsTree = dbgnew CItemsMgr( IDC_DIPLOMACY_TREE, &itemsDB, &treeDB, "DiplomaciesTree", "Diplomacies" );
		for ( int i = 1; i <= 16; ++i )
			res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, (string( "Diplomacy" ) + IToA( i )).c_str(), CVariant(), true );

		res.pSelectCb  = &SetDiplomacyView;
		res.bHideTree  = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_DIALOGS_TREE;
		res.szTabName  = "Dialogs";
		res.szRootName = "Dialogs";
		res.pItemsTree = dbgnew CItemsMgr( IDC_DIALOGS_TREE, &itemsDB, &treeDB, "DialogsTree", "Dialogs" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "Description" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "Code" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_BROWSE, "SrcName" );

		res.pSelectCb = &SetEmptyView;
		res.bHideTree = bHideResources;
		res.pDoExport = ExportDialogs;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_DIALOG_PERS_TREE;
		res.szTabName  = "DialogPers";
		res.szRootName = "DialogPers";
		res.pItemsTree = dbgnew CItemsMgr( IDC_DIALOG_PERS_TREE, &itemsDB, &treeDB, "DialogPersTree", "DialogPers" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "Code" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "PersID" );
		res.pItemsTree->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Hero" );
		res.pItemsTree->SetRelation( "PersID", IDC_RPG_PERS_TREE );

		res.pSelectCb = &SetEmptyView;
		res.bHideTree = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_RPG_PERKS_TREE;
		res.szTabName  = "Perks";
		res.szRootName = "Perks";
		res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_PERKS_TREE, &itemsDB, &treeDB, "RPGPerksTree", "RPGPerks" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ToolTip" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Icon" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "IconDisabled" );
		res.pItemsTree->AddProperty( CVariant::VT_STR, DT_STR, "IDText" );
		res.pItemsTree->SetRelation( "ToolTip", IDC_STRINGS_TREE );
		res.pItemsTree->SetRelation( "Icon", IDC_UITEXTURES_TREE );
		res.pItemsTree->SetRelation( "IconDisabled", IDC_UITEXTURES_TREE );

		res.pSelectCb = &SetEmptyView;
		res.bHideTree = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_RPG_MINES_TREE;
		res.szTabName  = "Mines";
		res.szRootName = "Mines";
		res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_MINES_TREE, &itemsDB, &treeDB, "RPGMinesTree", "RPGMines" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ItemID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "APToSet", 30 );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Explosion" );
		res.pItemsTree->SetRelation( "ItemID", IDC_RPG_ITEMS_TREE );
		res.pItemsTree->SetRelation( "Explosion", IDC_RPG_GRENADES_TREE );

		res.pSelectCb = &SetEmptyView;
		res.bHideTree = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_RPG_TOOLS_TREE;
		res.szTabName  = "Tools";
		res.szRootName = "Tools";
		res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_TOOLS_TREE, &itemsDB, &treeDB, "RPGToolsTree", "RPGTools" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ItemID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "Effect", 0 );
		res.pItemsTree->SetRelation( "ItemID", IDC_RPG_ITEMS_TREE );
		
		res.pSelectCb = &SetEmptyView;
		res.bHideTree = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	{
		SResTree res;
		res.nTreeID    = IDC_RPG_KEYS_TREE;
		res.szTabName  = "Keys";
		res.szRootName = "Keys";
		res.pItemsTree = dbgnew CItemsMgr( IDC_RPG_KEYS_TREE, &itemsDB, &treeDB, "RPGKeysTree", "RPGKeys" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "ItemID" );
		res.pItemsTree->AddProperty( CVariant::VT_INT, DT_DEC, "KeyID", 0 );
		res.pItemsTree->SetRelation( "ItemID", IDC_RPG_ITEMS_TREE );

		res.pSelectCb = &SetEmptyView;
		res.bHideTree = bHideResources;
		theApp.resTreesHash[res.nTreeID] = res;
	}
	// загрузка
	for ( unordered_map<int, SResTree>::iterator i = theApp.resTreesHash.begin(); i != theApp.resTreesHash.end(); ++i )
		i->second.pTreeDlg = new CTreeSelItemDlg( vector<SResTree>( 1, i->second ) );
	// комбинированные ресурсы
	{
		SResTree res;
		res.nTreeID    = IDC_PLACABLE_TREE;
		res.szTabName  = "Objects";
		res.szRootName = "Items";
		res.pItemsTree = 0;
		res.pSelectCb  = &SetEmptyView;
		res.bHideTree  = true;
		vector<SResTree> trees;
		trees.push_back( *theApp.GetResTree( IDC_OBJECTS_TREE ) );
		trees.push_back( *theApp.GetResTree( IDC_RPG_ITEMS_TREE ) );
		res.pTreeDlg = new CPlacableItemsDlg( trees );
		theApp.resTreesHash[res.nTreeID] = res;
	}
	//
	SetupObjects();
	//
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ReleaseResources()
{
	const unordered_map<int, SResTree> &resTreesHash = theApp.GetResTrees();
	for ( unordered_map<int, SResTree>::const_iterator i = resTreesHash.begin(); i != resTreesHash.end(); ++i )
	{
		if ( i->second.pTreeDlg )
			delete i->second.pTreeDlg;
		if ( i->second.pItemsTree )
			delete i->second.pItemsTree;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
