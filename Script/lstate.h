/*
** $Id: lstate.h,v 1.41 2000/10/05 13:00:17 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/

#ifndef lstate_h
#define lstate_h

#include "lobject.h"
#include "lua.h"
#include "luadebug.h"



typedef int StkId;  /* index to stack elements */

/*
** marks for Reference array
*/
#define NONEXT          -1      /* to end the free list */
#define HOLD            -2
#define COLLECTED       -3
#define LOCK            -4

struct Ref {
	ZDATA
  TObject o;
  int st;  /* can be LOCK, HOLD, COLLECTED, or next (for free list) */
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&o); f.Add(3,&st); return 0; }
};

struct TM;  /* defined in ltm.h */

//////////////////////////////////////////////////////////////////////////
/// The current state of Lua internal virtual mashine
struct SLuaVMState
{
	ZDATA
	int nProto; // pointer to tf
	int nClosure;
	StkId base;
  int currPC;
	int nResults;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nProto); f.Add(3,&nClosure); f.Add(4,&base); f.Add(5,&currPC); f.Add(6,&nResults); return 0; }
};
//////////////////////////////////////////////////////////////////////////
class CLuaThread : public CObjectBase
{
	OBJECT_NOCOPY_METHODS(CLuaThread);
public:
	ZDATA
  StkId top;  // first free slot in the stack
  StkId Cbase;  // base for current C function
  vector<TObject> stack;  // stack base
	vector<SLuaVMState> executedCalls;
	int thisThreadIsSleeping;
	bool bErrorInThread;
	// retail CLuaThread +0x34 (PDB, 7 members): the thread's debug name, save tag 8 (@0x3e0940).
	// Disasm-proven creation names: "First thread" (lua_open), the FILENAME (lua_dofile),
	// "Buffer thread" (lua_dobuffer), "Message thread" (message), "thread made by StartThread"
	// (lua_startThread), "Thread to have at least one" (lua_executeThreads), the called function
	// name (NScript::luaCallFunction).
	string name;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&top); f.Add(3,&Cbase); f.Add(4,&stack); f.Add(5,&executedCalls); f.Add(6,&thisThreadIsSleeping); f.Add(7,&bErrorInThread); f.Add(8,&name); return 0; }

	CLuaThread( const char *szName = "" );   // retail ctor @0x3de050 takes the debug name
	bool HasValidTop() const { return top >= 0 && top < stack.size(); }
};

CLuaThread* lua_newThread( lua_State *L, const char *szName = "" );   // retail @0x3de190 (L, name)
void lua_setThread( lua_State *L, CLuaThread *pThread );

//////////////////////////////////////////////////////////////////////////
template<class T>
class CVectorList
{
	struct SHolder
	{
		int nNextFree;
		T *pData;
		SHolder() {}
		SHolder( T* pT ) : pData(pT) {}
		int operator&( CStructureSaver &f )
		{
			bool bValid;
			if ( !f.IsReading() )
				bValid = pData;
			f.Add( 2, &nNextFree );
			f.Add( 3, &bValid );
			if ( bValid )
			{
				if ( f.IsReading() )
					pData = new T;
				f.Add( 4, pData );
			}
			else
				pData = 0;
			return 0;
		}
	};
	vector<SHolder> data;
	int nFirstFree;
public:
	CVectorList() : nFirstFree(-1) {}
	void clear()
	{
		for ( int i = 0; i < data.size(); ++i )
		{
			if ( data[i].pData )
				erase(i);
		}
	}
	~CVectorList() { clear(); }
	T* operator[]( int i ) { return data[i].pData; }
	int size() const { return data.size(); }
	int push( T* pT ) 
	{
		if ( nFirstFree >= 0 )
		{
			int nRet = nFirstFree;
			data[ nFirstFree ].pData = pT;
			nFirstFree = data[ nFirstFree ].nNextFree;
			return nRet;
		}
		else
		{
			data.push_back( SHolder( pT ) );
			return data.size() - 1;
		}
	}
	void erase( int i ) 
	{ 
		ASSERT( data[i].pData );
		delete data[i].pData;
		data[i].pData = 0; 
		data[i].nNextFree = nFirstFree; 
		nFirstFree = i; 
	}
	bool empty() const  
	{ 
		for ( int i = 0; i < data.size(); ++i ) 
		{
			if ( data[i].pData )
				return false; 
		}
		return true;
	} 
	int operator&( CStructureSaver &f );
};
//////////////////////////////////////////////////////////////////////////
typedef list<CObj<CLuaThread> > CThreads;
typedef unordered_map<TString, bool, TStringHash> CLuaStrings;
//////////////////////////////////////////////////////////////////////////
struct lua_State
{
	// retail lua_State +0x00 (PDB, size 0xb4): the owning engine object, save tag 21 (@0x3de370).
	// The retail ctor nulls it and the dtor releases it; no other retail writer was found in the
	// decomp corpus (see the W3 convergence notes) -- it round-trips through the save stream.
	CPtr<CObjectBase> pContext;
  // thread-specific state
	CObj<CLuaThread> pCT; // current thread
  vector<char> Mbuffer;  // buffer
  // global state 
	CThreads threads;
  int nGT;  // index of table for globals 
  vector<TM> TMtable;  // table for tag methods
  vector<Ref> refArray;  // locked objects
  int refFree;  // list of free positions in refArray
  unsigned long nGCticks;  // number of `bytes' currently allocated
	int nGCAvoid;
  int allowhooks;
	int nNoWait;
	
  lua_Hook callhook;
  lua_Hook linehook;
	CLuaStrings strings;
  CVectorList<Proto> protos;  
  CVectorList<Closure> closures;  
  CVectorList<Hash> tables;  
  CVectorList<UserData> userdatas;
	CVectorList<CallInfo> callInfos;

	int operator&( CStructureSaver &f );
	Hash* GetHash( StkId from ) { return tables[ pCT->stack[ from ].GetH() ]; }
	Closure *GetClosure( StkId from ) { return closures[ pCT->stack[ from ].GetCL() ]; }
	UserData *GetUData( StkId from ) { return userdatas[ pCT->stack[ from ].GetUData() ]; }
	CallInfo *GetCallInfo( StkId from ) { return callInfos[ pCT->stack[ from ].GetCI() ]; }
};
//////////////////////////////////////////////////////////////////////////
inline TObject *LObj( lua_State *L, StkId st ) { return &L->pCT->stack[ st ]; }
inline bool iscfunction( lua_State *L, TObject *o )	
{ 
	return o->GetType() == LUA_TFUNCTION && L->closures[ o->GetCL() ]->isC; 
}
//////////////////////////////////////////////////////////////////////////
#endif

