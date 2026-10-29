#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <climits>
#include <sys/types.h>

namespace mango_target {
inline bool number(const char* text,uint64_t maximum,uint64_t& out){
    if(!text||!*text)return false;
    uint64_t value=0;
    for(const unsigned char* p=reinterpret_cast<const unsigned char*>(text);*p;++p){
        if(*p<'0'||*p>'9')return false;
        const unsigned digit=*p-'0';
        if(value>maximum/10||(value==maximum/10&&digit>maximum%10))return false;
        value=value*10+digit;
    }
    out=value;return true;
}
struct Identity {unsigned uid=UINT_MAX;uint64_t started=0;};
inline bool read(int pid,const char* package,Identity& out){
    if(pid<=0||!package||!*package)return false;
    char path[64],text[2048]{};
    std::snprintf(path,sizeof(path),"/proc/%d/cmdline",pid);
    FILE* file=std::fopen(path,"rb");if(!file)return false;
    const size_t length=std::fread(text,1,sizeof(text)-1,file);std::fclose(file);
    if(!length||!std::memchr(text,0,length)||std::strcmp(text,package))return false;
    std::snprintf(path,sizeof(path),"/proc/%d/status",pid);
    file=std::fopen(path,"r");if(!file)return false;
    int tgid=0;unsigned uid=UINT_MAX;char state=0;
    while(std::fgets(text,sizeof(text),file)){
        if(!std::strncmp(text,"Tgid:",5))std::sscanf(text+5,"%d",&tgid);
        else if(!std::strncmp(text,"Uid:",4))std::sscanf(text+4,"%u",&uid);
        else if(!std::strncmp(text,"State:",6))std::sscanf(text+6," %c",&state);
    }
    std::fclose(file);
    if(tgid!=pid||uid==UINT_MAX||!state||state=='Z'||state=='X'||state=='x')return false;
    std::snprintf(path,sizeof(path),"/proc/%d/stat",pid);
    file=std::fopen(path,"r");if(!file)return false;
    const bool have=std::fgets(text,sizeof(text),file)!=nullptr;std::fclose(file);
    if(!have)return false;
    // comm is parenthesized and may contain spaces or ')'. Field 22 is the
    // start time; compare it again after attach to reject a recycled PID.
    char* cursor=std::strrchr(text,')');if(!cursor)return false;++cursor;
    for(unsigned field=3;field<=22;++field){
        while(*cursor==' '||*cursor=='\t')++cursor;
        if(!*cursor||*cursor=='\n')return false;
        char* end=cursor;while(*end&&*end!=' '&&*end!='\t'&&*end!='\n')++end;
        if(field==22){char saved=*end;*end=0;uint64_t started=0;const bool valid=number(cursor,UINT64_MAX,started);*end=saved;if(!valid)return false;out={uid,started};return true;}
        cursor=end;
    }
    return false;
}
inline bool same(const Identity& a,const Identity& b){return a.uid==b.uid&&a.started==b.started;}
}
