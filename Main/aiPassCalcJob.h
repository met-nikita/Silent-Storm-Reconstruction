#ifndef __aiPassCalcJob_H_
#define __aiPassCalcJob_H_
#include "aiPosition.h"
namespace NAI
{
	class CLayersGroup;
	void AddNewPassCalcerJob( IAIJobManager *pWhere, IAIMap *pMap, CLayersGroup *pGroup, const SPathPlace &_where );
}
#endif
