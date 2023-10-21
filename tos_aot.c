#include <stdint.h>
#include <string.h>
#include "3d.h"
#include <stdio.h>
#define IET_END			0
//reserved
#define IET_REL_I0		2 //Fictitious
#define IET_IMM_U0		3 //Fictitious
#define IET_REL_I8		4
#define IET_IMM_U8		5
#define IET_REL_I16		6
#define IET_IMM_U16		7
#define IET_REL_I32		8
#define IET_IMM_U32		9
#define IET_REL_I64		10
#define IET_IMM_I64		11
#define IEF_IMM_NOT_REL		1
//reserved
#define IET_REL32_EXPORT	16
#define IET_IMM32_EXPORT	17
#define IET_REL64_EXPORT	18 //Not implemented
#define IET_IMM64_EXPORT	19 //Not implemented
#define IET_ABS_ADDR		20
#define IET_CODE_HEAP		21 //Not really used
#define IET_ZEROED_CODE_HEAP	22 //Not really used
#define IET_DATA_HEAP		23
#define IET_ZEROED_DATA_HEAP	24 //Not really used
#define IET_MAIN		25
map_vec_CHash_t TOSLoader;
static void HashAdd(CHash *h,char *name) {
    vec_CHash_t vec;
    if(!map_get(&TOSLoader,name)) {
        vec_init(&vec);
        map_set(&TOSLoader,name,vec);
    }
    vec_push(map_get(&TOSLoader,name),*h);
}
static void LoadOneImport(char **_src,char *mod_base,int64_t ld_flags) {
    char *src=*_src,*ptr,*st_ptr,*ptr2=NULL;
    int64_t i,etype,idx;
    vec_CHash_t *_tmpex;
    CHash *tmpex;
    CHash tmpiss;
    char first=1;
    while(etype=*src++) {
        i=*(int32_t*)src;
        src+=4;
        st_ptr=src;
        src+=strlen(st_ptr)+1;
        if(*st_ptr) {
			if(!first) {
				*_src=st_ptr-5;
				return;
			} else {
				first=0;
				loop:
				if(!(_tmpex=map_get(&TOSLoader,st_ptr))) {
					if(map_get(&Loader.symbols,st_ptr)) {
						ptr=map_get(&Loader.symbols,st_ptr)->value_ptr;
						tmpiss.type=HTT_FUN;
						tmpiss.val=ptr;
						HashAdd(&tmpiss,st_ptr);
						goto loop;
					}
					printf("Unresolved reference %s\n",st_ptr);
					tmpiss.type=HTT_IMPORT_SYS_SYM;
					tmpiss.mod_header_entry=st_ptr-5;
					tmpiss.mod_base=mod_base;
					HashAdd(&tmpiss,st_ptr);
				}
			}
		}
		if(_tmpex) {
			ptr2=mod_base+i;
			vec_foreach_ptr(_tmpex,tmpex,idx) {
                if(tmpex->type==HTT_IMPORT_SYS_SYM) continue;
                i=tmpex->val;
                switch(etype) {
                    case IET_REL_I8: *ptr2=(char*)i-ptr2-1; break;
                    case IET_IMM_U8: *ptr2=(char*)i; break;
                    case IET_REL_I16: *(int16_t*)ptr2=(char*)i-ptr2-2; break;
                    case IET_IMM_U16: *(int16_t*)ptr2=(int64_t)i; break;
                    case IET_REL_I32: *(int32_t*)ptr2=(char*)i-ptr2-4; break;
                    case IET_IMM_U32: *(int32_t*)ptr2=(int64_t)i; break;
                    case IET_REL_I64: *(int64_t*)ptr2=(char*)i-ptr2-8; break;
                    case IET_IMM_I64: *(int64_t*)ptr2=(int64_t)i;
                    break;
                }
                break;
            }
        }
    }
    *_src=src-1;
}
static void SysSymImportsResolve(char *st_ptr,int64_t ld_flags) {
    CHash *tmpiss;
    char *ptr;
    int64_t idx;
    vec_CHash_t *find;
    loop:
    if(find=map_get(&TOSLoader,st_ptr)) {
        vec_foreach_ptr(find,tmpiss,idx) {
            if(tmpiss->type==HTT_IMPORT_SYS_SYM) {
                ptr=tmpiss->mod_header_entry;
                LoadOneImport(&ptr,(char*)tmpiss->mod_base,ld_flags);
                tmpiss->type=HTT_INVALID;
            }
        }
    }
}
static void LoadPass1(char *src,char *mod_base,int64_t ld_flags) {
    char *ptr2,*ptr3,*st_ptr;
    int64_t i,j,cnt,etype;
    CHash tmpex;
    while(etype=*src++) {
        i=*((int32_t*)src);
        src+=4;
        st_ptr=src;
        src+=strlen(st_ptr)+1;
        switch(etype) {
            case IET_REL32_EXPORT:
            case IET_IMM32_EXPORT:
            case IET_REL64_EXPORT:
            case IET_IMM64_EXPORT:
            tmpex.type=HTT_EXPORT_SYS_SYM;
            if(etype==IET_IMM32_EXPORT||etype==IET_IMM64_EXPORT)
                tmpex.val=(void*)i;
            else
                tmpex.val=i+mod_base;
            HashAdd(&tmpex,st_ptr);
            SysSymImportsResolve(st_ptr,0);
            break;
            case IET_REL_I0...IET_IMM_I64:
            src=st_ptr-5;
            LoadOneImport(&src,mod_base,ld_flags);
            break;
            case IET_ABS_ADDR:
            {
                cnt=i;
                for(j=0;j<cnt;j++) {
                    ptr2=mod_base+*(int32_t*)src;
                    src+=4;
                    *(int32_t*)ptr2+=mod_base;
                }
            }
            break;
            case IET_ZEROED_CODE_HEAP:
            case IET_CODE_HEAP:
            abort();
            //ptr3=PoopMAlloc32(*(int32_t*)src);
            src+=4;
            fill_data:            
            if(*st_ptr) {
                tmpex.type=HTT_EXPORT_SYS_SYM;
                tmpex.val=ptr3;
                HashAdd(&tmpex,st_ptr);
            }
            cnt=i;
            for(j=0;j<cnt;j++) {
                    ptr2=mod_base+*(int32_t*)src;
                    src+=4;
                    *(int32_t*)ptr2+=(int32_t)mod_base;
            }
            break;
            case IET_DATA_HEAP:
            case IET_ZEROED_DATA_HEAP:
            abort();
            //ptr3=PoopMAlloc32(*(int32_t*)src);
            src+=4;
            goto fill_data;
        }
    }
}
static void LoadPass2(char *src,char *mod_base,int64_t f) {
    char *st_ptr;
    int64_t i,etype;
    while(etype=*src++) {
        i=*(int32_t*)src;
        src+=4;
        st_ptr=src;
        src+=strlen(st_ptr)+1;
        switch(etype) {
            case IET_MAIN:
            FFI_CALL_TOS_0_ZERO_BP(i+mod_base);
            break;
            case IET_ABS_ADDR:
            src+=sizeof(int32_t)*i;
            break;
            case IET_CODE_HEAP:
            case IET_ZEROED_CODE_HEAP:
            src+=4+sizeof(int32_t)*i;
            break;
            case IET_DATA_HEAP:
            case IET_ZEROED_DATA_HEAP:
            src+=8+sizeof(int32_t)*i;
            break;
        }
    }
}
/*
class CBinFile
{
  U16	jmp;
  U8	module_align_bits,
	reserved;
  U32	bin_signature; //'TOSB'
  I64	org,
	patch_table_offset,
	file_size;
};
*/
#pragma pack(1)
struct CBinFile {
    int16_t jmp;
    int8_t mod_align_bits,pad;
    char bin_signature[4];
    int64_t org,patch_table_offset,file_size;
} ;
typedef struct CBinFile CBinFile ;
void *Load(char *fn,int64_t ld_flags) {
	FILE *f;
    char *mod_base,*absname;
    int64_t size,mod_align,misalign;
    CBinFile *bfh,*bfh_addr;
    if(!(f=fopen(fn,"rb"))) {
        return NULL;
    }
    fseek(f,0,SEEK_SET);
    size=ftell(f);
    fseek(f,0,SEEK_END);
    size=ftell(f)-size;
    fseek(f,0,SEEK_SET);
    fread(bfh_addr=bfh=NewVirtualChunk(size,1),1,size,f);
    fclose(f);

    assert(bfh->bin_signature[0]=='T');
    assert(bfh->bin_signature[1]=='O');
    assert(bfh->bin_signature[2]=='S');
    assert(bfh->bin_signature[3]=='B');
    
    mod_base=(char*)bfh_addr+sizeof(CBinFile);
    LoadPass1((char*)bfh_addr+bfh_addr->patch_table_offset,mod_base,ld_flags);
    vec_CHash_t *FualtCB=map_get(&TOSLoader,"FualtRoutine");
	/*if(FualtCB) {
		#ifndef TARGET_WIN32
		signal(SIGBUS,FualtCB->data[0].val);
		#endif
	}*/
	#ifndef TARGET_WIN32
	CHash yield=map_get(&TOSLoader,"__InteruptCoreRoutine")->data[0];
	signal(SIGUSR2,yield.val);
	#endif
	SetupDebugger();
    LoadPass2((char*)bfh_addr+bfh_addr->patch_table_offset,mod_base,ld_flags);    
    //Stuff may still be using data once we exit
    //PoopFree(bfh_addr);
}
static int CmpPtr(void *a,void *b) {
	return map_get(&TOSLoader,*(char**)a)->data[0].val>map_get(&TOSLoader,*(char**)b)->data[0].val;
}
static char *BackTrace(void *ptr) {
	static char **sorted;
	static int64_t sz;
	int64_t idx;
	char *cur;
	char *last;
	if(!sz) {
		map_iter_t iter=map_iter(&TOSLoader);
		while(map_next(&TOSLoader,&iter))
			sz++;
		sorted=calloc(sz,8);
		idx=0;
		iter=map_iter(&TOSLoader);
		while(cur=map_next(&TOSLoader,&iter))
			sorted[idx++]=cur;
		qsort(sorted,sz,8,&CmpPtr);
	}
	void **rbp=__builtin_frame_address(0);
	void *oldp;
	ptr=__builtin_return_address(1);
	while(rbp) {
		oldp=NULL;
		last="UNKOWN";
		for(idx=0;idx!=sz;idx++) {
			void *curp=map_get(&TOSLoader,sorted[idx])->data[0].val; 
			if(curp==ptr) {
				puts(sorted[idx]);
			} 
			if(curp>ptr) {
				printf("%s[%p+%p]\n",last,ptr,ptr-oldp);
				goto next;
			}
			oldp=curp;
			last=sorted[idx];
		}
		next:;
		ptr=*(rbp+1);
		rbp=*rbp;
    }
}
void FualtCB() {
	BackTrace(NULL);
}
