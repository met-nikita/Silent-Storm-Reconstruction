#ifndef __EXPORTACKS_H_
#define __EXPORTACKS_H_
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckInfoAccessor
{
public:
	LONG  m_nID;
	TCHAR m_szUserName[256];
	LONG  m_nWhoID;
	LONG  m_nStringID;
	TCHAR m_szIntonation[256];
	LONG  m_nSoundID;
	LONG  m_nSoundID1;
	LONG  m_nSoundID2;
	LONG  m_nSoundID3;
	LONG  m_nSoundID4;
	LONG  m_nSoundID5;

	BEGIN_ACCESSOR_MAP( CAckInfoAccessor, 4 )
		BEGIN_ACCESSOR( 0, true )
		COLUMN_ENTRY(1, m_nID)
		COLUMN_ENTRY(2, m_szUserName)
		COLUMN_ENTRY(4, m_nWhoID)
		COLUMN_ENTRY(5, m_nStringID)
		COLUMN_ENTRY(6, m_szIntonation)
		COLUMN_ENTRY(7, m_nSoundID)
	END_ACCESSOR()
	BEGIN_ACCESSOR( 1, false )
		COLUMN_ENTRY(2, m_szUserName)
		COLUMN_ENTRY(4, m_nWhoID)
		COLUMN_ENTRY(5, m_nStringID)
		COLUMN_ENTRY(6, m_szIntonation)
		COLUMN_ENTRY(7, m_nSoundID)
	END_ACCESSOR()
	BEGIN_ACCESSOR( 2, false )
		COLUMN_ENTRY(2, m_szUserName)
		COLUMN_ENTRY(4, m_nWhoID)
		COLUMN_ENTRY(5, m_nStringID)
		COLUMN_ENTRY(6, m_szIntonation)
		COLUMN_ENTRY(7, m_nSoundID)
		COLUMN_ENTRY(10, m_nSoundID1)
		COLUMN_ENTRY(12, m_nSoundID2)
		COLUMN_ENTRY(14, m_nSoundID3)
		COLUMN_ENTRY(16, m_nSoundID4)
		COLUMN_ENTRY(18, m_nSoundID5)
	END_ACCESSOR()
	BEGIN_ACCESSOR( 3, false )
		COLUMN_ENTRY(7, m_nSoundID)
		COLUMN_ENTRY(10, m_nSoundID1)
		COLUMN_ENTRY(12, m_nSoundID2)
		COLUMN_ENTRY(14, m_nSoundID3)
		COLUMN_ENTRY(16, m_nSoundID4)
		COLUMN_ENTRY(18, m_nSoundID5)
	END_ACCESSOR()
	END_ACCESSOR_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const int ACK_STRING_FOLDER = 11;
const int ACK_SOUND_FOLDER = 83;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckInfoDB: public CBaseDBCmd<CAccessor<CAckInfoAccessor> >
{
public:
	bool OpenInfo( int nID );
	string GetInfoString();
	void GetInfoSound( string soundFiles[6] );
	int Insert( const string &szUserName, int nStrID, int nSndID, int nWhoID, const string &szIntonation );
	int InsertHero( const string &szUserName, int nStrID, 
		int nSndID, int nSnd1, int nSnd2, int nSnd3, int nSnd4, int nSnd5,
		int nWhoID, const string &szIntonation );
	void DeleteInfo( int nID );
	bool DeleteSounds( int nID );
	bool SetSounds( int nAckInfoID, int nSndID, int nSnd1, int nSnd2, int nSnd3, int nSnd4, int nSnd5 );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAckOperation
{
	string szCondition;
	string szText;
	string szIntonation;
	string szFile;
	bool b6Voices;
	string szSndFiles[6];
	SAckOperation() : b6Voices(false) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void LogAckOperation( const vector<SAckOperation> &ops, const string &szTable );
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef unordered_map<string, int> CFolderHash;
int GetFolder( int nTreeID, int nRootFolder, const string &szPath, CFolderHash &fhash );
int AddString( int nFolder, const string &szName, const string &szText );
int AddSound( int nFolder, const string &szName, const string &szSrc );
string GetAckName( int nPersID );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __EXPORTACKS_H_