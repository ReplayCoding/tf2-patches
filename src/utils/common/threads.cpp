//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#define	USED

#if defined(_WIN32)
#include <windows.h>
#elif defined(POSIX)
#include <pthread.h>
#include <unistd.h>
#else
#error "No threading API for this OS"
#endif

#include "cmdlib.h"
#define NO_THREAD_NAMES
#include "threads.h"
#include "pacifier.h"

#include "tier0/threadtools.h"

#define	MAX_THREADS	16


class CRunThreadsData
{
public:
	int m_iThread;
	void *m_pUserData;
	RunThreadsFn m_Fn;
};

CRunThreadsData g_RunThreadsData[MAX_THREADS];


int		dispatch;
int		workcount;
qboolean		pacifier;

qboolean	threaded;
bool g_bLowPriorityThreads = false;


// The name is required because of collisions with threadtools
#if defined(_WIN32)
typedef HANDLE UtilThreadHandle_t;
#elif defined(POSIX)
typedef pthread_t UtilThreadHandle_t;
#else
#error "Define a thread handle type here"
#endif

UtilThreadHandle_t g_ThreadHandles[MAX_THREADS];



/*
=============
GetThreadWork

=============
*/
int	GetThreadWork (void)
{
	int	r;

	ThreadLock ();

	if (dispatch == workcount)
	{
		ThreadUnlock ();
		return -1;
	}

	UpdatePacifier( (float)dispatch / workcount );

	r = dispatch;
	dispatch++;
	ThreadUnlock ();

	return r;
}


ThreadWorkerFn workfunction;

void ThreadWorkerFunction( int iThread, void *pUserData )
{
	int		work;

	while (1)
	{
		work = GetThreadWork ();
		if (work == -1)
			break;
		 
		workfunction( iThread, work );
	}
}

void RunThreadsOnIndividual (int workcnt, qboolean showpacifier, ThreadWorkerFn func)
{
	if (numthreads == -1)
		ThreadSetDefault ();
	
	workfunction = func;
	RunThreadsOn (workcnt, showpacifier, ThreadWorkerFunction);
}


/*
===================================================================

WIN32

===================================================================
*/

int		numthreads = -1;
CThreadMutex		crit;
static int enter;


void SetLowPriority()
{
	// TODO(replaycoding): linux version
#ifdef _WIN32
	SetPriorityClass( GetCurrentProcess(), IDLE_PRIORITY_CLASS );
#endif
}


void ThreadSetDefault (void)
{
#if defined(_WIN32)
	SYSTEM_INFO info;

	if (numthreads == -1)	// not set manually
	{
		GetSystemInfo (&info);
		numthreads = info.dwNumberOfProcessors;
		if (numthreads < 1 || numthreads > 32)
			numthreads = 1;
	}
#elif defined(_LINUX)
	numthreads = sysconf(_SC_NPROCESSORS_ONLN);
#else
	numthreads = 1;
#endif

	Msg ("%i threads\n", numthreads);
}


void ThreadLock (void)
{
	if (!threaded)
		return;
	crit.Lock();
	if (enter)
		Error ("Recursive ThreadLock\n");
	enter = 1;
}

void ThreadUnlock (void)
{
	if (!threaded)
		return;
	if (!enter)
		Error ("ThreadUnlock without lock\n");
	enter = 0;
	crit.Unlock();
}


// This runs in the thread and dispatches a RunThreadsFn call.
#ifdef _WIN32
DWORD WINAPI
#else
void* 
#endif
InternalRunThreadsFn( void* pParameter )
{
	CRunThreadsData *pData = (CRunThreadsData*)pParameter;
	pData->m_Fn( pData->m_iThread, pData->m_pUserData );
	return 0;
}


void RunThreads_Start( RunThreadsFn fn, void *pUserData, ERunThreadsPriority ePriority )
{
	Assert( numthreads > 0 );
	threaded = true;

	if ( numthreads > MAX_TOOL_THREADS )
		numthreads = MAX_TOOL_THREADS;

	for ( int i=0; i < numthreads ;i++ )
	{
		g_RunThreadsData[i].m_iThread = i;
		g_RunThreadsData[i].m_pUserData = pUserData;
		g_RunThreadsData[i].m_Fn = fn;

#if defined(_WIN32)
		DWORD dwDummy;
		g_ThreadHandles[i] = CreateThread(
		   NULL,	// LPSECURITY_ATTRIBUTES lpsa,
		   0,		// DWORD cbStack,
		   InternalRunThreadsFn,	// LPTHREAD_START_ROUTINE lpStartAddr,
		   &g_RunThreadsData[i],	// LPVOID lpvThreadParm,
		   0,			// DWORD fdwCreate,
		   &dwDummy );
#elif defined(POSIX)
		pthread_create(&g_ThreadHandles[i], NULL, InternalRunThreadsFn, &g_RunThreadsData[i]);
#else
#error "Need a thread creation function"
#endif

		// TODO: linux support
#ifdef _WIN32
		if ( ePriority == k_eRunThreadsPriority_UseGlobalState )
		{
			if( g_bLowPriorityThreads )
				SetThreadPriority( g_ThreadHandles[i], THREAD_PRIORITY_LOWEST );
		}
		else if ( ePriority == k_eRunThreadsPriority_Idle )
		{
			SetThreadPriority( g_ThreadHandles[i], THREAD_PRIORITY_IDLE );
		}
#endif
	}
}


void RunThreads_End()
{
#ifdef _WIN32
	WaitForMultipleObjects( numthreads, g_ThreadHandles, TRUE, INFINITE );
#endif
	for ( int i=0; i < numthreads; i++ )
	{
#if defined(_WIN32)
		CloseHandle( g_ThreadHandles[i] );
#elif defined(POSIX)
		pthread_join( g_ThreadHandles[i], NULL );
#else
#error "Need a thread join function"
#endif
	}

	threaded = false;
}
	

/*
=============
RunThreadsOn
=============
*/
void RunThreadsOn( int workcnt, qboolean showpacifier, RunThreadsFn fn, void *pUserData )
{
	int		start, end;

	start = Plat_FloatTime();
	dispatch = 0;
	workcount = workcnt;
	StartPacifier("");
	pacifier = showpacifier;

#ifdef _PROFILE
	threaded = false;
	(*func)( 0 );
	return;
#endif

	
	RunThreads_Start( fn, pUserData );
	RunThreads_End();


	end = Plat_FloatTime();
	if (pacifier)
	{
		EndPacifier(false);
		printf (" (%i)\n", end-start);
	}
}


