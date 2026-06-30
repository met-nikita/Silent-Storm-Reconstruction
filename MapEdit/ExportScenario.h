#ifndef __EXPORTSCENARIO_H_
#define __EXPORTSCENARIO_H_

class CItemsMgr;
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportScenario( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport );
void CreateScenarioXLS( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport );
void ReadScenarioXLS( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __EXPORTSCENARIO_H_