#include "GlTextureOverride.h"
#include "GlTextureQuality.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#ifdef _WIN32
#define BSI_GL_CALL __stdcall
#else
#define BSI_GL_CALL
#endif

namespace BsiShaders {
namespace {
using U=unsigned;using I=int;using F=float;using B=unsigned char;
constexpr U texture2d=0x0de1,texture0=0x84c0;
struct Sampler {
    I min=0,mag=0,s=0,t=0,r=0,compare=0,function=0,decode=0,reduction=0;
    F lo=0,hi=0,bias=0,anisotropy=1;
    std::array<F,4> border{};
    std::array<F,17> key()const{return {F(min),F(mag),F(s),F(t),F(r),F(compare),F(function),F(decode),F(reduction),lo,hi,bias,anisotropy,border[0],border[1],border[2],border[3]};}
};
bool colorFormat(I f){
    // Normalized RGB/RGBA only (including common game compression formats).
    // Never depth/stencil/integer/HDR/data-only single-channel resources.
    return f==0x1907||f==0x1908||f==0x8051||f==0x8058||f==0x8c41||f==0x8c43||f==0x8056||f==0x8d62
        ||(f>=0x83f0&&f<=0x83f3)||(f>=0x8c4c&&f<=0x8c4f)||f==0x8d64
        ||(f>=0x9274&&f<=0x9279)||(f>=0x93b0&&f<=0x93bd)||(f>=0x93d0&&f<=0x93dd)||f==0x8e8c||f==0x8e8d;
}
const char *depthRejection(TextureDepthFailure reason){
    switch(reason){
    case TextureDepthFailure::TestDisabled:return "Potato bypass: depth testing is disabled on this captured draw. No scene sampler was inspected; UI/composition stays native.";
    case TextureDepthFailure::WritesDisabled:return "Potato bypass: depth writes are disabled on this captured draw. Read-only-depth passes are not qualified; native sampling retained.";
    case TextureDepthFailure::Never:return "Potato bypass: the captured draw uses depth function NEVER. No scene sampler was inspected.";
    case TextureDepthFailure::Always:return "Potato bypass: the captured draw uses depth function ALWAYS. UI/composition stays native.";
    case TextureDepthFailure::MissingAttachment:return "Potato bypass: the current draw framebuffer has no depth attachment. No scene sampler was inspected; composition stays native.";
    case TextureDepthFailure::InsufficientPrecision:return "Potato bypass: the current draw depth attachment has fewer than 16 bits. No scene sampler was inspected.";
    default:return "Potato bypass: scene depth could not be qualified. Native sampling retained.";
    }
}
}
struct GlTextureOverride::Impl {
#define GL_FUNCTION(result,name,args) using name##Type=result(BSI_GL_CALL*)args;name##Type name=nullptr;
    GL_FUNCTION(const unsigned char*,GetString,(U))
    GL_FUNCTION(const unsigned char*,GetStringi,(U,U))
    GL_FUNCTION(void,GetIntegerv,(U,I*))
    GL_FUNCTION(void,GetFloatv,(U,F*))
    GL_FUNCTION(void,GetBooleanv,(U,B*))
    GL_FUNCTION(B,IsEnabled,(U))
    GL_FUNCTION(void,ActiveTexture,(U))
    GL_FUNCTION(void,GetProgramiv,(U,U,I*))
    GL_FUNCTION(void,GetActiveUniform,(U,U,I,I*,I*,U*,char*))
    GL_FUNCTION(I,GetUniformLocation,(U,const char*))
    GL_FUNCTION(void,GetUniformiv,(U,I,I*))
    GL_FUNCTION(void,GetTexLevelParameteriv,(U,I,U,I*))
    GL_FUNCTION(void,GetTexParameteriv,(U,U,I*))
    GL_FUNCTION(void,GetTexParameterfv,(U,U,F*))
    GL_FUNCTION(void,GetSamplerParameteriv,(U,U,I*))
    GL_FUNCTION(void,GetSamplerParameterfv,(U,U,F*))
    GL_FUNCTION(void,GetFramebufferAttachmentParameteriv,(U,U,U,I*))
    GL_FUNCTION(U,CheckFramebufferStatus,(U))
    GL_FUNCTION(void,GenSamplers,(I,U*))
    GL_FUNCTION(void,DeleteSamplers,(I,const U*))
    GL_FUNCTION(void,SamplerParameteri,(U,U,I))
    GL_FUNCTION(void,SamplerParameterf,(U,U,F))
    GL_FUNCTION(void,SamplerParameterfv,(U,U,const F*))
    GL_FUNCTION(void,BindSampler,(U,U))
#undef GL_FUNCTION
    bool ready=false,es=false,anisotropic=false,srgbDecode=false,minmax=false,protectedTextures=false;
    GlQualityCapabilities qualityCapabilities;
    I units=0,attachments=0,extensionCount=0;
    std::map<std::array<F,17>,U> cache;
    struct Binding {U unit,original;};
    std::vector<Binding> changed;
    bool extension(const char *needle){
        for(I i=0;i<extensionCount;++i){const auto *text=GetStringi(0x1f03,U(i));if(text&&std::strcmp(reinterpret_cast<const char*>(text),needle)==0)return true;}
        return false;
    }
    struct Inspection {
        Impl &g; I active=0; bool keepBindings=false;
        explicit Inspection(Impl &api):g(api){g.GetIntegerv(0x84e0,&active);}
        ~Inspection(){
            // Allocations (uniform names/cache/detail) may fail after selecting
            // a unit or even binding one replacement. Fail BEFORE the game's
            // original draw, not merely in the hook's after-draw destructor.
            if(!keepBindings)g.restoreBindings();
            g.ActiveTexture(U(active));
        }
    };
    void restoreBindings()noexcept{
        for(const auto &binding:changed)BindSampler(binding.unit,binding.original);
        changed.clear();
    }
    void integer(U sampler,U key,I *out){if(sampler)GetSamplerParameteriv(sampler,key,out);else GetTexParameteriv(texture2d,key,out);}
    void floating(U sampler,U key,F *out){if(sampler)GetSamplerParameterfv(sampler,key,out);else GetTexParameterfv(texture2d,key,out);}
    Sampler readSampler(U sampler){
        Sampler v;
        integer(sampler,0x2801,&v.min);integer(sampler,0x2800,&v.mag);
        integer(sampler,0x2802,&v.s);integer(sampler,0x2803,&v.t);integer(sampler,0x8072,&v.r);
        integer(sampler,0x884c,&v.compare);integer(sampler,0x884d,&v.function);
        floating(sampler,0x813a,&v.lo);floating(sampler,0x813b,&v.hi);
        if(!es){floating(sampler,0x8501,&v.bias);floating(sampler,0x1004,v.border.data());}
        if(anisotropic)floating(sampler,0x84fe,&v.anisotropy);
        if(srgbDecode)integer(sampler,0x8a48,&v.decode);
        if(minmax)integer(sampler,0x9366,&v.reduction);
        return v;
    }
    U privateSampler(const Sampler &v){
        const auto key=v.key();
        for(const auto value:key)if(!std::isfinite(value))return 0;
        const auto found=cache.find(key);if(found!=cache.end())return found->second;
        if(cache.size()>=128)return 0;
        U sampler=0;GenSamplers(1,&sampler);if(!sampler)return 0;
        struct PendingSampler {
            Impl &g;U name;
            ~PendingSampler(){if(name)g.DeleteSamplers(1,&name);}
        } pending{*this,sampler};
        SamplerParameteri(sampler,0x2801,v.min);SamplerParameteri(sampler,0x2800,v.mag);
        SamplerParameteri(sampler,0x2802,v.s);SamplerParameteri(sampler,0x2803,v.t);SamplerParameteri(sampler,0x8072,v.r);
        SamplerParameteri(sampler,0x884c,v.compare);SamplerParameteri(sampler,0x884d,v.function);
        SamplerParameterf(sampler,0x813a,v.lo);SamplerParameterf(sampler,0x813b,v.hi);
        if(!es){SamplerParameterf(sampler,0x8501,v.bias);SamplerParameterfv(sampler,0x1004,v.border.data());}
        if(anisotropic)SamplerParameterf(sampler,0x84fe,v.anisotropy);
        if(srgbDecode)SamplerParameteri(sampler,0x8a48,v.decode);
        if(minmax)SamplerParameteri(sampler,0x9366,v.reduction);
        // Never count/bind an unverified replacement (and never consume the
        // application's GL error queue to detect our own failures).
        const auto actual=readSampler(sampler);
        if(actual.key()!=key)return 0;
        cache.emplace(key,sampler);pending.name=0;return sampler;
    }
    bool attached(U texture){
        I framebuffer=0;GetIntegerv(0x8ca6,&framebuffer); // DRAW_FRAMEBUFFER_BINDING
        if(!framebuffer)return false;
        const auto match=[&](U attachment){
            I type=0,name=0;GetFramebufferAttachmentParameteriv(0x8ca9,attachment,0x8cd0,&type);
            if(type==0x1702)GetFramebufferAttachmentParameteriv(0x8ca9,attachment,0x8cd1,&name);
            return type==0x1702&&U(name)==texture;
        };
        for(I i=0;i<attachments;++i)if(match(0x8ce0+U(i)))return true;
        return match(0x8d00)||match(0x8d20);
    }
    TextureDepthFailure sceneDepth(){
        // Stop at the FIRST blocker. Besides making the rejection actionable,
        // a depth-disabled compositor needs one query, not sampler inspection.
        if(!IsEnabled(0x0b71))return TextureDepthFailure::TestDisabled;
        B write=0;GetBooleanv(0x0b72,&write);
        if(!write)return TextureDepthFailure::WritesDisabled;
        I function=0;GetIntegerv(0x0b74,&function);
        if(function==0x0200)return TextureDepthFailure::Never;
        if(function==0x0207)return TextureDepthFailure::Always;
        I framebuffer=0,type=0,bits=0;GetIntegerv(0x8ca6,&framebuffer);
        const U attachment=framebuffer?0x8d00:0x1801; // FBO depth vs default GL_DEPTH
        GetFramebufferAttachmentParameteriv(0x8ca9,attachment,0x8cd0,&type);
        if(!type)return TextureDepthFailure::MissingAttachment;
        // GL_DEPTH_BITS was removed from desktop core: use the legal GL3/ES3
        // attachment query instead, including for the default framebuffer.
        GetFramebufferAttachmentParameteriv(0x8ca9,attachment,0x8216,&bits);
        return bits>=16?TextureDepthFailure::None:TextureDepthFailure::InsufficientPrecision;
    }
    I mipLevels(){
        if(protectedTextures){I protectedContent=1;GetTexParameteriv(texture2d,0x8bfa,&protectedContent);if(protectedContent)return 0;}
        I base=0,last=0;GetTexParameteriv(texture2d,0x813c,&base);GetTexParameteriv(texture2d,0x813d,&last);
        if(base<0||base>30||last<=base||last>1000)return 0;
        I width=0,height=0,format=0;
        GetTexLevelParameteriv(texture2d,base,0x1000,&width);GetTexLevelParameteriv(texture2d,base,0x1001,&height);
        GetTexLevelParameteriv(texture2d,base,0x1003,&format);
        if(width<64||height<64||width>16384||height>16384||!colorFormat(format))return 0;
        I levels=1;
        while((width>1||height>1)&&base+levels<=last&&levels<16){
            width=(std::max)(1,width/2);height=(std::max)(1,height/2);
            I w=0,h=0,f=0;GetTexLevelParameteriv(texture2d,base+levels,0x1000,&w);
            GetTexLevelParameteriv(texture2d,base+levels,0x1001,&h);GetTexLevelParameteriv(texture2d,base+levels,0x1003,&f);
            if(w!=width||h!=height||f!=format)return 0;
            ++levels;
        }
        return levels;
    }
};
GlTextureOverride::GlTextureOverride():impl(std::make_unique<Impl>()){}
GlTextureOverride::~GlTextureOverride()=default;
bool GlTextureOverride::initialize(ResolveGl resolve,void *user,std::string *error,bool externalProtectionPossible){
    auto &g=*impl;
    if(g.ready)return true;
    const auto fail=[&](const char *message){if(error)*error=message;return false;};
    if(!resolve)return fail("Potato needs a current, owned game GL context.");
#define LOAD(name) g.name=reinterpret_cast<Impl::name##Type>(resolve("gl" #name,user));if(!g.name)return fail("Potato sampler capture is missing gl" #name ". Original textures retained.")
    LOAD(GetString);LOAD(GetStringi);LOAD(GetIntegerv);LOAD(GetBooleanv);LOAD(IsEnabled);
    LOAD(ActiveTexture);LOAD(GetProgramiv);LOAD(GetActiveUniform);LOAD(GetUniformLocation);LOAD(GetUniformiv);
    LOAD(GetTexLevelParameteriv);LOAD(GetTexParameteriv);LOAD(GetTexParameterfv);
    LOAD(GetSamplerParameteriv);LOAD(GetSamplerParameterfv);LOAD(GetFramebufferAttachmentParameteriv);
    LOAD(GenSamplers);LOAD(DeleteSamplers);LOAD(SamplerParameteri);LOAD(SamplerParameterf);LOAD(SamplerParameterfv);LOAD(BindSampler);
#undef LOAD
    // Optional to the older draw-only contract. Without it, transfer capture
    // fails closed but the existing qualified private-sampler path still works.
    g.CheckFramebufferStatus=reinterpret_cast<Impl::CheckFramebufferStatusType>(resolve("glCheckFramebufferStatus",user));
    const auto *raw=g.GetString(0x1f02);if(!raw)return fail("GL version could not be identified for Potato.");
    const std::string version(reinterpret_cast<const char*>(raw));g.es=version.find("OpenGL ES")!=std::string::npos;
    int major=0,minor=0;const auto first=version.find_first_of("0123456789");
    if(first==std::string::npos||std::sscanf(version.c_str()+first,"%d.%d",&major,&minor)!=2
       ||major<3||(major==3&&minor<(g.es?1:3)))
        return fail("Potato requires GL 3.3 / GLES 3.1 with queryable texture levels and private sampler objects. This path is left untouched.");
    g.GetIntegerv(0x821d,&g.extensionCount);
    if(g.extensionCount<0||g.extensionCount>4096)return fail("GL extension/protection state could not be qualified; original textures retained.");
    for(I i=0;i<g.extensionCount;++i)if(!g.GetStringi(0x1f03,U(i)))return fail("GL extension/protection state is incomplete; original textures retained.");
    g.protectedTextures=g.extension("GL_EXT_protected_textures");
    if(externalProtectionPossible&&!g.protectedTextures)return fail("Protected EGL resources are possible but per-texture protection inspection is unavailable; native sampling retained.");
    if(g.protectedTextures){I flags=0;g.GetIntegerv(0x821e,&flags);if(flags&0x10)return fail("Protected GL contexts are never modified by Potato.");}
    g.qualityCapabilities={};
    if(!g.es){
        // Desktop sampler bias is legal here; NEVER query this enum on GLES.
        // A missing/invalid query is not permission to assume the tested 3.0.
        g.GetFloatv=reinterpret_cast<Impl::GetFloatvType>(resolve("glGetFloatv",user));
        if(!g.GetFloatv)return fail("Desktop Potato needs a queryable LOD-bias limit; native sampling retained.");
        g.GetFloatv(0x84fd,&g.qualityCapabilities.maximumLodBias); // GL_MAX_TEXTURE_LOD_BIAS
        if(!std::isfinite(g.qualityCapabilities.maximumLodBias)||g.qualityCapabilities.maximumLodBias<=0)
            return fail("Desktop LOD-bias support could not be qualified; native sampling retained.");
    }
    g.GetIntegerv(0x8b4d,&g.units);g.GetIntegerv(0x8cdf,&g.attachments);
    if(g.units<1||g.units>256||g.attachments<1||g.attachments>32)return fail("Unqualified GL sampler/attachment limits; original textures retained.");
    g.anisotropic=g.extension("GL_EXT_texture_filter_anisotropic")||g.extension("GL_ARB_texture_filter_anisotropic");
    g.srgbDecode=g.extension("GL_EXT_texture_sRGB_decode");g.minmax=g.extension("GL_EXT_texture_filter_minmax");
    g.changed.reserve(32);g.ready=true;return true;
}
bool GlTextureOverride::usage(GlTextureUsage *out){
    if(!out)return false;
    *out={};auto &g=*impl;if(!g.ready||g.IsEnabled(0x0be2))return false;
    I program=0,count=0,maxName=0;g.GetIntegerv(0x8b8d,&program);if(!program)return false;
    g.GetProgramiv(U(program),0x8b86,&count);g.GetProgramiv(U(program),0x8b87,&maxName);
    if(count<0||count>256||maxName<0||maxName>512)return false;
    B mask[4]{};g.GetBooleanv(0x0c23,mask); // buffer-0 COLOR_WRITEMASK, no pixel data
    if(!mask[0]&&!mask[1]&&!mask[2]&&!mask[3])return false;
    I framebuffer=0,slot=0;g.GetIntegerv(0x8ca6,&framebuffer);out->defaultFramebuffer=framebuffer==0;
    g.GetIntegerv(0x8825,&slot);if(!slot)return false; // DRAW_BUFFER0, legal in GL3/ES3
    if(framebuffer&&slot>=0x8ce0&&slot<0x8ce0+g.attachments){
        // Capture only buffer 0, whose mask is known on BOTH APIs. Do not
        // authorize other MRT attachments using buffer 0's indexed mask.
        I type=0,name=0;g.GetFramebufferAttachmentParameteriv(0x8ca9,U(slot),0x8cd0,&type);
        if(type==0x1702)g.GetFramebufferAttachmentParameteriv(0x8ca9,U(slot),0x8cd1,&name);
        if(type==0x1702&&name)out->writes[out->writeCount++]=U(name);
        // Renderbuffer names live in a DIFFERENT namespace. Where protected
        // resources are possible there is no binding-free core GLES query for
        // their protection; do not extend capture to that unqualified output.
        if(type==0x8d41&&!g.protectedTextures){
            g.GetFramebufferAttachmentParameteriv(0x8ca9,U(slot),0x8cd1,&name);
            if(name)out->writes[out->writeCount++]=GlColorResource::renderbuffer(U(name));
        }
    }
    struct ActiveRestore {Impl &g;I active;~ActiveRestore(){g.ActiveTexture(U(active));}} restore{g,0};
    g.GetIntegerv(0x84e0,&restore.active);std::array<bool,32> used{};
    for(I i=0;i<count;++i){
        char name[512]{};I length=0,size=0;U type=0;
        g.GetActiveUniform(U(program),U(i),512,&length,&size,&type,name);
        if(type!=0x8b5e||size<1||size>32||length<1||length>=511)continue;
        std::string stem(name);if(size>1&&stem.size()>3&&stem.substr(stem.size()-3)=="[0]")stem.resize(stem.size()-3);
        for(I element=0;element<size;++element){
            const std::string uniform=size==1?std::string(name):stem+"["+std::to_string(element)+"]";
            const I location=g.GetUniformLocation(U(program),uniform.c_str());if(location<0)continue;
            I unit=-1;g.GetUniformiv(U(program),location,&unit);
            if(unit<0||unit>=g.units||unit>=32||used[std::size_t(unit)])continue;
            used[std::size_t(unit)]=true;g.ActiveTexture(texture0+U(unit));I texture=0,sampler=0;
            g.GetIntegerv(0x8069,&texture);g.GetIntegerv(0x8919,&sampler);
            if(!texture||g.attached(U(texture)))continue;
            if(g.protectedTextures){I secret=1;g.GetTexParameteriv(texture2d,0x8bfa,&secret);if(secret)continue;}
            I base=0,width=0,height=0,format=0,compare=0;
            g.GetTexParameteriv(texture2d,0x813c,&base);if(base<0||base>30)continue;
            g.GetTexLevelParameteriv(texture2d,base,0x1000,&width);g.GetTexLevelParameteriv(texture2d,base,0x1001,&height);
            g.GetTexLevelParameteriv(texture2d,base,0x1003,&format);g.integer(U(sampler),0x884c,&compare);
            if(width<1||height<1||width>16384||height>16384||!colorFormat(format)||compare)continue;
            if(out->readCount<out->reads.size())out->reads[out->readCount++]=U(texture);
        }
    }
    return true;
}
bool GlTextureOverride::transfer(const GlColorTransfer &call,GlTextureUsage *out){
    if(!out)return false;
    *out={};auto &g=*impl;if(!g.ready||!g.CheckFramebufferStatus)return false;
    const auto colorBuffer=[&](bool draw,GlColorResource *resource,bool *native){
        const U target=draw?0x8ca9:0x8ca8; // DRAW/READ_FRAMEBUFFER
        if(g.CheckFramebufferStatus(target)!=0x8cd5)return false;
        I framebuffer=0,slot=0;g.GetIntegerv(draw?0x8ca6:0x8caa,&framebuffer);
        g.GetIntegerv(draw?0x8825:0x0c02,&slot);if(!slot)return false;
        *native=framebuffer==0;
        const auto normalized=[&](U attachment){
            I component=0,red=0,green=0,blue=0;
            g.GetFramebufferAttachmentParameteriv(target,attachment,0x8211,&component);
            g.GetFramebufferAttachmentParameteriv(target,attachment,0x8212,&red);
            g.GetFramebufferAttachmentParameteriv(target,attachment,0x8213,&green);
            g.GetFramebufferAttachmentParameteriv(target,attachment,0x8214,&blue);
            return component==0x8c17&&red>0&&green>0&&blue>0;
        };
        if(!framebuffer){
            // ES default attachment is BACK; desktop core requires a specific
            // FRONT/BACK_LEFT/RIGHT attachment, not the Draw/ReadBuffer alias.
            const U color=g.es?0x0405:slot==0x0405?0x0402:slot==0x0404?0x0400:U(slot);
            if((g.es&&slot!=0x0405)||(!g.es&&(color<0x0400||color>0x0403))||!normalized(color))return false;
            *resource=GlColorResource::pbuffer(0);return true;
        }
        if(slot<0x8ce0||slot>=0x8ce0+g.attachments||g.protectedTextures)return false;
        // Core GLES has no binding-free per-attachment protection query. Do
        // not infer an unprotected texture/RBO merely from a public object name.
        I type=0,name=0;
        g.GetFramebufferAttachmentParameteriv(target,U(slot),0x8cd0,&type);
        if(type!=0x1702&&type!=0x8d41)return false;
        g.GetFramebufferAttachmentParameteriv(target,U(slot),0x8cd1,&name);
        if(name<=0||!normalized(U(slot)))return false;
        *resource=type==0x1702?GlColorResource(U(name)):GlColorResource::renderbuffer(U(name));return true;
    };
    GlTextureUsage captured;GlColorResource source,destination;bool nativeRead=false,nativeDraw=false;
    if(call.kind==GlColorTransfer::BlitFramebuffer){
        if(call.mask!=0x4000||(call.filter!=0x2600&&call.filter!=0x2601))return false;
        for(auto value:call.source)if(value<0||value>16384)return false;
        for(auto value:call.destination)if(value<0||value>16384)return false;
        if(call.source[0]==call.source[2]||call.source[1]==call.source[3]
           ||call.destination[0]==call.destination[2]||call.destination[1]==call.destination[3])return false;
        if(!colorBuffer(false,&source,&nativeRead)||!colorBuffer(true,&destination,&nativeDraw))return false;
    }else if(call.kind==GlColorTransfer::CopyTexSubImage2D){
        if(call.target!=texture2d||call.level!=0)return false;
        const auto x=call.source[0],y=call.source[1],width=call.source[2],height=call.source[3];
        const auto dx=call.destination[0],dy=call.destination[1];
        if(x<0||y<0||width<=0||height<=0||x>16384-width||y>16384-height||dx<0||dy<0)return false;
        I texture=0,base=-1,w=0,h=0,format=0;g.GetIntegerv(0x8069,&texture);if(texture<=0)return false;
        if(g.protectedTextures){I secret=1;g.GetTexParameteriv(texture2d,0x8bfa,&secret);if(secret)return false;}
        g.GetTexParameteriv(texture2d,0x813c,&base);if(base!=0)return false;
        g.GetTexLevelParameteriv(texture2d,0,0x1000,&w);g.GetTexLevelParameteriv(texture2d,0,0x1001,&h);
        g.GetTexLevelParameteriv(texture2d,0,0x1003,&format);
        if(w<1||h<1||w>16384||h>16384||width>w||height>h||dx>w-width||dy>h-height||!colorFormat(format))return false;
        if(!colorBuffer(false,&source,&nativeRead))return false;
        destination=U(texture);
    }else return false;
    // Two different native default drawables cannot be identified by GL alone;
    // leave both placeholders for the caller to resolve before equality/proof.
    if(!nativeRead&&!nativeDraw&&source==destination)return false;
    captured.defaultFramebuffer=nativeDraw;
    captured.reads[captured.readCount++]=source;captured.writes[captured.writeCount++]=destination;
    *out=captured;return true;
}
unsigned GlTextureOverride::begin(TextureQuality mode,bool reviewed,U primitive,std::string *detail,TextureDrawReason *reason,TextureDepthFailure *depthFailure,TextureDrawPolicy policy){
    auto &g=*impl;end();
    if(reason)*reason=TextureDrawReason::NotInspected;
    if(depthFailure)*depthFailure=TextureDepthFailure::None;
    const auto fail=[&](TextureDrawReason why,const char *message)->U{if(reason)*reason=why;if(detail)*detail=message;return 0;};
    if(!g.ready||mode==TextureQuality::Native)return 0;
    if(!reviewed)return fail(TextureDrawReason::Unreviewed,"Potato is not applied: review existing driver LOD/AA overrides first; no profile is reset.");
    if(policy!=TextureDrawPolicy::DepthQualifiedScene&&policy!=TextureDrawPolicy::OwnedMipCompatibility)
        return fail(TextureDrawReason::Policy,"Potato bypass: unknown sampling policy; native bindings retained.");
    if(primitive!=0x0004&&primitive!=0x0005&&primitive!=0x0006)
        return fail(TextureDrawReason::Primitive,"Potato bypass: non-triangle draw retains native sampling.");
    if(policy==TextureDrawPolicy::DepthQualifiedScene){
        const auto depth=g.sceneDepth();
        if(depth!=TextureDepthFailure::None){
            if(depthFailure)*depthFailure=depth;
            return fail(TextureDrawReason::Depth,depthRejection(depth));
        }
    }
    // The user-enabled compatibility strategy follows the original's LOD
    // approach WITHOUT depending on depth-test/write/attachment state. It is
    // still scoped by the caller to the selected drawable or a resource-proven
    // color-FBO dependency, then by the real shader/mip/protection/descriptor checks.
    // Blended UI/transparent passes remain native, even in the game context.
    if(g.IsEnabled(0x0be2))return fail(TextureDrawReason::Blended,"Potato bypass: blended/UI draw retains native sampling.");
    if(reason)*reason=TextureDrawReason::NoSampler;
    I program=0,count=0,maxName=0;g.GetIntegerv(0x8b8d,&program);
    if(!program)return fail(TextureDrawReason::NoSampler,"Potato bypass: no inspectable scene shader program; native sampling retained.");
    g.GetProgramiv(U(program),0x8b86,&count);g.GetProgramiv(U(program),0x8b87,&maxName);
    if(count<0||count>256||maxName<0||maxName>512)return fail(TextureDrawReason::NoSampler,"Potato bypass: shader uniform layout exceeds safe inspection bounds.");
    Impl::Inspection inspection(g);
    unsigned samplers=0,bound=0,feedback=0,noChain=0,descriptor=0,replacementFailure=0;
    std::set<U> used;
    for(I i=0;i<count;++i){
        char name[512]{};I length=0,size=0;U type=0;
        g.GetActiveUniform(U(program),U(i),512,&length,&size,&type,name);
        if(type!=0x8b5e||size<1||size>32||length<1||length>=511)continue; // sampler2D ONLY
        samplers+=U(size);
        std::string stem(name);
        if(size>1&&stem.size()>3&&stem.substr(stem.size()-3)=="[0]")stem.resize(stem.size()-3);
        for(I element=0;element<size;++element){
            const std::string uniform=size==1?std::string(name):stem+"["+std::to_string(element)+"]";
            const I location=g.GetUniformLocation(U(program),uniform.c_str());if(location<0)continue;
            I unit=-1;g.GetUniformiv(U(program),location,&unit);
            if(unit<0||unit>=g.units||unit>=32||!used.insert(U(unit)).second)continue;
            g.ActiveTexture(texture0+U(unit));I texture=0,original=0;
            g.GetIntegerv(0x8069,&texture);g.GetIntegerv(0x8919,&original);
            if(!texture)continue;
            ++bound;if(g.attached(U(texture))){++feedback;continue;}
            const I levels=g.mipLevels();if(levels<2){++noChain;continue;}
            Sampler sampler=g.readSampler(U(original));
            // GLES extension-only border/mirror-clamp state is not part of our
            // descriptor clone. Never silently replace it with default borders.
            const auto basicWrap=[](I wrap){return wrap==0x2901||wrap==0x8370||wrap==0x812f;};
            if(g.es&&(!basicWrap(sampler.s)||!basicWrap(sampler.t)||!basicWrap(sampler.r))){++descriptor;continue;}
            if(g.minmax&&sampler.reduction!=0x9367){++descriptor;continue;}
            TextureQualityProof proof;proof.renderer=g.es?Renderer::OpenGLES:Renderer::OpenGL;
            proof.use=TextureUse::SampledColor;proof.gameSurfaceIdentified=proof.resourceOwnershipProven=true;
            proof.privateReplacementSupported=true;proof.externalOverridesReviewed=reviewed;proof.visibleMipLevels=U(levels);
            GlQualitySampler source;source.minimumFilter=sampler.min;source.comparisonMode=sampler.compare;
            source.minimumLod=sampler.lo;source.maximumLod=sampler.hi;source.lodBias=sampler.bias;source.maximumAnisotropy=sampler.anisotropy;
            const auto plan=planGlTextureQuality(mode,proof,source,g.qualityCapabilities);if(!plan.changed()){++descriptor;continue;}
            sampler.lo=plan.descriptor.minimumLod;sampler.bias=plan.descriptor.lodBias;
            const U replacement=g.privateSampler(sampler);if(!replacement){++replacementFailure;continue;}
            g.changed.push_back({U(unit),U(original)});g.BindSampler(U(unit),replacement);
        }
    }
    if(detail){
        if(policy==TextureDrawPolicy::OwnedMipCompatibility)
            *detail=g.changed.empty()?"Depth-independent Potato inspected this owned game draw but found no eligible mipmapped color sampler; native sampling retained.":"Depth-independent Potato prepared private coarser-mip samplers for this qualified game draw; original bindings restore after this draw. Opaque mipmapped HUD/RGB data maps may also change. Not measured game pixels/FPS.";
        else *detail=g.changed.empty()?"No qualified scene-color mip sampler in this game draw; native sampling retained.":"Potato prepared private coarser-mip samplers for an owned game scene draw; original bindings restore immediately afterward. This is not a measurement of game output or FPS.";
    }
    if(detail&&g.changed.empty()){
        char counts[384]{};
        std::snprintf(counts,sizeof(counts)," sampler2D slots=%u; bound textures=%u; draw-attachment feedback=%u; ineligible mip/color/protection=%u; sampler-policy refusals=%u; private-sampler failures=%u.",samplers,bound,feedback,noChain,descriptor,replacementFailure);
        *detail+=counts;
    }
    if(reason&&!g.changed.empty())*reason=TextureDrawReason::Prepared;
    inspection.keepBindings=true;
    return U(g.changed.size());
}
void GlTextureOverride::end()noexcept{
    // begin() restores ACTIVE_TEXTURE, including on an allocation failure.
    // This changes sampler bindings only, never application texture/program/FBO.
    impl->restoreBindings();
}
void GlTextureOverride::release(){
    auto &g=*impl;end();for(const auto &entry:g.cache)g.DeleteSamplers(1,&entry.second);g.cache.clear();g.ready=false;
}
} // namespace BsiShaders
