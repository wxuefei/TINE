#include "3d.h"
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <unistd.h>
#include <setjmp.h>
#ifndef TARGET_WIN32
#include <errno.h>
#include <pthread.h>
#else
#include <windows.h>
#include <synchapi.h>
#include <sysinfoapi.h>
#include <processthreadsapi.h>
#include <time.h>
#endif
#ifdef linux
//https://man7.org/linux/man-pages/man2/futex.2.html
#include <sys/syscall.h>
#include <linux/futex.h>
#elif defined __FreeBSD__
#include <sys/types.h>
#include <sys/umtx.h>
#endif
static int64_t GetTicks() {
	#ifndef TARGET_WIN32
	//https://stackoverflow.com/questions/2958291/equivalent-to-gettickcount-on-linux
	struct timespec ts;
    int64_t theTick = 0U;
    clock_gettime( CLOCK_REALTIME, &ts );
    theTick  = ts.tv_nsec / 1000000;
    theTick += ts.tv_sec * 1000;
    return theTick;
    #else
    return GetTickCount();
    #endif
}
static __thread void *__fs;
static __thread void *__gs;
static __thread int __core_num;
void SleepABit() {
	sleep(1);
}
void *GetFs() {
	if(!__fs) __fs=TD_MALLOC(2048);
	return __fs;
}
void SetFs(void *f) {
	__fs=f;
}
int CoreNum() {
	return __core_num;
}
void *GetGs() {
	if(!__gs)
		__gs=TD_MALLOC(1024);
	return __gs;
}
typedef struct {
	#ifndef TARGET_WIN32
	pthread_t thread;
	int64_t is_sleeping;
	jmp_buf jmp_to;
	#else
	HANDLE thread;
	HANDLE event;
	HANDLE mtx;
	int64_t awake_at;
	//
	// Windows has not good support for high precision waiting,so I will have
	//  the amount compensation that needs to be made to emulate not excessilvy sleeping
	//
	int64_t sleep_compensation;
	int64_t sleep_compensation_ts;
	#endif
	int core_num,is_alive;
	void (*fp)();
} CCore;
static CCore cores[64];
static void ExitCore(int sig) {
	#ifndef TARGET_WIN32
	pthread_exit(0);
	#else
	ExitThread(0);
	#endif
} 
static void LaunchCore(void *c) {
	SetupDebugger();
	VFsThrdInit();
	CHash init=map_get(&TOSLoader,"TaskInit")->data[0];
	FFI_CALL_TOS_2(init.val,GetFs(),0);
	__core_num=c;
	#ifndef TARGET_WIN32
	CHash yield=map_get(&TOSLoader,"__InteruptCoreRoutine")->data[0];
	signal(SIGUSR2,yield.val);
	signal(SIGUSR1,&ExitCore);
	#endif
	FFI_CALL_TOS_0_ZERO_BP(cores[__core_num].fp);
}
void InteruptCore(int core) {
	#ifndef TARGET_WIN32
	pthread_kill(cores[core].thread,SIGUSR2);
	#else
	vec_CHash_t *hash;
	CONTEXT ctx;
	hash=map_get(&TOSLoader,"__InteruptCoreRoutine");
	if(hash) {
		memset(&ctx,0 ,sizeof ctx);
		ctx.ContextFlags=CONTEXT_FULL; 
		SuspendThread(cores[core].thread);
		GetThreadContext(cores[core].thread,&ctx);
		ctx.Rsp-=8;
		((int64_t*)ctx.Rsp)[0]=ctx.Rip;
		ctx.Rip=hash->data[0].val;
		SetThreadContext(cores[core].thread,&ctx);
		ResumeThread(cores[core].thread);
	}
	assert(hash);
	#endif
}
void LaunchCore0(void *fp) {
	int core=0;
	cores[core].core_num=core;
	cores[core].fp=NULL;
	#ifndef TARGET_WIN32
	pthread_create(&cores[core].thread,NULL,FFI_CALL_TOS_0_ZERO_BP,fp);
	#else
	cores[core].thread=CreateThread(NULL,0,FFI_CALL_TOS_0_ZERO_BP,fp,0,NULL);
	SetThreadPriority(cores[core].thread,THREAD_PRIORITY_HIGHEST);
	cores[core].mtx=CreateMutex(NULL,FALSE,NULL);
	cores[core].event=CreateEvent(NULL,0,0,NULL);
	cores[core].is_alive=1;
	#endif
}
void WaitForCore0() {
	#ifndef TARGET_WIN32
	pthread_join(cores[0].thread,NULL);
	#else
	WaitForSingleObject(cores[0].thread,INFINITE);
	#endif
}
void CreateCore(int core,void *fp) {
	cores[core].core_num=core;
	cores[core].fp=fp;
	#ifndef TARGET_WIN32
	pthread_create(&cores[core].thread,NULL,LaunchCore,core);
	#else
	cores[core].thread=CreateThread(NULL,0,LaunchCore,core,0,NULL);
	cores[core].mtx=CreateMutex(NULL,FALSE,NULL);
	SetThreadPriority(cores[core].thread,THREAD_PRIORITY_HIGHEST);
	cores[core].is_alive=1;
	cores[core].event=CreateEvent(NULL,0,0,NULL);
	#endif
}
void __ShutdownCore(int core) {
	#ifndef TARGET_WIN32
	pthread_kill(cores[core].thread,SIGUSR1);
	pthread_join(cores[core].thread,NULL);
	#else
	TerminateThread(cores[core].thread,0);
	#endif
}
void __ShutdownCores() {
	int c;
	for(c=0;c<mp_cnt(NULL);c++) {
	   if(c!=__core_num)
	   	  __ShutdownCore(c);
	}
	__ShutdownCore(__core_num);
}
void multicAwaken(int64_t core) {
	#ifdef linux
	int64_t old=1;
	atomic_compare_exchange_strong(&cores[core].is_sleeping,&old,0);
	syscall(SYS_futex,&cores[core].is_sleeping,FUTEX_WAKE,1,NULL,NULL,0);
	#elif defined __FreeBSD__
	int64_t old=1;
	atomic_compare_exchange_strong(&cores[core].is_sleeping,&old,0);
	_umtx_op(&cores[core].is_sleeping,UMTX_OP_WAKE,1,0,0);
	#elif defined TARGET_WIN32
	WaitForSingleObject(cores[core].mtx,INFINITE);
	cores[core].awake_at=0;
	SetEvent(cores[core].event);
	ReleaseMutex(cores[core].mtx);	 
	#endif
}
#ifdef TARGET_WIN32
static int64_t ticks=0;
static int64_t tick_inc=1;
static void update_ticks(UINT tid,UINT msg,DWORD_PTR dw_user,void *ul,void* ul2) {
	ticks+=tick_inc;
	for(int64_t idx=0;idx!=mp_cnt(NULL);idx++) {
		if(cores[idx].is_alive) {
			WaitForSingleObject(cores[idx].mtx,INFINITE);
			if(cores[idx].awake_at) {
				if(ticks>=cores[idx].awake_at) {
					SetEvent(cores[idx].event);
					cores[idx].awake_at=0;
				}
			}
			ReleaseMutex(cores[idx].mtx);
		}
	}
}
int64_t __GetTicksHP() {
	static int64_t init;
	TIMECAPS tc; 
	if(!init) {
		init=1;
		timeGetDevCaps(&tc,sizeof tc);
		tick_inc=tc.wPeriodMin;
		timeSetEvent(tick_inc,tick_inc,&update_ticks,NULL,TIME_PERIODIC);
	}
	return ticks;
}
#endif 
void multicSleepHP(int64_t us) {
	#if defined __FreeBSD__
	struct timespec ts={0};
	ts.tv_nsec=us*1000;
	__atomic_store_n(&cores[__core_num].is_sleeping,1,__ATOMIC_RELAXED)	;
	_umtx_op(&cores[__core_num].is_sleeping,UMTX_OP_WAIT_UINT,1,0,&ts);
	#elif defined linux
	struct timespec ts={0};
	ts.tv_nsec=us*1000;
	__atomic_store_n(&cores[__core_num].is_sleeping,1,__ATOMIC_RELAXED)	;
	syscall(SYS_futex,&cores[__core_num].is_sleeping,FUTEX_WAIT,1,&ts,NULL,0);
	#else
	int64_t s,e,core_num=__core_num;
	s=__GetTicksHP();
	WaitForSingleObject(cores[core_num].mtx,INFINITE);
	cores[core_num].awake_at=s+us/1000;
	ReleaseMutex(cores[core_num].mtx);
	WaitForSingleObject(cores[core_num].event,INFINITE);
	#endif
}

void multicSleep(int64_t ms) {
	multicSleepHP(ms*1000);
}
