#include "3d.h"
static int RootPathLen();
//I dislike this file. Windows uses '\\' as a delimeter ,but TempleOS uses '/'
//So key an eye out for delim/TOS_delim

//Send a prayer to whoever wrote this (https://www.daniweb.com/programming/software-development/threads/197362/how-to-open-a-directory-using-createfile)
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#ifndef TARGET_WIN32
#include <pthread.h>
#else
#include <windows.h>
#include <synchapi.h>
#include <processthreadsapi.h>
static int64_t FILETIME2Unix(FILETIME *t) {
	//https://www.frenk.com/2009/12/convert-filetime-to-unix-timestamp/	
	int64_t time=t->dwLowDateTime|((int64_t)t->dwHighDateTime<<32),adj;
	adj=10000*(int64_t)11644473600000ll;
	time-=adj;
	return time/10000000ll;
}
#endif
void VFsGlobalInit() {
}
#define TOS_delim '/'
#ifdef TARGET_WIN32
#include <windows.h>
#include <fileapi.h>
#define delim '\\'
#else
#define delim '/'
#include <sys/types.h>
#include <sys/stat.h>
#endif
static char *VFsInplaceConvertDelims(char *p) {
	if(TOS_delim!=delim)
		for(;strchr(p,delim);)
			*strchr(p,delim)=TOS_delim;
	return p;
}
static char *VFsInplaceHostDelims(char *p) {
	if(TOS_delim!=delim)
		for(;strchr(p,TOS_delim);)
			*strchr(p,TOS_delim)=delim;
	return p;
}
static char *VFsInplaceRemoveRepeatSlashes(char *p) {
	char *dst=p,*o=p;
	for(;*p;p++) {
	  if(dst!=p)
	    *dst=*p;
	  if(*p=='/'&&p[1]=='/') {
	  } else
		dst++;
	}
	*dst=0;
	return o;
}
__thread char thrd_pwd[1024];
__thread char thrd_drv;
void VFsThrdInit() {
	strcpy(thrd_pwd,"/");
	thrd_drv='T';
}
void VFsSetDrv(char d) {
	if(!isalpha(d)) return;
	thrd_drv=toupper(d);
}
void VFsSetPwd(char *pwd) {
	if(!pwd) pwd="/";
	strcpy(thrd_pwd,pwd);
}
#ifdef TARGET_WIN32
static int __FExists(char *path) {
	char buf[strlen(path)+128];
	strcpy(buf,path);
	if(buf[strlen(path)-1]==delim)
		buf[strlen(path)-1]=0;
	path=buf;
	if(strchr(buf,'*')||strchr(buf,'?')) {
		//Not in the mood to match wildcards,we do that in HolyC
		return 0;
	}
    return PathFileExistsA(buf);
}
static int __FIsDir(char *path) {
	int r=0;
	char buf[strlen(path)+128];
	strcpy(buf,path);
	if(buf[strlen(path)-1]==delim)
		buf[strlen(path)-1]=0;
	path=buf;
    if(strchr(buf,'*')||strchr(buf,'?')) {
		//Not in the mood to match wildcards,we do that in HolyC
		return 0;
	}
	return PathIsDirectoryA(buf);
}
#else
static int __FExists(char *path) {
    return access(path,F_OK)==0;
}
static int __FIsDir(char *path) {
    struct stat s;
    stat(path,&s);
    return (s.st_mode&S_IFMT)==S_IFDIR;
}
#endif
int VFsCd(char *to,int flags) {
	to=__VFsFileNameAbs(to);
	if(__FExists(to)&&__FIsDir(to)) {
		TD_FREE(to);
		return 1;
	} else if(flags&VFS_CDF_MAKE) {
		#ifndef TARGET_WIN32
		mkdir(to,0700);
		#else
		mkdir(to);
		#endif
		TD_FREE(to);
		return 1;
	}
	return 0;
}
#ifndef TARGET_WIN32
static void DelDir(char *p) {
	DIR *d=opendir(p); 
	struct dirent *d2;
	char od[2048];
	while(d2=readdir(d)) {
		if(!strcmp(".",d2->d_name)||!strcmp("..",d2->d_name))
			continue;
		strcpy(od,p);
		strcat(od,"/");
		strcat(od,d2->d_name);
		if(__FIsDir(od)) {
			DelDir(od);
		} else {
			remove(od);
		}
	}
	closedir(d);
	rmdir(p);
}
#else
static void DelDir(char *p) {
	DIR *d=opendir(p); 
	struct dirent *d2;
	char od[PATH_MAX];
	while(d2=readdir(d)) {
		if(!strcmp(".",d2->d_name)||!strcmp("..",d2->d_name))
			continue;
		strcpy(od,p);
		strcat(od,"\\");
		strcat(od,d2->d_name);
		if(__FIsDir(od)) {
			DelDir(od);
		} else {
			DeleteFileA(od);
		}
	}
	closedir(d);
	RemoveDirectory(p);
}
#endif
int64_t VFsDel(char *p) {
	int r;
	p=__VFsFileNameAbs(p);
	if(!__FExists(p))
		return TD_FREE(p),0;
	#ifdef TARGET_WIN32
	if(__FIsDir(p))
		DelDir(p);
	else
		DeleteFileA(p);
	#else
	if(__FIsDir(p)) {
		DelDir(p);
	} else 
		remove(p);
	#endif
	r=!__FExists(p);
	TD_FREE(p);
	return r;
}
static char *mount_points['z'-'a'+1];
//Returns Host OS location of file
char *__VFsFileNameAbs(char *name) {
	char computed[1024];
	strcpy(computed,mount_points[toupper(thrd_drv)-'A']);
	computed[strlen(computed)+1]=0;
	computed[strlen(computed)]=delim;
	strcat(computed,thrd_pwd);
	computed[strlen(computed)+1]=0;
	computed[strlen(computed)]=delim;
	strcat(computed,name);
	return strdup(computed);
}
#ifndef TARGET_WIN32
int64_t VFsUnixTime(char *name) {
	char *fn=__VFsFileNameAbs(name);
	struct stat s;
	stat(fn,&s);
	int64_t r=mktime(localtime(&s.st_ctime));
	TD_FREE(fn);
	return r;
}
int64_t VFsFSize(char *name) {
	struct stat s;
	long cnt;
	DIR *d;
	char *fn=__VFsFileNameAbs(name);
	if(!__FExists(fn)) {
		TD_FREE(fn);
		return -1;
	} else if(__FIsDir(fn)) {
		d=opendir(fn);
		cnt=0;
		while(readdir(d))
			cnt++;
		closedir(d);
		TD_FREE(fn);
		return cnt;
	}
	stat(fn,&s);
	TD_FREE(fn);
	return s.st_size;
}
#else
int64_t VFsUnixTime(char *name) {
	char *fn=__VFsFileNameAbs(name);
	FILETIME t;
	if(!fn) return 0;
	if(!__FExists(fn))
	  return 0;
	HANDLE fh=CreateFileA(fn,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
	GetFileTime(fh,NULL,NULL,&t);
	TD_FREE(fn);
	CloseHandle(fh);
	return FILETIME2Unix(&t);
}
int64_t VFsFSize(char *name) {
	char *fn=__VFsFileNameAbs(name);
	int64_t s64;
	int32_t h32;
	if(!fn) return 0;
	if(!__FExists(fn)) {
		TD_FREE(fn);
		return 0;
    }
	if(__FIsDir(fn)) {
		WIN32_FIND_DATAA data;
		HANDLE dh;
		char buffer[strlen(fn)+4];
		strcpy(buffer,fn);
		strcat(buffer,"\\*");
		s64=0;
		dh=FindFirstFileA(buffer,&data);
		while(FindNextFileA(dh,&data))
			s64++;
		TD_FREE(fn);
		CloseHandle(dh);
		return s64;
	}
	HANDLE fh=CreateFileA(fn,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
	s64=GetFileSize(fh,&h32);
	s64|=(int64_t)h32<<32;
	TD_FREE(fn);
	CloseHandle(fh);
	return s64;
}
#endif
int64_t VFsFileWrite(char *name,char *data,int64_t len) {
    FILE *f;
    name=__VFsFileNameAbs(name);
    if(name) {
        f=fopen(name,"wb");
        if(f) {
			fwrite(data,1,len,f);
			fclose(f);
		}
    }
    TD_FREE(name);
    return !!name;
}
int64_t VFsFileRead(char *name,int64_t *len) {
    if(len) *len=0;
    FILE *f;
    int64_t s,e;
    void *data=NULL;
    name=__VFsFileNameAbs(name);
    if(!name) return NULL;
    if(__FExists(name))
    if(!__FIsDir(name)) {
        f=fopen(name,"rb");
        if(!f) goto end;
        s=ftell(f);
        fseek(f,0,SEEK_END);
        e=ftell(f);
        fseek(f,0,SEEK_SET);
        fread(data=HolyMAlloc(e-s+1),1,e-s,f);
        fclose(f);
        if(len) *len=e-s;
        ((char*)data)[e-s]=0;
    }
    end:
    TD_FREE(name);
    return data;
}
#ifdef TARGET_WIN32
#include <shlobj.h> 
char *HostHomeDir() {
	char home[MAX_PATH];
	if(S_OK==SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, home))
		return strdup(home);
	return NULL;
}
#else
#include <pwd.h>
//https://stackoverflow.com/questions/2910377/get-home-directory-in-linux
char *HostHomeDir() {
	char buf[0x2000],*ret;
	struct passwd pwd;
	struct passwd *result;
	getpwuid_r(getuid(),&pwd,buf,0x2000,&result);
	if(result) {
		return strdup(result->pw_dir);
	}
	return NULL;
}
#endif
char **VFsDir(char *fn) {
	int64_t sz;
	char **ret;
    fn=__VFsFileNameAbs("");
    DIR *dir=opendir(fn);
    if(!dir) {
        TD_FREE(fn);
        return NULL;
    }
    struct dirent *ent;
    vec_str_t items;
    vec_init(&items);
    while(ent=readdir(dir)) {
		//CDIR_FILENAME_LEN  is 38(includes nul terminator)
		if(strlen(ent->d_name)<=37)
		  vec_push(&items,HolyStrDup(ent->d_name));
	}
    vec_push(&items,NULL);
    TD_FREE(fn);
    sz=items.length*sizeof(char*);
    ret=memcpy(HolyMAlloc(sz),items.data,sz);
    vec_deinit(&items);
    closedir(dir);
    return ret;
}
//Creates a virtual drive by a template
static void CopyDir(char *dst,char *src) {
	if(!__FExists(dst)) {
		#ifdef TARGET_WIN32
		mkdir(dst);
		#else
		mkdir(dst,0700);
		#endif
	}
	char buf[1024],sbuf[1024],*s,buffer[0x10000];
	int64_t root,sz,sroot,r;
	strcpy(buf,dst);
	buf[root=strlen(buf)]=delim;
	buf[++root]=0;
	
	strcpy(sbuf,src);
	sbuf[sroot=strlen(sbuf)]=delim;
	sbuf[++sroot]=0;
	
	DIR *d=opendir(src);
	struct dirent *ent;
	while(ent=readdir(d)) {
		if(!strcmp(ent->d_name,".")||!strcmp(ent->d_name,".."))
			continue;
		buf[root]=0;
		sbuf[sroot]=0;
		strcat(buf,ent->d_name);
		strcat(sbuf,ent->d_name);
		if(__FIsDir(sbuf)) {
			CopyDir(buf,sbuf);
		} else {
			FILE *read=fopen(sbuf,"rb"),*write=fopen(buf,"wb");
			while(r=fread(buffer,1,sizeof(buffer),read)) {
				if(r<0) break;
				fwrite(buffer,1,r,write);
			}
			fclose(read);
			fclose(write);
		}
	}
}

static int __FIsNewer(char *fn,char *fn2) {
	#ifndef TARGET_WIN32
	struct stat s,s2;
	stat(fn,&s),stat(fn2,&s2);
	int64_t r=mktime(localtime(&s.st_ctime)),r2=mktime(localtime(&s2.st_ctime));
	if(r>r2) return 1;
	else return 0;
	#else
	int32_t h32;
	int64_t s64,s64_2;
	FILETIME t;
	HANDLE fh=CreateFileA(fn,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL),
		fh2=CreateFileA(fn2,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
	GetFileTime(fh,NULL,NULL,&t);
	s64=t.dwLowDateTime|((int64_t)t.dwHighDateTime<<32);
	GetFileTime(fh2,NULL,NULL,&t);
	s64_2=t.dwLowDateTime|((int64_t)t.dwHighDateTime<<32);
	CloseHandle(fh),CloseHandle(fh2);
	return s64>s64_2;
	#endif
}

int CreateTemplateBootDrv(char *to,char *template,int overwrite) {
	char buffer[1024],drvl[16],buffer2[1024];
	if(!overwrite)
		if(__FExists(to)) {
			if(!__FExists(template)) {
				VFsMountDrive('T',to);
				return 1;
			} else if(!__FIsNewer(template,to)) {
				VFsMountDrive('T',to);
				return 1;
			}
		}
	if(__FExists(to)) {
				int64_t try;
				for(try=0;try!=0x10000;try++) {
					sprintf(buffer,"%s_BAKCUP.%d",to,try);
					if(!__FExists(buffer)) {
						printf("Newer Template drive found,backing up old drive to \"%s\".\n",buffer);
						//Rename the old boot drive to something else
						#ifdef TARGET_WIN32
						MoveFile(to,buffer);
						#else
						rename(to,buffer);
						#endif
						break;
					}
				}
			}
	#ifdef TARGET_WIN32
    strcpy(buffer2,template);
    strcat(buffer2,"\\");
    #else
    strcpy(buffer2,template);
    #endif
    if(!__FExists(buffer2)) {
		fprintf(stderr,"Use \"./3d_loader -t T\" to specify the T drive.\n");
		return 0; 
	}
    CopyDir(to,buffer2);
    VFsMountDrive('T',to);
    return 1;
}
int VFsIsDir(char *path) {
	path=__VFsFileNameAbs(path);
	int e=__FIsDir(path);
	TD_FREE(path);
	return e;
}
int VFsFileExists(char *path) {
	path=__VFsFileNameAbs(path);
	int e=__FExists(path);
	TD_FREE(path);
	return e;
}
int VFsMountDrive(char let,char *path) {
	int idx=toupper(let)-'A';
	if(mount_points[idx])
	  TD_FREE(mount_points[idx]);
	mount_points[idx]=malloc(strlen(path)+1);
	strcpy(mount_points[idx],path);
}
FILE *VFsFOpen(char *path,char *m) {
	path=__VFsFileNameAbs(path);
	FILE *f=fopen(path,m);
	TD_FREE(path);
	return f;
}
