#ifndef __IWYSIWYG_H_
#define __IWYSIWYG_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Main\iMain.h"
#include "..\Main\Camera.h"
#include "..\Main\BuildingInfo.h"
namespace NAI
{
	class IAIMap;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EBrushType 
{ 
	BT_GEOMETRY = 0x1, 
	BT_SUBTEMPLATE = 0x2,
	BT_TEXSPOT  = 0x4, 
	BT_WAYPOINT = 0x8,
	BT_OBJECT   = 0x10,
	BT_WALLSPOT = 0x20,
	BT_LADDER   = 0x40,
	BT_TERRAIN  = 0x80,
	BT_MATERIAL = 0x100,
	BT_UNIT = 0x200,
	BT_TERRHOLE = 0x400,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
const int N_FOV = 35;
int MakeUserID( EBrushType nBrush, int nFragmentID );
void GetFragmentID( int nUserID, EBrushType *pnBrush, int *pnFragmentID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICWysiwyg: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICWysiwyg);
	int nPlacementID;
	ICamera::SCameraPos cameraPos;
	HWND hWnd;
	bool bPasteClipboard;
public:
	CICWysiwyg() {}
	CICWysiwyg( HWND hWnd, int nPlacementID, const ICamera::SCameraPos &_cameraPos, bool bPasteClipboard = false );
	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __IWYSIWYG_H_
