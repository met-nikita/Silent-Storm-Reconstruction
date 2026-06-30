#ifndef __TEMPLMGR_H__
#define __TEMPLMGR_H__

#include <map>

class CPlacement;
class CTemplate;
class CItemsMgr;
class CFinElement;

////////////////////////////////////////////////////////////////////////////////////////////////////
class CTemplateMgr
{
private:
  list<CPlacement* >  placementList;
  vector<CTemplate* >   templates;
  vector<CFinElement* > finElements;
  vector<int>     modifiedTemplates;            // используется в UpdateModified()
  vector<int>     modifiedFinElems;
  vector<int>     modifiedUnits;
  vector<int>     modifiedFloors;
  std::map<int, int> templID2Ind;               // отображение ID темплейта в индекс в векторе templates
  std::map<int, int> plcmnID2FinInd;            // отобр. ID расстановки в индекс в векторе finElements
  std::map<int, int> finID2FinInd;              // отобр. ID конеч. элем. в индекс в векторе finElements
  int  iMaxWallID;                              // для получения временных wallID
  int  iCurTempl;                               // указатель на текущий темплейт

  CTemplateMgr( const CTemplateMgr &tm ) {}

  bool LoadFinElements();
  bool LoadTemplateTree();
	bool LoadPlacement( int nVarID, CPlacement *pPl );

  void SetupTemplateMap();
  void SetupFinElemMap();
  int  GetTemplIndex( int id ) const;
  int  GetFinIndex( int id ) const;
  int  GetFinIndex4ElemID( int id ) const;
  bool UpdateModifiedTempls();
  bool UpdateModifiedFinElems();

public:
  CTemplateMgr();
  ~CTemplateMgr();

  bool Create();

  int  GetTempWallID();
  bool UpdateModified();

  // Функции для работы с темплейтами

  void MoveFirstTempl();
  bool MoveNextTempl();
  int  GetCurTemplID();
  CTemplate*   GetCurTempl();
  CTemplate*   GetTempl( int templID ) ;
  void SetModifiedTempl( int templID );
  bool AddTemplate( int nTemplID, int width, int height );

  // Функции для работы с расстановками

  bool IsFinalElement( int placementID );
  CPlacement*  GetPlacement( int id );
	void ReleasePlacement( CPlacement *pPl );
  CFinElement* GetFinElement( int id );
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//    GLOBAL Variables!
extern CTemplateMgr theTemplMgr;

////////////////////////////////////////////////////////////////////////////////////////////////////
//    Inline Functions
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить индекс в векторе темплейтов по ID темплейта
inline int CTemplateMgr::GetTemplIndex( int id ) const 
{
  std::map<int, int>::const_iterator it = templID2Ind.find( id );
  if ( templID2Ind.end() == it )
    return -1;
  return it->second;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить индекс в векторе конечных элементов по ID расстановки
inline int CTemplateMgr::GetFinIndex( int id ) const
{
  std::map<int, int>::const_iterator it = plcmnID2FinInd.find( id );
  if ( plcmnID2FinInd.end() == it )
    return -1;
  return it->second;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить индекс в векторе  конечных элементов по ID элемента
inline int CTemplateMgr::GetFinIndex4ElemID( int id ) const
{
  std::map<int, int>::const_iterator it = finID2FinInd.find( id );
  if ( finID2FinInd.end() == it )
    return -1;
  return it->second;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// В этой расстановке находится конечный элемент или прямоугольники ?
// В первом случае расстановка представляется типом CFinElement, во втором CPlacement
inline bool CTemplateMgr::IsFinalElement( int placementID )
{
  return GetFinIndex( placementID ) != -1;
}

#endif // __TEMPLMGR_H__