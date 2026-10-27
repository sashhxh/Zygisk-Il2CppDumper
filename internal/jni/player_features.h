#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include "WeaponMotion.h"

// Passive player options ported from the supplied reference.
// Runs on Unity's callback thread and uses the reference object's field layouts.
namespace mango_player {
class Controller {
    struct Patch {
        uintptr_t object=0,klass=0,offset=0;
        size_t size=0;
        std::array<unsigned char,28> original{};
        bool touched=false;
    };
    std::vector<Patch> patches;
    WeaponMotion::Controller motion;
    int read_fd=-1,write_fd=-1;
    uintptr_t owner=0,weapon_owner=0;

    bool read(uintptr_t at,void* out,size_t size)const{
        return at&&read_fd>=0&&size<=UINTPTR_MAX-at&&
            pread64(read_fd,out,size,off64_t(at))==ssize_t(size);
    }
    template<class T>bool get(uintptr_t at,T& out)const{return read(at,&out,sizeof(out));}
    uintptr_t pointer(uintptr_t at)const{uintptr_t out=0;get(at,out);return out;}
    bool isA(uintptr_t object,const char* name)const{
        for(uintptr_t klass=pointer(object),depth=0;klass&&depth<32;++depth){
            const uintptr_t text=pointer(klass+0x10);
            char found[96]{};bool complete=false;
            for(size_t i=0;text&&i<sizeof(found)-1;++i){
                if(!read(text+i,&found[i],1))break;
                if(!found[i]){complete=true;break;}
            }
            if(complete&&std::strcmp(found,name)==0)return true;
            const uintptr_t parent=pointer(klass+0x58);
            if(parent==klass)break;
            klass=parent;
        }
        return false;
    }
    bool ensure_open(){
        if(read_fd>=0&&write_fd>=0)return true;
        if(read_fd>=0)close(read_fd);
        if(write_fd>=0)close(write_fd);
        read_fd=open("/proc/self/mem",O_RDONLY|O_CLOEXEC);
        write_fd=open("/proc/self/mem",O_RDWR|O_CLOEXEC);
        return read_fd>=0&&write_fd>=0;
    }
    void restore_one(const Patch& patch){
        if(pointer(patch.object)!=patch.klass)return;
        std::array<unsigned char,28> current{};
        if(!read(patch.object+patch.offset,current.data(),patch.size))return;
        for(size_t i=0;i<patch.size;++i)if(current[i])return;
        pwrite64(write_fd,patch.original.data(),patch.size,off64_t(patch.object+patch.offset));
    }
    void restore_parameters(){for(const auto& patch:patches)restore_one(patch);patches.clear();}
    void zero(uintptr_t object,uintptr_t offset,size_t size,bool backup=true){
        if(!object||!size||size>28||size%4||offset>UINTPTR_MAX-object||write_fd<0)return;
        const uintptr_t klass=pointer(object);if(!klass)return;
        std::array<unsigned char,28> current{};
        if(!read(object+offset,current.data(),size))return;
        for(size_t i=0;i<size;i+=4){
            float value=0;std::memcpy(&value,current.data()+i,4);
            if(!std::isfinite(value))return;
        }
        auto it=patches.end();
        if(backup){
            it=std::find_if(patches.begin(),patches.end(),[&](const Patch& p){
                return p.object==object&&p.offset==offset;
            });
            if(it!=patches.end()&&(it->klass!=klass||it->size!=size)){patches.erase(it);it=patches.end();}
            if(it==patches.end()){
                Patch patch;patch.object=object;patch.klass=klass;patch.offset=offset;
                patch.size=size;patch.original=current;patches.push_back(patch);it=patches.end()-1;
            }
            it->touched=true;
        }
        const std::array<unsigned char,28> zeros{};
        const ssize_t n=pwrite64(write_fd,zeros.data(),size,off64_t(object+offset));
        if(n>0&&n!=ssize_t(size))pwrite64(write_fd,current.data(),size,off64_t(object+offset));
    }
    void no_recoil(uintptr_t player,uintptr_t weapon){
        // The current event is transient; do not replay an old recoil impulse on disable.
        const uintptr_t events=pointer(player+0x78),event=pointer(events?events+0x138:0);
        if(event)zero(event,0x20,16,false);
        if(!isA(weapon,"FPHitscan"))return;
        const uintptr_t config=pointer(weapon+0x130);
        if(!isA(config,"HitscanWeaponConfig"))return;
        zero(config,0x3C,4);
        const uintptr_t recoil=pointer(config+0x70),dispersion=pointer(config+0x68);
        if(isA(recoil,"RecoilConfig"))zero(recoil,0x18,28);
        if(isA(dispersion,"DispersionConfig"))zero(dispersion,0x18,20);
        const uintptr_t runtime_recoil=pointer(weapon+0x200);
        if(isA(runtime_recoil,"RecoilConfig"))zero(runtime_recoil,0x18,28);
        // _lastHorizontal (float) and _lastDirection (int).
        zero(weapon,0x210,4,false);
        const uint32_t direction=0;pwrite64(write_fd,&direction,4,off64_t(weapon+0x214));
    }
    void no_spread(uintptr_t weapon){
        uintptr_t offset=0;
        if(isA(weapon,"FPHitscan"))offset=0x130;
        else if(isA(weapon,"FPRocketLauncher"))offset=0x128;
        else if(isA(weapon,"FPCrossbow"))offset=0x138;
        if(!offset)return;
        const uintptr_t config=pointer(weapon+offset);float speed=0;
        if(!config||!get(config+0x30,speed)||!std::isfinite(speed)||
           (speed!=0&&(speed<1||speed>5000)))return;
        zero(config,0x3C,4);
        // Preserve the reference option's ballistic and dispersion behavior.
        const uintptr_t ballistic=pointer(config+0x60),dispersion=pointer(config+0x68);
        if(isA(ballistic,"BallisticProfile"))zero(ballistic,0x10,12);
        if(isA(dispersion,"DispersionConfig"))zero(dispersion,0x18,20);
    }
public:
    Controller()=default;
    Controller(const Controller&)=delete;
    Controller& operator=(const Controller&)=delete;
    ~Controller(){reset();}
    void reset(){
        if(read_fd>=0&&write_fd>=0){
            restore_parameters();motion.restore(read_fd,write_fd,getpid());
        }else patches.clear();
        if(read_fd>=0)close(read_fd);
        if(write_fd>=0)close(write_fd);
        read_fd=write_fd=-1;owner=weapon_owner=0;
    }
    void update(uintptr_t player,bool recoil,bool spread,bool weapon_motion){
        if(!player||(!recoil&&!spread&&!weapon_motion)){reset();return;}
        if(!ensure_open())return;
        const uintptr_t manager=pointer(player+0x90);
        const uintptr_t weapon=isA(manager,"FPManager")?pointer(manager+0x50):0;
        if(!isA(weapon,"FPObject")||pointer(weapon+0xC0)!=player||!pointer(weapon+0x10)){
            restore_parameters();motion.restore(read_fd,write_fd,getpid());owner=weapon_owner=0;return;
        }
        if(owner&&(owner!=player||weapon_owner!=weapon)){
            restore_parameters();motion.restore(read_fd,write_fd,getpid());
        }
        owner=player;weapon_owner=weapon;
        for(auto& patch:patches)patch.touched=false;
        if(recoil)no_recoil(player,weapon);
        if(spread)no_spread(weapon);
        for(auto it=patches.begin();it!=patches.end();){
            if(!it->touched){restore_one(*it);it=patches.erase(it);}else ++it;
        }
        if(weapon_motion)motion.update(read_fd,write_fd,getpid(),player);
        else if(motion.hasPending())motion.restore(read_fd,write_fd,getpid());
    }
};
} // namespace mango_player
