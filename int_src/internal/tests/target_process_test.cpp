#include "target_process.h"
#include <atomic>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <initializer_list>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

int main(int argc,char** argv){
    uint64_t n=99;
    for(const char* bad:{"","-1","+1","1x"," 1","1 ","2147483648","999999999999999999999999"})assert(!mango_target::number(bad,INT_MAX,n));
    assert(mango_target::number("2147483647",INT_MAX,n)&&n==INT_MAX);
    assert(mango_target::number("0",0,n)&&n==0);
    assert(!mango_target::number("1",0,n));
    assert(mango_target::number("18446744073709551615",UINT64_MAX,n)&&n==UINT64_MAX);
    assert(!mango_target::number("18446744073709551616",UINT64_MAX,n));
    mango_target::Identity self;
    assert(argc&&mango_target::read(getpid(),argv[0],self)&&self.uid==getuid());
    assert(!mango_target::read(-1,argv[0],self));
    assert(!mango_target::read(getpid(),"different-package",self));
    std::atomic<int> tid{0};std::atomic<bool> release{false};
    std::thread thread([&]{tid.store(int(syscall(SYS_gettid)));while(!release.load())std::this_thread::yield();});
    while(!tid.load())std::this_thread::yield();
    mango_target::Identity identity;assert(!mango_target::read(tid.load(),argv[0],identity));
    release.store(true);thread.join();
    int ready[2];assert(pipe2(ready,O_CLOEXEC)==0);
    const char* package="com.catsbit.oxidesurvivalisland";
    const pid_t child=fork();assert(child>=0);
    if(!child){close(ready[0]);execl("/bin/sleep",package,"60",static_cast<char*>(nullptr));_exit(127);}
    close(ready[1]);char byte;assert(read(ready[0],&byte,1)==0);close(ready[0]);
    mango_target::Identity first,again;assert(mango_target::read(child,package,first));
    assert(first.uid==getuid()&&mango_target::read(child,package,again)&&mango_target::same(first,again));
    assert(!mango_target::read(child,"com.catsbit.oxidesurvivalisland:other",again));
    auto changed=first;++changed.started;assert(!mango_target::same(first,changed));
    changed=first;++changed.uid;assert(!mango_target::same(first,changed));
    assert(kill(child,SIGTERM)==0);siginfo_t stopped{};
    assert(waitid(P_PID,child,&stopped,WEXITED|WNOWAIT)==0);
    assert(!mango_target::read(child,package,again));
    assert(waitpid(child,nullptr,0)==child);
    std::puts("PASS: explicit PID identity, real procfs UID/start time, thread and zombie rejection, strict numeric arguments");
}
