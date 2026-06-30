#ifndef __OBJECTINFO_H_
#define __OBJECTINFO_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GResource.h"
#include "MakeBuilding.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
	class CGeometryInfo;
	class CFileSkinPoints;
}
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjectInfoPieces
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SLoadVertex
{
	CVec3 pos;
	CVec3 normal;
	CVec2 tex;
	CVec3 texU, texV;
	DWORD dwColor;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFacesVector
{
	vector<SLoadVertex> points;
	SPolygonIndices polys;//indices;
	//vector<WORD> indices;
	//vector<WORD> polys;

	int operator&( CStructureSaver &f )
	{ 
		f.Add( 1, &points );
		f.Add( 2, &polys.indices );
		f.Add( 3, &polys.polys );
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// клипинфо для объектов, содержится инфрмация о видимых кусках
struct SClipShare
{
	SPartKey src;
	DWORD dwParts; // битовая маска видимых кусочков
	int nSubBlockID; // идекс сабкуска
	int nClip;
	
	SClipShare() {}
	SClipShare( const NBuilding::SClipInfo &info )
		:dwParts(info.dwParts), nSubBlockID(info.nSubBlockID), nClip(info.nClip) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool operator== ( const SClipShare &op1, const SClipShare &op2 )
{
	return 0 == memcmp( &op1, &op2, sizeof(SClipShare) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SClipInfoHash
{
	int operator()( const SClipShare &k ) const { return (k.dwParts << 16) ^ (k.src.nPart << 12) ^ k.src.nID; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
/*struct STriSort
{
	vector<WORD> tlist[NBuilding::ESIZE];
	
	STriSort();
	void Sort( const vector<SLoadVertex> &verts, const vector<STriangle> &tris );
	
private:
	enum EAlig
	{
		AYX,
		AYZ,
		AZX,
		ANONE
	};
	const vector<STriangle> *pTris;  // временные переменные используемые во время сортировки
	const vector<SLoadVertex> *pVerts;
	EAlig GetAlignment( const STriangle &tri, const vector<SLoadVertex> &verts );
	void  CheckPlanes( int testTri, int coord, vector<WORD> &minPlane, vector<WORD> &maxPlane );
};*/
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::unordered_map<int, SFacesVector > CPieceMap;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectInfoPieces : public CObjectBase
{
	OBJECT_BASIC_METHODS(CObjectInfoPieces);
public:
	CPieceMap faces;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjectInfoLoader
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectInfoLoader : public CLazyResourceLoader<SPartKey, CObjectInfo>
{
	OBJECT_BASIC_METHODS(CObjectInfoLoader);
	virtual CFileRequest* CreateRequest();
	virtual void RecalcValue( CFileRequest *p );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIGeometryConverter : public CPtrFuncBase<CObjectInfo>
{
	OBJECT_BASIC_METHODS(CAIGeometryConverter);
	ZDATA
	CDGPtr<CPtrFuncBase<NAI::CGeometryInfo> > pAIGeom;
	vector<int> parts;
	bool bTakeAll;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pAIGeom); f.Add(3,&parts); f.Add(4,&bTakeAll); return 0; }
protected:
	virtual void Recalc();
	virtual bool NeedUpdate() { return pAIGeom.Refresh(); }
public:
	CAIGeometryConverter() {}
	CAIGeometryConverter( int _nID, const vector<int> &_parts );
	void SetKey( int _nID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAISkinObjectConverter : public CPtrFuncBase<CObjectInfo>
{
	OBJECT_BASIC_METHODS(CAISkinObjectConverter);
	ZDATA
	CDGPtr<CPtrFuncBase<NAI::CFileSkinPoints> > pAIGeom;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pAIGeom); return 0; }
protected:
	virtual void Recalc();
	virtual bool NeedUpdate() { return pAIGeom.Refresh(); }
public:
	CAISkinObjectConverter() {}
	CAISkinObjectConverter( int _nID ) { SetKey(_nID); }
	void SetKey( int _nID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjectInfoPieceLoader
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectInfoPiecesLoader : public CLazyResourceLoader<SPartKey, CObjectInfoPieces>
{
	OBJECT_BASIC_METHODS(CObjectInfoPiecesLoader);
	virtual CFileRequest* CreateRequest();
	virtual void RecalcValue( CFileRequest *p );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWallObjectInfoClipper
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::unordered_map<int, int> CIntMap;

class CWallObjectInfoClipper : public CHoldedPtrFuncBase<CObjectInfo>
{
	OBJECT_BASIC_METHODS(CWallObjectInfoClipper);
	ZDATA
	SClipShare clipInfo;
	CDGPtr< CPtrFuncBase<CObjectInfoPieces> > pSrc;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&clipInfo); f.Add(3,&pSrc); return 0; }
	
	void ClipWall();
	bool GetClipPlane( SPlane *pPlane, short nClip, bool bLeft );
protected:
	virtual void Recalc();
public:
	void SetKey( const SClipShare &key );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSolidObjectInfoClipper
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSolidObjectInfoClipper : public CHoldedPtrFuncBase<CObjectInfo>
{
	OBJECT_BASIC_METHODS(CSolidObjectInfoClipper);
	ZDATA	
	SClipShare clipInfo;
	CDGPtr< CPtrFuncBase<CObjectInfoPieces> > pSrc;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&clipInfo); f.Add(3,&pSrc); return 0; }

	void ClipSolid();
protected:
	virtual void Recalc();
public:
	void SetKey( const SClipShare &key );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}	
#endif // __OBJECTINFO_H_