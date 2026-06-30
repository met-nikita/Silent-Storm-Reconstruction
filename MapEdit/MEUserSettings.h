#ifndef __MEUSERSETTINGS_H_
#define __MEUSERSETTINGS_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{
	struct SRawMaterialApply;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EEditMode
{
	EM_SELECT,
	EM_GEOMETRY,
	EM_MATERIAL,
	EM_RECTANGULAR_SELECTION,
	EM_ZOOM,
	EM_PAN,
	EM_FILL,
	EM_ERASE,
	EM_SELECTMATERIAL,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EMoveMode
{
	MM_XY,
	MM_XZ,
	MM_YZ,
	MM_X,
	MM_Y,
	MM_Z,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_MATERIALSET_SIZE = 4;
class CItemsMgr;
////////////////////////////////////////////////////////////////////////////////////////////////////
class IUserSettings
{
public:
	virtual EEditMode GetMode() const = 0;
	virtual EMoveMode GetMoveMode() const = 0;
	virtual int   GetActiveLayerID() const = 0;
	virtual float GetActiveFloor() const = 0;
	virtual int  GetActiveRotationID() const = 0;
	virtual int  GetSelectedBrushID( int nLayerID ) const = 0;
	virtual bool GetActiveMaterial( vector<NBuilding::SRawMaterialApply> *pMaterials ) const = 0; // sizeof materials = 1 or N_MATERIALSET_SIZE
	virtual bool CanEditAnyLayer() const = 0;
	virtual void GetVisibleLayers( vector<int> *pLayers ) const = 0;
	virtual int  GetSpotMaterialID() const = 0;
	virtual bool IsCameraReset() const = 0;
	virtual CItemsMgr* GetResourceManager( int nResourceTypeID ) const = 0;
	virtual int  GetBrushSize() const = 0;
	virtual bool IsGridVisible() const = 0;
	virtual int  GetSubTemplateDepth() const = 0;
	virtual bool IsWireSubTemplate() const = 0;
	virtual float GetParam( int nParamID ) const = 0;
	virtual CVec3 GetSelectionCenter() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const IUserSettings& GetUserSettings();
bool IsLayerVisible( int nLayerID );
string GetMEParamName( int nParamID );
float GetMEParamDefValue( int nParamID );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __MEUSERSETTINGS_H_