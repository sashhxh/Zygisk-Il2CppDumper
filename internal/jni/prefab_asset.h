#pragma once
#include "chams.h"

namespace mango_graphics {
// Async injected AssetBundle methods return native AsyncOperation pointers;
// their C# wrappers construct an AsyncOperation only after the native call.
// Hold/release these native references directly and never reinterpret them as
// UnityEngine.Object handles or managed request objects.
class PrefabAsset {
    void*(*load_file_)(mango_chams::Span*,uint32_t,uint64_t)=nullptr;
    void*(*load_asset_)(void*,mango_chams::Span*,void*)=nullptr;
    bool(*done_)(void*)=nullptr;
    void*(*bundle_)(void*)=nullptr;void*(*result_)(void*)=nullptr;
    void(*release_)(void*)=nullptr;void(*unload_)(void*,bool)=nullptr;
    void* request_=nullptr,*native_bundle_=nullptr,*native_prefab_=nullptr;
    void* probe_request_=nullptr;double probe_started_=0;
    int32_t prefab_id_=0;
    double started_=0,last_poll_=-1,next_retry_=0;
    bool bound_=false;unsigned retries_=0;
    void finish(mango_chams::Api& api){
        if(request_){release_(request_);request_=nullptr;}
        if(native_bundle_){unload_(native_bundle_,false);native_bundle_=nullptr;}
    }
public:
    int status=0; // 1/2 loading bundle/prefab, 3 ready, negative failure stage.
    unsigned attempts()const{return retries_;}
    void retry(double time,bool force=false){
        if(force){retries_=0;next_retry_=0;}
        if(status>=0)return;
        if(time<next_retry_||retries_>=3)return;
        next_retry_=time+5;
        ++retries_;status=0;bound_=false;last_poll_=-1;started_=0;
    }
    bool bind(mango_chams::Api::Resolve resolve){
        if(bound_)return true;
#define SH_BIND(member,key) member=reinterpret_cast<decltype(member)>(resolve(key));if(!member){status=-1;return false;}
        SH_BIND(load_file_,"UnityEngine.AssetBundle::LoadFromFileAsync_Internal_Injected(UnityEngine.Bindings.ManagedSpanWrapper&,System.UInt32,System.UInt64)");
        SH_BIND(load_asset_,"UnityEngine.AssetBundle::LoadAssetAsync_Internal_Injected(System.IntPtr,UnityEngine.Bindings.ManagedSpanWrapper&,System.Type)");
        SH_BIND(done_,"UnityEngine.AsyncOperation::get_isDone_Injected(System.IntPtr)");
        SH_BIND(bundle_,"UnityEngine.AssetBundleCreateRequest::get_assetBundle_Injected(System.IntPtr)");
        SH_BIND(result_,"UnityEngine.AssetBundleRequest::GetResult_Injected(System.IntPtr)");
        SH_BIND(release_,"UnityEngine.AsyncOperation::InternalDestroy(System.IntPtr)");
        SH_BIND(unload_,"UnityEngine.AssetBundle::Unload_Injected(System.IntPtr,System.Boolean)");
#undef SH_BIND
        bound_=true;return true;
    }
    void update(mango_chams::Api& api,mango_chams::Span path,void* prefab_type,double time){
        if(!bound_||status<0||status==3||time-last_poll_<.1)return;
        last_poll_=time;
        if(!status){
            if(!path.length||!prefab_type)return;
            request_=load_file_(&path,0,0);started_=time;
            status=request_?1:-2;return;
        }
        if(!request_){status=-3;finish(api);return;}
        if(!done_(request_)){
            if(time-started_>30){status=-4;finish(api);}return;
        }
        if(status==1&&!prefab_type){
            if(time-started_>30){status=-4;finish(api);}return;
        }
        if(status==1){
            native_bundle_=api.native_return(bundle_(request_));
            if(!native_bundle_){status=-5;finish(api);return;}
            api.set_flags(native_bundle_,32);
            auto name=mango_chams::span(u"cfesp/avatar");
            void* next=load_asset_(native_bundle_,&name,prefab_type);
            release_(request_);request_=next;status=next?2:-6;started_=time;
            if(!next)finish(api);return;
        }
        void* prefab=api.decode(result_(request_));
        native_prefab_=api.native(prefab);prefab_id_=api.id(prefab);
        if(native_prefab_&&prefab_id_){api.set_flags(native_prefab_,32);if(api.retain)api.retain(prefab_id_,reinterpret_cast<uintptr_t>(native_prefab_));status=3;}
        else status=-7;
        finish(api);
    }
    // Diagnostic: load a bundle file only to see whether Unity accepts it.
    int probe(mango_chams::Api& api,mango_chams::Span path,double time,bool begin){
        if(!bound_)return -9;
        if(begin){probe_request_=load_file_(&path,0,0);probe_started_=time;return probe_request_?0:-2;}
        if(!probe_request_)return -3;
        if(!done_(probe_request_)){
            if(time-probe_started_>10){release_(probe_request_);probe_request_=nullptr;return -4;}
            return 0;
        }
        void* bundle=api.native_return(bundle_(probe_request_));
        release_(probe_request_);probe_request_=nullptr;
        if(!bundle)return -5;
        unload_(bundle,true);return 1;
    }
    int32_t id()const{return status==3?prefab_id_:0;}
};
}
