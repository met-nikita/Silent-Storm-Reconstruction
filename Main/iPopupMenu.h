#ifndef __A5_INTERFACE_POPUP_H__
#define __A5_INTERFACE_POPUP_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAckIcon
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPopupMenu: public CDecorator<CWindow>
{
	OBJECT_NOCOPY_METHODS(CPopupMenu);
private:
	struct SItem
	{
		ZDATA
		bool bSeparator;
		string szID;
		wstring wsText;
		CPtr<IWindow> pLine;
		CDBPtr<NDb::CUITexture> pIcon;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bSeparator); f.Add(3,&szID); f.Add(4,&wsText); f.Add(5,&pLine); f.Add(6,&pIcon); return 0; }
	};
	ZDATA_(TBaseClass)
	bool bUpdated;
	bool bComplete;
	string szReturnID;
	CPtr<IWindow> pUp;
	CPtr<IWindow> pDown;
	vector<SItem> itemsSet;
	CPtr<ICursorHandler> pCursorHandler;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&bUpdated); f.Add(3,&bComplete); f.Add(4,&szReturnID); f.Add(5,&pUp); f.Add(6,&pDown); f.Add(7,&itemsSet); f.Add(8,&pCursorHandler); return 0; }

public:
	CPopupMenu() {}
	CPopupMenu( IWindow *pContainer );

	void AddItem( const string &szID, const wstring &wsText, NDb::CUITexture *pIcon = 0 );
	void AddSeparator();
	void RemoveAll();

	void Do();
	bool IsComplete();
	const string& GetReturnID();

	bool ProcessMessage( const SEvent &sEvent );

	void Update( const SScene &sScene );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
