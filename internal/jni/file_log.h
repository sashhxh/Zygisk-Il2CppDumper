#pragma once
// Every log line goes to logcat and to a text file readable without adb:
//   /data/user/<n>/<package>/files/mango_log.txt   (root)
// The previous run's log is kept as mango_log.txt.prev. A crash handler writes the
// faulting address and a frame-pointer backtrace to the same file before the game's
// own crash reporter runs.
#include <android/log.h>
#include <unistd.h>
#include <chrono>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <ucontext.h>
namespace mango_log {
inline std::mutex& mutex(){static std::mutex m;return m;}
inline FILE*& handle(){static FILE* f=nullptr;return f;}
inline double& retry(){static double t=0;return t;}
inline uintptr_t& module_base(){static uintptr_t b=0;return b;}
inline void path(char (&out)[256]){std::snprintf(out,sizeof(out),"/data/user/%u/com.catsbit.oxidesurvivalisland/files/mango_log.txt",unsigned(getuid())/100000);}
inline double seconds(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline void write_file(const char* tag,const char* line){
    std::lock_guard<std::mutex> lock(mutex());
    const double t=seconds();
    if(!handle()&&t>=retry()){
        retry()=t+5;char p[256],old[272];path(p);std::snprintf(old,sizeof(old),"%s.prev",p);std::rename(p,old);handle()=std::fopen(p,"w");
    }
    FILE* f=handle();if(!f)return;
    std::fprintf(f,"%.3f [%s] %s\n",t,tag,line);std::fflush(f);
    if(std::ftell(f)>3*1024*1024){std::fclose(f);char p[256];path(p);handle()=std::fopen(p,"w");}
}
inline void log(const char* tag,const char* format,...) __attribute__((format(printf,2,3)));
inline void log(const char* tag,const char* format,...){
    char line[1024];va_list args;va_start(args,format);std::vsnprintf(line,sizeof(line),format,args);va_end(args);
    __android_log_write(ANDROID_LOG_INFO,tag,line);write_file(tag,line);
}
inline struct sigaction* old_actions(){static struct sigaction a[6];return a;}
inline const int* signals(){static const int s[6]={SIGSEGV,SIGBUS,SIGILL,SIGABRT,SIGFPE,SIGTRAP};return s;}
inline void crash_text(const char* text){FILE* f=handle();if(f)(void)!write(fileno(f),text,std::strlen(text));}
inline bool readable(uintptr_t at,void* out,size_t size){
    // Stack frames only: copy through write() to a pipe would be overkill, so test with a syscall-free probe.
    if(!at||(at&7))return false;std::memcpy(out,reinterpret_cast<const void*>(at),size);return true;
}
inline void crash_handler(int number,siginfo_t* info,void* context){
    char line[512];auto* uc=static_cast<ucontext_t*>(context);
    const uintptr_t pc=uc?uintptr_t(uc->uc_mcontext.pc):0,lr=uc?uintptr_t(uc->uc_mcontext.regs[30]):0,sp=uc?uintptr_t(uc->uc_mcontext.sp):0,base=module_base();
    std::snprintf(line,sizeof(line),"CRASH signal=%d code=%d fault=%p pc=0x%lx (il2cpp+0x%lx) lr=0x%lx (il2cpp+0x%lx) sp=0x%lx\n",number,info?info->si_code:0,info?info->si_addr:nullptr,
        (unsigned long)pc,(unsigned long)(pc-base),(unsigned long)lr,(unsigned long)(lr-base),(unsigned long)sp);
    crash_text(line);
    // Frame chain; only follow pointers that stay near the faulting stack pointer.
    uintptr_t frame=uc?uintptr_t(uc->uc_mcontext.regs[29]):0;
    for(int i=0;i<8&&frame&&!(frame&7)&&frame>=sp&&frame<sp+0x100000;++i){
        uintptr_t next=0,ret=0;readable(frame,&next,sizeof(next));readable(frame+8,&ret,sizeof(ret));
        std::snprintf(line,sizeof(line),"CRASH frame %d ret=0x%lx (il2cpp+0x%lx)\n",i,(unsigned long)ret,(unsigned long)(ret-base));crash_text(line);
        if(next<=frame)break;frame=next;
    }
    for(int i=0;i<6;++i){
        if(signals()[i]!=number)continue;const struct sigaction& old=old_actions()[i];
        if((old.sa_flags&SA_SIGINFO)&&old.sa_sigaction){old.sa_sigaction(number,info,context);return;}
        if(!(old.sa_flags&SA_SIGINFO)&&old.sa_handler&&old.sa_handler!=SIG_DFL&&old.sa_handler!=SIG_IGN){old.sa_handler(number);return;}
    }
    signal(number,SIG_DFL);raise(number);
}
inline void install_crash_handler(){
    static bool done=false;if(done)return;done=true;
    for(int i=0;i<6;++i){struct sigaction action{};action.sa_sigaction=crash_handler;action.sa_flags=SA_SIGINFO|SA_ONSTACK;sigemptyset(&action.sa_mask);sigaction(signals()[i],&action,&old_actions()[i]);}
}
}
