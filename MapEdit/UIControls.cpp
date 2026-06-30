#include "StdAFx.h"
#include "UIControls.h"
#include "CtrlObjectInspector.h"
#include "MapEdit.h"
#include "ItemsMgr.h"
#include "MainFrm.h"
#include "UIView.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
extern SDBConnection dbConnection;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUIProp: public CProp
{
	OBJECT_BASIC_METHODS(CUIProp);
	CVariant value;
	CUIControl *pControl;
	void *pCtrlValue;

public:
	CUIProp() {}
	CUIProp( const string &szName, int nPropertyID, int nType, int nViewType, CUIControl *pCtrl, void *pValue, bool bReadOnly = false );

  virtual const CVariant& GetValue() const { return value; }
	virtual const CVariant GetDefValue() const { return CVariant(); }
  virtual void SetValue( const CVariant &value, bool bModified = true ) const;
	virtual CProp* Clone() const { return 0; }

	void SetControlValue() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUIProp::CUIProp( const string &szName, int nPropertyID, int nType, int nViewType, CUIControl *pCtrl, void *pVal, bool bReadOnly )
:CProp( szName, nPropertyID, nType, nViewType, bReadOnly ), pControl( pCtrl ), pCtrlValue( pVal )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIProp::SetValue( const CVariant &val, bool bModified /* = true  */ ) const
{
	CVariant oldVal = value;
	const_cast<CVariant&>( value ) = val;
	if ( bModified && !pControl->Update() )
		const_cast<CVariant&>( value ) = oldVal;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIProp::SetControlValue() const
{
	if ( !pCtrlValue )
		return;
	switch ( GetType() )
	{
		case CVariant::VT_INT:
			*((LONG*)pCtrlValue) = value.GetType() == CVariant::VT_NULL ? 0 : (int)value;
			break;
		case CVariant::VT_BOOL:
			*((USHORT*)pCtrlValue) = value.GetType() == CVariant::VT_NULL ? false : (int)value;
			break;
		case CVariant::VT_STR:
			StrCpy( PTSTR( pCtrlValue ), value.GetType() == CVariant::VT_NULL ? "" : (const char*)value );
			break;
		default:
			ASSERT( 0 );
			return;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// class CUIControl
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIControl::Update( bool bSaveData /* = true  */ )
{
	if ( bSaveData )
	{
		CUIControlAccessor copy = *this;
		// открываем в базе запись, которую хотим обновить
		string szQuery = string( "SELECT * FROM " ) + UICONTROLS_TBL + " WHERE ID=" + IToA( nID );
		HRESULT hr = Open( szQuery.c_str() );
		if ( FAILED( hr ) ||S_OK != MoveNext() )
		{
			*static_cast<CUIControlAccessor*>(this) = copy;
			return false;
		}
	}
	for ( CPropMap::const_iterator i = props.begin(); i != props.end(); ++i )
		((CUIProp*)((CProp*)i->second))->SetControlValue();
	//
	if ( m_nNestedUIContainerID > 0 )
	{
		const SResTree *pTree = theApp.GetResTree( IDC_INTERFACES_TREE );
		if ( pTree )
		{
			const CPropMap *pProps = pTree->pItemsTree->GetPropList( m_nNestedUIContainerID );
			if ( pProps )
			{
				CPropMap::const_iterator iw = pProps->find( "Width" );
				CPropMap::const_iterator ih = pProps->find( "Height" );
				if ( iw != pProps->end() && ih != pProps->end() )
				{
					rect.right = rect.left + (int)iw->second->GetValue();
					rect.bottom = rect.top + (int)ih->second->GetValue();
				}
				pTree->pItemsTree->ReleasePropList( pProps );
			}
		}
	}
	//
	if ( bSaveData )
	{
		// заполняем данные
		m_nUIContainerID = nUIContainerID;
		m_nLeft   = rect.left;
		m_nTop    = rect.top;
		m_nRight  = rect.right;
		m_nBottom = rect.bottom;
		// записываем данные в базу, используя соответсвующий аксессор
		HRESULT hr = SetData( m_nType );
		if ( FAILED( hr ) )
		{
			DisplayOLEDBErrorRecords( hr );
			return false;
		}
		CUIControlAccessor copy = *this;
		Close(); // сбрасывает все данные в аксессоре
		*static_cast<CUIControlAccessor*>(this) = copy;
	}
	CMainFrame *pMF = dynamic_cast<CMainFrame*>( theApp.GetMainWnd() );
	if ( pMF )
		pMF->GetUIView()->Invalidate( FALSE );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CPropMap* CUIControl::GetPropMap() const
{
	return &props;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIControl::AddProp( const string &szName, int nType, int nViewType, const CVariant &val, void *pValue, bool bReadOnly, int nGroup )
{
	CProp *pProp = new CUIProp( szName, props.size(), nType, nViewType, this, pValue, bReadOnly );
	pProp->SetValue( val, false );
	pProp->SetGroup( nGroup );
	pProp->SetOwner( SOwner( nID, -1 ) );
	props[szName] = pProp;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIControl::SetRelation( const string &szName, int nResTreeID )
{
	CPropMap::const_iterator i = props.find( szName );
	if ( i != props.end() )
	{
		i->second->SetRelation( nResTreeID );
		i->second->SetGroup( nResTreeID );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIControl::SetPrefix( const string &szName, const string &szPrefix )
{
	CPropMap::const_iterator i = props.find( szName );
	if ( i != props.end() )
		i->second->SetPrefix( szPrefix );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIControl::SetTip( const string &szName, int nResStrID )
{
	CPropMap::const_iterator i = props.find( szName );
	if ( i == props.end() )
		return;
	CString str;
	if ( !str.LoadString( nResStrID ) )
		return;
	i->second->SetTip( (LPCTSTR)str );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIControl::SetStrType( int nStrID )
{
	CString str;

	str.LoadString( nStrID );
	AddProp( "Type", CVariant::VT_STR, DT_STR, (LPCTSTR)str.Left( str.Find( '\n' ) ), 0, true );
	AddProp( "ID", CVariant::VT_STR, DT_STR, m_szID, &m_szID );
	AddProp( "TooltipID", CVariant::VT_INT, DT_DEC, m_nTooltipID, &m_nTooltipID );
	AddProp( "Depth", CVariant::VT_INT, DT_DEC, m_nDepth, &m_nDepth );
	SetRelation( "TooltipID", IDC_STRINGS_TREE );
	//
	/*
	AddProp( "Left", CVariant::VT_INT, DT_DEC, m_nLeft, &m_nLeft );
	AddProp( "Top", CVariant::VT_INT, DT_DEC, m_nTop, &m_nTop);
	AddProp( "Right", CVariant::VT_INT, DT_DEC, m_nRight, &m_nRight );
	AddProp( "Bottom", CVariant::VT_INT, DT_DEC, m_nBottom, &m_nBottom );
	*/
	AddProp( "Left", CVariant::VT_INT, DT_DEC, m_nLeft, &rect.left );
	AddProp( "Top", CVariant::VT_INT, DT_DEC, m_nTop, &rect.top);
	AddProp( "Right", CVariant::VT_INT, DT_DEC, m_nRight, &rect.right );
	AddProp( "Bottom", CVariant::VT_INT, DT_DEC, m_nBottom, &rect.bottom );
	AddProp( "Freeze", CVariant::VT_INT, DT_BOOL, m_nFreeze, &m_nFreeze, false );
	AddProp( "Transparent", CVariant::VT_INT, DT_BOOL, m_bTransparent, &m_bTransparent, false );
	AddProp( "Visible", CVariant::VT_INT, DT_BOOL, m_bVisible, &m_bVisible, false );
	AddProp( "Topmost", CVariant::VT_INT, DT_BOOL, m_bTopmost, &m_bTopmost, false );
	AddProp( "DefaultWindow", CVariant::VT_INT, DT_BOOL, m_bDefaultWindow, &m_bDefaultWindow, false );
	AddProp( "Bottommost", CVariant::VT_INT, DT_BOOL, m_bBottommost, &m_bBottommost, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIControl::Setup( int _nUIContainerID )
{
	SetConnection( &dbConnection );
	nID = m_nID;
	nUIContainerID = _nUIContainerID;
	m_nUIContainerID = _nUIContainerID;
	props.clear();

	rect.left   = m_nLeft;
	rect.right  = m_nRight;
	rect.top    = m_nTop;
	rect.bottom = m_nBottom;

	switch ( m_nType )
	{
		case NDb::UI_TEXT:
			SetStrType( ID_UI_TEXT );
			AddProp( "StringID", CVariant::VT_INT, DT_DEC, m_nStringID, &m_nStringID );
			SetRelation( "StringID", IDC_STRINGS_TREE );
			break;
		case NDb::UI_IMAGE:
			SetStrType( ID_UI_IMAGE );
			AddProp( "ImageID", CVariant::VT_INT, DT_DEC, m_nTexture0, &m_nTexture0 );
			AddProp( "Color", CVariant::VT_INT, DT_DEC, m_nColor, &m_nColor );
			SetRelation( "ImageID", IDC_UITEXTURES_TREE );
			break;
		case NDb::UI_MODEL:
			SetStrType( ID_UI_MODEL );
			AddProp( "ModelID", CVariant::VT_INT, DT_DEC, m_nModel0, &m_nModel0 );
			SetRelation( "ModelID", IDC_MODELS_TREE );
			break;
		case NDb::UI_BUTTON:
			SetStrType( ID_UI_BUTTON );
			AddProp( "Glow", CVariant::VT_INT, DT_DEC, m_nTexture0, &m_nTexture0 );
			AddProp( "BaseUp", CVariant::VT_INT, DT_DEC, m_nTexture1, &m_nTexture1 );
			AddProp( "BaseDown", CVariant::VT_INT, DT_DEC, m_nTexture2, &m_nTexture2 );
			AddProp( "BaseDisabled", CVariant::VT_INT, DT_DEC, m_nTexture3, &m_nTexture3 );
			AddProp( "StringID", CVariant::VT_INT, DT_DEC, m_nStringID, &m_nStringID );
			AddProp( "UIContainerID", CVariant::VT_INT, DT_DEC, m_nNestedUIContainerID, &m_nNestedUIContainerID );
			AddProp( "Sound0", CVariant::VT_INT, DT_DEC, m_nSound0, &m_nSound0 );
			AddProp( "Sound1", CVariant::VT_INT, DT_DEC, m_nSound1, &m_nSound1 );
			SetRelation( "UIContainerID", IDC_INTERFACES_TREE );
			SetRelation( "Glow", IDC_UITEXTURES_TREE );
			SetRelation( "BaseUp", IDC_UITEXTURES_TREE );
			SetRelation( "BaseDown", IDC_UITEXTURES_TREE );
			SetRelation( "BaseDisabled", IDC_UITEXTURES_TREE );
			SetRelation( "StringID", IDC_STRINGS_TREE );
			SetRelation( "Sound0", IDC_SOUNDS_TREE );
			SetRelation( "Sound1", IDC_SOUNDS_TREE );
			break;
		case NDb::UI_PUSHBUTTON:
			SetStrType( ID_UI_PUSHBUTTON );
			AddProp( "Glow", CVariant::VT_INT, DT_DEC, m_nTexture0, &m_nTexture0 );
			AddProp( "BaseUp", CVariant::VT_INT, DT_DEC, m_nTexture1, &m_nTexture1 );
			AddProp( "BaseDown", CVariant::VT_INT, DT_DEC, m_nTexture2, &m_nTexture2 );
			AddProp( "BaseDisabled", CVariant::VT_INT, DT_DEC, m_nTexture3, &m_nTexture3 );
			AddProp( "StringID", CVariant::VT_INT, DT_DEC, m_nStringID, &m_nStringID );
			AddProp( "UIContainerID", CVariant::VT_INT, DT_DEC, m_nNestedUIContainerID, &m_nNestedUIContainerID );
			AddProp( "Sound0", CVariant::VT_INT, DT_DEC, m_nSound0, &m_nSound0 );
			AddProp( "Sound1", CVariant::VT_INT, DT_DEC, m_nSound1, &m_nSound1 );
			SetRelation( "UIContainerID", IDC_INTERFACES_TREE );
			SetRelation( "Glow", IDC_UITEXTURES_TREE );
			SetRelation( "BaseUp", IDC_UITEXTURES_TREE );
			SetRelation( "BaseDown", IDC_UITEXTURES_TREE );
			SetRelation( "BaseDisabled", IDC_UITEXTURES_TREE );
			SetRelation( "StringID", IDC_STRINGS_TREE );
			SetRelation( "Sound0", IDC_SOUNDS_TREE );
			SetRelation( "Sound1", IDC_SOUNDS_TREE );
			break;
		case NDb::UI_CHECKBUTTON:
			SetStrType( ID_UI_CHECKBUTTON );
			AddProp( "Glow", CVariant::VT_INT, DT_DEC, m_nTexture0, &m_nTexture0 );
			AddProp( "BaseUp", CVariant::VT_INT, DT_DEC, m_nTexture1, &m_nTexture1 );
			AddProp( "BaseDown", CVariant::VT_INT, DT_DEC, m_nTexture2, &m_nTexture2 );
			AddProp( "BaseDisabled", CVariant::VT_INT, DT_DEC, m_nTexture3, &m_nTexture3 );
			AddProp( "IconNormal", CVariant::VT_INT, DT_DEC, m_nTexture4, &m_nTexture4 );
			AddProp( "IconDisabled", CVariant::VT_INT, DT_DEC, m_nTexture5, &m_nTexture5 );
			AddProp( "StringID", CVariant::VT_INT, DT_DEC, m_nStringID, &m_nStringID );
			AddProp( "UIContainerID", CVariant::VT_INT, DT_DEC, m_nNestedUIContainerID, &m_nNestedUIContainerID );
			AddProp( "Sound0", CVariant::VT_INT, DT_DEC, m_nSound0, &m_nSound0 );
			AddProp( "Sound1", CVariant::VT_INT, DT_DEC, m_nSound1, &m_nSound1 );
			SetRelation( "UIContainerID", IDC_INTERFACES_TREE );
			SetRelation( "Glow", IDC_UITEXTURES_TREE );
			SetRelation( "BaseUp", IDC_UITEXTURES_TREE );
			SetRelation( "BaseDown", IDC_UITEXTURES_TREE );
			SetRelation( "BaseDisabled", IDC_UITEXTURES_TREE );
			SetRelation( "IconNormal", IDC_UITEXTURES_TREE );
			SetRelation( "IconDisabled", IDC_UITEXTURES_TREE );
			SetRelation( "StringID", IDC_STRINGS_TREE );
			SetRelation( "Sound0", IDC_SOUNDS_TREE );
			SetRelation( "Sound1", IDC_SOUNDS_TREE );
			break;
		case NDb::UI_RADIOBUTTON:
			SetStrType( ID_UI_RADIOBUTTON );
			AddProp( "Glow", CVariant::VT_INT, DT_DEC, m_nTexture0, &m_nTexture0 );
			AddProp( "BaseUp", CVariant::VT_INT, DT_DEC, m_nTexture1, &m_nTexture1 );
			AddProp( "BaseDown", CVariant::VT_INT, DT_DEC, m_nTexture2, &m_nTexture2 );
			AddProp( "BaseDisabled", CVariant::VT_INT, DT_DEC, m_nTexture3, &m_nTexture3 );
			AddProp( "IconNormal", CVariant::VT_INT, DT_DEC, m_nTexture4, &m_nTexture4 );
			AddProp( "IconDisabled", CVariant::VT_INT, DT_DEC, m_nTexture5, &m_nTexture5 );
			AddProp( "StringID", CVariant::VT_INT, DT_DEC, m_nStringID, &m_nStringID );
			AddProp( "UIContainerID", CVariant::VT_INT, DT_DEC, m_nNestedUIContainerID, &m_nNestedUIContainerID );
			AddProp( "Sound0", CVariant::VT_INT, DT_DEC, m_nSound0, &m_nSound0 );
			AddProp( "Sound1", CVariant::VT_INT, DT_DEC, m_nSound1, &m_nSound1 );
			SetRelation( "UIContainerID", IDC_INTERFACES_TREE );
			SetRelation( "Glow", IDC_UITEXTURES_TREE );
			SetRelation( "BaseUp", IDC_UITEXTURES_TREE );
			SetRelation( "BaseDown", IDC_UITEXTURES_TREE );
			SetRelation( "BaseDisabled", IDC_UITEXTURES_TREE );
			SetRelation( "IconNormal", IDC_UITEXTURES_TREE );
			SetRelation( "IconDisabled", IDC_UITEXTURES_TREE );
			SetRelation( "StringID", IDC_STRINGS_TREE );
			SetRelation( "Sound0", IDC_SOUNDS_TREE );
			SetRelation( "Sound1", IDC_SOUNDS_TREE );
			break;
		case NDb::UI_SCROLL:
			SetStrType( ID_UI_SCROLL );
			AddProp( "UIContainerID", CVariant::VT_INT, DT_DEC, m_nNestedUIContainerID, &m_nNestedUIContainerID );
			SetRelation( "UIContainerID", IDC_INTERFACES_TREE );
			SetTip( "BkImageID", IDS_TIP_BKIMAGE );
			break;
		case NDb::UI_SLIDER:
			SetStrType( ID_UI_SLIDER );
			AddProp( "UIContainerID", CVariant::VT_INT, DT_DEC, m_nNestedUIContainerID, &m_nNestedUIContainerID );
			SetRelation( "UIContainerID", IDC_INTERFACES_TREE );
			SetTip( "BkImageID", IDS_TIP_BKIMAGE );
			break;
		case NDb::UI_EDIT:
			SetStrType( ID_UI_EDIT );
			AddProp( "BkImageID", CVariant::VT_INT, DT_DEC, m_nTexture0, &m_nTexture0 );
			SetRelation( "BkImageID", IDC_UITEXTURES_TREE );
			SetTip( "BkImageID", IDS_TIP_BKIMAGE );
			break;
		case NDb::UI_IMAGELIST:
			SetStrType( ID_UI_IMAGELIST );
			AddProp( "UIContainerID", CVariant::VT_INT, DT_DEC, m_nNestedUIContainerID, &m_nNestedUIContainerID );
			AddProp( "BkImageID", CVariant::VT_INT, DT_DEC, m_nTexture0, &m_nTexture0 );
			SetRelation( "UIContainerID", IDC_INTERFACES_TREE );
			SetRelation( "BkImageID", IDC_UITEXTURES_TREE );
			SetTip( "BkImageID", IDS_TIP_BKIMAGE );
			break;
		case NDb::UI_COMBOBOX:
			SetStrType( ID_UI_COMBOBOX );
			AddProp( "UIContainerID", CVariant::VT_INT, DT_DEC, m_nNestedUIContainerID, &m_nNestedUIContainerID );
			AddProp( "BkImageID", CVariant::VT_INT, DT_DEC, m_nTexture0, &m_nTexture0 );
			SetRelation( "UIContainerID", IDC_INTERFACES_TREE );
			SetRelation( "BkImageID", IDC_UITEXTURES_TREE );
			SetTip( "BkImageID", IDS_TIP_BKIMAGE );
			break;
		case NDb::UI_MESSAGEBOX:
			SetStrType( ID_UI_MESSAGEBOX );
			AddProp( "BkImageID", CVariant::VT_INT, DT_DEC, m_nTexture0, &m_nTexture0 );
			AddProp( "StringID", CVariant::VT_INT, DT_DEC, m_nStringID, &m_nStringID );
			SetRelation( "BkImageID", IDC_UITEXTURES_TREE );
			SetRelation( "StringID", IDC_STRINGS_TREE );
			SetTip( "BkImageID", IDS_TIP_BKIMAGE );
			break;
		case NDb::UI_GROUP:
			SetStrType( ID_UI_GROUP );
			AddProp( "UIContainerID", CVariant::VT_INT, DT_DEC, m_nNestedUIContainerID, &m_nNestedUIContainerID );
			SetRelation( "UIContainerID", IDC_INTERFACES_TREE );
			break;
		case NDb::UI_WINDOW:
			SetStrType( ID_UI_WINDOW );
			AddProp( "UIContainerID", CVariant::VT_INT, DT_DEC, m_nNestedUIContainerID, &m_nNestedUIContainerID );
			AddProp( "Texture0", CVariant::VT_INT, DT_DEC, m_nTexture0, &m_nTexture0 );
			AddProp( "Texture1", CVariant::VT_INT, DT_DEC, m_nTexture1, &m_nTexture1 );
			AddProp( "Texture2", CVariant::VT_INT, DT_DEC, m_nTexture2, &m_nTexture2 );
			AddProp( "Texture3", CVariant::VT_INT, DT_DEC, m_nTexture3, &m_nTexture3 );
			AddProp( "Texture4", CVariant::VT_INT, DT_DEC, m_nTexture4, &m_nTexture4 );
			AddProp( "Texture5", CVariant::VT_INT, DT_DEC, m_nTexture5, &m_nTexture5 );
			AddProp( "Model0", CVariant::VT_INT, DT_DEC, m_nModel0, &m_nModel0 );
			AddProp( "Model1", CVariant::VT_INT, DT_DEC, m_nModel1, &m_nModel1 );
			AddProp( "Model2", CVariant::VT_INT, DT_DEC, m_nModel2, &m_nModel2 );
			AddProp( "Model3", CVariant::VT_INT, DT_DEC, m_nModel3, &m_nModel3 );
			AddProp( "Model4", CVariant::VT_INT, DT_DEC, m_nModel4, &m_nModel4 );
			AddProp( "StringID", CVariant::VT_INT, DT_DEC, m_nStringID, &m_nStringID );
			AddProp( "Text", CVariant::VT_STR, DT_STR, m_szText, &m_szText );
			AddProp( "Sound0", CVariant::VT_INT, DT_DEC, m_nSound0, &m_nSound0 );
			AddProp( "Sound1", CVariant::VT_INT, DT_DEC, m_nSound1, &m_nSound1 );
			AddProp( "Sound2", CVariant::VT_INT, DT_DEC, m_nSound2, &m_nSound2 );
			AddProp( "Sound3", CVariant::VT_INT, DT_DEC, m_nSound3, &m_nSound3 );
			AddProp( "Sound4", CVariant::VT_INT, DT_DEC, m_nSound4, &m_nSound4 );
			SetRelation( "Texture0", IDC_UITEXTURES_TREE );
			SetRelation( "Texture1", IDC_UITEXTURES_TREE );
			SetRelation( "Texture2", IDC_UITEXTURES_TREE );
			SetRelation( "Texture3", IDC_UITEXTURES_TREE );
			SetRelation( "Texture4", IDC_UITEXTURES_TREE );
			SetRelation( "Texture5", IDC_UITEXTURES_TREE );
			SetRelation( "Model0", IDC_MODELS_TREE );
			SetRelation( "Model1", IDC_MODELS_TREE );
			SetRelation( "Model2", IDC_MODELS_TREE );
			SetRelation( "Model3", IDC_MODELS_TREE );
			SetRelation( "Model4", IDC_MODELS_TREE );
			SetRelation( "StringID", IDC_STRINGS_TREE );
			SetRelation( "UIContainerID", IDC_INTERFACES_TREE );
			SetRelation( "Sound0", IDC_SOUNDS_TREE );
			SetRelation( "Sound1", IDC_SOUNDS_TREE );
			SetRelation( "Sound2", IDC_SOUNDS_TREE );
			SetRelation( "Sound3", IDC_SOUNDS_TREE );
			SetRelation( "Sound4", IDC_SOUNDS_TREE );
			break;
		case NDb::UI_PROGRESSBAR:
			SetStrType( ID_UI_PROGRESSBAR );
			AddProp( "UIContainerID", CVariant::VT_INT, DT_DEC, m_nNestedUIContainerID, &m_nNestedUIContainerID );
			AddProp( "BarImageID", CVariant::VT_INT, DT_DEC, m_nTexture0, &m_nTexture0 );
			SetRelation( "UIContainerID", IDC_INTERFACES_TREE );
			SetRelation( "BarImageID", IDC_UITEXTURES_TREE );
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIControl::CheckRect()
{
	CPropMap::const_iterator it = props.find( "UIContainerID" );
	if ( it != props.end() )
	{
		const SResTree *pTree = theApp.GetResTree( IDC_INTERFACES_TREE );
		if ( pTree )
		{
			const CPropMap *pProps = pTree->pItemsTree->GetPropList( it->second->GetValue() );
			if ( pProps )
			{
				CPropMap::const_iterator iw = pProps->find( "Width" );
				CPropMap::const_iterator ih = pProps->find( "Height" );
				if ( iw != pProps->end() && ih != pProps->end() )
					rect = CRect( rect.TopLeft(), CSize( (int)iw->second->GetValue(), (int)ih->second->GetValue() ) );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIControl::SetRect( CRect newrect, bool bSaveData )
{
	CRect oldrect = rect;
	rect = newrect;
	m_nLeft = newrect.left;
	m_nTop = newrect.top;
	m_nRight = newrect.right;
	m_nBottom = newrect.bottom;
	props["Left"]->SetValue( newrect.left, false );
	props["Right"]->SetValue( newrect.right, false );
	props["Top"]->SetValue( newrect.top, false );
	props["Bottom"]->SetValue( newrect.bottom, false );
	CheckRect();
	bool bRet = Update( bSaveData );
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIControl::Open()
{
	HRESULT hr = Open( string( "SELECT * FROM " ) + UICONTROLS_TBL + " WHERE ID=" + IToA( nID ) );
	if ( FAILED( hr ) )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIControl::SyncWithImageSize()
{
	const SResTree *pUITree = theApp.GetResTree( IDC_UITEXTURES_TREE );
	const SResTree *pTree = theApp.GetResTree( IDC_TEXTURES_TREE );
	if ( pTree && pTree )
	{
		const CPropMap *pUITProps = pUITree->pItemsTree->GetPropList( m_nTexture0 );
		if ( pUITProps )
		{
			CPropMap::const_iterator itex = pUITProps->find( "R_1024x768" );
			if ( itex == pUITProps->end() )
				return false;
			const CPropMap *pProps = pTree->pItemsTree->GetPropList( itex->second->GetValue() );
			if ( pProps )
			{
				CPropMap::const_iterator iw = pProps->find( "Width" );
				CPropMap::const_iterator ih = pProps->find( "Height" );
				if ( iw != pProps->end() && ih != pProps->end() )
				{
					SetRect( CRect( rect.TopLeft(), CSize( (int)iw->second->GetValue(), (int)ih->second->GetValue() ) ) );
				}
				pTree->pItemsTree->ReleasePropList( pProps );
			}
			pUITree->pItemsTree->ReleasePropList( pUITProps );
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CUIControl );
////////////////////////////////////////////////////////////////////////////////////////////////////
