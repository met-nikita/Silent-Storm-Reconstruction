#include "stdafx.h"
#include "DataAck.h"
#include "DataFormat.h"
#include "DataSound.h"

namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBAckCondition  (reconstructed from release; columns + id verbatim)
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBAckCondition::Import()
{
	NDatabase::ImportField( "Priority", &nPriority );
	NDatabase::ImportField( "Probability", &fProbability );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBAckInfo
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBAckInfo::Import()
{
	NDatabase::ImportField( "WhoID", &nRPGPersID );
	NDatabase::ImportField( "StringID", &pText );
	voices.resize( 6 );
	NDatabase::ImportField( "SoundID", &voices[ 0 ].pSound );
	NDatabase::ImportField( "HeadSequenceID", &voices[ 0 ].pSequence );
	NDatabase::ImportField( "SoundID1", &voices[ 1 ].pSound );
	NDatabase::ImportField( "HeadSequenceID1", &voices[ 1 ].pSequence );
	NDatabase::ImportField( "SoundID2", &voices[ 2 ].pSound );
	NDatabase::ImportField( "HeadSequenceID2", &voices[ 2 ].pSequence );
	NDatabase::ImportField( "SoundID3", &voices[ 3 ].pSound );
	NDatabase::ImportField( "HeadSequenceID3", &voices[ 3 ].pSequence );
	NDatabase::ImportField( "SoundID4", &voices[ 4 ].pSound );
	NDatabase::ImportField( "HeadSequenceID4", &voices[ 4 ].pSequence );
	NDatabase::ImportField( "SoundID5", &voices[ 5 ].pSound );
	NDatabase::ImportField( "HeadSequenceID5", &voices[ 5 ].pSequence );
}
//////////////////////////////////////////////////////////////////////////////////////	
const SAckVoice& CDBAckInfo::GetVoice( int nVoice ) const
{
	ASSERT( nVoice >= 0 && nVoice <= 5 );
	if ( nVoice >= 0 && nVoice <= 5 && IsValid( voices[ nVoice ].pSound ) )
		return voices[ nVoice ];
	else
		return voices[ 0 ]; // default voice
}
//////////////////////////////////////////////////////////////////////////////////////	
// CDBAckSequence
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBAckSequence::Import()
{
	NDatabase::ImportField( "Priority", &nPriority );
	NDatabase::ImportField( "AckID0", &pDBAckInfo[0] );
	NDatabase::ImportField( "AckID1", &pDBAckInfo[1] );
	NDatabase::ImportField( "AckID2", &pDBAckInfo[2] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CDBAckSequence::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &nPriority );
	for ( int i = 0; i < N_ACKINFO_MAX_COUNT; ++i )
		f.Add( 3 + i ,&pDBAckInfo[i] );
	return 0; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBAck
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBAck::Import()
{
	NDatabase::ImportField( "WhoID", &nRPGPersID );
	NDatabase::ImportField( "Probability", &fProbability );
	NDatabase::ImportField( "AckSeqID", &pAckSequence );
	NDatabase::ImportField( "ConditionID", &nConditionID );
	NDatabase::ImportField( "Param0", &sParam[0] );
	NDatabase::ImportField( "Param1", &sParam[1] );
	NDatabase::ImportField( "Param2", &sParam[2] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CDBAck::operator&( CStructureSaver &f ) 
{
	f.Add(1,(CDBRecord*)this);
	f.Add(2,&fProbability); 
	f.Add(3,&nRPGPersID);
	f.Add(4,&pAckSequence); 
	f.Add(5,&nConditionID); 
	for ( int i = 0; i < N_ACK_MAX_PARAM_COUNT; ++i )	
		f.Add( 6+i ,&sParam[i] );
	return 0; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CDBAck* GetDBAck( int nID ) 
{ 
	CDBTable<CDBAck> *pTable = NDatabase::GetTable<CDBAck>();
	if ( !pTable )
		return 0;
	return pTable->GetRecord( nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBDialog
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBDialog::Import()
{
	NDatabase::ImportField( "Code", &szCode );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBDialogSeq
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBDialogSeq::Import()
{
	NDatabase::ImportField( "DialogID", &nDialogID );
	NDatabase::ImportField( "AckInfoID", &pAckInfo );
	NDatabase::ImportField( "AckInfoOrder", &nAckInfoOrder );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBDialogPers
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBDialogPers::Import()
{
	NDatabase::ImportField( "PersID", &nPersID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CDBDialog* GetDBDialogByCode( const string &szCode )
{
	CDBTable<NDb::CDBDialog> *pTable = NDatabase::GetTable<NDb::CDBDialog>();
	CDBIterator<NDb::CDBDialog> i(*pTable);
	while ( pTable && i.MoveNext() )
	{
		CDBPtr<NDb::CDBDialog> pDialog = i.Get();
		if ( IsValid( pDialog ) && stricmp( pDialog->szCode.c_str(), szCode.c_str() ) == 0 )
			return pDialog;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NDb;
//
REGISTER_SAVELOAD_CLASS( 0x52512130, CDBAckInfo );
REGISTER_SAVELOAD_CLASS( 0x52512131, CDBAckSequence );
REGISTER_SAVELOAD_CLASS( 0x52512132, CDBAck );
REGISTER_SAVELOAD_CLASS( 0x23075b00, CDBAckCondition );
REGISTER_SAVELOAD_CLASS( 0x53102110, CDBDialog );
REGISTER_SAVELOAD_CLASS( 0x51322120, CDBDialogSeq );
REGISTER_SAVELOAD_CLASS( 0x51722140, CDBDialogPers );