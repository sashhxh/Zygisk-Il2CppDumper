#pragma once

// Procedural first-person weapon motion. Field layouts: Abdump/dump.cs.
// External reads/writes only; no game function calls or animator disabling.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

namespace WeaponMotion {
class Controller {
    struct Patch {
        uint64_t object=0, klass=0, offset=0;
        size_t size=0;
        std::array<unsigned char,12> original{};
        bool touched=false;
    };
    std::vector<Patch> patches;
    pid_t session=0;
    uint64_t owner=0, ownerClass=0, localPlayer=0;
    int readFd=-1, writeFd=-1;

    bool read(uint64_t address, void* out, size_t size) const {
        return address && readFd>=0 && pread64(readFd,out,size,off64_t(address))==ssize_t(size);
    }
    uint64_t pointer(uint64_t address) const {
        uint64_t result=0; read(address,&result,sizeof(result)); return result;
    }
    bool isA(uint64_t object, const char* name) const {
        uint64_t klass=pointer(object);
        for (int depth=0; klass && depth<32; ++depth) {
            char text[96]{};
            if (!read(pointer(klass+0x10),text,sizeof(text)-1)) return false;
            if (std::strcmp(text,name)==0) return true;
            const uint64_t parent=pointer(klass+0x58);
            if (parent==klass) break;
            klass=parent;
        }
        return false;
    }
    void restoreOne(const Patch& patch) {
        if (writeFd<0 || pointer(patch.object)!=patch.klass) return;
        std::array<unsigned char,12> current{};
        if (!read(patch.object+patch.offset,current.data(),patch.size)) return;
        // Restore only our zeros; preserve changes made elsewhere after the write.
        for (size_t i=0; i<patch.size; ++i) if (current[i]!=0) return;
        pwrite64(writeFd,patch.original.data(),patch.size,off64_t(patch.object+patch.offset));
    }
    void zero(uint64_t object, uint64_t offset, size_t size) {
        if (!object || !size || size>12 || writeFd<0) return;
        const uint64_t klass=pointer(object);
        if (!klass) return;
        auto found=std::find_if(patches.begin(),patches.end(),[&](const Patch& p) {
            return p.object==object && p.offset==offset;
        });
        if (found!=patches.end() && found->klass!=klass) {
            patches.erase(found); found=patches.end();
        }
        if (found==patches.end()) {
            Patch patch; patch.object=object; patch.klass=klass; patch.offset=offset; patch.size=size;
            if (!read(object+offset,patch.original.data(),size)) return;
            if (size==1) {
                if (patch.original[0]>1) return;
            } else {
                for (size_t i=0; i<size; i+=4) {
                    float value=0; std::memcpy(&value,patch.original.data()+i,4);
                    if (!std::isfinite(value)) return;
                }
            }
            patches.push_back(patch); found=patches.end()-1;
        }
        const std::array<unsigned char,12> zeros{};
        const ssize_t written=pwrite64(writeFd,zeros.data(),size,off64_t(object+offset));
        if (written!=ssize_t(size)) {
            if (written>0) pwrite64(writeFd,found->original.data(),size,off64_t(object+offset));
            return;
        }
        found->touched=true;
    }
    void muteSway(uint64_t sway) {
        if (!isA(sway,"Sway")) return;
        zero(sway,0x18,1); // Enabled
        zero(sway,0x1C,8); // Magnitude (Vector2)
    }
    void muteBob(uint64_t bob) {
        if (!isA(bob,"TrigonometricBob")) return;
        zero(bob,0x10,1); // m_Enabled
        zero(bob,0x1C,4); zero(bob,0x20,4); // m_AmountX/Y
    }
    void muteShake(uint64_t shake) {
        if (!isA(shake,"GenericShake")) return;
        zero(shake,0x10,4); // m_Magnitude
        zero(shake,0x20,12); zero(shake,0x2C,12); zero(shake,0x38,12); // position/rotation/FOV influence
    }
    void muteSpring(uint64_t profile) {
        if (!isA(profile,"WeaponHandsSpringProfile")) return;
        zero(profile,0x48,4); zero(profile,0x4C,8); // horizontal impulse/speed force
        zero(profile,0x58,4); zero(profile,0x5C,4); // vertical impulse/speed force
        zero(profile,0x64,4); zero(profile,0x68,12); // velocity impulse/movement rotation
        zero(profile,0x74,12); zero(profile,0x80,12); // look inertia/drag
        zero(profile,0x8C,4); zero(profile,0x90,4); // yaw roll/linear inertia
        zero(profile,0xA8,4); // aim force scale
        // Stiffness, damping, simulation rate and limits remain numerically valid.
    }
    void muteGait(uint64_t gait) {
        if (!isA(gait,"WeaponGaitProfile")) return;
        zero(gait,0x24,12); // RotationScale; keep the resting ApplicationOffset.
        for (uint64_t offset : {0x38u,0x40u,0x48u}) {
            const uint64_t state=pointer(gait+offset);
            if (!isA(state,"GaitState")) continue;
            zero(state,0x14,12); zero(state,0x20,12); zero(state,0x44,12); // force/offset/angle
            zero(state,0x68,4); zero(state,0x70,12); zero(state,0x7C,12); zero(state,0x88,4);
        }
    }
    uint64_t animatorFor(uint64_t weapon) const {
        uint64_t offset=0;
        if (isA(weapon,"FPHitscan")) offset=0x1E8;
        else if (isA(weapon,"FPBow") || isA(weapon,"FPCrossbow")) offset=0x130;
        else if (isA(weapon,"FPRocketLauncher")) offset=0x138;
        else if (isA(weapon,"FPChainsaw")) offset=0x198;
        if (!offset) return 0;
        const uint64_t animator=pointer(weapon+offset);
        return isA(animator,"FPAnimator") && pointer(animator+0x58)==weapon ? animator : 0;
    }
public:
    bool hasPending() const { return !patches.empty(); }
    void restore(int rfd, int wfd, pid_t pid) {
        readFd=rfd; writeFd=wfd;
        if (pid==session && rfd>=0 && wfd>=0)
            for (const auto& patch : patches) restoreOne(patch);
        patches.clear(); session=0; owner=ownerClass=localPlayer=0;
    }
    void update(int rfd, int wfd, pid_t pid, uint64_t player) {
        readFd=rfd; writeFd=wfd;
        // Never replay backups from a different process through the new descriptors.
        if (session && session!=pid) { patches.clear(); owner=ownerClass=localPlayer=0; }
        if (rfd<0 || wfd<0 || pid<=0 || !player) { restore(rfd,wfd,pid); return; }
        const uint64_t manager=pointer(player+0x90);
        const uint64_t weapon=isA(manager,"FPManager") ? pointer(manager+0x50) : 0;
        const uint64_t klass=pointer(weapon);
        if (!isA(weapon,"FPObject") || pointer(weapon+0xC0)!=player || !pointer(weapon+0x10)) {
            restore(rfd,wfd,pid); return;
        }
        if (owner && (owner!=weapon || ownerClass!=klass || localPlayer!=player)) restore(rfd,wfd,pid);
        session=pid; owner=weapon; ownerClass=klass; localPlayer=player;
        for (auto& patch : patches) patch.touched=false;
        const uint64_t animator=animatorFor(weapon);
        if (animator) {
            if (isA(animator,"FPHitscanAnimator")) muteShake(pointer(animator+0xA0));
            else if (isA(animator,"FPBowAnimator") || isA(animator,"FPCrossbowAnimator")) muteShake(pointer(animator+0x88));
            else if (isA(animator,"FPRocketAnimator")) muteShake(pointer(animator+0x90));
            else if (isA(animator,"FPChainsawAnimator")) muteShake(pointer(animator+0x80));
            // FPAnimator.KgB -> FPHandsIK.bMu (UW implementation).
            const uint64_t hands=pointer(animator+0x40);
            const uint64_t motion=isA(hands,"FPHandsIK") ? pointer(hands+0x40) : 0;
            const uint64_t events=pointer(player+0x78);
            if (isA(motion,"FPMotion") && pointer(motion+0xD0)==weapon && events && pointer(motion+0x120)==events) {
                muteSway(pointer(motion+0x88)); muteBob(pointer(motion+0x90));
                const uint64_t aim=pointer(motion+0x38);
                if (isA(aim,"WeaponAimMotionProfile")) {
                    zero(aim,0x20,4); muteSway(pointer(aim+0x18)); muteBob(pointer(aim+0x28));
                }
                const uint64_t body=pointer(motion+0x78);
                if (isA(body,"WeaponBodySettings")) {
                    muteSpring(pointer(body+0x10)); muteGait(pointer(body+0x20));
                }
            }
        }
        // Restore an old runtime copy/profile when its reference changes.
        for (auto it=patches.begin(); it!=patches.end();) {
            if (!it->touched) { restoreOne(*it); it=patches.erase(it); }
            else ++it;
        }
    }
};
} // namespace WeaponMotion
