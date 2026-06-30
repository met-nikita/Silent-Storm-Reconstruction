#ifndef __ITEMSMGR_H__
#define __ITEMSMGR_H__

#include "PropMap.h"

const int EMPTY_VALUE = 0;
class CItemsMgr;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CElemProp : public CProp
{
	OBJECT_BASIC_METHODS(CElemProp);
private:
  CItemsMgr *pItems;
  CVariant  value;
	CVariant  defValue;

public:
	CElemProp() {}
  CElemProp( const string &szName, int nID, int nType, int nViewType, 
		CItemsMgr *pItemsMgr, const CVariant& defValue, bool bReadOnly = false );
	
  const CVariant& GetValue() const;
	const CVariant GetDefValue() const;
  void SetValue( const CVariant &value, bool bModified = true ) const;
	CProp* Clone() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class ITreeView
{
public:
  virtual int  FolderAdded( int nTreeID, int nFolderID ) = 0;
  virtual void FolderDeleted( int nTreeID, int nFolderID ) = 0;
  virtual void ParentChanged( int nTreeID, int nFolderID, int nOldParent ) = 0;
  virtual int  ItemAdded( int nTreeID, int nItemID ) = 0;
  virtual void ItemDeleted( int nTreeID, int nItemID ) = 0;
  virtual void ItemFolderChanged( int nTreeID, int nItemID, int nOldFolder ) = 0;
  virtual void ResetTree() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CItemsDBCmd;
class CTreeDBCmd;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CItemsMgr
{
private:
  struct SFolder
  {
    int     nID;
    string  szName;
    int     nParentID;
		DWORD		nUserColor;

    SFolder() : nParentID( -1 ), nUserColor( 0 ) {}
    bool operator!= ( const SFolder &fol )
    {
      return nID != fol.nID || nParentID != nParentID || szName != fol.szName || nUserColor != nUserColor;
    }
  };
  struct SElemProp
  {
    CVariant  value;
  };
  struct SItem
  {
    int     nID;
    int     nFolderID;
    string  szName;
		DWORD		color;
		vector<int> variants;
  };
  
	int nTreeID;
  CItemsDBCmd         *const pDB;
  CTreeDBCmd          *const pTreeDB;
  string              szDBFolderTable;      // таблица в базе данных, в которой содержится дерево папок
  string              szDBItemsTable;
	string							szVarTable;
  vector<SFolder>     folderList;
  vector<SItem>       itemList;
  std::unordered_map<int, int>  foldID2Index;    // отображение ID папки в индекс в векторе folderList
  std::unordered_map<int, int>  itemID2Index;    // отображение ID итема в индекс в векторе itemList
  int                 iFolder;              // указатель на текущую папку
  int                 iItem;                // указатель на текущий элемент
  vector<ITreeView*>  listeners;            // объекты, которые хотят получать информацию об изменениях 
                                            // в структуре дерева
  vector<int>         modifiedItems;

  vector<SElemProp>   properties;
  CPropMap            propMap;              // редактирумые св-ва объекта (эталонная копия)
  list<CPropMap* >     allocatedMaps;       // архив указателй на списки св-в, которые были выделены, но еще не осовбождены
  
  bool bLoaded;
	bool bUniTemplate;												// элементы являются универсальными темплейтами

  int  GetFolderIndex( int id ) const;
  int  GetItemIndex( int id ) const;
  void SetupFolderMap();
  void SetupItemMap();
	bool HasVariant( int nItemIndex, int nVariantID );
  
  CItemsMgr( const CItemsMgr& ) : pDB(0), pTreeDB(0) {}
  void PushItem( int nItemID, int nFolderID, const string &szName, DWORD color );
  bool LoadFolders();
  bool LoadItems();

  friend class CElemProp;
  const CVariant& GetValue( int nItemID, int nValID ) const;
  bool  SetValue( SOwner itemID, const string &szPropName, const CVariant value, bool bModified = true );
  bool  ReloadFromDB();
  
public:
  CItemsMgr( int nID, CItemsDBCmd *pDB, CTreeDBCmd *pTreeDB, const char *pszFolderTbl, const char *pszItemsTbl, const char *pszVarTbl = 0 );
  ~CItemsMgr();
  
  bool Load();
  void Clear();
  bool Reload();
  int  AddListener( ITreeView *pListener );
  void RemoveListener( int nListenerID );
  bool IsLoaded() const {return bLoaded;}
	bool IsUniTemplate() const { return bUniTemplate; }
  
  bool UpdateModified();
  
	//папки
  int  AddFolder( int nListenerID, int nParentID, string szName );
  bool DeleteFolder( int nListenerID, int nFolderID );
	bool SetFolderName( int nListenerID, int nFolderID, const string &szName );
  bool SetParentFolder( int nListenerID, int nFolderID, int nParentID );
	DWORD GetFolderColor( int nFolderID );
	bool SetFolderColor( int nFolderID, DWORD cr );

  const char* ID2FolderName( int nFolderID ) const;
  int  GetParentID( int nFolderID ) const;
  bool IsSubfolder( int nFolderID, int nSubfolderID );
  void MoveFirstFolder();
  bool MoveNextFolder();
  int  GetFolderID() const;
  int  GetFolderCount() const { return folderList.size(); }
	int  DoesFolderExist( const string &szFolder, int nParentID = -1 );
  
  // итема
	string GetItemsTable() const { return szDBItemsTable; }
  int  AddItem( int nListenerID, int nFolderID, string szName, bool	bInsertVariant = true );
	int  AddVariant( int nItemID );
  void DeleteItem( int nListenerID, int nItemID );
	void DeleteVariant( int nItemID, int nVariantID );
  void SetFolderForItem( int nListenerID, int nItemID, int nFolderID );
	bool SetItemName( int nListenerID, int nItemID, const string &szName );
	bool SetItemProps( int nItemID, const CPropMap *pProps );
	bool SetVariantProps( int nItemID, int nVarID, const CPropMap *pProps );
	bool SetItemColor( int nItemID, DWORD cr );

  void MoveFirstItem();
  bool MoveNextItem();
  int  GetItemID() const;
  int  GetItemFolderID() const;
  const char* GetItemName() const;
  bool IsExist( int nItemID );
	int  GetItemByProp( const string &szProp, const CVariant &val ) const;
  int  GetItemFolderID( int nItemID ) const;
  const char* GetItemName( int nItemID ) const;
	string GetItemPath( int nItemID ) const;
	string GetFolderPath( int nFolderID ) const;
	bool GetItemVariants( int nItemID, vector<int> *pVarIDs );
	bool GetVariantFlags( int nVariantID, vector<unordered_map<int, bool> > *pFlags );
	bool SetVariantFlags( int nVariantID, const vector<unordered_map<int, bool> > &flags );
	int  FindVariant( int nVariantID ); // ID итема, которому принадлежит вариант
	DWORD GetItemColor( int nItemID ) const;

  
  //Работа со свойствами
  const CPropMap* GetPropList( int nItemID, int nVariantID = -1 );
  void ReleasePropList( const CPropMap* pPropMap );
  int  AddProperty( int type, int viewType, const char *szPropName, const CVariant &defVal = CVariant(), int bReadOnly = false, int nGroup = CProp::PROPDEF_GROUP );
  bool SetRelation( const char *szPropName, int nTableID );
  bool SetPrefix( const char *szPropName, const char *szStr );
	bool SetTip( const char *szPropName, const char *szStr );
	bool SetTip( const char *szPropName, int nResStrID );
  bool AddString( const char *szPropName, const char *szStr );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получение индекса в folderList по ID папки
// "-1", если ID не найден
inline int CItemsMgr::GetFolderIndex( int id ) const 
{
  std::unordered_map<int, int>::const_iterator it = foldID2Index.find( id );
  if ( foldID2Index.end() == it )
    return -1;
  return it->second;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получение индекса в itemList по ID итема
// "-1", если ID не найден
inline int CItemsMgr::GetItemIndex( int id ) const 
{
  std::unordered_map<int, int>::const_iterator it = itemID2Index.find( id );
  if ( itemID2Index.end() == it )
    return -1;
  return it->second;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// true, если объект nItemID есть в таблице
inline bool CItemsMgr::IsExist( int nItemID )
{
  return itemID2Index.end() != itemID2Index.find( nItemID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __ITEMSMGR_H__