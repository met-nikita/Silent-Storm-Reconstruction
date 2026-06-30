#ifndef __TEMPL_H__
#define __TEMPL_H__

#include <map>
#include <string>

using std::string;
class CVariantsDBCmd;

////////////////////////////////////////////////////////////////////////////////////////////////////
class CTemplate
{
private:
  int           id;
  std::string   name;
  int           type;               // пака, в которой находится темплейт
  int           width;
  int           height;
  vector<int>   variants;
  int           refCnt;             // кол-во ссылок на данный темпл. во всех расстановках
  int           iCurPlacement;      // указатель на текущую расстановку
  std::map<int, int> varID2Ind;     // отображение ID расстановки в индекс в векторе variants

  bool          bInitialized;
	CVariantsDBCmd *const pDBVars;

  CTemplate( const CTemplate& t ) : pDBVars( 0 ) {}

  void SetupPlacementMap();

  // Получение индекса в variants по ID расстановки
  // "-1", если ID не найден
  int  GetPlacementIndex( int id ) const 
  {
    std::map<int, int>::const_iterator it = varID2Ind.find( id );
    if ( varID2Ind.end() == it )
      return -1;
    return it->second;
  }

public:
  CTemplate( CVariantsDBCmd *pDBVars );
  ~CTemplate();

  void  Init( int id, int type, int width, int height, const char* name );

  int   GetID() const { return id; }
  const char* GetName() const { return name.c_str(); }
  int   GetWidth()  const { return width; }
  int   GetHeight() const { return height; }
  int   GetTypeID() const;
  
  bool  IsRootNode() const;
  bool  IsInitialized() const { return bInitialized; }

  void  SetName( const char *name );
  bool  SetType( int typeID );
  void  AddPlacement( int varID, bool bSetModified = true );
  int   AddPlacement();

  void  AddRef() { ++refCnt; }
  void  Release() { --refCnt; }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  Inline functions 
////////////////////////////////////////////////////////////////////////////////////////////////////
// истина, если на темплейт никто не ссылается
inline bool CTemplate::IsRootNode() const 
{ 
  return 0 == refCnt; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// тип темплейта (индекс в таблице типов)
inline int CTemplate::GetTypeID() const
{
  return type;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CTemplate::SetName( const char *_name )
{
  name = _name;
  theTemplMgr.SetModifiedTempl( id );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __TEMPL_H__