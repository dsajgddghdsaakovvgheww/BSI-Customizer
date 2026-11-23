#pragma once
// Authored public-GL dispatch/state double for REAL Windows EGL/WGL detours.
// No GL driver, pixels, command-buffer memory or protected GPU memory executes.
#include "GlDrawCalls.h"
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

namespace EglDrawFixture {
using U=unsigned;using I=int;using F=float;using B=unsigned char;
struct Sampler {
    I min=0x2703,mag=0x2601,s=0x812f,t=0x812f,r=0x812f,compare=0,function=0x0203;
    F low=-1000,high=1000,bias=0;
};
struct State {
    std::array<Sampler,128> samplers{};
    std::array<U,4> bindings{{1,2,0,0}},textures{{1,2,0,0}};
    I active=3,depthFunction=0x0203,depthBits=24,framebuffer=0,program=1;
    std::array<U,4> extraOutputs{}; // optional MRT color attachments/draw slots 1..3
    bool drawBufferNone=false;
    U output=50,readOutput=70;
    I readFramebuffer=0,outputType=0x1702,readOutputType=0x1702;
    I defaultDrawBuffer=0x0405,defaultReadBuffer=0x0405;
    bool desktop=false,protectionExtension=true,readComplete=true,drawComplete=true;
    bool depth=true,depthWrite=true,blend=false,mips=true,protectedTexture=false,colorWrite=true;
    U next=16;
};
State &state();
bool protectedContext();
inline volatile LONG draws=0,privateDraws=0,invalid=0,textureDeletes=0,renderbufferDeletes=0,transfers=0,stateQueries=0;
inline std::uint64_t expectedHash=0,lastHash=0,expectedTransferHash=0,lastTransferHash=0;
inline void (WINAPI *onTransfer)()=nullptr;
// Optional adversarial Win32 error-state oracle. State inspection may change
// LastError; a detour must restore the incoming value before its original and
// preserve the original's outgoing value after metadata/diagnostic work.
inline bool transferErrorProbe=false;
inline DWORD transferIncomingError=0;
inline void transferEntered(){if(transferErrorProbe)transferIncomingError=GetLastError();}
inline void transferReturned(){if(transferErrorProbe)SetLastError(0x424288);}
inline U lastPrivateBindings=0,nestedPrivateBindings=0,nestedMode=0;
inline bool nestedArgumentsIntact=true;
inline F expectedMinimumLod=3;
inline void nestedOriginal(U kind);
inline void bad(){InterlockedIncrement(&invalid);}
inline const unsigned char *WINAPI getString(U name){InterlockedIncrement(&stateQueries);return reinterpret_cast<const unsigned char*>(name==0x1f02?(state().desktop?"4.5 BSI WGL dispatch double":"OpenGL ES 3.1 BSI dispatch double"):"");}
inline const unsigned char *WINAPI getStringi(U,U index){InterlockedIncrement(&stateQueries);
    if(state().protectionExtension){if(!index)return reinterpret_cast<const unsigned char*>("GL_EXT_protected_textures");--index;}
    if(state().desktop&&!index)return reinterpret_cast<const unsigned char*>("GL_NV_copy_image");bad();return nullptr;
}
inline void WINAPI getInteger(U name,I *out){InterlockedIncrement(&stateQueries);
    if(transferErrorProbe)SetLastError(0x987643);
    auto &g=state();
    switch(name){
    case 0x821d:*out=(g.protectionExtension?1:0)+(g.desktop?1:0);break;case 0x821e:if(!g.protectionExtension)bad();*out=protectedContext()?0x10:0;break;
    case 0x8b4d:case 0x8cdf:case 0x8824:*out=4;break;case 0x8b8d:*out=g.program;break;
    case 0x0d33:*out=16384;break;
    case 0x0b74:*out=g.depthFunction;break;case 0x84e0:*out=0x84c0+g.active;break;
    case 0x8069:*out=I(g.textures.at(std::size_t(g.active)));break;
    case 0x8919:*out=I(g.bindings.at(std::size_t(g.active)));break;
    case 0x8ca6:*out=g.framebuffer;break;case 0x8825:*out=g.drawBufferNone?0:g.framebuffer?0x8ce0:g.defaultDrawBuffer;break;
    case 0x8826:case 0x8827:case 0x8828:{const U i=name-0x8825;*out=g.framebuffer&&g.extraOutputs[i]?I(0x8ce0+i):0;break;}
    case 0x8caa:*out=g.readFramebuffer;break;case 0x0c02:*out=g.readFramebuffer?0x8ce0:g.defaultReadBuffer;break;
    default:*out=0;bad();break;
    }
}
inline void WINAPI getFloat(U name,F *out){InterlockedIncrement(&stateQueries);if(!state().desktop||name!=0x84fd)bad();*out=16;}
inline void WINAPI getBoolean(U name,B *out){InterlockedIncrement(&stateQueries);
    if(name==0x0c23){for(unsigned i=0;i<4;++i)out[i]=B(state().colorWrite);return;}
    if(name!=0x0b72)bad();*out=B(state().depthWrite);
}
inline B WINAPI enabled(U name){InterlockedIncrement(&stateQueries);return B(name==0x0b71?state().depth:name==0x0be2?state().blend:false);}
inline void WINAPI active(U unit){state().active=I(unit-0x84c0);}
inline void WINAPI program(U,U name,I *out){InterlockedIncrement(&stateQueries);*out=name==0x8b86?2:128;}
inline const char *const uniforms[]{"test_owned_first_color","test_owned_second_color"};
inline void WINAPI uniform(U,U index,I,I *length,I *size,U *type,char *name){InterlockedIncrement(&stateQueries);*length=I(std::strlen(uniforms[index]));*size=1;*type=0x8b5e;std::strcpy(name,uniforms[index]);}
inline I WINAPI location(U,const char *name){InterlockedIncrement(&stateQueries);return std::strcmp(name,uniforms[0])==0?0:std::strcmp(name,uniforms[1])==0?1:-1;}
inline void WINAPI uniformValue(U,I slot,I *out){InterlockedIncrement(&stateQueries);*out=slot;}
inline void WINAPI level(U,I mip,U name,I *out){InterlockedIncrement(&stateQueries);*out=name==0x1003?0x8058:(!state().mips&&mip>0)?0:mip<=6?(64>>mip):0;}
inline void samplerInteger(const Sampler &s,U name,I *out){
    switch(name){
    case 0x2801:*out=s.min;break;case 0x2800:*out=s.mag;break;case 0x2802:*out=s.s;break;
    case 0x2803:*out=s.t;break;case 0x8072:*out=s.r;break;case 0x884c:*out=s.compare;break;
    case 0x884d:*out=s.function;break;default:*out=0;bad();break;
    }
}
inline void WINAPI textureInteger(U,U name,I *out){InterlockedIncrement(&stateQueries);
    if(name==0x8bfa){if(!state().protectionExtension)bad();*out=state().protectedTexture;return;}
    if(name==0x813c){*out=0;return;}if(name==0x813d){*out=1000;return;}
    if(name==0x912f){*out=0;return;} // current authored resources are mutable
    samplerInteger(Sampler{},name,out);
}
inline void WINAPI samplerInt(U sampler,U name,I *out){InterlockedIncrement(&stateQueries);samplerInteger(state().samplers.at(sampler),name,out);}
inline void WINAPI samplerFloat(U sampler,U name,F *out){InterlockedIncrement(&stateQueries);
    const auto &s=state().samplers.at(sampler);
    if(name==0x1004){if(!state().desktop)bad();for(unsigned i=0;i<4;++i)out[i]=0;return;}
    if(name==0x8501){if(!state().desktop)bad();*out=s.bias;return;}
    *out=name==0x813a?s.low:name==0x813b?s.high:1;
}
inline void WINAPI textureFloat(U,U name,F *out){samplerFloat(0,name,out);}
inline U WINAPI framebufferStatus(U target){InterlockedIncrement(&stateQueries);
    if(target!=0x8ca8&&target!=0x8ca9)bad();
    return (target==0x8ca8?state().readComplete:state().drawComplete)?0x8cd5:0x8cd6;
}
inline void WINAPI attachment(U target,U slot,U name,I *out){InterlockedIncrement(&stateQueries);
    auto &g=state();const bool read=target==0x8ca8;
    if(target!=0x8ca9&&!read){bad();*out=0;return;}
    if((read?g.readFramebuffer:g.framebuffer)&&((slot>=0x8ce0&&slot<0x8ce4)||slot==0x8d20)){
        if(name!=0x8cd0&&name!=0x8cd1&&name!=0x8211&&(name<0x8212||name>0x8214))bad();
        const U extra=slot>0x8ce0&&slot<0x8ce4&&!read?g.extraOutputs[slot-0x8ce0]:0;
        *out=slot!=0x8ce0?(name==0x8cd0?(extra?0x1702:0):name==0x8cd1?I(extra):0)
            :name==0x8cd0?(read?g.readOutputType:g.outputType):name==0x8cd1?I(read?g.readOutput:g.output):name==0x8211?0x8c17:8;return;
    }
    if(!(read?g.readFramebuffer:g.framebuffer)&&((g.desktop&&slot>=0x0400&&slot<=0x0403)||(!g.desktop&&slot==0x0405))){
        if(name!=0x8211&&(name<0x8212||name>0x8214))bad();
        *out=name==0x8211?0x8c17:8;return;
    }
    if(slot!=(g.framebuffer?0x8d00u:0x1801u)||(name!=0x8cd0&&name!=0x8216))bad();
    *out=name==0x8cd0?(g.depthBits?(g.framebuffer?0x8d41:0x8218):0):g.depthBits;
}
inline void WINAPI gen(I count,U *out){for(I i=0;i<count;++i){auto &g=state();if(g.next>=g.samplers.size()){bad();out[i]=0;}else out[i]=g.next++;}}
inline void WINAPI remove(I,const U*){}
inline void WINAPI deleteTextures(I count,const U*){InterlockedExchangeAdd(&textureDeletes,count);}
inline void WINAPI deleteRenderbuffers(I count,const U*){InterlockedExchangeAdd(&renderbufferDeletes,count);}
inline void WINAPI parameterInt(U sampler,U name,I value){
    auto &s=state().samplers.at(sampler);
    switch(name){case 0x2801:s.min=value;break;case 0x2800:s.mag=value;break;case 0x2802:s.s=value;break;
    case 0x2803:s.t=value;break;case 0x8072:s.r=value;break;case 0x884c:s.compare=value;break;case 0x884d:s.function=value;break;default:bad();break;}
}
inline void WINAPI parameterFloat(U sampler,U name,F value){
    if(name==0x813a)state().samplers.at(sampler).low=value;
    else if(name==0x813b)state().samplers.at(sampler).high=value;
    else if(name==0x8501&&state().desktop)state().samplers.at(sampler).bias=value;else bad();
}
inline void WINAPI parameterVector(U,U name,const F *values){if(!state().desktop||name!=0x1004)bad();for(unsigned i=0;i<4;++i)if(values[i]!=0)bad();}
inline void WINAPI bind(U unit,U sampler){state().bindings.at(unit)=sampler;}

template<typename T>std::uint64_t valueHash(T value){
    if constexpr(std::is_pointer_v<T>)return std::uint64_t(reinterpret_cast<std::uintptr_t>(value));
    else return std::uint64_t(value);
}
template<typename... Args>std::uint64_t hash(U kind,U alias,Args... args){
    std::uint64_t result=kind*17u+alias;
    ((result=(result^valueHash(args))*1099511628211ull),...);return result;
}
template<typename... Args>void record(U kind,U alias,Args... args){
    InterlockedIncrement(&draws);lastHash=hash(kind,alias,args...);lastPrivateBindings=0;
    const auto &g=state();if(g.active!=3)bad();
    for(U unit=0;unit<2;++unit)if(g.bindings[unit]>=16){
        ++lastPrivateBindings;if(g.samplers.at(g.bindings[unit]).low!=expectedMinimumLod||(g.desktop&&g.samplers.at(g.bindings[unit]).bias!=expectedMinimumLod))bad();
    }
    if(lastPrivateBindings)InterlockedIncrement(&privateDraws);
}
#define BSI_FIXTURE_DRAW(id,params,args,...) \
    template<U Alias> void WINAPI draw##id params { \
        const auto native=[&](auto... values){record(static_cast<U>(BsiShaders::GlDrawKind::id),Alias,values...);}; \
        native args; \
        nestedOriginal(static_cast<U>(BsiShaders::GlDrawKind::id)); \
    } \
    template<std::size_t... Index> std::array<void*,sizeof...(Index)> callbacks##id(std::index_sequence<Index...>){return {{reinterpret_cast<void*>(&draw##id<Index>)...}};}
BSI_GL_DRAW_CALLS(BSI_FIXTURE_DRAW)
#undef BSI_FIXTURE_DRAW
inline const std::array<std::array<void*,BsiShaders::glDrawAliases>,BsiShaders::glDrawKinds> callbacks{{
#define BSI_FIXTURE_CALLBACKS(id,params,args,...) callbacks##id(std::make_index_sequence<BsiShaders::glDrawAliases>{}),
    BSI_GL_DRAW_CALLS(BSI_FIXTURE_CALLBACKS)
#undef BSI_FIXTURE_CALLBACKS
}};
template<U Alias> void WINAPI blitFramebuffer(I sx0,I sy0,I sx1,I sy1,I dx0,I dy0,I dx1,I dy1,U mask,U filter){
    transferEntered();
    InterlockedIncrement(&transfers);lastTransferHash=hash(100,Alias,sx0,sy0,sx1,sy1,dx0,dy0,dx1,dy1,mask,filter);
    if(state().active!=3||state().bindings!=std::array<U,4>{{1,2,0,0}})bad();
    if(onTransfer)onTransfer();transferReturned();
}
template<U Alias> void WINAPI copyTexSubImage2D(U target,I level,I dx,I dy,I x,I y,I width,I height){
    transferEntered();
    InterlockedIncrement(&transfers);lastTransferHash=hash(101,Alias,target,level,dx,dy,x,y,width,height);
    if(state().active!=3||state().bindings!=std::array<U,4>{{1,2,0,0}})bad();
    if(onTransfer)onTransfer();transferReturned();
}
inline bool restored();
inline B WINAPI isTexture(U name){InterlockedIncrement(&stateQueries);return name==50||name==51||name==1||name==2;}
inline B WINAPI isRenderbuffer(U name){InterlockedIncrement(&stateQueries);return name==70;}
inline void WINAPI namedTexture(U name,U key,I *out){
    InterlockedIncrement(&stateQueries);if(!state().desktop||!isTexture(name)){bad();*out=0;return;}
    switch(key){case 0x1006:*out=0x0de1;break;case 0x813c:*out=0;break;case 0x813d:*out=1000;break;
    case 0x2801:case 0x2800:*out=0x2601;break;default:*out=0;bad();}
}
inline void WINAPI namedLevel(U name,I mip,U key,I *out){
    InterlockedIncrement(&stateQueries);if(!state().desktop||!isTexture(name)||mip<0||mip>6){bad();*out=0;return;}
    if(key!=0x1000&&key!=0x1001&&key!=0x1003)bad();*out=key==0x1003?0x8058:(64>>mip);
}
inline void WINAPI namedRenderbuffer(U name,U key,I *out){
    InterlockedIncrement(&stateQueries);if(!state().desktop||!isRenderbuffer(name)){bad();*out=0;return;}
    switch(key){case 0x8d42:case 0x8d43:*out=64;break;case 0x8d44:*out=0x8058;break;case 0x8cab:*out=0;break;default:*out=0;bad();}
}
template<U Alias> void WINAPI copyImage(U src,U st,I sl,I sx,I sy,I sz,U dst,U dt,I dl,I dx,I dy,I dz,I width,I height,I depth){
    transferEntered();
    InterlockedIncrement(&transfers);lastTransferHash=hash(102,Alias,src,st,sl,sx,sy,sz,dst,dt,dl,dx,dy,dz,width,height,depth);
    if(!restored())bad();if(onTransfer)onTransfer();transferReturned();
}
inline const std::array<void*,4> blitTargets{{reinterpret_cast<void*>(&blitFramebuffer<0>),reinterpret_cast<void*>(&blitFramebuffer<1>),reinterpret_cast<void*>(&blitFramebuffer<2>),reinterpret_cast<void*>(&blitFramebuffer<3>)}};
inline const std::array<void*,2> copyTargets{{reinterpret_cast<void*>(&copyTexSubImage2D<0>),reinterpret_cast<void*>(&copyTexSubImage2D<1>)}};
inline const char *const blitNames[]{"glBlitFramebuffer","glBlitFramebufferEXT","glBlitFramebufferANGLE","glBlitFramebufferNV"};
inline const char *const copyNames[]{"glCopyTexSubImage2D","glCopyTexSubImage2DEXT"};
inline void invokeTransfer(bool copy,U alias){
    if(copy){
        constexpr U target=0x0de1;constexpr I level=0,dx=3,dy=4,x=1,y=2,width=57,height=56;
        expectedTransferHash=hash(101,alias,target,level,dx,dy,x,y,width,height);
        using Function=void(WINAPI*)(U,I,I,I,I,I,I,I);void *volatile entry=copyTargets.at(alias);
        reinterpret_cast<Function>(entry)(target,level,dx,dy,x,y,width,height);
    }else{
        constexpr I sx0=1,sy0=2,sx1=61,sy1=62,dx0=3,dy0=4,dx1=59,dy1=60;constexpr U mask=0x4000,filter=0x2600;
        expectedTransferHash=hash(100,alias,sx0,sy0,sx1,sy1,dx0,dy0,dx1,dy1,mask,filter);
        using Function=void(WINAPI*)(I,I,I,I,I,I,I,I,U,U);void *volatile entry=blitTargets.at(alias);
        reinterpret_cast<Function>(entry)(sx0,sy0,sx1,sy1,dx0,dy0,dx1,dy1,mask,filter);
    }
}
inline void *resolve(const char *name){
#define ENTRY(gl,fn) if(std::strcmp(name,"gl" #gl)==0)return reinterpret_cast<void*>(&fn)
    ENTRY(IsTexture,isTexture);ENTRY(IsRenderbuffer,isRenderbuffer);ENTRY(GetTextureParameteriv,namedTexture);ENTRY(GetTextureLevelParameteriv,namedLevel);ENTRY(GetNamedRenderbufferParameteriv,namedRenderbuffer);
    ENTRY(GetString,getString);ENTRY(GetStringi,getStringi);ENTRY(GetIntegerv,getInteger);ENTRY(GetFloatv,getFloat);ENTRY(GetBooleanv,getBoolean);
    ENTRY(IsEnabled,enabled);ENTRY(ActiveTexture,active);ENTRY(GetProgramiv,program);ENTRY(GetActiveUniform,uniform);
    ENTRY(GetUniformLocation,location);ENTRY(GetUniformiv,uniformValue);ENTRY(GetTexLevelParameteriv,level);
    ENTRY(GetTexParameteriv,textureInteger);ENTRY(GetTexParameterfv,textureFloat);ENTRY(GetSamplerParameteriv,samplerInt);
    ENTRY(GetSamplerParameterfv,samplerFloat);ENTRY(GetFramebufferAttachmentParameteriv,attachment);ENTRY(CheckFramebufferStatus,framebufferStatus);ENTRY(GenSamplers,gen);
    ENTRY(DeleteSamplers,remove);ENTRY(DeleteTextures,deleteTextures);ENTRY(DeleteRenderbuffers,deleteRenderbuffers);ENTRY(SamplerParameteri,parameterInt);ENTRY(SamplerParameterf,parameterFloat);
    ENTRY(SamplerParameterfv,parameterVector);ENTRY(BindSampler,bind);
#undef ENTRY
    if(std::strcmp(name,"glCopyImageSubData")==0)return reinterpret_cast<void*>(&copyImage<0>);
    if(std::strcmp(name,"glCopyImageSubDataNV")==0)return reinterpret_cast<void*>(&copyImage<1>);
    for(U alias=0;alias<blitTargets.size();++alias)if(std::strcmp(name,blitNames[alias])==0)return blitTargets[alias];
    for(U alias=0;alias<copyTargets.size();++alias)if(std::strcmp(name,copyNames[alias])==0)return copyTargets[alias];
    for(U kind=0;kind<BsiShaders::glDrawKinds;++kind)for(U alias=0;alias<BsiShaders::glDrawAliases;++alias)
        if(BsiShaders::glDrawNames[kind][alias]&&std::strcmp(name,BsiShaders::glDrawNames[kind][alias])==0)return callbacks[kind][alias];
    return nullptr;
}
inline bool restored(){const auto &g=state();return g.active==3&&g.bindings==std::array<U,4>{{1,2,0,0}}&&g.samplers[1].low==-1000&&g.samplers[2].low==-1000;}
inline void invoke(U kind,U alias,U mode){
    // Deliberately distinctive offsets/counts/signed base vertices and a >32-bit
    // indirect-count offset. None of these sentinel addresses may be dereferenced.
    const int first=7,count=13,instances=3,baseVertex=-11,drawCount=2,maxDrawCount=5,stride=28;
    const U type=0x1403,start=17,end=71,baseInstance=29;
    const std::intptr_t countOffset=std::intptr_t(0x123456789ull);
    const void *indices=reinterpret_cast<void*>(0x123400),*indirect=reinterpret_cast<void*>(0x234500);
    const int firsts[]{3,7},counts[]{5,11},baseVertices[]{-3,13};
    const void *const offsets[]{reinterpret_cast<void*>(0x345600),reinterpret_cast<void*>(0x456700)};
    const auto original=[&](auto... values){
        expectedHash=hash(kind,alias,values...);
        using Function=void(WINAPI*)(decltype(values)...);
        // Even a fixture-internal constant-kind nested call must go THROUGH
        // the detoured entry, not an optimizer-inlined copy of its original.
        void *volatile target=callbacks[kind][alias];
        reinterpret_cast<Function>(target)(values...);
    };
    switch(static_cast<BsiShaders::GlDrawKind>(kind)){
#define BSI_FIXTURE_INVOKE(id,params,args,...) case BsiShaders::GlDrawKind::id:original args;break;
        BSI_GL_DRAW_CALLS(BSI_FIXTURE_INVOKE)
#undef BSI_FIXTURE_INVOKE
    default:bad();break;
    }
}
// Model a translator which sets real depth state only INSIDE its original
// outer entry, then calls another detoured entry. This is authored dispatch,
// not evidence that the user's player has this particular call stack.
inline void nestedOriginal(U kind){
    if(!nestedMode||kind!=static_cast<U>(BsiShaders::GlDrawKind::Arrays))return;
    auto &g=state();const auto previousDepth=g.depth;
    const auto outerExpected=expectedHash,outerHash=lastHash;
    const auto outerPrivate=lastPrivateBindings;
    if(nestedMode==1)g.depth=true;
    if(nestedMode==3)g.depth=false;
    invoke(static_cast<U>(BsiShaders::GlDrawKind::ElementsBaseVertex),0,4);
    nestedArgumentsIntact=nestedArgumentsIntact&&lastHash==expectedHash;
    nestedPrivateBindings=lastPrivateBindings;
    // Restore only the fixture's own depth state and argument oracle. Never
    // mask a bad production sampler/active-unit restoration by resetting GL.
    g.depth=previousDepth;expectedHash=outerExpected;lastHash=outerHash;lastPrivateBindings=outerPrivate;
}
#undef BSI_GL_DRAW_CALLS
} // namespace EglDrawFixture
