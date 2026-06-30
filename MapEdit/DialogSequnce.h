#ifndef __DIALOGSEQUNCE_H_
#define __DIALOGSEQUNCE_H_
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDialogSeqAccessor
{
public:
	LONG  m_nDialogID;
	LONG  m_nAckInfoID;
	LONG  m_nID;

	BEGIN_ACCESSOR_MAP( CDialogSeqAccessor, 2 )
		BEGIN_ACCESSOR( 0, true )
		COLUMN_ENTRY(1, m_nDialogID)
		COLUMN_ENTRY(2, m_nAckInfoID)
		COLUMN_ENTRY(3, m_nID)
	END_ACCESSOR()
	BEGIN_ACCESSOR( 1, false )
		COLUMN_ENTRY(1, m_nDialogID)
		COLUMN_ENTRY(2, m_nAckInfoID)
	END_ACCESSOR()
	END_ACCESSOR_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SDialogSeqEntry
{
	int nSeqID;
	string szText;
	string szIntonation;
	int nPersID;
	int nAckInfoID;
};
struct SAckOperation;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDialogSeqDB: public CBaseDBCmd<CAccessor<CDialogSeqAccessor> >
{
public:
	void GetSequence( vector<SDialogSeqEntry> *pSequnce, int nDialogID );
	bool DeleteEntry( SAckOperation *pRes, int nSeqID );
	int  InsertEntry( int nDialogID, int nAckInfoID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DIALOGSEQUNCE_H_