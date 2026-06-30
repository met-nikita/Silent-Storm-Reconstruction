#ifndef __UNIT_H__
#define __UNIT_H__

#include "FinalElem.h"

class CUnitDBCmd;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitElement : public CFinElement
{
public:
  CUnitElement( CFinDBCmd *pDB, int varID, int elemID );
  virtual ~CUnitElement() {}

  virtual int  GetModelID() const;
  virtual bool DeleteFromDB();  
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __UNIT_H__