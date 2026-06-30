#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////////
class CSpin: public CObjectBase
{
	OBJECT_BASIC_METHODS(CSpin);

	int nID;
	struct SSpin{
		CSpinButtonCtrl *pSpin;
		CButton *pStatic;
		SSpin(): pSpin(new CSpinButtonCtrl), pStatic(new CButton) {}
		~SSpin() { delete pSpin; delete pStatic; }
	} s;
	vector<int> variants;
	int nActiveVar;
	void SetPos( int nPos );

public:
	CSpin() {}
	CSpin( CWnd *pParent, int nID );

	void Move( int nColumn, int nRow );
	void SetVariants( const vector<int> &variants );
	bool SetActiveVariant( int nID );
	bool NextVar();
	bool PrevVar();
	bool SetActiveVariantIndex( int nIndex );

	int  GetID() const { return nID; }
	int  GetActiveVariant() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
