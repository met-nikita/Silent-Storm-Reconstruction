#include "StdAfx.h"
#include "ItemsMgr.h"
#include "..\Misc\StrProc.h"

#include "TreeDBCmd.h"
#include "ItemsDBCmd.h"
#include "CtrlObjectInspector.h"
#include "ListProp.h"

static int nItemSafePropID = 1321111;
extern string IToA( int n );
inline void PushUnique( vector<int> *pVec, int item )
{
  if ( !std::binary_search( pVec->begin(), pVec->end(), item ) )
  {
    pVec->push_back( item );
    std::sort( pVec->begin(), pVec->end() );
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//            CLASS CItemsMgr
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
CItemsMgr::CItemsMgr( int nID, CItemsDBCmd *_pDB, 
	CTreeDBCmd *_pTreeDB, 
	const char *pszFolderTable, 
	const char *pszItemsTable,
	const char *pszVarTable
	) : nTreeID( nID ), pDB( _pDB ), pTreeDB( _pTreeDB )
{
  ASSERT( pDB );
	bUniTemplate = pszVarTable ? true : false;
  szDBFolderTable = pszFolderTable;
  szDBItemsTable  = pszItemsTable;
	szVarTable = pszVarTable ? pszVarTable : "";
  bLoaded = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CItemsMgr::~CItemsMgr()
{
  Clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemsMgr::Load()
{
  if ( !LoadFolders() )
    return false;
  if ( !LoadItems() )
    return false;
  bLoaded = true;
  MoveFirstFolder();
  MoveFirstItem();
  
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsMgr::Clear()
{
  for ( list<CPropMap*>::iterator lit = allocatedMaps.begin(); lit != allocatedMaps.end(); ++lit )
    if ( *lit )
      delete (*lit);
      
  propMap.clear();
  allocatedMaps.clear();
  properties.clear();
  itemList.clear();
  folderList.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Загружает дерево папок из таблицы в базе данных
bool CItemsMgr::LoadFolders()
{
  HRESULT hr = pTreeDB->OpenTable( szDBFolderTable.c_str() );
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }

  foldID2Index.clear();
  folderList.clear();
  
  while( pTreeDB->MoveNext() == S_OK )
  {
    SFolder fol;

    fol.nID    = pTreeDB->m_FolderID;
    fol.szName = pTreeDB->m_FolderName;
    fol.nParentID = pTreeDB->m_ParentID;
		fol.nUserColor = pTreeDB->m_UserColor;
		pTreeDB->SetData( 1 );
    if ( fol.szName == "root" )
      fol.szName = "";
    folderList.push_back( fol );
    foldID2Index[fol.nID] = folderList.size() - 1;
  }
  MoveFirstFolder();
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemsMgr::LoadItems()
{
  HRESULT hr = pDB->Open( szDBItemsTable, true, 0 );
  
  if ( S_OK != hr )
    return false;
  
  while ( S_OK == pDB->MoveNext() )
  {
    int nID, nFoldID;
    char szName[255];
		DWORD color;
    
    if ( !pDB->GetItem( &nID, &nFoldID, szName, &color ) )
    {
      return false;
    }
    PushItem( nID, nFoldID, szName, color );
  }
  pDB->Close();
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Перечитать таблицу из базы данных
// Возвращаемые значения:
// true - произошли изменения
// false - изменений нет
bool CItemsMgr::Reload()
{
  if ( ReloadFromDB() )
  {
    // Сообщить всем заинтересованным объектам, что данные изменились
    for( int i = 0; i < listeners.size(); ++i )
      if ( listeners[i] )
        listeners[i]->ResetTree();
     return true;
  }
  return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemsMgr::ReloadFromDB()
{
  bool bChanges = false;

  {
    //folderList.clear();
    vector<SFolder> updFolders;
    HRESULT hr = pTreeDB->OpenTable( szDBFolderTable.c_str() );
    if ( FAILED(hr) )
    {
      // таблица больше недоступна
      folderList.clear();
      itemList.clear();
      return true;
    }
    
    const int nmax = folderList.size();
    int i = 0;
    while( pTreeDB->MoveNext() == S_OK )
    {
      SFolder fol;
      
      fol.nID    = pTreeDB->m_FolderID;
      fol.szName = pTreeDB->m_FolderName;
      fol.nParentID = pTreeDB->m_ParentID;
			fol.nUserColor = pTreeDB->m_UserColor;
      if ( fol.szName == "root" )
        fol.szName = "";
      updFolders.push_back( fol );
      if ( i >= nmax || updFolders[i] != folderList[i] )
        bChanges = true;
      ++i;
    }
    bChanges = nmax != updFolders.size() ? true : bChanges;
    if ( bChanges )
    {
      folderList.resize( updFolders.size() );
      for( i = 0; i < folderList.size(); ++i )
        folderList[i] = updFolders[i];
      SetupFolderMap();
      MoveFirstFolder();      
    }
    pTreeDB->Close();
  }
  //
  {
    if ( S_OK != pDB->Open( szDBItemsTable, true, 0 ) )
    {
      itemList.clear();    
      pDB->Close();
      return true;
    }
    
    const int n = itemList.size();
    int i = 0;
		if ( n > 0 )
		{
			while ( S_OK == pDB->MoveNext() )
			{
				int nID, nFoldID;
				char szName[50];
				DWORD color;
				
				if ( !pDB->GetItem( &nID, &nFoldID, szName, &color ) )
				{
					itemList.resize( i );
					pDB->Close();
					return true;
				}
				const SItem &item = itemList[i];
				if ( nID != item.nID || nFoldID != item.nFolderID || item.szName != szName )
				{
					bChanges = true;
					itemList[i].nID = nID;
					itemList[i].nFolderID = nFoldID;
					itemList[i].szName = szName;
					itemList[i].color = color;
					itemList.resize( ++i );
					break;    // все оставшиеся элементы можно не проверять
				}
				if ( ++i == n )
					break;
			}
		}
    while ( S_OK == pDB->MoveNext() )
    {
      bChanges = true; // изменилось число элементво
      int nID, nFoldID;
      char szName[50];
			DWORD color;
      
      if ( !pDB->GetItem( &nID, &nFoldID, szName, &color ) )
      {
        pDB->Close();
        return true;
      }
      PushItem( nID, nFoldID, szName, color );
      ++i;
    }
    itemList.resize( i );
    bChanges = n != itemList.size() ? true : bChanges;
    pDB->Close();
  }
  if ( bChanges )
  {
    SetupItemMap();    
    MoveFirstItem();
  }  
  return bChanges;  
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает строковое представление типа
// возвр. NULL, если тип с данным id не найден
const char* CItemsMgr::ID2FolderName( int id ) const
{
  int ind = GetFolderIndex( id );
  if ( -1 != ind )
    return folderList[ind].szName.c_str();
  return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemsMgr::SetFolderName( int nListenerID, int nFolderID, const string &szName )
{
  int ind = GetFolderIndex( nFolderID );
  if ( -1 == ind )
		return false;

  vector<int> items( 1, nFolderID );
  string      szQuery;
  
  MakeQueryStr( szQuery, szDBFolderTable, items );
  if ( FAILED( pTreeDB->Open( szQuery ) ) )
    return false;

  if ( S_OK != pTreeDB->MoveNext() )
    return false;
	strncpy( pTreeDB->m_FolderName, szName.c_str(), sizeof( pTreeDB->m_FolderName) );
  if ( S_OK != pTreeDB->SetData( 1 ) )
    return false;
	
  pTreeDB->Close();
	// Оповещаем заинтересованные объекты
  for( int i = 0; i < listeners.size(); ++i )
  {
    if ( i == nListenerID )
      continue;
    if ( listeners[i] )
      listeners[i]->ResetTree();
  }
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить ID родительского типа
// возвр. "-1", если нет родителя или ошибка
int CItemsMgr::GetParentID( int id ) const
{
  int ind = GetFolderIndex( id );

  if ( ind != -1 )
    return folderList[ind].nParentID;
  return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD CItemsMgr::GetFolderColor( int id )
{
	int ind = GetFolderIndex( id );
	
  if ( ind != -1 )
    return folderList[ind].nUserColor;
  return 0;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemsMgr::SetFolderColor( int nFolderID, DWORD cr )
{
  int folderInd  = GetFolderIndex( nFolderID );
	
  if ( -1 == folderInd )
    return false;
	
  folderList[folderInd].nUserColor = cr;

	vector<int> items( 1, nFolderID );
  string      szQuery;
	
	MakeQueryStr( szQuery, szDBFolderTable, items );
  HRESULT hr = pTreeDB->Open( szQuery );
  if ( FAILED(hr) )
    return false;
  if ( S_OK != pTreeDB->MoveNext() )
    return false;
  pTreeDB->m_UserColor = cr;
  if ( S_OK != pTreeDB->SetData( 1 ) )
    return false;
	
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Установка нового родительского типа
// Возвр. false, если хотя бы один из типов не существует
bool CItemsMgr::SetParentFolder( int nListenerID, int nFolderID, int nParentID )
{
  int nOldParent = GetParentID( nFolderID );
  int folderInd  = GetFolderIndex( nFolderID );
  int parentInd  = GetFolderIndex( nParentID );

  if ( -1 == folderInd )
    return false;

  folderList[folderInd].nParentID = nParentID;

  vector<int> items( 1, nFolderID );
  string      szQuery;
  
  MakeQueryStr( szQuery, szDBFolderTable, items );
  HRESULT hr = pTreeDB->Open( szQuery );
  if ( FAILED(hr) )
    return false;
  if ( S_OK != pTreeDB->MoveNext() )
    return false;
  pTreeDB->m_ParentID = nParentID;
  if ( S_OK != pTreeDB->SetData( 1 ) )
    return false;

  // Оповещаем заинтересованные объекты
  for( int i = 0; i < listeners.size(); ++i )
  {
    if ( i == nListenerID )
      continue;
    if ( listeners[i] )
      listeners[i]->ParentChanged( nTreeID, nFolderID, nOldParent );
  }
  pTreeDB->Close();
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Добавление новой папки в дерево и в базу данных
// Возвр. ID новой папки, -1 при ошибке
int CItemsMgr::AddFolder( int nListenerID, int nParentID, string szName )
{
  vector<int> items( 1, -1 );
  string      szQuery;
  
  MakeQueryStr( szQuery, szDBFolderTable, items );
  HRESULT hr = pTreeDB->Open( szQuery );
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  }

  strncpy( pTreeDB->m_FolderName, szName.c_str(), sizeof( pTreeDB->m_FolderName ) );
  pTreeDB->m_FolderName[sizeof( pTreeDB->m_FolderName ) - 1] = 0;
  pTreeDB->m_ParentID = nParentID;
  hr = pTreeDB->Insert( 1 );
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    pTreeDB->Close();
    return -1;
  }
  pTreeDB->MoveNext();

  SFolder fol;
  // ID папки задается базой данных
  fol.nID    = pTreeDB->m_FolderID;
  fol.szName = pTreeDB->m_FolderName;
  fol.nParentID = pTreeDB->m_ParentID;
  folderList.push_back( fol );
  foldID2Index[fol.nID] = folderList.size() - 1;
  // Оповещаем заинтересованные объекты
  for( int i = 0; i < listeners.size(); ++i )
  {
    if ( i == nListenerID )
      continue;
    if ( listeners[i] )
      listeners[i]->FolderAdded( nTreeID, fol.nID );
  }
  pTreeDB->Close();
  return fol.nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Удаляет паку из дерева и базы данных
// Желательно чтобы папка была пустая перед удаленением,
// иначе в базе останутся потерянные элементы
bool CItemsMgr::DeleteFolder( int nListenerID, int nFolderID )
{
  vector<int> items( 1, nFolderID );
  string      szQuery;
  int         ind = GetFolderIndex( nFolderID );

  if ( -1 == ind )
    return false;
  MakeQueryStr( szQuery, szDBFolderTable, items );
  HRESULT hr = pTreeDB->Open( szQuery );
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }

  if ( S_OK != pTreeDB->MoveNext() || S_OK != pTreeDB->Delete() )
    return false;
  
  // Оповещаем заинтересованные объекты
  for( int i = 0; i < listeners.size(); ++i )
  {
    if ( i == nListenerID )
      continue;
    if ( listeners[i] )
      listeners[i]->FolderDeleted( nTreeID, nFolderID );
  }
  folderList.erase( folderList.begin() + ind );
  SetupFolderMap();
  pTreeDB->Close();
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const int MAX_DB_TRY = 2; // максимальное кол-во попыток вставки новой записи в таблицу
////////////////////////////////////////////////////////////////////////////////////////////////////
// Добваление нового итема в дерево
// Возвр. ID нового итема или -1 при ошибке
int CItemsMgr::AddItem( int nListenerID, int nFolderID, string szName, bool	bInsertVariant )
{
  SItem item;
  int i;

  for ( i = 0; i < MAX_DB_TRY; ++i )
  {
		if ( IsUniTemplate() )
			item.nID = pDB->Insert( szDBItemsTable, nFolderID, szName.c_str(), 0 );
		else
			item.nID = pDB->Insert( szDBItemsTable, nFolderID, szName.c_str(), &propMap );
    if ( -1 != item.nID )
      break;
  }
  if ( -1 == item.nID )
    return -1;
  pDB->Close();  
  item.nFolderID = nFolderID;
  item.szName = szName;
	item.color = 0;
  
  itemList.push_back( item );
  itemID2Index[item.nID] = itemList.size() - 1;
  
  // Оповещаем заинтересованные объекты
  for( i = 0; i < listeners.size(); ++i )
  {
    if ( i == nListenerID )
      continue;
    if ( listeners[i] )
      listeners[i]->ItemAdded( nTreeID, item.nID );
  }
  pDB->Close();
	if ( IsUniTemplate() && bInsertVariant )
	{
		AddVariant( item.nID );
	}
	pDB->OpenItem( szDBItemsTable, -1, true, 0 );
	pDB->Close();
  return item.nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Добваление нового варианта в элемент
// Возвр. ID нового варианта или -1 при ошибке
int CItemsMgr::AddVariant(int nItemID )
{
	if ( !IsUniTemplate() )
	{
		ASSERT( false );
		return -1;
	}
	int ind = GetItemIndex( nItemID );
	if ( -1 == ind )
		return -1;
	int nVarID = -1;
  for ( int i = 0; i < MAX_DB_TRY; ++i )
  {
    nVarID = pDB->InsertVariant( szVarTable, nItemID, &propMap );
    if ( -1 != nVarID )
      break;
  }
  if ( -1 == nVarID )
    return -1;
  pDB->Close();
	itemList[ind].variants.push_back( nVarID );

  return nVarID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Используется при чтении из базы данных
inline void CItemsMgr::PushItem( int nItemID, int nFolderID, const string &szName, DWORD cr )
{
  SItem item;
  
  item.nID = nItemID;
  item.nFolderID = nFolderID;
  item.szName = szName;
	item.color = cr;
  
  itemList.push_back( item );
  itemID2Index[item.nID] = itemList.size() - 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsMgr::DeleteItem( int nListenerID, int nItemID )
{
  int ind = GetItemIndex( nItemID );
  
  if ( -1 == ind )
    return;
  // Сперва оповещаем заинтересованные объекты, 
  // т.к им нужна информация об удаляемом объекте
  for( int i = 0; i < listeners.size(); ++i )
  {
    if ( i == nListenerID )
      continue;
    if ( listeners[i] )
      listeners[i]->ItemDeleted( nTreeID, nItemID );
  }

  pDB->OpenItem( szDBItemsTable, nItemID, true, 0 );
  pDB->DeleteItem();

  itemList.erase( itemList.begin() + ind );
  SetupItemMap();
  pDB->Close();
	pDB->OpenItem( szDBItemsTable, -1, true, 0 );
	pDB->Close();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsMgr::DeleteVariant(int nItemID, int nVariantID  )
{
	if ( !IsUniTemplate() )
	{
		ASSERT( false );
		return;
	}		
  int ind = GetItemIndex( nItemID );
  
  if ( -1 == ind )
    return;
	if ( !HasVariant( ind, nVariantID ) )
		return;
	
  pDB->OpenVariant( szVarTable, nVariantID, true, 0 );
  pDB->DeleteItem();
	
	vector<int> vec = itemList[ind].variants;
	vector<int>::iterator i = find( vec.begin(), vec.end(), nVariantID );
	ASSERT( i != vec.end() );  // не должно быть, т.к. уже была проверка HasVariant()
  vec.erase( i );
	pDB->Close();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Переместить итем в другую папку, корректируется запись в базе данных 
void CItemsMgr::SetFolderForItem( int nListenerID, int nItemID, int nFolderID )
{
  int ind = GetItemIndex( nItemID );

  if ( -1 == ind )
    return;
  int nOldFolder = itemList[ind].nFolderID;
  itemList[ind].nFolderID = nFolderID;

  pDB->Close();
  if ( FAILED(pDB->OpenItem( szDBItemsTable, nItemID, true, 0 )) )
    return;
  if ( !pDB->UpdateItem( nFolderID, itemList[ind].szName.c_str() ) )
    return;

  // Оповещаем заинтересованные объекты
  for( int i = 0; i < listeners.size(); ++i )
  {
    if ( i == nListenerID )
      continue;
    if ( listeners[i] )
      listeners[i]->ItemFolderChanged( nTreeID, nItemID, nOldFolder );
  }
  pDB->Close();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemsMgr::SetItemName( int nListenerID, int nItemID, const string &szName )
{
  int ind = GetItemIndex( nItemID );
	
  if ( -1 == ind )
    return false;
	
  if ( FAILED(pDB->OpenItem( szDBItemsTable, nItemID, true, 0 )) )
    return false;
  if ( !pDB->UpdateItem( itemList[ind].nFolderID, szName.c_str() ) )
    return false;
	itemList[ind].szName = szName;

	// Оповещаем заинтересованные объекты
  for( int i = 0; i < listeners.size(); ++i )
  {
    if ( i == nListenerID )
      continue;
    if ( listeners[i] )
      listeners[i]->ResetTree();
  }
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Сохранение измененных типов в базе данных
bool CItemsMgr::UpdateModified()
{
  if ( modifiedItems.empty() )
    return true;

  for ( int i = 0; i < modifiedItems.size(); ++i )
  {
    int ind = GetItemIndex( modifiedItems[i] );
    const CPropMap *pProps = GetPropList( modifiedItems[i] );
    if ( -1 == ind  || !pProps )
      continue;

    if ( FAILED( pDB->OpenItem( szDBItemsTable, modifiedItems[i], false, pProps ) ) )
      return false;
    if ( !pDB->UpdateItem( itemList[ind].nFolderID, itemList[ind].szName.c_str(), pProps ) )
      return false;
    ReleasePropList( pProps );
    pDB->Close();
  }
  modifiedItems.clear();
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Сброс указателя на текущий элемент
// для установки на первый тип нужно вызвать MoveNext()
void CItemsMgr::MoveFirstFolder()
{
  iFolder = -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsMgr::MoveFirstItem()
{
  iItem = -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Продвижение указателя на текущий элемент в следующую позицию
// возвращает "false", если достигнут конец списка
bool CItemsMgr::MoveNextFolder()
{
  if ( iFolder >= (int)folderList.size() - 1 )
    return false;
  ++iFolder;
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemsMgr::MoveNextItem()
{
  if ( iItem >= (int)itemList.size() - 1 )
    return false;
  ++iItem;
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает ID текущего элемента 
// -1, если ошибка
int CItemsMgr::GetFolderID() const
{
  if ( iFolder != -1 )
    return folderList[iFolder].nID;
  return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CItemsMgr::GetItemID() const
{
  if ( iItem != -1 )
    return itemList[iItem].nID;
  return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CItemsMgr::GetItemFolderID() const
{
  if ( iItem != -1 )
    return itemList[iItem].nFolderID;
  return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const char* CItemsMgr::GetItemName() const
{
  if ( iItem != -1 )
    return itemList[iItem].szName.c_str();
  return "";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CItemsMgr::GetItemFolderID( int nItemID ) const
{
  int ind = GetItemIndex( nItemID );
  if ( ind != -1 )
    return itemList[ind].nFolderID;
  return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const char* CItemsMgr::GetItemName( int nItemID ) const
{
  int ind = GetItemIndex( nItemID );
  if ( ind != -1 )
    return itemList[ind].szName.c_str();
  return "";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Зарегестрить в списоке объектов, заинтересованных в отслеживании изменений в дереве
// новый элемент
// Возвращается id под которым зарегистрен элемент
// Если объект уже был в списке, возвращается старый ID
int CItemsMgr::AddListener( ITreeView *pListener )
{
  for( int i = 0; i < listeners.size(); ++i )
    if ( listeners[i] == pListener )
      return i;
  listeners.push_back( pListener );
  return listeners.size() - 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsMgr::RemoveListener( int nListenerID )
{
  if ( nListenerID < 0 || nListenerID >= listeners.size() )
    return;
  listeners[nListenerID] = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsMgr::SetupFolderMap()
{
  foldID2Index.clear();

  for ( int i = 0; i < folderList.size(); ++i )
  {
    foldID2Index[folderList[i].nID] = i;
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsMgr::SetupItemMap()
{
  itemID2Index.clear();
  
  for ( int i = 0; i < itemList.size(); ++i )
  {
    itemID2Index[itemList[i].nID] = i;
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Проверить, лежит ли nSubfolderID в nFolderID или в ее подпапках
bool CItemsMgr::IsSubfolder( int nFolderID, int nSubfolderID )
{
  while ( nSubfolderID > 0 )
  {
    if ( nSubfolderID == nFolderID )
      return true;
    nSubfolderID = GetParentID( nSubfolderID );
  }
  return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool CItemsMgr::HasVariant( int nItemIndex, int nVariantID )
{
	vector<int> &vec = itemList[nItemIndex].variants;
	if ( vec.empty() )
		GetItemVariants( itemList[nItemIndex].nID, &vec );
	vector<int>::iterator i = find( vec.begin(), vec.end(), nVariantID );
	return vec.end() != i;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CItemsMgr::FindVariant( int nVariantID )
{
	if ( !IsUniTemplate() )
		return -1;
	for ( int i = 0; i < itemList.size(); ++i )
	{
		vector<int> &vec = itemList[i].variants;
		if ( vec.empty() )
			GetItemVariants( itemList[i].nID, &vec );

		vector<int>::iterator it = find( vec.begin(), vec.end(), nVariantID );
		if ( vec.end() != it )
			return itemList[i].nID;
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetTableID( const string &szTable )
{
	int nItemsTable = -1;
	return nItemsTable;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить список св-в для итема nItemID
// Когда список становится ненужным, его необходимо удалить посредством ReleasePropList
const CPropMap* CItemsMgr::GetPropList( int nItemID, int nVariantID )
{
	if ( nItemID <= 0 )
		return 0;
	int nInd = GetItemIndex( nItemID );
	if ( -1 == nInd )
	{
		int nID, nFoldID;
		char szName[255];
		DWORD color;

		string szQuery = "SELECT ID, FolderID, UserName, MEUserColor FROM ";
		szQuery += szDBItemsTable;
		szQuery += " WHERE ID=";
		szQuery += IToA( nItemID );
		if ( FAILED( pDB->OpenQuery( szQuery, true ) ) )
			return 0;
		if ( pDB->MoveNext() != S_OK || !pDB->GetItem( &nID, &nFoldID, szName, &color ) )
			return 0;
		PushItem( nID, nFoldID, szName, color );
		nInd = GetItemIndex( nItemID );
		if ( -1 == nInd )
			return 0;
	}
  CPropMap *pProps = CreatePropMap( propMap );
  allocatedMaps.push_back( pProps );
	//
	if ( bUniTemplate )
	{
		if ( -1 == nVariantID )
		{
			if ( itemList[nInd].variants.empty() )
			{
				vector<int> vars;
				GetItemVariants( nItemID, &vars );
			}
			if ( !itemList[nInd].variants.empty() )
				nVariantID = itemList[nInd].variants[0];
		}
		if ( !HasVariant( nInd, nVariantID ) )
			return 0;
		pDB->OpenVariant( szVarTable, nVariantID, true, pProps );
	}
	else
		pDB->OpenItem( szDBItemsTable, nItemID, true, pProps );
	//
#ifdef _DEBUG
	vector<string> strs;
	for ( CPropMap::const_iterator it = pProps->begin(); it != pProps->end(); ++it )
		strs.push_back( it->first );
#endif
  pDB->ReadProps( pProps );
  for ( CPropMap::iterator it = pProps->begin(); it != pProps->end(); ++it )
	{
	  CDynamicCast<CListProp> plist(it->second.GetPtr());
		if (plist)
		{
			if ( bUniTemplate )
			{
				plist->SetValue( nVariantID );
				plist->SetInfo( GetTableID( szVarTable ), nVariantID );
			}
			else
			{
				plist->SetValue( nItemID );
				plist->SetInfo( GetTableID( szDBItemsTable ), nItemID );
			}
		}
    it->second->SetOwner( SOwner( nItemID, nVariantID ) );
	}
  pDB->Close();
  return pProps;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsMgr::ReleasePropList( const CPropMap* pPropMap )
{
  for ( list<CPropMap*>::iterator it = allocatedMaps.begin(); it != allocatedMaps.end(); ++it )
    if ( (*it) == pPropMap )
    {
      delete (*it);
      allocatedMaps.erase( it );
      return;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Добавить редактируемое св-во для данной таблицы
int CItemsMgr::AddProperty( int type, int viewType, const char *szPropName, const CVariant &defVal, int bReadOnly, int nGroup )
{
  // т.к имена свойств соотвествуют столбцам в базе данных
  // то все св-ва должны быть добавлены до загрузки из базы данных
  ASSERT( !IsLoaded() );

  SElemProp sprop;
  properties.push_back( sprop );

	int nPropID = ++nItemSafePropID;
	CProp *pProp = 0;
	if ( DT_RELLIST == viewType )
		pProp = new CInstancesList( szPropName, nPropID );
	else
		pProp = new CElemProp( szPropName, nPropID, type, viewType, this, defVal, bReadOnly );
	pProp->SetGroup( nGroup );
  propMap.insert( CPropMap::value_type( szPropName, pProp) );

  return nPropID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Изменить значение св-ва nValID итемаn nItemID на value
bool CItemsMgr::SetValue( SOwner itemID, const string &szPropName, const CVariant value, bool bModified )
{
  int ind = GetItemIndex( itemID.nItemID );
  
  if ( ind != -1 )
  {
    const CPropMap *pProps = GetPropList( itemID.nItemID, itemID.nVariantID );
		if ( !pProps )
			return false;
    CPropMap::const_iterator i = pProps->find( szPropName );
    if ( i == pProps->end() )
      return false;
    i->second->SetValue( value, false );
		if ( IsUniTemplate() )
		{
			pDB->OpenVariant( szVarTable, itemID.nVariantID, false, pProps );
			pDB->UpdateVariant( itemID.nItemID, pProps );
		}
		else
		{
			pDB->OpenItem( szDBItemsTable, itemID.nItemID, false, pProps );
			pDB->UpdateItem( itemList[ind].nFolderID, itemList[ind].szName.c_str(), pProps );
		}
    ReleasePropList( pProps );
    pDB->Close();
		return true;
  }
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// запись в базу свойств pProps для итема nItemID
bool CItemsMgr::SetItemProps( int nItemID, const CPropMap *pProps )
{
  int ind = GetItemIndex( nItemID );
  
  if ( -1 == ind )
		return false;
	bool bRet = false;
	if ( IsUniTemplate() )
	{
		vector<int> vars;
		GetItemVariants( nItemID, &vars );
		if ( vars.empty() )
			return false;
		pDB->OpenVariant( szVarTable, vars.front(), false, pProps );
		bRet = pDB->UpdateVariant( nItemID, pProps );
	}
	else
	{
		pDB->OpenItem( szDBItemsTable, nItemID, false, pProps );
		bRet = pDB->UpdateItem( itemList[ind].nFolderID, itemList[ind].szName.c_str(), pProps );
	}
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemsMgr::SetVariantProps( int nItemID, int nVarID, const CPropMap *pProps )
{
	if ( !IsUniTemplate() )
	{
		ASSERT( false );
		return false;
	}
	int ind = GetItemIndex( nItemID );
  if ( -1 == ind )
		return false;
	if ( !HasVariant( ind, nVarID ) )
		return false;
	pDB->OpenVariant( szVarTable, nVarID, false, pProps );
	return pDB->UpdateVariant( nItemID, pProps );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Связать св-во szPropName с таблицей nTableID
bool CItemsMgr::SetRelation( const char *szPropName, int nTableID )
{
  CPropMap::iterator it = propMap.find( szPropName );

  if ( it == propMap.end() )
    return false;
  // если поле не может быть индексом, то связь установить нельзя
  if ( it->second->GetType() != CVariant::VT_INT )
    return false;
/*
  // проверям на повторяющиеся связи
  for ( CPropMap::const_iterator cit = propMap.begin(); cit != propMap.end(); ++cit )
    if ( cit->second->GetRelation() == nTableID )
    {
      cit->second->SetGroup( nTableID );
      it->second->SetGroup( nTableID ); // не должно по идее много лишних раз вызываться :)
    }
*/
	if ( it->second->GetGroup() == CProp::PROPDEF_GROUP )
		it->second->SetGroup( nTableID );
  it->second->SetRelation( nTableID );
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Выставить директорию-префикс для browse-свойства
bool CItemsMgr::SetPrefix( const char *szPropName, const char *szStr )
{
  CPropMap::iterator it = propMap.find( szPropName );
  if ( it == propMap.end() )
    return false;
  it->second->SetPrefix( szStr );
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemsMgr::SetTip( const char *szPropName, const char *szStr )
{
  CPropMap::iterator it = propMap.find( szPropName );  
  if ( it == propMap.end() )
    return false;
  it->second->SetTip( szStr );
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemsMgr::SetTip( const char *szPropName, int nResStrID )
{
	CString str;
	if ( !str.LoadString( nResStrID ) )
		return false;
	return SetTip( szPropName, str );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Добавление строки в комбобокс св-ва
bool CItemsMgr::AddString( const char *szPropName, const char *szStr )
{
  CPropMap::iterator it = propMap.find( szPropName );
  
  if ( it == propMap.end() )
    return false;
  it->second->AddString( szStr );
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CItemsMgr::GetItemByProp( const string &szProp, const CVariant &val ) const
{
	if ( CVariant::VT_NULL == val.GetType() )
		return -1;
	if ( IsUniTemplate() )
	{
		if ( FAILED( pDB->FindOpenVariant( szVarTable, szProp, val ) ) || FAILED( pDB->MoveNext() ) )
			return -1;
		int nVarID, nID;
		if ( !pDB->GetVariant( &nVarID, &nID ) )
			return -1;
		return nID;
	}
  if ( FAILED( pDB->FindOpen( szDBItemsTable, szProp, val ) ) || FAILED( pDB->MoveNext() ) )
		return -1;
	//
	int nID, nFoldID;
	char szName[MAX_DBSTRING];
	DWORD color;
	
	if ( !pDB->GetItem( &nID, &nFoldID, szName, &color ) )
		return -1;
	return nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string CItemsMgr::GetItemPath( int nItemID ) const
{
	if ( GetItemIndex( nItemID ) < 0 )
		return "";
	int fID = GetItemFolderID( nItemID );
	return GetFolderPath( fID ) + GetItemName( nItemID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string CItemsMgr::GetFolderPath( int nFolderID ) const
{
	vector<string> path;
	while ( nFolderID > 0 )
	{
		const char *psz = ID2FolderName( nFolderID );
		if ( !psz )
			break;
		path.push_back( string( psz ) + "\\" );
		nFolderID = GetParentID( nFolderID );
	}
	string szPath;
	for ( vector<string>::reverse_iterator rit = path.rbegin(); rit != path.rend(); ++rit )
		szPath += *rit;
	return szPath;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить список вариантов
// Возвр. false, если ресурс не явл. универсальным темплейтом или итем не найден
bool CItemsMgr::GetItemVariants( int nItemID, vector<int> *pVarIDs )
{
	ASSERT( pVarIDs );
	if ( !IsUniTemplate() )
	{
		ASSERT( false );
		return false;
	}
	int nInd = GetItemIndex( nItemID );
	if ( nInd <  0 )
		return false;
	itemList[nInd].variants.clear();
	const string szQuery = string( "SELECT ID, TemplateID FROM " ) + szVarTable + " WHERE TemplateID=" + IToA( nItemID );
	if ( FAILED( pDB->OpenQuery( szQuery, true ) ) )
		return false;
	while ( S_OK == pDB->MoveNext() )
	{
		int nVarID, nTemplateID;
		if ( !pDB->GetVariant( &nVarID, &nTemplateID ) )
			return false;
		itemList[nInd].variants.push_back( nVarID );
	}
	*pVarIDs = itemList[nInd].variants;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// получение флагов для варианта nVariantID
// может существовать несколько наборов флагов для варианта
bool CItemsMgr::GetVariantFlags( int nVariantID, vector<unordered_map<int, bool> > *pFlags )
{
	ASSERT( pFlags );
	if ( !IsUniTemplate() )
	{
		ASSERT( false );
		return false;
	}
	pFlags->clear();
	
	CPropMap props;
	props["Flags"] = new CElemProp( "Flags", 1, CVariant::VT_STR, 0, this, CVariant() );
	
	if ( FAILED( pDB->OpenVariant( szVarTable, nVariantID, true, &props ) ) )
		return false;
	if ( !pDB->ReadProps( &props ) )
		return false;
	//
	string szFlags = props["Flags"]->GetValue();
	if ( szFlags.empty() )
		return true;
	vector<string> sets;
	// наборы флагов между собой разделяются ';'
	NStr::SplitString( szFlags, sets, ';' );
	pFlags->resize( sets.size() );
	for ( int i = 0; i < sets.size(); ++i )
	{
		vector<string> szAttrInds;
		// идексы флагов разделяются ','
		NStr::SplitString( sets[i], szAttrInds, ',' );
		for ( int j = 0; j < szAttrInds.size(); ++j )
			(*pFlags)[i][atoi( szAttrInds[j].c_str() )] = true;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemsMgr::SetVariantFlags( int nVariantID, const vector<unordered_map<int, bool> > &flags )
{
	if ( !IsUniTemplate() )
	{
		ASSERT( false );
		return false;
	}
	
	CPropMap props;
	props["Flags"] = new CElemProp( "Flags", 1, CVariant::VT_STR, 0, this, CVariant() );
	
	if ( FAILED( pDB->OpenVariant( szVarTable, nVariantID, true, &props ) ) )
		return false;

	string szFlags;
	for ( int i = 0; i < flags.size(); ++i )
	{
		for ( unordered_map<int, bool>::const_iterator j = flags[i].begin(); j != flags[i].end(); ++j )
			if ( j->second )
				szFlags += IToA( j->first ) + ',';
		NStr::TrimRight( szFlags, ',' );
		szFlags += ';';
	}
	NStr::TrimLeft( szFlags, ",;" );
	NStr::TrimRight( szFlags, ",;" );

	props["Flags"]->SetValue( szFlags, false ); // CRAP чтобы правильно работал SetValue, нужен ownerID
	return pDB->UpdateVariant( -1, &props );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CItemsMgr::DoesFolderExist( const string &szFolder, int nParentID )
{
	MoveFirstFolder();
	while ( MoveNextFolder() )
	{
		string sz = ID2FolderName( GetFolderID() );
		if ( sz == szFolder )
		{
			if ( nParentID <= 0 )
				return GetFolderID();
			else if ( GetParentID( GetFolderID() ) == nParentID )
				return GetFolderID();
		}
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemsMgr::SetItemColor( int nItemID, DWORD cr )
{
	int ind = GetItemIndex( nItemID );
	if ( ind == -1 )
		return false;
	if ( !pDB->SetItemColor( szDBItemsTable, nItemID, cr ) )
		return false;
	itemList[ind].color = cr;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD CItemsMgr::GetItemColor( int nItemID ) const
{
	int ind = GetItemIndex( nItemID );
	if ( ind != -1 )
		return itemList[ind].color;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS CElemProp
////////////////////////////////////////////////////////////////////////////////////////////////////
CElemProp::CElemProp( const string &szName, int nID, int nType, int nViewType, CItemsMgr *pItemsMgr, const CVariant& _defValue, bool bReadOnly )
  : CProp( szName, nID, nType, nViewType, bReadOnly ), pItems( pItemsMgr ), defValue( _defValue )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить значение св-ва
const CVariant& CElemProp::GetValue() const
{
  ASSERT( pItems );
  return value;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить значение св-ва
const CVariant CElemProp::GetDefValue() const
{
  return defValue;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Изменить значение св-ва
void CElemProp::SetValue( const CVariant &newValue, bool bModified ) const
{
  ASSERT( pItems );
	bool bSet = true;
  if ( bModified )
    bSet = pItems->SetValue( GetOwnerID(), GetName(), newValue, bModified );
	if ( bSet )
		*(const_cast<CVariant*>(&value)) = newValue;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CProp* CElemProp::Clone() const
{
	CElemProp *pClone = new CElemProp;
	*pClone = *this;
	return pClone;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
