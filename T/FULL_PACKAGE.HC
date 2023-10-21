#define COMPONET_COMPILER 1
#define COMPONET_GRAPHICS 1
#define IMPORT_BUILTINS 1
#define INIT_EVERYTHING 1
#define FPTRS 1
//#define GEN_HEADERS 1
#define USE_DOLDOC 1
#include "HCRT_TOS.HC"
#ifdef GEN_HEADERS
#include"GenHeaders.HC";
CDoc *unfound=DocNew("unfound.DD");
#endif
static U0 BootstrapImportSymbol(U8 *name,U8 *ptr,I64 type) {
    CHash *find=HashFind(name,Fs->hash_table,-1);
    if(find&&find->type&HTT_GLBL_VAR) {
        find(CHashGlblVar*)->data_addr=ptr;
    } else if(find&&find->type&HTT_FUN&&!(find(CHashFun*)->flags&1<<Ff_INTERNAL)) {
        find(CHashFun*)->exe_addr=ptr;
        Btr(&find(CHashFun*)->flags,Cf_EXTERN);
    } else if(name[0]=='_'||!StrNCmp(name,"SYS",3)) {
	find=CAlloc(sizeof(CHashExport));
	find->str=StrNew(name);
	find->type=HTT_EXPORT_SYS_SYM;
	find(CHashExport*)->val=ptr;
	HashAdd(find,Fs->hash_table);
#ifdef GEN_HEADERS
       DocPrint(unfound,"%s\n",name);
#endif
    } else {
#ifdef GEN_HEADERS
       DocPrint(unfound,"%s\n",name);
#endif
    }
}
static U0 __BootstrapForeachSymbol__(U0 *cptr) {
    __BootstrapForeachSymbol(cptr);
}
asm {
//I am embeding these into the binary so end-users don't mess up thier KERNEL*.HH files
  KERNELA_BIN::
  BINFILE "KERNELA.HH";
  DU8 0;
  KERNELB_BIN::
  BINFILE "KERNELB.HH";
  DU8 0;

};
U0 LoadImps() {
  try {
    ExePutS2("#define FPTRS\n;;");
    ExePutS2(KERNELA_BIN,"T:/KERNELA.HH");
    __BootstrapForeachSymbol(&BootstrapImportSymbol);
#ifdef GEN_HEADERS
    DocWrite(unfound);
    DocDel(unfound);
#endif
//Load asm externs second
    ExePrint2("#define BOOT_3DAYS\n;;");
    ExePutS2(KERNELB_BIN,"T:/KERNELB.HH");
  } catch {
    TOSPrint("FDSF:%s\n",&(Fs->except_ch));
    Fs->catch_except=TRUE;
  };
}
static U0 StartUpCore1(I64) {
}
U0 StartUpTasks()
{
  CTask *user1,*user2;
  user1=User;
  user2=User;
  WinToTop(user1);
  WinTileVert;
  try {
    ExeFile("/WallPaper.HC");
    ExePrint("WallPaperInit;;");
  } catch
    PutExcept;
  ACInit("T:/KERNEL*.HH*");
  AutoComplete(ON);
}
U0 Adam(U8 *) {
	I64 idx;
	LoadImps;
	CTask *user;
	cpu_structs[Gs->num=__CoreNum]=Gs;//It should be 0
	for(idx=Gs->num+1;idx<mp_cnt;idx++) {
		__SpawnCore(idx,&CoreAPSethTask);
                while(!cpu_structs[idx])
                  PAUSE;
	}
#ifndef BC_ENABLE
    if(FileFind("HCRT.DBG.Z"))
		DbgInfoRead("HCRT.DBG.Z");
#else
    if(FileFind("HCRT_BC.DBG.Z"))
		DbgInfoRead("HCRT_BC.DBG.Z");
#endif
        if(__CmdLineBootText)
          ExePrint2("%s;\n",__CmdLineBootText);
	if(!__IsCmdLine) {
		sys_winmgr_task=Spawn(&WinMgrTask);
		//Grab pointer
		ExePrint("#include \"AMOUSE.HC.Z\";;");
	} else {
		user=User;
		DeathWait(&user);
		Exit3Days;
	}
        Spawn(&StartUpCore1,NULL,,mp_cnt-1);
	Refresh; //Wait for window manager.
	StartUpTasks;
	//We are "adam" task
	SrvTaskCont;
}
//Our entering thread doesnt have a cur_thrd,so spawn one
adam_task=Spawn(&Adam,NULL,"Adam",,);
Gs->seth_task=Spawn(&CoreAPSethTask,NULL,"Seth");
//Dont Exit
BirthWait(&adam_task);
DeathWait(&adam_task);
#include "PUBLIC.HC";;
