#include "3d.h"
#include "ext/argtable3/argtable3.h"
#include <signal.h>
#ifndef TARGET_WIN32
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#define HCRT_INSTALLTED_DIR "/usr/local/include/3Days/HCRT.BIN"
#define HCRT_INSTALLTED_DIR_BC "/usr/local/include/3Days/HCRT_BC.BIN"
#include <libgen.h>
#include "ext/C_Unescaper/escaper.h"
#define DFT_T_DRIVE ".3DAYS_BOOT"
#define DFT_TEMPLATE "/usr/local/include/3Days/T/"
#include <unistd.h>
#else
#include <windows.h>
#include <libloaderapi.h>
#include <processthreadsapi.h>
#include <fileapi.h>
#include <userenv.h>
#include <winnt.h>
#include <synchapi.h> 
#define HCRT_INSTALLTED_DIR "\\HCRT\\HCRT_TOS.BIN"
#define HCRT_INSTALLTED_DIR_BC "\\HCRT\\HCRT_BC.BIN"
#define DFT_T_DRIVE "3DAYS_BOOT"
//Is relative to install dir on windows
#define DFT_TEMPLATE "\\T"
#endif
static void Core0Exit(int sig) {
	#ifndef TARGET_WIN32
	pthread_exit(0);
	#else
	ExitThread(0);
	#endif
} 
static struct arg_lit *helpArg;
static struct arg_str *encPasswdArg;
static struct arg_file *TDriveArg;
static struct arg_lit *sixty_fps;
static struct arg_file *cmdLineFiles;
static struct arg_lit *OverwriteBootDrvArg;
static struct arg_lit *commandLineArg;
static struct arg_lit *bounds_check_enable;
static struct arg_lit *softwareScaling;
static struct arg_end *endArg;
char CompilerPath[1024];
#ifdef TARGET_WIN32
BOOL WINAPI CtrlCHandlerRoutine(DWORD c) {
  printf("User Abort.\n");
  return FALSE;
}
#include <windows.h>
#include <dbghelp.h>
#endif
#include <signal.h>
void SignalHandler(int sig) {
    CSymbol *sym=map_get(&Loader.symbols,"Backtrace");
    if(sym) {
        ((void(*)(long ,long))sym->value_ptr)(0,0);
    }
    exit(0);
}
static char *hcrt_bin_loc=NULL;
static void Core0() {
	VFsThrdInit();
	#ifndef TARGET_WIN32
	signal(SIGUSR1,&Core0Exit);
	#endif
	Load(hcrt_bin_loc,0);
} 
#ifndef TARGET_WIN32
static pthread_t core0;
#else
static HANDLE core0;
#endif
static int is_cmd_line=0;
int64_t _shutdown=0;
int64_t IsCmdLine() {
	return is_cmd_line;
}
static char *cmd_ln_boot_txt=NULL;
char *CmdLineBootText() {
	if(!cmd_ln_boot_txt) return NULL;
	return strdup(cmd_ln_boot_txt);
}
#if 0
int _main(int argc,char **argv)
#else
int main(int argc,char **argv)
#endif
{
    char *header=NULL,*t_drive=NULL,*tmp;
    VFsGlobalInit();
    void *argtable[]= {
        helpArg=arg_lit0("h", "help", "Display this help message."),
        encPasswdArg=arg_str0(NULL,"enc_passwd","<encription password>","The encription password for .ENC[,Z] files."),
        sixty_fps=arg_lit0("6", "60fps", "Run in 60 fps mode."),
        commandLineArg=arg_lit0("c", "com", "Start in command line mode,mount drive '/' at /."),
        TDriveArg=arg_file0("t",NULL,"T(boot) Drive","This tells 3days where to use(or create) the boot drive folder."),
        OverwriteBootDrvArg=arg_lit0("O", "overwrite", "Create a fresh version of the boot drive folder."),
        cmdLineFiles=arg_filen(NULL,NULL,"<files>",0,100,"Files for use with command line mode."),
        softwareScaling=arg_lit0("s","software","Enable software scaling."),
        bounds_check_enable=arg_lit0("b", "bounds", "Use the bounds checker HCRT_BC.BIN runtime,use \"Option(OPTf_BOUNDS_CHECK,1);\"."),
        endArg=arg_end(1),
    };
    int errs=arg_parse(argc, argv, argtable);
    int run=1;
    if(encPasswdArg->count)
       cipher_passwd=encPasswdArg->sval[0];
    if(helpArg->count||errs) {
        printf("Usage is: 3d");
        arg_print_syntaxv(stdout, argtable, "\n");
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        exit(0);
    }
    if(softwareScaling->count)
		__3DaysSetSoftwareRender(1);
    if(TDriveArg->count) {
		t_drive=strdup(TDriveArg->filename[0]);
		VFsMountDrive('T',t_drive);
	} else  {
		tmp=HostHomeDir();
		t_drive=TD_MALLOC(1+1+strlen(tmp)+strlen(DFT_T_DRIVE));
		strcpy(t_drive,tmp);
		TD_FREE(tmp);
		#ifdef TARGET_WIN32
		strcat(t_drive,"\\" DFT_T_DRIVE);
		#else
		strcat(t_drive,"/" DFT_T_DRIVE);
		#endif
		#ifdef TARGET_WIN32
		char template[MAX_PATH];
		GetModuleFileNameA(NULL,template,sizeof(template));
		dirname(template);
		strcat(template,DFT_TEMPLATE);
		#endif
		#ifndef TARGET_WIN32 
		char *template=DFT_TEMPLATE;
		#endif
		//CreateTemplateBootDrv Checks if exists too
		//DFT_TEMPLATE IS RELATIVE TO PROGRAM IN WINDOWS
		if(!CreateTemplateBootDrv(t_drive,template,OverwriteBootDrvArg->count)) {
			#ifndef TARGET_WIN32
			if(access("./T",F_OK)==0) {
				fprintf(stderr,"Looks like a T drive exists here,using that.\n");
				VFsMountDrive('T',"T");
			}
			#else
			if(PathFileExistsA("./T")) {
				fprintf(stderr,"Looks like a T drive exists here,using that.\n");
				VFsMountDrive('T',"T");
			}
			#endif
			else {
				fprintf(stderr,"No T drive specified!!!.\n");
				exit(EXIT_FAILURE);
			}
		}
	}
	//IMPORTANT,init thread VFs after we make drive T
	VFsThrdInit();
    TOS_RegisterFuncPtrs();
    if(commandLineArg->count||sixty_fps->count) {
		char buf[1024];
		vec_char_t boot_str;
		vec_init(&boot_str);
		if(commandLineArg->count) {
			VFsMountDrive('Z',"/");
			is_cmd_line=1;
			strcpy(buf,"Cd(\"Z:/\");;\nCd(\"");
			vec_pusharr(&boot_str,buf,strlen(buf));
			#ifndef TARGET_WIN32
			getcwd(buf,sizeof(buf));
			#else
			getcwd(buf,sizeof(buf));
			#endif
			vec_pusharr(&boot_str,buf,strlen(buf));
			strcpy(buf,"\");;\n");
			vec_pusharr(&boot_str,buf,strlen(buf));
			int64_t i;
			for(i=0;i!=cmdLineFiles->count;i++) {
				sprintf(buf,"#include \"%s\";\n",cmdLineFiles->filename[i]);
				vec_pusharr(&boot_str,buf,strlen(buf));
			}
		}
		if(sixty_fps->count) {
			char *set="SetFPS(60.);;\n";
			vec_pusharr(&boot_str,set,strlen(set));
		}
		vec_push(&boot_str,0);
		cmd_ln_boot_txt=boot_str.data;
	}
	InitSound();
	if(1) {
		//Create the Window,there is 1 screen God willing.
		if(!is_cmd_line) {
			NewDrawWindow();
		}
        int flags=0;
    #ifndef TARGET_WIN32
		if(!bounds_check_enable->count) {
			if(0==access("HCRT.BIN",F_OK)) {
				puts("Using ./HCRT.BIN as the default binary.");
				hcrt_bin_loc="HCRT.BIN";
				LaunchCore0(Core0);
			} else if(0==access( HCRT_INSTALLTED_DIR,F_OK)) {
				hcrt_bin_loc=HCRT_INSTALLTED_DIR;
				LaunchCore0(Core0);
			}
		} else {
			if(0==access("HCRT_BC.BIN",F_OK)) {
				puts("Using ./HCRT_BC.BIN as the default binary.");
				hcrt_bin_loc="HCRT_BC.BIN";
				LaunchCore0(Core0);
			} else if(0==access( HCRT_INSTALLTED_DIR,F_OK)) {
				hcrt_bin_loc=HCRT_INSTALLTED_DIR;
				LaunchCore0(Core0);
			}

		}
    #else
      char buffer[MAX_PATH];
      GetModuleFileNameA(NULL,buffer,sizeof(buffer));
      dirname(buffer);
      if(!bounds_check_enable->count) {
		strcat(buffer,"\\HCRT.BIN");
	  } else 
	  	strcat(buffer,"\\HCRT_BC.BIN");
      hcrt_bin_loc=strdup(buffer);
      puts(buffer);
      if(GetFileAttributesA(buffer)!=INVALID_FILE_ATTRIBUTES) {
		LaunchCore0(Core0);
      }
    #endif
    }
    if(!commandLineArg->count) {
		InputLoop(&_shutdown);
		exit(0);
	} else {
		WaitForCore0();
	}
    exit(0);
    return 0;
}
CLoader Loader;
void __Shutdown() {
	_shutdown=1;
	__ShutdownCores();
	exit(0);
}
char *cipher_passwd;
