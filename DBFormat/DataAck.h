#ifndef __DATAACK_H_
#define __DATAACK_H_

#include "..\ADOImport\BasicDB.h"
#include "DataFaceGen.h"	// EFaceExpression (retail SAckVoice carries the phrase's facial expression)

namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_ACKINFO_MAX_COUNT = 3;
const int N_ACK_MAX_PARAM_COUNT = 3;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGPers;
class CSound;
class CString;
class CSequence;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBAckCondition  (reconstructed from release Game.exe; saveload id 0x23075b00)
// operator& does NOT chain to CDBRecord (matches the binary), so no ZPARENT.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBAckCondition: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBAckCondition );
public:
	ZDATA
	int   nPriority;
	float fProbability;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nPriority); f.Add(3,&fProbability); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBAckInfo
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAckVoice
{
	ZDATA
	CPtr<CSound> pSound; // ����
	CPtr<CSequence> pSequence; // ��� ������
	// retail SAckVoice +0x8 (sizeof 12, gen/include/s2_types.h:13221): the phrase's facial
	// expression -- one "FaceExpression" column seeds it for all six voice slots (Import @0x42d280);
	// the dialog UI resolves it to an emotion sequence via GetSequenceByExpression @0x42cf30.
	EFaceExpression eExpression;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pSound); f.Add(3,&pSequence); f.Add(4,&eExpression); return 0; }
	//
	SAckVoice(): eExpression( FE_NORMAL ) {}
	SAckVoice( CSound *_pSound, CSequence *_pSequence ): pSound( _pSound ), pSequence( _pSequence ), eExpression( FE_NORMAL ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBAckInfo: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBAckInfo );
	ZDATA
	ZPARENT( CDBRecord );
public:
	int nRPGPersID; // whose ack this is (the owning persona id)
	CPtr<CString> pText; // the ack's display text
	vector< SAckVoice > voices;
	// retail CDBAckInfo +0x24 (operator& @0x420090 tag 6; Import @0x42d280 column "FemaleStringID"):
	// the female-voiced variant of the ack text; without it female units show the male ack text.
	CPtr<CString> pFemaleText;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&nRPGPersID); f.Add(4,&pText); f.Add(5,&voices); f.Add(6,&pFemaleText); return 0; }

	virtual void Import();
	const SAckVoice& GetVoice( int nVoice ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBAckSequence
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBAckSequence: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBAckSequence );
public:
	CPtr<CDBAckInfo> pDBAckInfo[N_ACKINFO_MAX_COUNT]; // ack-�
	int nPriority; // ���������

	int operator&( CStructureSaver &f );
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBAck
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBAck: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBAck );
public:
	float fProbability; // ����������� ����������
	int nRPGPersID; // ��� ����������� �������
	CPtr<CDBAckSequence> pAckSequence; // ������������������ ack
	string sParam[N_ACK_MAX_PARAM_COUNT]; // ��������� �������
	int nConditionID; // ����� ������������ ������� ( ������������� ������ )
	// retail CDBAck::pCondition (+0x40; Import @0x42d0b0 resolves the SAME "ConditionID" column as a
	// RECORD REF via ImportField<CDBAckCondition>; operator& @0x42d110 tag 9). CRITICAL with the
	// Steam game.db: EVERY AckSeqs row has Priority 0 -- the retail ack priorities (death=10,
	// order-confirm=1, ...) and the per-condition probability FACTORS (order-confirm 0.1,
	// enemy-visible 0.2, ...) live ONLY on these AckConditions records (retail GetAckPriority
	// @0x338e40 / roulette weight @0x339230). Without this link the dev ack competition ran at
	// all-zero priority / raw weight.
	CPtr<CDBAckCondition> pCondition;

	int operator&( CStructureSaver &f );
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBDialogSeq
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBDialogSeq: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBDialogSeq );
	ZDATA
	ZPARENT( CDBRecord );
public:
	int nDialogID;
	CPtr<CDBAckInfo> pAckInfo;
	int nAckInfoOrder;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&nDialogID); f.Add(4,&pAckInfo); f.Add(5,&nAckInfoOrder); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBDialog
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBDialog: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBDialog );
	ZDATA
	ZPARENT( CDBRecord );
public:
	string szCode;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&szCode); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBDialogPers
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBDialogPers: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBDialogPers );
	ZDATA
	ZPARENT( CDBRecord );
public:
	int nPersID; // we don't need CPtr<CRPGPers>
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&nPersID); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CDBAck* GetDBAck( int nID );
CDBDialog* GetDBDialog( int nID );
NDb::CDBDialog* GetDBDialogByCode( const string &szCode );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif