#include "StdAfx.h"
#include "RectsDBCmd.h"
#include "Variant.h"
#include "DbInl.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"

namespace NDb
{
	extern void ExportRectCoords( CVec2 *pptCenter, float fWidth, float fHeight, float fRotation );
}
extern HRESULT InitDB( CSession &session, CDBPropSet &propset );
extern string IToA( int n );
extern SDBConnection dbConnection;
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRectPosDB::SetPos( int nID, const CVec2 &ptPos, float fDZ, int nFloor, float fRotation )
{
	if ( !HasConnection() )
		SetConnection( &dbConnection );
	string szQuery = "SELECT * FROM Rects WHERE ID=";
	szQuery += IToA( nID );
	if ( FAILED( Open( szQuery ) ) || MoveNext() != S_OK )
		return false;
	CVec2 pt = ptPos;
	NDb::ExportRectCoords( &pt, m_Width, m_Height, fRotation );
	m_CenterX = pt.x;
	m_CenterY = pt.y;
	m_fDZ = fDZ;
	m_Floor = nFloor;
	m_Rotation = fRotation;
  HRESULT hr = SetData( 2 );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRectPosDB::Delete( int nID )
{
	if ( !HasConnection() )
		SetConnection( &dbConnection );
	string szQuery = "SELECT * FROM Rects WHERE ID=";
	szQuery += IToA( nID );
	if ( FAILED( Open( szQuery ) ) || MoveNext() != S_OK )
		return false;
	HRESULT hr = CBaseDBCmd<CAccessor<CRectsDBCmdAccessor> >::Delete();
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRectPosDB::Insert( int nVariantID, int nTemplateID )
{
	if ( !HasConnection() )
		SetConnection( &dbConnection );
	NDb::CTemplate *pTemplate = NDb::GetTemplate( nTemplateID );
	if ( !pTemplate )
		return -1;
	string szQuery = "SELECT * FROM Rects WHERE ID=-1";
  if ( FAILED( Open( szQuery ) ) )
		return -1;
	m_VariantID = nVariantID;
	m_TemplateLink = nTemplateID;
	m_Width = pTemplate->nWidth;
	m_Height = pTemplate->nHeight;
	HRESULT hr = CBaseDBCmd<CAccessor<CRectsDBCmdAccessor> >::Insert( 1 );
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  }
	hr = MoveNext();
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  }
	return m_RectID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
