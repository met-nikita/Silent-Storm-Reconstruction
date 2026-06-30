// Variant.h: interface for the CVariant class.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_VARIANT_H__77531325_97A6_47DC_B434_9D8D9E3ECD1F__INCLUDED_)
#define AFX_VARIANT_H__77531325_97A6_47DC_B434_9D8D9E3ECD1F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CVariant
{
public:
	enum EVarialeType
	{
		VT_NULL = 0,
		VT_INT = 0x1,
		VT_FLOAT = 0x2,
		VT_STR = 0x4,
		VT_BOOL = 0x8,
	};

private:
	EVarialeType	m_eType;
	mutable DWORD	m_flagsOptimized;
	mutable float			m_floatVal;
	mutable int				m_intVal;
	mutable string			m_strVal;

	void OptimizeInt() const;
	void OptimizeFloat() const;
	void OptimizeStr() const;
	void OptimizeBool() const;

	// Optimeze flags
	bool HasFlag( int nFlag ) const { return ( m_flagsOptimized & nFlag ) != 0; }
	void AddFlag( int nFlag ) const { m_flagsOptimized |= nFlag; }
	void SetFlag( int nFlag, bool bSet ) { if ( bSet ) m_flagsOptimized |= nFlag; else m_flagsOptimized &= ~nFlag; }
	void ClearFlag() { m_flagsOptimized = 0; }

public:
	CVariant() : m_eType(VT_NULL), m_flagsOptimized(0) {}

	EVarialeType GetType()	const { return m_eType; }
	operator bool()			const { OptimizeInt();	return (m_intVal!=0);	}
	operator int()			const { OptimizeInt();	return m_intVal;		}
	operator float()		const { OptimizeFloat();	return m_floatVal;		}
	operator double()		const { OptimizeFloat();	return m_floatVal;		}
	operator const char *()	const { OptimizeStr();	return m_strVal.c_str();}
	//operator string()		const { OptimizeStr();	return m_strVal;		}
  bool operator != ( const CVariant &var ) const;

	CVariant( const CVariant &Var ) { *this = Var; }
	CVariant( const bool bVal )		{ m_intVal = bVal;			m_flagsOptimized = m_eType = VT_BOOL;	}
	CVariant( const int nVal )		{ m_intVal = nVal;			m_flagsOptimized = m_eType = VT_INT;	}
	CVariant( const float fVal )	{ m_floatVal = fVal;		m_flagsOptimized = m_eType = VT_FLOAT;	}
	CVariant( const double fVal )	{ m_floatVal = float(fVal);m_flagsOptimized = m_eType = VT_FLOAT;	}
	CVariant( const char *pVal )	{ m_strVal = pVal;			m_flagsOptimized = m_eType = VT_STR;	}
	CVariant( const string strVal )	{ m_strVal = strVal;		m_flagsOptimized = m_eType = VT_STR;	}
  
  void SetNewValue( const string strVal );
};

#endif // !defined(AFX_VARIANT_H__77531325_97A6_47DC_B434_9D8D9E3ECD1F__INCLUDED_)
