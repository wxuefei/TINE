#include "poopalloc.h"
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include <stdlib.h>
#include <signal.h>
#ifndef TARGET_WIN32
#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdatomic.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#else
#include <memoryapi.h>
#include <sysinfoapi.h>
#include <winnt.h>
#endif
static void *min32;
static int64_t Hex2I64(char *ptr,char **_res) {
	int64_t res=0;
	while(isxdigit(*ptr)) {
		res<<=4;
		if(isalpha(*ptr))
			res+=toupper(*ptr)-'A'+10;
		else
			res+=*ptr-'0';
		ptr++;
	}
	if(_res) *_res=ptr;
	return res;
}
void *NewVirtualChunk(int64_t sz,int64_t low32) {
	#ifndef TARGET_WIN32
	static int64_t ps;
	if(!ps) {
			ps=sysconf(_SC_PAGE_SIZE);
			min32=ps;
	}
	int64_t pad=ps;
	void *ret;
	pad=sz%ps;
	if(pad)
		pad=ps;
    if(low32) {
		ret=mmap(min32,sz/ps*ps+pad,PROT_EXEC|PROT_WRITE|PROT_READ,MAP_PRIVATE|MAP_ANON|MAP_32BIT,-1,0);
		//Ok Linus Torvalds,im going to have to look at the /proc/self/maps to find some of that 32bit jazz
		char buffer[1<<16];
		char *ptr;
		//I hear that poo poo linux doesn't like addresses within the first 16bits 
		void *down=(void*)0x11000;
		FILE *map;
		if(ret==MAP_FAILED) {
			map=fopen("/proc/self/maps","r");
			int64_t len;
			buffer[fread(buffer,1,1<<16,map)]=0;
			ptr=buffer;
			while(1) {
				void *lower=(void*)Hex2I64(ptr,&ptr);
				if((lower-down)>=(sz/ps*ps+pad)&&lower>down) {
					goto found;
				}
				//Ignore '-'
				ptr++;
				void *upper=(void*)Hex2I64(ptr,&ptr);
				down=upper;
				ptr=strchr(ptr,'\n');
				if(!ptr) break;
				ptr++;
			}
			found:
			fclose(map);
			ret=mmap(down,sz/ps*ps+pad,PROT_EXEC|PROT_WRITE|PROT_READ,MAP_PRIVATE|MAP_ANON|MAP_FIXED|MAP_32BIT,-1,0);
		}
    } else
        ret=mmap(NULL,sz/ps*ps+pad,PROT_WRITE|PROT_READ,MAP_PRIVATE|MAP_ANON,-1,0);
    if(ret==MAP_FAILED) return NULL;
    return ret;
	#else
    if(low32) {
        //https://stackoverflow.com/questions/54729401/allocating-memory-within-a-2gb-range
        MEMORY_BASIC_INFORMATION ent;
        static int64_t dwAllocationGranularity;
        if (!dwAllocationGranularity)
        {
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            dwAllocationGranularity = si.dwAllocationGranularity;
        }
        int64_t try=dwAllocationGranularity,addr;
        for(;(try&0xFFffFFff)==try;) {
            if(!VirtualQuery((void*)try,&ent,sizeof(ent))) return NULL;
            try=(int64_t)ent.BaseAddress+ent.RegionSize;
            //Fancy code to round up because address is rounded down with VirtualAlloc
            addr=((int64_t)ent.BaseAddress+dwAllocationGranularity-1)&~(dwAllocationGranularity-1);	
            if((ent.State==MEM_FREE)&&(sz<=(try-addr))) {
                return VirtualAlloc((void*)addr,sz,MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            }
        }
        printf("Out of 32bit address space memory.");
        exit(-1);
    } else
        return VirtualAlloc(NULL,sz,MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	#endif
}
void FreeVirtualChunk(void *ptr,size_t s) {
	#ifdef TARGET_WIN32
	VirtualFree(ptr,0,MEM_RELEASE);
	#else
	static int64_t ps;
	if(!ps) {
			ps=sysconf(_SC_PAGE_SIZE);
	}
	int64_t pad;
	pad=s%ps;
	if(pad)
		pad=ps;
	if(ptr<min32)
		min32=ptr;
	munmap(ptr,s/ps*ps+pad);
	#endif
}
