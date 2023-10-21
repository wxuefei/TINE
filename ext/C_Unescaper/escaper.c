#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "escaper.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
typedef int(*digitGetter)(char*);
char* unescapeString(uint8_t* str,uint8_t* where) {
	while(*str!=0) {
		//sorry for long code
		if(*str=='\\') {
			memcpy(where,"\\\\",2);
			where+=2;
			str++;
			continue;
		}
		if(*str=='\a') {
			memcpy(where,"\\a",2);
			where+=2;
			str++;
			continue;
		}
		if(*str=='\b') {
			memcpy(where,"\\b",2);
			where+=2;
			str++;
			continue;
		}
		if(*str=='\f') {
			memcpy(where,"\\f",2);
			str++;
			where+=2;
			continue;
		}
		if(*str=='\n') {
			memcpy(where,"\\n",2);
			str++;
			where+=2;
			continue;
		}
		if(*str=='\r') {
			memcpy(where,"\\r",2);
			str++;
			where+=2;
			continue;
		}
		if(*str=='\t') {
			memcpy(where,"\\t",2);
			str++;
			where+=2;
			continue;
		}
		if(*str=='\v') {
			memcpy(where,"\\v",2);
			str++;
			where+=2;
			continue;
		}
		if(*str=='\"') {
			memcpy(where,"\\\"",2);
			where+=2;
			str++;
			continue;
		}
		//if cant be inputed with a (us) keyboard,escape

		const char* valids=" ~!@#$%^&*()_+|}{[]\\;':\",./<>?";
		bool isValid=false;
		if(*str>='a'&&'z'>=*str)
			isValid=true;
		else if(*str>='A'&&'Z'>=*str)
			isValid=true;
		else if(*str>='0'&&*str<='9')
			isValid=true;
		else if(strchr(valids,*str)!=NULL)
			isValid=true;
		if(!isValid) {
			char temp[5];
			sprintf(temp,"%o",str[0]);
			int len=strlen(temp);
			memmove(temp+3-len, temp, len);
			memset(temp, '0', 3-len);
			where[0]='\\';
			memcpy(where+1,temp,4);
			str++;
			where+=4;
			continue;
		}
		*where=*str;
		str++;
		where++;
	}
	*where=0;
	return (char*)where;
}
