#include "stdafx.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <hash_set>
#include <algorithm>
#include "CThreadPool.h"
#include "GenericWork.h"
#include "GenericWait.h"
#include "GenericTimer.h"
#include "GenericIoCompletion.h"


void
CThreadPool::insertItem(IThreadPoolItem *pItem)
{
	// protect access to the hash table
	m_lock.acquire();
	if( !m_items )
		m_items.reset(new ::std::hash_set<IThreadPoolItem *>);
	m_items->insert(pItem);
	m_lock.release();
}

void
CThreadPool::removeItem(IThreadPoolItem *pItem)
{
	m_lock.acquire();
	m_items->erase(pItem);
	if( m_items->size() == 0 )
		m_items.reset();
	m_lock.release();
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

	CloseThreadpoolCleanupGroupMembers(env()->CleanupGroup, TRUE, NULL);
	CloseThreadpoolCleanupGroup(env()->CleanupGroup);
	env()->CleanupGroup = NULL;
	PTP_POOL pool = env()->Pool;
	DestroyThreadpoolEnvironment(&m_env);
	memset(&m_env, '\0', sizeof(m_env));
	CloseThreadpool(pool);
}

void
CThreadPool::deleteWorkItem(IWork *&pWork)
{
	if( pWork )
	{
		// WaitForThreadpoolWorkCallbacks(pWork->handle(), TRUE);
		CloseThreadpoolWork(pWork);
		delete pWork;
		pWork = NULL;
	}
}

bool
CThreadPool::CloseThreadpoolWork(class IWork *work)
{
	bool bRetVal = (NULL != work) && Enabled();
	if( bRetVal )
	{
		::CloseThreadpoolWork(work->handle());
		IWorkImplPrivate *ptr = dynamic_cast<IWorkImplPrivate *>(work);
		if( ptr )
			ptr->setPtp(NULL);
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
	bool bRetVal = (NULL != pIo) && Enabled() && (NULL != pIo->pio());
	if( bRetVal )
		::StartThreadpoolIo(pIo->pio());
	return bRetVal;
}

bool
CThreadPool::CancelThreadpoolIo(class IIoCompletion *pIo)
{
	bool bRetVal = (pIo && Enabled());
	if( bRetVal )
		::CancelThreadpoolIo(pIo->pio());
	return bRetVal;
}

bool
CThreadPool::CloseThreadpoolIo(class IIoCompletion *pIo)
{
	bool bRetVal = (pIo && Enabled());
	if( bRetVal )
		::CloseThreadpoolIo(pIo->pio());
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

CThreadPool::CThreadPool(const IThreadPool::Priority priority, DWORD dwMinThreads, DWORD dwMaxThreads) :
m_bEnabled(FALSE),
m_lock(false)
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

//virtual
PTP_CALLBACK_ENVIRON
CThreadPool::env()
{ 
	return &m_env;
}

IWorkPtr
CThreadPool::newWork(const IWork::FuncPtr &f)
{
	::std::shared_ptr<IWorkImpl<IThreadPoolItemImpl<GenericWork> > >  ptr = ::std::make_shared<IWorkImpl<IThreadPoolItemImpl<GenericWork> > >();
	if( ptr )
	{
		ptr->setPool(this);
		ptr->setWork(f);
		createThreadPoolWork(ptr.get());
	}
	return ::std::static_pointer_cast<IWork>(ptr);
}

IWaitPtr
CThreadPool::newWait(const IWait::FuncPtr &f)
{
	::std::shared_ptr<IWaitImpl<IThreadPoolItemImpl<GenericWait> > >  ptr = ::std::make_shared<IWaitImpl<IThreadPoolItemImpl<GenericWait> > >();
	if( ptr )
	{
		ptr->setPool(this);
		ptr->setWait(f);
		createThreadPoolWait(ptr.get());
	}
	return ::std::static_pointer_cast<IWait>(ptr);
}

ITimerPtr
CThreadPool::newTimer(const ITimer::FuncPtr &f)
{
	::std::shared_ptr<ITimerImpl<IThreadPoolItemImpl<GenericTimer> > >  ptr = ::std::make_shared<ITimerImpl<IThreadPoolItemImpl<GenericTimer> > >();
	if( ptr )
	{
		ptr->setPool(this);
		ptr->setTimer(f);
		createThreadPoolTimer(ptr.get());
	}
	return ::std::static_pointer_cast<ITimer>(ptr);
}


IIoCompletionPtr
CThreadPool::newIoCompletion(HANDLE hIoObject, const IIoCompletion::FuncPtr &f)
{
	::std::shared_ptr<IIoCompletionImpl<IThreadPoolItemImpl<GenericIoCompletion> > >  ptr = ::std::make_shared<IIoCompletionImpl<IThreadPoolItemImpl<GenericIoCompletion> > >();
	if( ptr )
	{
		ptr->setPool(this);
		ptr->setIoComplete(f);
		createThreadPoolIoCompletion(hIoObject, ptr.get());
	}
	return ::std::static_pointer_cast<IIoCompletion>(ptr);
}

template <class C>
void 
CThreadPool::createThreadPoolWork(IWorkImpl<C> *pWork)
{
	if( pWork && Enabled())
	{
		PVOID Param = reinterpret_cast<PVOID>(static_cast<C *>(pWork));
		PTP_WORK ptpWork = ::CreateThreadpoolWork(IWork_callback, Param, env());
		
		if( NULL == ptpWork )
		{
			_ftprintf_s(stderr,TEXT("CreateThreadpoolWork failed, GLE=%d.\n"), GetLastError()); 
		}
		pWork->setPtp(ptpWork);
	}
}

template <class C>
void
CThreadPool::createThreadPoolWait(IWaitImpl<C> *pWait)
{
	if( pWait && Enabled())
	{
		PTP_WAIT ptpWait = ::CreateThreadpoolWait(IWait_callback, reinterpret_cast<PVOID>(pWait), env());
		
		if( NULL == ptpWait )
		{
			_ftprintf_s(stderr,TEXT("CreateThreadpoolWait failed, GLE=%d.\n"), GetLastError()); 
		}
		pWait->setPtp(ptpWait);
	}
}

template <class C>
void
CThreadPool::createThreadPoolTimer(ITimerImpl<C> *pTimer)
{
	if( pTimer && Enabled())
	{
		PTP_TIMER ptpTimer = ::CreateThreadpoolTimer(ITimer_callback, reinterpret_cast<PVOID>(pTimer), env());
		
		if( NULL == ptpTimer )
		{
			_ftprintf_s(stderr,TEXT("CreateThreadpoolTimer failed, GLE=%d.\n"), GetLastError()); 
		}
		pTimer->setPtp(ptpTimer);
	}
}

template <class C>
void
CThreadPool::createThreadPoolIoCompletion(HANDLE hIo, IIoCompletionImpl<C> *pIoCompletion)
{
	if( pIoCompletion && Enabled() )
	{
		PTP_IO pio = ::CreateThreadpoolIo(hIo, IIoCompletion_callback, (PVOID)(IIoCompletion *)pIoCompletion, env());
		if( pio )
			pIoCompletion->setPio(pio);
	}
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

template<class C>
IThreadPoolItemImpl<C>::IThreadPoolItemImpl() : m_pool(NULL)
{
	TRACE(TEXT(">>>%S::IThreadPoolItemImpl(), this=0x%x.\n"), typeid(*this).name() , this); 
}

template<class C>
IThreadPoolItemImpl<C>::IThreadPoolItemImpl(class CThreadPool *p) : m_pool(p)
{
	TRACE(TEXT(">>>%S::IThreadPoolItemImpl(p), this=0x%x.\n"), typeid(*this).name() , this); 
	p->insertItem(this);
}
template<class C>
IThreadPoolItemImpl<C>::~IThreadPoolItemImpl()
{
	TRACE(TEXT("<<<%S::~IThreadPoolItemImpl(), this=0x%x.\n"), typeid(*this).name() , this); 
	static_cast<CThreadPool *>(pool())->removeItem(this);
}
template<class C>
void
IThreadPoolItemImpl<C>::setPool(IThreadPool *p)
{
	static_cast<CThreadPool *>(p)->insertItem(this);
	m_pool = p;
}

THREADPOOL_API
::std::shared_ptr<IThreadPool>
IThreadPool::newPool(const IThreadPool::Priority priority, const DWORD dwMinThreads, const DWORD dwMaxThreads)
{
	return ::std::static_pointer_cast<IThreadPool>(::std::make_shared<CThreadPool>(priority, dwMinThreads, dwMaxThreads));
}