// Variant.cpp: implementation of the CVariant class.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Variant.h"
#include <algorithm>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


void CVariant::OptimizeInt() const
{
	if ( !HasFlag(VT_INT) )
	{
		switch(m_eType)
		{
			case VT_FLOAT:
				m_intVal = int(m_floatVal);
				break;
			case VT_STR:
				m_intVal = atoi(m_strVal.c_str());
				break;
			case VT_NULL:
				m_intVal = 0;
				break;
		}
		AddFlag(VT_INT);
	}
}

void CVariant::OptimizeFloat() const
{
	if ( !HasFlag(VT_FLOAT) )
	{
		switch(m_eType)
		{
			case VT_BOOL:
			case VT_INT:
				m_floatVal = float(m_intVal);
				break;
			case VT_STR:
				m_floatVal = atof(m_strVal.c_str());
				break;
			case VT_NULL:
				m_floatVal = 0;
				break;
		}
		AddFlag(VT_FLOAT);
	}
}

void CVariant::OptimizeStr() const
{
	if ( !HasFlag(VT_STR) )
	{
		CString szTemp;
		switch(m_eType)
		{
			case VT_BOOL:
			case VT_INT:
				szTemp.Format("%i", m_intVal );
				m_strVal = szTemp;
				break;
			case VT_FLOAT:
				szTemp.Format("%g", m_floatVal );
				m_strVal = szTemp;
				break;
		}
		AddFlag(VT_STR);
	}
}

void CVariant::OptimizeBool() const
{
	if ( !HasFlag(VT_BOOL) )
	{
		switch(m_eType)
		{
			case VT_STR:
				{
					string str;
					std::transform( m_strVal.begin(), m_strVal.end(), str.begin(), tolower );
					m_intVal = ( str == "yes" || str == "true" );
				}
				break;
			default:
				OptimizeInt();
				break;
		}
		AddFlag(VT_BOOL);
	}
}

bool CVariant::operator != ( const CVariant &var ) const
{
  if ( m_eType != var.m_eType )
    return true;
  switch( m_eType )
  {
  case VT_BOOL:
  case VT_INT:
    return m_intVal != var.m_intVal;
  case VT_FLOAT:
    return fabs(m_floatVal - var.m_floatVal) > FP_EPSILON;
  case VT_STR:
    return m_strVal != var.m_strVal;
  case VT_NULL:
    return false;
  }
  return true;
}

void CVariant::SetNewValue( const string strVal )
{
  m_flagsOptimized = m_eType;

	if ( VT_NULL == m_eType )
	{
		*this = strVal;
		return;
	}
  switch( m_eType )
  {
  case VT_BOOL:
  case VT_INT:
    m_intVal = atoi( strVal.c_str() );
    break;
  case VT_FLOAT:
    m_floatVal = atof( strVal.c_str() );
    break;
  case VT_STR:
    m_strVal = strVal;
    break;
  }
}