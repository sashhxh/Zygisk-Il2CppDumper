#pragma once
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
namespace mango_avatar {
// Unity supplies the actual application cache directory. The worker publishes
// a complete file atomically; the Unity thread only queues the request.
class Storage {
    std::mutex mutex_;std::string directory_;std::u16string published_;
public:
    std::atomic<int> state{0}; // 0 idle, 1 queued, 2 writing, 3 ready, -errno.
    bool request(const std::string& directory){
        if(directory.empty()||directory[0]!='/'||directory.size()>400||directory.find('\0')!=std::string::npos)return false;
        std::lock_guard<std::mutex> lock(mutex_);if(state.load()>0)return false;
        directory_=directory;state.store(1,std::memory_order_release);return true;
    }
    void flush(const unsigned char* data,size_t size){
        if(state.load(std::memory_order_acquire)!=1)return;
        std::string directory;{std::lock_guard<std::mutex> lock(mutex_);if(state.load()!=1)return;directory=directory_;state.store(2);}
        auto fail=[&](int error){state.store(-error,std::memory_order_release);};
        if(mkdir(directory.c_str(),0700)&&errno!=EEXIST){fail(errno);return;}
        const std::string path=directory+"/cfesp-avatar-r19.bundle",temporary=path+".new";
        const int fd=open(temporary.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);
        if(fd<0){fail(errno);return;}
        size_t at=0;int error=0;
        while(at<size){ssize_t n=write(fd,data+at,size-at);if(n<0&&errno==EINTR)continue;if(n<=0){error=n<0?errno:EIO;break;}at+=size_t(n);}
        if(close(fd)&&!error)error=errno;
        if(!error&&rename(temporary.c_str(),path.c_str()))error=errno;
        if(error){unlink(temporary.c_str());fail(error);return;}
        published_.clear();for(unsigned char c:path)published_.push_back(char16_t(c));
        state.store(3,std::memory_order_release);
    }
    const std::u16string& path()const{return published_;} // Read only after state==3.
};
}
