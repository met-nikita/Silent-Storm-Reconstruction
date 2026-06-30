#ifndef __USERSETTINGS_SETUP_H_
#define __USERSETTINGS_SETUP_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Main\Camera.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{
	struct SRawMaterialApply;
}
enum EEditMode;
enum EMoveMode;
enum EBrushType;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EMessage
{
	MSG_SELECT,
	MSG_EDIT,
	MSG_UPDATE_OBJECTS,
	MSG_UPDATE_LAYERLIST,
};
struct SMessage
{
	EMessage msg;
	EBrushType brush;
	DWORD data;
};
enum EMaterialSet
{
	MSET_FIRST = 0,
	MSET_SECOND,
	MSET_THIRD,
	MSET_FOURTH,
	MSET_FIFTH,
	MSET_SIXTH,
	MSET_SIZE,
};
string GetMSetString( EMaterialSet set, int nIndex );
bool GetMSetID( const string &str, EMaterialSet *pSet, int *pnIndex );
////////////////////////////////////////////////////////////////////////////////////////////////////
class IUserSettingsSetup
{
public:
	virtual void SetMode( EEditMode mode ) = 0;
	virtual void PushMode( EEditMode mode ) = 0;
	virtual void PopMode() = 0;
	virtual void SetMoveMode( EMoveMode mode ) = 0;
	virtual void SetActiveLayerID( int nID ) = 0;
	virtual void SetActiveFloor( float fFloor ) = 0;
	virtual void SetActiveRotationID( int nID ) = 0;
	virtual void SetSelectedBrushID( int nLayerID, int nBrushID ) = 0;
	virtual void SetMaterial( EMaterialSet set, const vector<NBuilding::SRawMaterialApply> &materials ) = 0; // sizeof materials = 1 or N_MATERIALSET_SIZE
	virtual void SetActiveMaterialSet( EMaterialSet set ) = 0;
	virtual EMaterialSet GetActiveMaterialSet() = 0;
	virtual void GetMaterial( EMaterialSet set, vector<NBuilding::SRawMaterialApply> *pMaterials ) const = 0;
	virtual void SetCanEditAnyLayer( bool bCan ) = 0;
	virtual void SetVisibleLayers( const vector<int> &layers ) = 0;
	virtual void SetSpotMaterial( int nMaterialID ) = 0;
	virtual void SendMessage( SMessage msg ) = 0;
	virtual void SetCameraInfo( const ICamera::SCameraPos &pos, float fFOV ) = 0;
	virtual void SetDBCameraInfo( const ICamera::SCameraPos &pos, float fFOV ) = 0;
	virtual void SetBrushSize( int nSize ) = 0;
	virtual void SetGridVisible( bool bVisible ) = 0;
	virtual void SetSubTemplateDepth( int nDepth ) = 0;
	virtual void SetWireSubTemplates( bool bWire ) = 0;
	virtual void SetParam( int nParamID, float fValue ) = 0;
	virtual void SetSelectionCenter( const CVec3 &ptCenter ) = 0;
	virtual void ShowPropertyBrowser( const int nObjectID ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IUserSettingsSetup& GetUserSettingsSetup();
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __USERSETTINGS_SETUP_H_
