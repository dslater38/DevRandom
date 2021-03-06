#include "stdafx.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <unordered_set>
#include <algorithm>
#include "CThreadPool.h"
#include "GenericWork.h"
#include "GenericWait.h"
#include "GenericTimer.h"
#include "GenericIoCompletion.h"


#ifdef TRACK_SPIN_COUNTS
static
class Max_LWSpinLock_Report
{
public:
	Max_LWSpinLock_Report() {}
	~Max_LWSpinLock_Report()
	{
		//TRACE(TEXT("LWSpinLock max iterations executed was: %d\n"), LWSpinLock::MaxIter());
		TCHAR buffer[2048];
		_stprintf_s(buffer,TEXT("LWSpinLock max iterations executed was:\t\t\t%I64u\nTotal iterations was:\t\t\t\t\t%I64u\nAverage Iterations was:\t\t\t\t\t%I64u\n"), LWSpinLock::MaxDt(), LWSpinLock::Sum(), LWSpinLock::Sum() / LWSpinLock::Calls() );
		::OutputDebugString(buffer);
		_stprintf_s(buffer,TEXT("LWSpinLocker max lock cycles count executed was:\t%I64u\nTotal cycles spent spinning was:\t\t\t%I64u\nAverage Cycles spent spinning was:\t\t\t%I64u\n"), LWSpinLocker::MaxDt(), LWSpinLocker::Sum(), LWSpinLocker::Sum() / LWSpinLocker::Calls() );
		::OutputDebugString(buffer);
//		_stprintf_s(buffer,TEXT("LWTrySpinLocker max lock cycles count executed was:\t%I64u\nTotal cycles spent spinning was:\t\t%I64u\nAverage Cycles spent spinning was:%I64u\n"), LWTrySpinLocker::MaxDt(), LWTrySpinLocker::Sum(), LWTrySpinLocker::Sum() / LWTrySpinLocker::Calls() );
//		::OutputDebugString(buffer);
	}
}_____fooReportIt;
#endif

void
CThreadPool::insertItem(IThreadPoolItem * pItem)
{
#ifdef TRACK_THREADPOOL_ITEMS
	// protect access to the hash table
	LWSpinLocker lock(m_lock);
	if( !m_items )
		m_items.reset(new ::std::unordered_set<IThreadPoolItem *>);
	m_items->insert(pItem);
#else
	UNREFERENCED_PARAMETER(pItem);
#endif // TRACK_THREADPOOL_ITEMS
}

void
CThreadPool::removeItem(IThreadPoolItem *pItem)
{
#ifdef TRACK_THREADPOOL_ITEMS
	LWSpinLocker lock(m_lock);
	m_items->erase(pItem);
	if( m_items->size() == 0 )
		m_items.reset();
#else
	UNREFERENCED_PARAMETER(pItem);
#endif // TRACK_THREADPOOL_ITEMS
}

DWORD
CThreadPool::GetThreadPoolSize()
{
	SYSTEM_INFO si = {0};
	GetSystemInfo(&si);
	
	DWORD dwNumCpu = si.dwNumberOfProcessors - 1;
	if( 0 == dwNumCpu )
		dwNumCpu = 1;
	
	return dwNumCpu;
}

bool
CThreadPool::Enabled()
{
	return (InterlockedCompareExchange(&m_bEnabled,TRUE,TRUE) == TRUE);
}

void
CThreadPool::Shutdown()
{
	InterlockedCompareExchange(&m_bEnabled,FALSE,TRUE);

	Sleep(10000);

	if( env() )
	{
		CloseThreadpoolCleanupGroupMembers(env()->CleanupGroup, TRUE, NULL);
		CloseThreadpoolCleanupGroup(env()->CleanupGroup);
		env()->CleanupGroup = NULL;
		PTP_POOL pool = env()->Pool;
		DestroyThreadpoolEnvironment(env());
		CloseThreadpool(pool);
	}
	memset(&m_env, '\0', sizeof(m_env));
}

bool
CThreadPool::CloseThreadpoolWork(class IWork *work)
{
	bool bRetVal = (NULL != work) && Enabled();
	if( bRetVal )
	{
		::CloseThreadpoolWork(work->handle());
	}

	return bRetVal;
}

bool
CThreadPool::CloseThreadpoolWait(class IWait *wait)
{
	bool bRetVal = (NULL != wait) && Enabled();
	if( bRetVal )
	{
		::CloseThreadpoolWait(wait->handle());
	}

	return bRetVal;
}

bool
CThreadPool::SetThreadpoolWait(class IWait *wait, HANDLE h, PFILETIME pftTimeout)
{
	bool bRetVal = (NULL != wait) && Enabled();
	if( bRetVal )
		::SetThreadpoolWait(wait->handle(),h,pftTimeout);
	return bRetVal;
}

bool
CThreadPool::SetThreadpoolTimer(PFILETIME pftDueTime, DWORD msPeriod, DWORD msWindowLength, class ITimer *timer)
{
	bool bRetVal = (NULL != timer) && Enabled();
	if( bRetVal )
		::SetThreadpoolTimer(timer->handle(),pftDueTime,msPeriod,msWindowLength);
	return bRetVal;
}

bool
CThreadPool::CloseThreadpoolTimer(class ITimer *timer)
{
	bool bRetVal = (NULL != timer) && Enabled();
	if( bRetVal )
		::CloseThreadpoolTimer(timer->handle());
	return bRetVal;
}

bool
CThreadPool::SubmitThreadpoolWork(class IWork *work)
{
	bool bRetVal = (NULL != work) && Enabled();
	if( bRetVal )
		::SubmitThreadpoolWork(work->handle());
	return bRetVal;
}

bool
CThreadPool::SetThreadpoolWait(HANDLE h, PFILETIME pftTImeout, class IWait *wait)
{
	bool bRetVal = (NULL != wait) && Enabled();
	if( bRetVal )
		::SetThreadpoolWait(wait->handle(),h,pftTImeout);
	return bRetVal;
}

bool
CThreadPool::StartThreadpoolIo(class IIoCompletion *pIo)
{
	bool bRetVal = (NULL != pIo) && Enabled() && (NULL != pIo->handle());
	if( bRetVal )
		::StartThreadpoolIo(pIo->handle());
	return bRetVal;
}

bool
CThreadPool::CancelThreadpoolIo(class IIoCompletion *pIo)
{
	bool bRetVal = (pIo && Enabled());
	if( bRetVal )
		::CancelThreadpoolIo(pIo->handle());
	return bRetVal;
}

bool
CThreadPool::CloseThreadpoolIo(class IIoCompletion *pIo)
{
	bool bRetVal = (pIo && Enabled());
	if( bRetVal )
		::CloseThreadpoolIo(pIo->handle());
	return bRetVal;
}

void
CThreadPool::WaitForThreadpoolWorkCallbacks(class IWork *work, BOOL bCancelPendingCallbacks)
{
	if( Enabled() && work && work->handle() )
		::WaitForThreadpoolWorkCallbacks(work->handle(), bCancelPendingCallbacks);
}

void
CThreadPool::WaitForThreadpoolIoCallbacks(class IIoCompletion *pio, BOOL bCancelPendingCallbacks)
{
	if( Enabled() && pio && pio->handle() )
		::WaitForThreadpoolIoCallbacks(pio->handle(), bCancelPendingCallbacks);
}

void
CThreadPool::WaitForThreadpoolWaitCallbacks(class IWait *wait, BOOL bCancelPendingCallbacks)
{
	if( Enabled() && wait && wait->handle() )
		::WaitForThreadpoolWaitCallbacks(wait->handle(), bCancelPendingCallbacks);
}

void
CThreadPool::WaitForThreadpoolTimerCallbacks(class ITimer *timer, BOOL bCancelPendingCallbacks)
{
	if( Enabled() && timer && timer->handle() )
		::WaitForThreadpoolTimerCallbacks(timer->handle(), bCancelPendingCallbacks);
}

CThreadPool::CThreadPool(int) :
m_bEnabled(TRUE)
{

}

void
CThreadPool::SetThreadpoolCallbackLibrary( void *mod)
{
	if( env() )
	{
		::SetThreadpoolCallbackLibrary(env(), mod);
	}
}

void
CThreadPool::SetThreadpoolCallbackRunsLong( )
{
	if( env() )
	{
		::SetThreadpoolCallbackRunsLong(env());
	}
}

CThreadPool::CThreadPool(const IThreadPool::Priority priority, DWORD dwMinThreads, DWORD dwMaxThreads) :
m_bEnabled(FALSE)
{
	PTP_POOL pool =  CreateThreadpool(NULL);
	// PTP_IO pio = CreateThreadpoolIo(

	if( 0 == dwMaxThreads )
		dwMaxThreads = GetThreadPoolSize();
	if( 0 == dwMinThreads )
		dwMinThreads = 1;

	_ftprintf_s(stdout, TEXT("Creating Thread Pool with %d max threads\n"), dwMaxThreads);

	SetThreadpoolThreadMinimum(pool, dwMinThreads);
	SetThreadpoolThreadMaximum(pool, dwMaxThreads);

	InitializeThreadpoolEnvironment(env());
	//SetThreadpoolCallbackRunsLong(&env);

	
		
	SetThreadpoolCallbackPool(env(), pool);

	PTP_CLEANUP_GROUP cleanup_group = CreateThreadpoolCleanupGroup();
	if( !cleanup_group )
	{
		_ftprintf_s(stderr,TEXT("CreateThreadpoolCleanupGroup failed, GLE=%d.\n"), GetLastError());
	}
		
	TP_CALLBACK_PRIORITY pri = TP_CALLBACK_PRIORITY_NORMAL;
	switch( priority )
	{
		case LOW:
			pri = TP_CALLBACK_PRIORITY_LOW;
			break;
		case HIGH:
			pri = TP_CALLBACK_PRIORITY_HIGH;
			break;
		case NORMAL:
		default:
			pri = TP_CALLBACK_PRIORITY_NORMAL;
			break;
	}


	SetThreadpoolCallbackPriority(env(), pri);


	SetThreadpoolCallbackCleanupGroup(env(), cleanup_group, NULL);

	InterlockedCompareExchange(&m_bEnabled,TRUE,FALSE);
		
}

CThreadPool::~CThreadPool()
{
	InterlockedCompareExchange(&m_bEnabled,FALSE,TRUE);

#ifdef TRACK_THREADPOOL_ITEMS
#ifdef _DEBUG
	if( m_items && m_items->size() > 0 )
	{
		TRACE(TEXT("m_items not empty. Dumping\n"));
		::std::for_each(m_items->begin(), m_items->end(), [&](const IThreadPoolItem *pItem) {
			TRACE(TEXT("item: %S\n"), typeid(*pItem).name());
		});
	}
#endif // _DEBUG
#endif
	if( env() )
	{
		if( env()->CleanupGroup )
		{
			CloseThreadpoolCleanupGroupMembers(env()->CleanupGroup, TRUE, NULL);
			CloseThreadpoolCleanupGroup(env()->CleanupGroup);
		}
		if( env()->Pool )
		{
			PTP_POOL pool = env()->Pool;
			DestroyThreadpoolEnvironment(env());
			CloseThreadpool(pool);
		}
	}
}

//virtual
PTP_CALLBACK_ENVIRON
CThreadPool::env()
{ 
	return &m_env;
}

IWork::Ptr
CThreadPool::newWork(const IWork::FuncPtr &f)
{
#if 0
#ifdef _DEBUG
	::std::shared_ptr<IWorkImpl<IThreadPoolItemImpl<GenericWork> > >  ptr(new IWorkImpl<IThreadPoolItemImpl<GenericWork> >);
#else
	::std::shared_ptr<IWorkImpl<IThreadPoolItemImpl<GenericWork> > >  ptr = ::std::make_shared<IWorkImpl<IThreadPoolItemImpl<GenericWork> > >();
#endif
#else
#ifdef _DEBUG
	::std::shared_ptr<GenericWork>  ptr(new GenericWork);
#else
	::std::shared_ptr<GenericWork>  ptr = ::std::make_shared<GenericWork>();
#endif
#endif
	if( ptr )
	{
		ptr->setPool(this);
		ptr->setWork(f);
		if( !createThreadPoolWork(ptr.get()) )
			ptr.reset();
	}
	return ::std::static_pointer_cast<IWork>(ptr);
}

IWait::Ptr
CThreadPool::newWait(const IWait::FuncPtr &f)
{
#ifdef _DEBUG
	::std::shared_ptr<GenericWait>  ptr(new GenericWait);
#else
	::std::shared_ptr<GenericWait>  ptr = ::std::make_shared<GenericWait>();
#endif
	if( ptr )
	{
		ptr->setPool(this);
		ptr->setWait(f);
		if( !createThreadPoolWait(ptr.get()) )
			ptr.reset();
	}
	return ::std::static_pointer_cast<IWait>(ptr);
}

ITimer::Ptr
CThreadPool::newTimer(const ITimer::FuncPtr &f)
{
#ifdef _DEBUG
	::std::shared_ptr<GenericTimer>  ptr(new GenericTimer);
#else
	::std::shared_ptr<GenericTimer>  ptr = ::std::make_shared<GenericTimer>();
#endif
	if( ptr )
	{
		ptr->setPool(this);
		ptr->setTimer(f);
		if( !createThreadPoolTimer(ptr.get()) )
			ptr.reset();
	}
	return ::std::static_pointer_cast<ITimer>(ptr);
}


IIoCompletion::Ptr
CThreadPool::newIoCompletion(HANDLE hIoObject, const IIoCompletion::FuncPtr &f)
{
#ifdef _DEBUG
	::std::shared_ptr<GenericIoCompletion>  ptr(new GenericIoCompletion);
#else
	::std::shared_ptr<GenericIoCompletion>  ptr = ::std::make_shared<GenericIoCompletion>();
#endif
	if( ptr )
	{
		ptr->setPool(this);
		ptr->setIoComplete(f);
		if( !createThreadPoolIoCompletion(hIoObject, ptr.get()) )
			ptr.reset();
	}
	return ::std::static_pointer_cast<IIoCompletion>(ptr);
}

bool
CThreadPool::createThreadPoolWork(class GenericWork *pWork)
{
	bool bRetVal = false;
	if( pWork && Enabled())
	{
		PVOID Param = reinterpret_cast<PVOID>(static_cast<IWork *>(pWork));
		PTP_WORK ptpWork = ::CreateThreadpoolWork(IWork_callback, Param, env());
		if( NULL != ptpWork )
		{
			pWork->setHandle(ptpWork);
			bRetVal = true;
		}
		else
			_ftprintf_s(stderr,TEXT("CreateThreadpoolWork failed, GLE=%d.\n"), GetLastError()); 
	}
	return bRetVal;
}

// template <class C>
bool
CThreadPool::createThreadPoolWait(GenericWait *pWait)
{
	bool bRetVal = false;
	if( pWait && Enabled())
	{
		PTP_WAIT ptpWait = ::CreateThreadpoolWait(IWait_callback, reinterpret_cast<PVOID>(static_cast<IWait *>(pWait)), env());
		if( NULL != ptpWait )
		{
			bRetVal = true;
			pWait->setHandle(ptpWait);
		}
		else
			_ftprintf_s(stderr,TEXT("CreateThreadpoolWait failed, GLE=%d.\n"), GetLastError()); 
	}
	return bRetVal;
}

bool
CThreadPool::createThreadPoolTimer(GenericTimer *pTimer)
{
	bool bRetVal = false;
	if( pTimer && Enabled())
	{
		PTP_TIMER ptpTimer = ::CreateThreadpoolTimer(ITimer_callback, reinterpret_cast<PVOID>(static_cast<ITimer *>(pTimer)), env());
		if( NULL != ptpTimer )
		{
			bRetVal = true;
			pTimer->setHandle(ptpTimer);
		}
		else
			_ftprintf_s(stderr,TEXT("CreateThreadpoolTimer failed, GLE=%d.\n"), GetLastError()); 	
	}
	return bRetVal;
}

bool
CThreadPool::createThreadPoolIoCompletion(HANDLE hIo, GenericIoCompletion *pIoCompletion)
{
	bool bRetVal = false;
	if( pIoCompletion && Enabled() )
	{
		PTP_IO pio = ::CreateThreadpoolIo(hIo, IIoCompletion_callback, reinterpret_cast<PVOID>(static_cast<IIoCompletion *>(pIoCompletion)), env());
		if( pio )
		{
			bRetVal = true;
			pIoCompletion->setPio(pio);
		}
		else
			_ftprintf_s(stderr,TEXT("CreateThreadpoolIo failed, GLE=%d.\n"), GetLastError()); 	
	}
	return bRetVal;
}

//static
VOID
CALLBACK 
CThreadPool::IWork_callback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK /*Work*/)
{
	IWork *pWork = reinterpret_cast<IWork *>(Context);
	if( pWork )
	{
		pWork->Execute(Instance);
	}
}

// static
VOID
CALLBACK
CThreadPool::IWait_callback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT /*Wait*/, TP_WAIT_RESULT WaitResult)
{
	IWait *pWait = reinterpret_cast<IWait *>(Context);
	if( pWait )
	{
		pWait->Execute(Instance,WaitResult);
	}
}

// static
VOID
CALLBACK
CThreadPool::ITimer_callback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER /*Wait*/)
{
	ITimer *pTimer = reinterpret_cast<ITimer *>(Context);
	if( pTimer )
	{
		pTimer->Execute(Instance);
	}
}

// static
VOID
CALLBACK
CThreadPool::IIoCompletion_callback(__inout      PTP_CALLBACK_INSTANCE Instance,
									__inout_opt  PVOID Context,
									__inout_opt  PVOID Overlapped,
									__in         ULONG IoResult,
									__in         ULONG_PTR NumberOfBytesTransferred,
									__inout      PTP_IO /*Io*/)
{
	if( 0 != Context )
	{
		IIoCompletion *pIo = reinterpret_cast<IIoCompletion *>(Context);
		pIo->OnComplete(Instance, Overlapped, IoResult, NumberOfBytesTransferred );
	}
}


class CDefaultThreadPool : public CThreadPool
{
public:
	CDefaultThreadPool() : CThreadPool((int)0) {}
	~CDefaultThreadPool() {}
	virtual PTP_CALLBACK_ENVIRON env()
	{
		return NULL;
	}
	static IThreadPool * getDefaultPool()
	{
		return &m_defPool;
	}
	virtual void Shutdown() {}
private:
	static CDefaultThreadPool m_defPool;
};

CDefaultThreadPool CDefaultThreadPool::m_defPool;


THREADPOOL_API
::std::shared_ptr<IThreadPool>
IThreadPool::newPool(const IThreadPool::Priority priority, const DWORD dwMinThreads, const DWORD dwMaxThreads)
{
#ifdef _DEBUG
	return ::std::static_pointer_cast<IThreadPool>(::std::shared_ptr<CThreadPool>(new CThreadPool(priority, dwMinThreads, dwMaxThreads)));
#else
	return ::std::static_pointer_cast<IThreadPool>(::std::make_shared<CThreadPool>(priority, dwMinThreads, dwMaxThreads));
#endif
}

// static
THREADPOOL_API
IThreadPool *
IThreadPool::getDefaultPool()
{
	return CDefaultThreadPool::getDefaultPool();
}