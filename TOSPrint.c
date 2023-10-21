#include "3d.h"
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
void TOSPrint(const char *fmt,int64_t argc,int64_t *argv) {
    char *txt=MStrPrint(fmt,argc,argv);
    printf("%s",txt);
    TD_FREE(txt);
}
char *MStrPrint(const char *fmt,int64_t argc,int64_t *argv) {
    vec_char_t ret;
    vec_init(&ret);
    int64_t arg=-1;
    char *start=fmt,*end;
loop:
    ;
    arg++;
    end=strchr(start,'%');
    if(!end) end=start+strlen(start);
    vec_pusharr(&ret,start,end-start);
    if(!*end) goto end;
    start=end+1;
    bool sign=*start=='-';
    if(sign) start++;
    bool zero=*start=='0';
    if(zero) start++;
    int64_t width=0,decimals=0;
    while(isdigit(*start)) {
        width*=10;
        width+=*start-'0';
        start++;
    }
    bool decimal=*start=='.';
    if(decimal) start++;
    while(isdigit(*start)) {
        decimals*=10;
        decimals+=*start-'0';
        start++;
    }
    bool tf=0,cf=0,$f=0,sf=0;
    while(strchr("t,$/",*start)) {
        switch(*start) {
        case 't':
            tf=true;
            break;
        case ',':
            cf=true;
            break;
        case '$':
            $f=true;
            break;
        case '/':
            sf=true;
            break;
        }
        start++;
    }
    int64_t aux=1;
    if(*start=='*') {
        aux=argv[arg++];
        start++;
    } else if(*start=='h') {
        while(isdigit(*start)) {
            aux*=10;
            aux+=*start-'0';
            start++;
        }
    }
    vec_char_t tmp;
    vec_init(&tmp);
    switch(*start) {
    case 'd':
    case 'i': {
        char buffer[216];
        sprintf(buffer,"%ld",argv[arg]);
        vec_pusharr(&tmp, buffer, strlen(buffer));
        break;
    }
    case 'u': {
        char buffer[216];
        sprintf(buffer,"%lu",argv[arg]);
        vec_pusharr(&tmp, buffer, strlen(buffer));
        break;
    }
    case 'o':  {
        char buffer[216];
        sprintf(buffer,"%lo",argv[arg]);
        vec_pusharr(&tmp, buffer, strlen(buffer));
        break;
    }
    case 'n': {
        char buffer[256];
        sprintf(buffer,"%lf",((double*)argv)[arg]);
        vec_pusharr(&tmp, buffer, strlen(buffer));
        break;
    }
    case 'c': {
        while(--aux>=0) {
          uint64_t chr=argv[arg];
            while(chr>0) {
                uint8_t c=chr&0xff;
                chr>>=8;
                if(c)
                    vec_push(&tmp,c);
            }
        }
        break;
    }
    case 'p': {
        char buffer[256];
        sprintf(buffer,"%p",(void*)argv[arg]);
        vec_pusharr(&tmp, buffer, strlen(buffer));
        break;
    }
    case 's': {
        while(--aux>=0) {
            char *str=((char**)argv)[arg];
            vec_pusharr(&tmp,str,strlen(str));
        }
        break;
    }
    case 'q': {
        char *str=((char**)argv)[arg];
        char *buf=TD_MALLOC(strlen(str)*4+1);
        unescapeString(str,buf);
        vec_pusharr(&tmp,buf,strlen(buf));
        TD_FREE(buf);
        break;
    }
    case '%': {
        vec_pusharr(&tmp,"%",1);
        break;
    }
    }
    start++;
    vec_extend(&ret, &tmp);
    vec_deinit(&tmp);
    goto loop;
end:
    vec_push(&ret, 0);
    char *r=strdup(ret.data);
    vec_deinit(&ret);
    return r;
}
