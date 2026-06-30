#include "StdAfx.h"
#include "MapBuild.h"

using namespace NDb;
/*
/////////////////////////////////////////////////////////////////////////////////////
void TraverseTemplateTree( const CPtr<CTemplate> &pTemplate, 
                            vector<SMapElement> &items, 
                            int nRotation, 
                            const CVec3 &ptLeftBottom )
{
  if ( !pTemplate->IsValid() )
    return;

  const float scale = float( pTemplate->variants.size() ) / RAND_MAX;
  int iRan = (int)(scale * rand());
  
  CPtr<CTemplVariant> pVar = pTemplate->variants[iRan];
  CPtr<CFinalElement> pFin = pVar->pFinalElement;

  if ( pFin->IsValid() )
  {
    SMapElement me;
    
    me.nModelID = pFin->GetRecordID();
    me.ptPos = ptLeftBottom;
    me.nRotation = nRotation + pFin->nRotation;
    items.push_back( me );
    return;
  }

  const int nmax = pVar->rects.size();
  for ( int i=0; i<nmax; ++i )
  {
    CPtr<CRectangle> pRec = pVar->rects[i];
    TraverseTemplateTree( pRec->pTemplate, 
                          items, 
                          nRotation + pRec->nRotation, 
                          ptLeftBottom + CVec3( (float)pRec->rect.left, (float)pRec->rect.bottom, 0 ) );
  }
}
/////////////////////////////////////////////////////////////////////////////////////
bool BuildMap( const CPtr<CTemplate> &pTemplate, vector<SMapElement> &items )
{
  TraverseTemplateTree( pTemplate, items, 0, CVec3( 0, 0, 0 ) );
  return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool BuildMap( int nTemplateID, vector<SMapElement> &items )
{
  CPtr<CTemplate> pTempl = new CTemplate;
  CDBTable *pTemplTable = NDatabase::GetTable( NDatabase::GetTableID( pTempl ) );

  if ( !pTemplTable )
    return false;

  CDBIterator it( *pTemplTable );

  while ( it.MoveNext() )
  {
    pTempl = (CTemplate*)it.Get();
    if ( pTempl && pTempl->GetRecordID() == nTemplateID )
      return BuildMap( pTempl, items );
  }
  return false;
}
/////////////////////////////////////////////////////////////////////////////////////
*/