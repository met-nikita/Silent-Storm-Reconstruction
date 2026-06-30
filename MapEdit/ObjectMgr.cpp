#include "StdAfx.h"
#include "ObjectMgr.h"
#include "ObjectDB.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
static CObjectDB db;
int CObjectMgr::nSafePropID = 1111;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectProperty : public CProp
{
	OBJECT_BASIC_METHODS(CObjectProperty);
protected:
	CVariant value;
	string szTable;
	int nObjectID;
	CObj<COnSetValue> pOnSetValue;
	CObj<COnObjectSetup> pOnObjectSetup;

public:
	CObjectProperty() {}
	CObjectProperty( const string &szName, int nID, int nType, int nViewType, CVariant defValue = CVariant(), bool bReadOnly = true )
		: CProp( szName, nID, nType, nViewType, bReadOnly ), nObjectID(-1) {}

	const CVariant& GetValue() const { return value; }
	const CVariant GetDefValue() const { return value; }
	CProp* Clone() const
	{
		CObjectProperty *p = new CObjectProperty;
		*p = *this;
		p->pOnSetValue = pOnSetValue;
		return p;
	}
	void SetObject( const string &szTbl, int _nObjectID, const CVariant &var ) 
	{ 
		szTable = szTbl; 
		nObjectID = _nObjectID; 
		const_cast<CVariant&>( value ) = IsValid( pOnObjectSetup ) ? pOnObjectSetup->OnObjectSetup( nObjectID, GetName() ) : var;
	}
	void SetHandler( COnSetValue *pOnSetV, COnObjectSetup *pOnSetup ) { pOnSetValue = pOnSetV; pOnObjectSetup = pOnSetup; }
	void SetValue( const CVariant &v, bool bModified = true ) const 
	{
		CVariant val = v;
		if ( GetName() == "Floor" ) // у нас не может быть этажей больше чем 4
			 val = Min( 4, int(val) );
		CVariant old = value;
		const_cast<CVariant&>( value ) = val; 
		if ( !bModified )
			return;
		bool bSet = true;
		if ( IsValid( pOnSetValue ) )
			bSet = pOnSetValue->OnSetValue( nObjectID, GetName(), val );
		if ( bSet && szTable != "" )
			bSet = db.SetValue( GetType(), szTable, GetName(), nObjectID, val );
		if ( !bSet )
			const_cast<CVariant&>( value ) = old;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
/*
class CParamsProperty: public CObjectProperty
{
	OBJECT_BASIC_METHODS(CParamsProperty)
public:
	CParamsProperty() {}
	CParamsProperty( const string &szName, int nID, int nType, int nViewType, CVariant defValue = CVariant(), bool bReadOnly = true )
		: CObjectProperty( szName, nID, nType, nViewType, defValue, bReadOnly ) {}

	void SetValue( const CVariant &v, bool bModified = true ) const 
	{
		if ( !bModified )
		{
			const_cast<CVariant&>( value ) = v; 
			return;
		}
		string szFlags;

		CObjectProperty::SetValue( szFlags );
		const_cast<CVariant&>( value ) = v; 
	}
};
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectPropertyContainer: public CProp
{
	OBJECT_BASIC_METHODS(CObjectPropertyContainer);
private:
	CVariant value;
	vector<CObj<CProp> > objects;

public:
	CObjectPropertyContainer() {}
	CObjectPropertyContainer( const string &szName, int nID, int nType, int nViewType, CVariant defValue = CVariant(), bool bReadOnly = true )
		: CProp( szName, nID, nType, nViewType, bReadOnly ) {}

	const CVariant& GetValue() const { return value; }
	const CVariant GetDefValue() const { return value; }
	CProp* Clone() const
	{
		ASSERT(0);
		return 0;
		//CObjectPropertyContainer *p = new CObjectPropertyContainer;
		//*p = *this;
		//return p;
	}

	void SetValue( const CVariant &v, bool bModified = true ) const 
	{
		if ( /*v.GetType() == CVariant::VT_NULL ||*/ (v.GetType() == CVariant::VT_STR && string( v ) == "" ) )
			return;
		if ( objects.empty() )
		{
			const_cast<CVariant&>( value ) = v;
			return;
		}
		//
		bool bRet = true;
		for ( int i = 0; i < objects.size(); ++i )
		{
			objects[i]->SetValue( v, bModified );
			bRet = bRet && !(objects[i]->GetValue() != v);
		}
		if ( bRet )
			const_cast<CVariant&>( value ) = v;
	}

	void PushProperty( CProp *pProp )
	{
		if ( objects.empty() )
		{
			value = pProp->GetValue();
			const vector<string> &strings = pProp->GetStrings();
			for ( int i = 0; i < strings.size(); ++i )
				AddString( strings[i] );
		}
		//
		objects.push_back( pProp );
		if ( value != pProp->GetValue() )
			value = CVariant();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectMgr::CObjectMgr( const string &_szTable ): szTable(_szTable)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectMgr::AddProperty( int type, int viewType, const char *szPropName, COnSetValue *pOnSetVal, int nGroup, 
	const CVariant &defVal, int bReadOnly, COnObjectSetup *pOnObjSetup )
{
	CObjectProperty *pProp = new CObjectProperty( szPropName, ++nSafePropID, type, viewType, defVal, bReadOnly );
	pProp->SetHandler( pOnSetVal, pOnObjSetup );
	pProp->SetGroup( nGroup );
	pattern.insert( CPropMap::value_type( szPropName, pProp) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectMgr::SetRelation( const char *szPropName, int nTableID )
{
	CPropMap::iterator i = pattern.find( szPropName );
	if ( i != pattern.end() )
	{
		i->second->SetRelation( nTableID );
		if ( i->second->GetGroup() == CProp::PROPDEF_GROUP )
			i->second->SetGroup( nTableID );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectMgr::MergeWith( CPropMap *pProps, int nObjectID ) const
{
	vector<CObjectDB::SValue> vals;

	for ( CPropMap::const_iterator i = pattern.begin(); i != pattern.end(); ++i )
	{
		if ( pProps->find( i->first ) != pProps->end() )
			continue;
		CProp *pProp = i->second->Clone();
		pProps->insert( CPropMap::value_type( i->first, pProp ) );
		vals.push_back( CObjectDB::SValue( pProp->GetType(), pProp->GetName() ) );
	}
	//
	if ( szTable != "" )
	{
		static CObjectDB db;
		if ( !db.GetValueBatch( &vals, szTable, nObjectID ) )
			return;
	}
	//
	for ( int i = 0; i < vals.size(); ++i )
	{
		const CObjectDB::SValue &val = vals[i];
		CPropMap::iterator it = pProps->find( val.szColumn );
		if ( it == pProps->end() )
			continue;
		CObjectProperty *prop = dynamic_cast<CObjectProperty*>( it->second.GetPtr() );
		if ( prop )
			prop->SetObject( szTable, nObjectID, val.value );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectMgr::Intersect( CPropMap *pDstProps, const CPropMap *pSrcProps )
{
	if ( pDstProps->empty() )
	{
		for ( CPropMap::const_iterator i = pSrcProps->begin(); i != pSrcProps->end(); ++i )
		{
			CProp *p = i->second;
			CObjectPropertyContainer *pc = new CObjectPropertyContainer( i->first, p->GetID(), p->GetType(), p->GetViewType(), p->GetDefValue(), p->IsReadOnly() );
			pc->SetRelation( p->GetRelation() );
			pc->SetGroup( p->GetGroup() );
			pc->PushProperty( p );
			pDstProps->insert( CPropMap::value_type( i->first, pc ) );
		}
		return;
	}
	//
	for ( CPropMap::iterator i = pDstProps->begin(); i != pDstProps->end(); )
	{
		CObjectPropertyContainer *p = dynamic_cast<CObjectPropertyContainer*>( i->second.GetPtr() );
		if ( !p )
			continue;
		CPropMap::const_iterator it = pSrcProps->find( i->first );
		if ( it == pSrcProps->end() || it->second->GetViewType() != p->GetViewType() || it->second->GetRelation() != p->GetRelation() )
		{
			CPropMap::iterator temp = i;
			++i;
			pDstProps->erase( temp );
			continue;
		}
		p->PushProperty( it->second );
		++i;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectMgr::SetObjectProps( int nDstObject, const CPropMap *pSrcProps ) const
{
	CPropMap props;
	for ( CPropMap::const_iterator i = pattern.begin(); i != pattern.end(); ++i )
	{
		CObjectProperty *pProp = dynamic_cast<CObjectProperty*>( i->second->Clone() );
		if ( !pProp )
		{
			ASSERT(0);
			return;
		}
		pProp->SetObject( szTable, nDstObject, CVariant() );
		props.insert( CPropMap::value_type( i->first, pProp ) );
	}
	//
	for ( CPropMap::const_iterator i = props.begin(); i != props.end(); ++i )
	{
		CPropMap::const_iterator it = pSrcProps->find( i->first );
		if ( it != pSrcProps->end() )
			i->second->SetValue( it->second->GetValue() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectMgr::AddString( const char *szPropName, const char *pszString )
{
	CPropMap::iterator it = pattern.find( szPropName );

	if ( it == pattern.end() )
		return;
	it->second->AddString( pszString );
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////