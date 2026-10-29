#include "avatar_storage.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
int main(){
 mango_avatar::Storage storage;
 assert(!storage.request("relative")&&!storage.request(std::string("/tmp/a\0b",8)));
 unsigned char data[]={1,2,3,4,5};char pattern[]="/tmp/avatar-cache-XXXXXX";char* directory=mkdtemp(pattern);assert(directory);
 assert(storage.request(directory)&&storage.state.load()==1&&!storage.request(directory));
 storage.flush(data,sizeof(data));assert(storage.state.load()==3);
 const std::string path(storage.path().begin(),storage.path().end());
 std::ifstream f(path,std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),{});assert(bytes==std::vector<unsigned char>(data,data+sizeof(data)));
 assert(access((path+".new").c_str(),F_OK)!=0);storage.flush(data,0); // ready file is immutable
 mango_avatar::Storage failure;assert(failure.request(path+"/child"));failure.flush(data,sizeof(data));assert(failure.state.load()<0);
 assert(failure.request(directory));failure.flush(data,sizeof(data));assert(failure.state.load()==3);
 unlink(path.c_str());rmdir(directory);
}
