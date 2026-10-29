// cfinject — root ptrace dlopen-инжектор, arm64 Android.
// usage: cfinject <package> </абсолютный/путь/libcfesp.so> [pid [uid]]
// Requires root and a device policy that permits ptrace and loading this .so.
// Runner does not change global SELinux mode.
#include <dirent.h>
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
#include "target_process.h"

#ifndef NT_ARM_SYSTEM_CALL
#define NT_ARM_SYSTEM_CALL 0x404
#endif

struct Regs { uint64_t x[31]; uint64_t sp, pc, pstate; };   // = user_pt_regs (arm64)

static int find_pid(const char* pkg) {
    DIR* d = opendir("/proc");
    if (!d) return -1;
    int res = -1;
    while (dirent* e = readdir(d)) {
        int pid = atoi(e->d_name);
        if (pid <= 0) continue;
        mango_target::Identity identity;
        if(mango_target::read(pid,pkg,identity)){
            if(res>0){res=-2;break;}
            res=pid;
        }
    }
    closedir(d);
    return res;
}

// lowest start-адрес маппинга; exact != 0 — полное совпадение пути, иначе подстрока
static uintptr_t mod_base(int pid, const char* path, bool exact) {
    char p[64]; snprintf(p, sizeof p, "/proc/%d/maps", pid);
    FILE* f = fopen(p, "r");
    if (!f) return 0;
    char line[600];
    uintptr_t best = 0;
    while (fgets(line, sizeof line, f)) {
        unsigned long s, e; char perm[8] = {0}, mp[300] = {0};
        if (sscanf(line, "%lx-%lx %7s %*s %*s %*s %299[^\n]", &s, &e, perm, mp) < 4) continue;
        bool hit = exact ? !strcmp(mp, path) : (strstr(mp, path) != nullptr);
        if (hit && (!best || s < best)) best = s;
    }
    fclose(f);
    return best;
}

static uintptr_t remote_sym(int pid, void* local) {
    Dl_info di;
    if (!dladdr(local, &di) || !di.dli_fname) return 0;
    uintptr_t rb = mod_base(pid, di.dli_fname, true);
    if (!rb) { const char* bn = strrchr(di.dli_fname, '/'); rb = mod_base(pid, bn ? bn : di.dli_fname, false); }
    if (!rb) return 0;
    return rb + ((uintptr_t)local - (uintptr_t)di.dli_fbase);
}

static bool get_regs(int pid, Regs* r) {
    iovec io = {r, sizeof *r};
    return ptrace(PTRACE_GETREGSET, pid, (void*)NT_PRSTATUS, &io) == 0;
}
static bool set_regs(int pid, Regs* r) {
    iovec io = {r, sizeof *r};
    return ptrace(PTRACE_SETREGSET, pid, (void*)NT_PRSTATUS, &io) == 0;
}
static bool get_sc(int pid, int* nr) {
    iovec io = {nr, sizeof *nr};
    return ptrace(PTRACE_GETREGSET, pid, (void*)NT_ARM_SYSTEM_CALL, &io) == 0;
}
static bool set_sc(int pid, int nr) {
    iovec io = {&nr, sizeof nr};
    return ptrace(PTRACE_SETREGSET, pid, (void*)NT_ARM_SYSTEM_CALL, &io) == 0;
}
static bool wr_mem(int pid, uintptr_t a, const void* b, size_t n) {
    char p[64]; snprintf(p, sizeof p, "/proc/%d/mem", pid);
    int fd = open(p, O_WRONLY);
    if (fd < 0) return false;
    ssize_t k = pwrite(fd, b, n, (off_t)a);
    close(fd);
    return k == (ssize_t)n;
}
static bool rd_mem(int pid, uintptr_t a, void* b, size_t n) {
    char p[64]; snprintf(p, sizeof p, "/proc/%d/mem", pid);
    int fd = open(p, O_RDONLY);
    if (fd < 0) return false;
    ssize_t k = pread(fd, b, n, (off_t)a);
    close(fd);
    return k > 0;
}

// вызов fn(a0,a1) в удалённом потоке. data!=null -> копируется на стек, a0 = его адрес.
// lr — адрес в не-исполняемом маппинге модуля приложения: возврат = SIGSEGV с pc==lr, x0 цел,
// и dlopen видит вызывающего из namespace приложения.
static bool remote_call(int pid, uintptr_t fn, uint64_t a0, uint64_t a1, uintptr_t lr,
                        const void* data, size_t dlen, uint64_t* ret) {
    Regs orig, r;
    int osc = -1;
    if (!get_regs(pid, &orig)) { perror("GETREGSET"); return false; }
    get_sc(pid, &osc);
    r = orig;
    uintptr_t sp = (orig.sp - 0x800) & ~(uintptr_t)0xF;
    if (data) {
        if (!wr_mem(pid, sp, data, dlen)) { perror("write mem"); return false; }
        a0 = sp;
    }
    r.sp = sp - 0x100;
    r.x[0] = a0; r.x[1] = a1; r.x[30] = lr; r.pc = fn;
    if (!set_regs(pid, &r)) { perror("SETREGSET"); return false; }
    set_sc(pid, -1);                                // отменить рестарт прерванного syscall

    bool ok = false;
    int sig = 0;
    for (;;) {
        if (ptrace(PTRACE_CONT, pid, 0, (void*)(long)sig) != 0) { perror("CONT"); break; }
        int st;
        if (waitpid(pid, &st, __WALL) < 0) { perror("waitpid"); break; }
        if (!WIFSTOPPED(st)) { fprintf(stderr, "target exited\n"); return false; }
        sig = WSTOPSIG(st);
        Regs cur;
        if (!get_regs(pid, &cur)) break;
        if ((sig == SIGSEGV || sig == SIGILL || sig == SIGBUS) && cur.pc == lr) { if (ret) *ret = cur.x[0]; ok = true; break; }
        if (sig == SIGSEGV || sig == SIGBUS || sig == SIGILL || sig == SIGABRT) {
            fprintf(stderr, "remote fault sig=%d pc=%llx\n", sig, (unsigned long long)cur.pc);
            break;
        }
        // чужой сигнал — пробросить и продолжить
    }
    set_regs(pid, &orig);
    if (osc != -1) set_sc(pid, osc);
    return ok;
}

int main(int argc, char** argv) {
    if (argc < 3 || argc > 5) { fprintf(stderr, "usage: %s <package> <abs path to .so> [pid [uid]]\n", argv[0]); return 1; }
    if (geteuid() != 0) { fprintf(stderr, "need root\n"); return 1; }
    const char* pkg = argv[1];
    const char* so = argv[2];

    int pid=-1;uint64_t value=0;unsigned expected_uid=UINT_MAX;
    if(argc>=4){
        if(!mango_target::number(argv[3],INT_MAX,value)||!value){fprintf(stderr,"invalid target PID\n");return 1;}
        pid=int(value);
        if(argc==5){if(!mango_target::number(argv[4],UINT_MAX-1,value)){fprintf(stderr,"invalid target UID\n");return 1;}expected_uid=unsigned(value);}
    }else pid=find_pid(pkg);
    if(pid==-2){fprintf(stderr,"multiple matching processes; supply an explicit PID\n");return 1;}
    if (pid < 0) { fprintf(stderr, "process %s not running\n", pkg); return 1; }
    mango_target::Identity selected;
    if(!mango_target::read(pid,pkg,selected)||(expected_uid!=UINT_MAX&&selected.uid!=expected_uid)){
        fprintf(stderr,"target PID %d no longer matches package/UID\n",pid);return 1;
    }
    printf("pid %d uid %u\n",pid,selected.uid);

    uintptr_t r_dlopen = remote_sym(pid, (void*)dlopen);
    uintptr_t r_dlerr  = remote_sym(pid, (void*)dlerror);
    uintptr_t lr = mod_base(pid, "libil2cpp.so", false);
    if (!lr) lr = mod_base(pid, "libunity.so", false);
    if (!r_dlopen || !lr) { fprintf(stderr, "resolve fail dlopen=%lx lr=%lx\n", (unsigned long)r_dlopen, (unsigned long)lr); return 1; }
    printf("remote dlopen=%lx lr=%lx\n", (unsigned long)r_dlopen, (unsigned long)lr);

    if (ptrace(PTRACE_ATTACH, pid, 0, 0) != 0) { perror("ATTACH"); return 1; }
    int st; waitpid(pid, &st, __WALL);
    if (!WIFSTOPPED(st)) { fprintf(stderr, "attach: not stopped\n"); return 1; }
    mango_target::Identity attached;
    if(!mango_target::read(pid,pkg,attached)||!mango_target::same(selected,attached)){
        fprintf(stderr,"target identity changed during attach\n");ptrace(PTRACE_DETACH,pid,0,0);return 1;
    }

    uint64_t h = 0;
    bool ok = remote_call(pid, r_dlopen, 0, 2 /*RTLD_NOW*/, lr, so, strlen(so) + 1, &h);
    printf("dlopen -> %s handle=%llx\n", ok ? "ok" : "FAILED", (unsigned long long)h);
    if (ok && !h && r_dlerr) {
        uint64_t p = 0;
        if (remote_call(pid, r_dlerr, 0, 0, lr, nullptr, 0, &p) && p) {
            char buf[256] = {0};
            if (rd_mem(pid, p, buf, sizeof buf - 1)) printf("dlerror: %s\n", buf);
        }
    }
    ptrace(PTRACE_DETACH, pid, 0, 0);
    return (ok && h) ? 0 : 2;
}
