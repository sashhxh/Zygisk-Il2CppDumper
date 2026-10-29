#pragma once
// Resolve APIs from already loaded ELF images, including app linker namespaces.
// No library is injected, unpacked, or loaded by this reader.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <elf.h>
#include <link.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <string>
#include <vector>

namespace mango_runtime {
struct Range {
    uintptr_t start=0,end=0;
    bool executable=false;
};
class Reader {
    std::vector<Range> readable_;
public:
    struct Mapping {uintptr_t start;uint64_t offset;std::string path;};
    std::vector<Mapping> starts;
    bool refresh(){
        readable_.clear();starts.clear();
        FILE* f=std::fopen("/proc/self/maps","r");if(!f)return false;
        char line[2048];
        while(std::fgets(line,sizeof(line),f)){
            unsigned long long start=0,end=0,offset=0;char perms[5]{},path[1536]{};
            int n=std::sscanf(line,"%llx-%llx %4s %llx %*s %*s %1535[^\n]",&start,&end,perms,&offset,path);
            if(n<4||perms[0]!='r'||end<=start)continue;
            readable_.push_back({uintptr_t(start),uintptr_t(end),perms[2]=='x'});
            // Kernel vvar pages can SIGBUS on read despite an 'r' maps flag.
            const bool kernel_page=std::strstr(path,"[vvar")||std::strstr(path,"[vsyscall");
            if(!kernel_page&&offset==0&&size_t(end-start)>=sizeof(Elf64_Ehdr))
                starts.push_back({uintptr_t(start),offset,n>=5?path:""});
        }
        std::fclose(f);
        std::sort(readable_.begin(),readable_.end(),[](const Range&a,const Range&b){return a.start<b.start;});
        return !readable_.empty();
    }
    bool contains(uintptr_t at,size_t size,bool executable=false)const{
        if(!size)return true;
        if(size>UINTPTR_MAX-at)return false;
        uintptr_t end=at+size;
        for(const auto& r:readable_){
            if(r.end<=at)continue;
            if(r.start>at|| (executable&&!r.executable))return false;
            if(r.end>=end)return true;
            at=r.end;
        }
        return false;
    }
    bool read(uintptr_t at,void* dst,size_t size)const{
        if(!contains(at,size))return false;
        if(!size)return true;
        // /proc/maps is a snapshot. A loader may unmap a region after contains();
        // kernel reads return EFAULT/EIO instead of faulting this application.
#if defined(SYS_process_vm_readv)
        iovec local{dst,size},remote{reinterpret_cast<void*>(at),size};
        const long n=syscall(SYS_process_vm_readv,getpid(),&local,1,&remote,1,0);
        if(n==static_cast<long>(size))return true;
#endif
        const int fd=open("/proc/self/mem",O_RDONLY|O_CLOEXEC);
        if(fd<0)return false;
        const ssize_t count=pread(fd,dst,size,static_cast<off_t>(at));
        close(fd);
        return count==static_cast<ssize_t>(size);
    }
    template<class T>bool read(uintptr_t at,T& out)const{return read(at,&out,sizeof(out));}
};

struct Module {
    uintptr_t bias=0,dynamic=0;
    size_t dynamic_size=0;
    std::string path;
    std::vector<Range> loads;
    bool owns(uintptr_t at,size_t size=1,bool executable=false)const{
        if(size>UINTPTR_MAX-at)return false;
        for(const auto& r:loads)if(at>=r.start&&at<r.end&&size<=r.end-at&&(!executable||r.executable))return true;
        return false;
    }
    uintptr_t pointer(uintptr_t value,const Reader& r)const{
        if(owns(value)&&r.contains(value,1))return value; // glibc relocated d_ptr
        if(value<=UINTPTR_MAX-bias&&owns(bias+value)&&r.contains(bias+value,1))return bias+value;
        return 0;
    }
};

class Resolver {
    Reader reader_;
    std::vector<Module> modules_;
    Module chosen_;
    bool ready_=false;
    static constexpr uint32_t limit=1u<<20;
    struct Tables {uintptr_t symbols=0,strings=0,gnu=0,sysv=0;size_t string_size=0;};
    static uint32_t gnu_hash(const char* s){uint32_t h=5381;while(*s)h=h*33+static_cast<unsigned char>(*s++);return h;}
    static uint32_t sysv_hash(const char* s){
        uint32_t h=0;while(*s){h=(h<<4)+static_cast<unsigned char>(*s++);uint32_t g=h&0xf0000000u;if(g)h^=g>>24;h&=~g;}return h;
    }
    bool tables(const Module& mod,Tables& t)const{
        if(!mod.dynamic||mod.dynamic_size<sizeof(Elf64_Dyn)||mod.dynamic_size>(4u<<20))return false;
        bool terminated=false;
        for(size_t i=0;i<mod.dynamic_size/sizeof(Elf64_Dyn);++i){
            Elf64_Dyn d{};if(!reader_.read(mod.dynamic+i*sizeof(d),d))return false;
            if(d.d_tag==DT_NULL){terminated=true;break;}
            switch(d.d_tag){
                case DT_SYMTAB:t.symbols=mod.pointer(d.d_un.d_ptr,reader_);break;
                case DT_STRTAB:t.strings=mod.pointer(d.d_un.d_ptr,reader_);break;
                case DT_STRSZ:t.string_size=d.d_un.d_val;break;
                case DT_GNU_HASH:t.gnu=mod.pointer(d.d_un.d_ptr,reader_);break;
                case DT_HASH:t.sysv=mod.pointer(d.d_un.d_ptr,reader_);break;
                case DT_SYMENT:if(d.d_un.d_val!=sizeof(Elf64_Sym))return false;break;
            }
        }
        return terminated&&t.symbols&&t.strings&&t.string_size&&t.string_size<=(64u<<20)&&
               mod.owns(t.strings,t.string_size)&&reader_.contains(t.strings,t.string_size);
    }
    uintptr_t symbol(const Module& mod,const Tables& t,uint32_t index,const char* name)const{
        if(index>=limit)return 0;
        const uintptr_t at=t.symbols+uintptr_t(index)*sizeof(Elf64_Sym);
        Elf64_Sym s{};if(!mod.owns(at,sizeof(s))||!reader_.read(at,s))return 0;
        const auto type=ELF64_ST_TYPE(s.st_info),bind=ELF64_ST_BIND(s.st_info);
        const size_t len=std::strlen(name);
        if(!s.st_shndx||!s.st_value||(type!=STT_FUNC&&type!=STT_NOTYPE)||
           (bind!=STB_GLOBAL&&bind!=STB_WEAK)||s.st_name>=t.string_size||
           len>=t.string_size-s.st_name)return 0;
        char actual_name[256]{};
        if(len>=sizeof(actual_name)||!reader_.read(t.strings+s.st_name,actual_name,len+1)||
           std::memcmp(actual_name,name,len+1))return 0;
        if(s.st_value>UINTPTR_MAX-mod.bias)return 0;
        uintptr_t result=mod.bias+s.st_value;
        return mod.owns(result,4,true)&&reader_.contains(result,4,true)?result:0;
    }
    uintptr_t lookup(const Module& mod,const char* name)const{
        Tables t{};if(!tables(mod,t))return 0;
        if(t.gnu){
            uint32_t header[4]{};
            if(reader_.read(t.gnu,header,sizeof(header))&&header[0]&&header[0]<=limit&&
               header[1]<limit&&header[2]&&header[2]<=limit){
                uintptr_t buckets=t.gnu+16+uintptr_t(header[2])*8;
                uintptr_t chains=buckets+uintptr_t(header[0])*4;
                uint32_t hash=gnu_hash(name),index=0;
                if(mod.owns(buckets,size_t(header[0])*4)&&
                   reader_.read(buckets+uintptr_t(hash%header[0])*4,index)&&index>=header[1]){
                    for(uint32_t steps=0;index<limit&&steps<limit;++index,++steps){
                        uint32_t chain=0;uintptr_t at=chains+uintptr_t(index-header[1])*4;
                        if(!mod.owns(at,4)||!reader_.read(at,chain))break;
                        if((chain|1u)==(hash|1u)){uintptr_t p=symbol(mod,t,index,name);if(p)return p;}
                        if(chain&1u)break;
                    }
                }
            }
        }
        if(t.sysv){
            uint32_t header[2]{};
            if(reader_.read(t.sysv,header,sizeof(header))&&header[0]&&header[0]<=limit&&header[1]<=limit){
                uintptr_t buckets=t.sysv+8,chains=buckets+uintptr_t(header[0])*4;
                uint32_t index=0;
                if(mod.owns(buckets,size_t(header[0])*4)&&reader_.read(buckets+(sysv_hash(name)%header[0])*4,index))
                    for(uint32_t steps=0;index&&index<header[1]&&steps<header[1];++steps){
                        uintptr_t p=symbol(mod,t,index,name);if(p)return p;
                        if(!mod.owns(chains+uintptr_t(index)*4,4)||!reader_.read(chains+uintptr_t(index)*4,index))break;
                    }
            }
        }
        return 0;
    }
    static int registered(dl_phdr_info* info,size_t,void* context){
        auto& out=*static_cast<std::vector<Module>*>(context);
        Module m;m.bias=info->dlpi_addr;m.path=info->dlpi_name?info->dlpi_name:"";
        for(ElfW(Half) i=0;i<info->dlpi_phnum;++i){
            const auto& p=info->dlpi_phdr[i];
            if(p.p_vaddr>UINTPTR_MAX-m.bias||p.p_memsz>UINTPTR_MAX-m.bias-p.p_vaddr)continue;
            if(p.p_type==PT_LOAD)m.loads.push_back({m.bias+p.p_vaddr,m.bias+p.p_vaddr+p.p_memsz,bool(p.p_flags&PF_X)});
            if(p.p_type==PT_DYNAMIC){m.dynamic=m.bias+p.p_vaddr;m.dynamic_size=p.p_memsz;}
        }
        if(m.dynamic&&!m.loads.empty())out.push_back(m);
        return 0;
    }
    bool mapped(const Reader::Mapping& map,Module& m)const{
        Elf64_Ehdr header{};
        if(!reader_.read(map.start,header)||std::memcmp(header.e_ident,ELFMAG,SELFMAG)||
           header.e_ident[EI_CLASS]!=ELFCLASS64||header.e_ident[EI_DATA]!=ELFDATA2LSB||
           header.e_type!=ET_DYN||!header.e_phnum||header.e_phnum>256||
           header.e_phentsize!=sizeof(Elf64_Phdr)||header.e_phoff>(1u<<20))return false;
        std::vector<Elf64_Phdr> ph(header.e_phnum);
        if(!reader_.read(map.start+header.e_phoff,ph.data(),ph.size()*sizeof(ph[0])))return false;
        bool base=false;
        for(const auto& p:ph)if(p.p_type==PT_LOAD&&p.p_offset==0&&p.p_filesz>=sizeof(header)&&map.start>=p.p_vaddr){
            m.bias=map.start-p.p_vaddr;base=true;break;
        }
        if(!base)return false;
        m.path=map.path;
        for(const auto& p:ph){
            if(p.p_vaddr>UINTPTR_MAX-m.bias||p.p_memsz>UINTPTR_MAX-m.bias-p.p_vaddr)return false;
            if(p.p_type==PT_LOAD)m.loads.push_back({m.bias+p.p_vaddr,m.bias+p.p_vaddr+p.p_memsz,bool(p.p_flags&PF_X)});
            if(p.p_type==PT_DYNAMIC){m.dynamic=m.bias+p.p_vaddr;m.dynamic_size=p.p_memsz;}
        }
        return m.dynamic&&!m.loads.empty();
    }
public:
    bool initialize(){
        if(ready_)return true;
        modules_.clear();if(!reader_.refresh())return false;
        dl_iterate_phdr(registered,&modules_);
        for(const auto& mapping:reader_.starts){
            Module m;if(!mapped(mapping,m))continue;
            auto same=std::find_if(modules_.begin(),modules_.end(),[&](const Module& old){return old.bias==m.bias;});
            if(same==modules_.end())modules_.push_back(m);
        }
        std::stable_sort(modules_.begin(),modules_.end(),[](const Module& a,const Module& b){
            return (a.path.find("libil2cpp")!=std::string::npos)>(b.path.find("libil2cpp")!=std::string::npos);
        });
        for(const auto& m:modules_){
            // Require a runtime API set, not a single coincidental symbol.
            if(lookup(m,"il2cpp_domain_get")&&lookup(m,"il2cpp_domain_get_assemblies")&&
               lookup(m,"il2cpp_runtime_invoke")){
                chosen_=m;ready_=true;return true;
            }
        }
        return false;
    }
    struct Signature {uintptr_t rva;const unsigned char* bytes;size_t size;};
    bool select_named_image(const char* name,const unsigned char* build_id,size_t id_size,
                            const Signature* signatures,size_t count){
        if(ready_)return true;
        if(!name||!build_id||!id_size||id_size>64||!signatures||!count||!reader_.refresh())return false;
        std::vector<Module> registered_modules;
        dl_iterate_phdr(registered,&registered_modules);
        auto named=[name](const std::string& path){
            const char* leaf=std::strrchr(path.c_str(),'/');
            return !std::strcmp(leaf?leaf+1:path.c_str(),name);
        };
        modules_.clear();
        for(const auto& m:registered_modules)if(named(m.path))modules_.push_back(m);
        for(const auto& mapping:reader_.starts){
            if(!named(mapping.path))continue;
            Module m;if(!mapped(mapping,m))continue;
            if(std::none_of(modules_.begin(),modules_.end(),[&](const Module& old){return old.bias==m.bias;}))modules_.push_back(m);
        }
        for(const auto& m:modules_){
            Elf64_Ehdr header{};
            if(!reader_.read(m.bias,header)||header.e_phnum>256||!header.e_phnum||
               header.e_phentsize!=sizeof(Elf64_Phdr)||header.e_phoff>(1u<<20))continue;
            bool id_matches=false;
            for(size_t i=0;i<header.e_phnum&&!id_matches;++i){
                Elf64_Phdr ph{};
                if(!reader_.read(m.bias+header.e_phoff+i*sizeof(ph),ph))break;
                if(ph.p_type!=PT_NOTE||ph.p_filesz>65536||ph.p_filesz<sizeof(Elf64_Nhdr)||
                   ph.p_vaddr>UINTPTR_MAX-m.bias||!m.owns(m.bias+ph.p_vaddr,ph.p_filesz))continue;
                std::vector<unsigned char> notes(ph.p_filesz);
                if(!reader_.read(m.bias+ph.p_vaddr,notes.data(),notes.size()))continue;
                for(size_t at=0;at+sizeof(Elf64_Nhdr)<=notes.size();){
                    Elf64_Nhdr note{};std::memcpy(&note,notes.data()+at,sizeof(note));at+=sizeof(note);
                    const size_t names=(size_t(note.n_namesz)+3)&~size_t(3);
                    const size_t desc=(size_t(note.n_descsz)+3)&~size_t(3);
                    if(names>notes.size()-at||desc>notes.size()-at-names)break;
                    if(note.n_type==NT_GNU_BUILD_ID&&note.n_namesz==4&&note.n_descsz==id_size&&
                       !std::memcmp(notes.data()+at,"GNU",4)&&
                       !std::memcmp(notes.data()+at+names,build_id,id_size)){id_matches=true;break;}
                    at+=names+desc;
                }
            }
            if(!id_matches)continue;
            bool matches=true;
            for(size_t i=0;i<count;++i){
                const auto& s=signatures[i];unsigned char actual[64]{};
                if(!s.bytes||!s.size||s.size>sizeof(actual)||s.rva>UINTPTR_MAX-m.bias||
                   !m.owns(m.bias+s.rva,s.size,true)||!reader_.contains(m.bias+s.rva,s.size,true)||
                   !reader_.read(m.bias+s.rva,actual,s.size)||std::memcmp(actual,s.bytes,s.size)){
                    matches=false;break;
                }
            }
            if(matches){chosen_=m;ready_=true;return true;}
        }
        return false;
    }
    // A known image can be selected without IL2CPP's public symbol table.
    // Every code signature must match before a caller uses version-specific RVAs.
    bool select_by_signatures(const Signature* signatures,size_t count){
        if(ready_)return true;
        if(!signatures||!count||modules_.empty())return false;
        for(const auto& m:modules_){
            if(m.path.find("libil2cpp.so")==std::string::npos)continue;
            bool match=true;
            for(size_t i=0;i<count;++i){
                const auto& s=signatures[i];
                if(!s.bytes||!s.size||s.size>64||s.rva>UINTPTR_MAX-m.bias||
                   !m.owns(m.bias+s.rva,s.size,true)||!reader_.contains(m.bias+s.rva,s.size,true)){
                    match=false;break;
                }
                unsigned char actual[64]{};
                if(!reader_.read(m.bias+s.rva,actual,s.size)||std::memcmp(actual,s.bytes,s.size)){
                    match=false;break;
                }
            }
            if(match){chosen_=m;ready_=true;return true;}
        }
        return false;
    }
    void* get(const char* name)const{return ready_?reinterpret_cast<void*>(lookup(chosen_,name)):nullptr;}
    uintptr_t base()const{return ready_?chosen_.bias:0;}
    const char* path()const{return ready_?chosen_.path.c_str():"";}
    const Module& module()const{return chosen_;}
    bool executable(uintptr_t at,size_t size=4)const{return ready_&&reader_.contains(at,size,true);}
};
} // namespace mango_runtime
