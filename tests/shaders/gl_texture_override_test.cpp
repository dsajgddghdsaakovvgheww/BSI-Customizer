// Deterministic public-GL doubles around the ACTUAL production sampler code.
// Tests legal protection queries and every C++ allocation failure, not pixels,
// a real protected driver/context, Windows detours or game-resource semantics.
#include "GlTextureOverride.h"
#include "GlPipeline.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <utility>

#ifdef _WIN32
#define GL_CALL __stdcall
#else
#define GL_CALL
#endif
namespace {
int failAfter=-1;
bool activeChanged=false,privateBound=false,failedAfterUnitChange=false,failedAfterBinding=false;
void *allocate(std::size_t size){
    if(failAfter==0){
        failAfter=-1;failedAfterUnitChange=activeChanged;failedAfterBinding=privateBound;
        throw std::bad_alloc();
    }
    if(failAfter>0)--failAfter;
    if(void *value=std::malloc(size?size:1))return value;
    throw std::bad_alloc();
}
void require(bool value,const char *message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
using U=unsigned;using I=int;using F=float;using B=unsigned char;
struct Sampler {
    I min=0x2703,mag=0x2601,s=0x812f,t=0x812f,r=0x812f,compare=0,function=0x0203;
    F low=-1000,high=1000,bias=0;
    std::array<F,4> border{};
    bool operator==(const Sampler &other)const{
        return min==other.min&&mag==other.mag&&s==other.s&&t==other.t&&r==other.r&&compare==other.compare
            &&function==other.function&&low==other.low&&high==other.high&&bias==other.bias&&border==other.border;
    }
};
struct NamedImage {bool live=false;I target=0x0de1,width=64,height=64,format=0x8058,samples=0,min=0x2601,mag=0x2601,base=0,max=1000,levels=7,immutable=0,storage=7,view=7;};
struct Fake {
    std::array<NamedImage,128> namedTextures{},namedRenderbuffers{};bool missingNamedQueries=false;I namedQueries=0;
    bool desktop=false,missingBiasQuery=false,storageExtension=false,viewExtension=false;
    I desktopVersion=45,maximumTextureSize=16384;
    bool immutable=false;
    I storageLevels=7,viewLevels=7,imageLevels=7,maximumMip=1000,extent=64,mismatchedLevel=-1;
    I immutableQueries=0,storageQueries=0,viewQueries=0,highestLevel=-1;
    F maximumBias=16;I biasLimitQueries=0;
    bool protectionExtension=false,protectedContext=false,protectedTexture=false,incompleteExtensions=false;
    I extensionCount=0,illegalQueries=0,flagsQueries=0,textureProtectionQueries=0;
    I active=3,program=1,depthWrite=1,depthFunction=0x0203,depthBits=24,framebuffer=0;
    I depthWriteQueries=0,depthFunctionQueries=0,depthAttachmentQueries=0,shaderQueries=0;
    bool depth=true,blend=false,mips=true,colorWrite=true,drawBufferNone=false;
    U colorAttachment=99,readColorAttachment=88;
    I readFramebuffer=2,colorType=0x1702,readColorType=0x1702,colorComponent=0x8c17,baseLevel=0;
    bool readBufferNone=false,readComplete=true,drawComplete=true,missingTransferQuery=false;
    I format=0x8058;
    std::array<Sampler,256> samplers{};
    std::array<U,4> bindings{{1,2,0,0}},textures{{1,2,0,0}};
    std::array<bool,256> live{};
    I creations=0,deletions=0;
    void reset(){*this=Fake{};activeChanged=privateBound=false;}
    void refresh(){activeChanged=active!=3;privateBound=bindings[0]>=16||bindings[1]>=16;}
} g;
const unsigned char *GL_CALL getString(U name){
    return reinterpret_cast<const unsigned char*>(name==0x1f02?(g.desktop?(g.desktopVersion==33?"3.3 BSI fixture":g.desktopVersion==42?"4.2 BSI fixture":"4.5 BSI desktop bias fixture"):"OpenGL ES 3.1 BSI protection fixture"):"");
}
const unsigned char *GL_CALL getStringi(U,U index){
    if(g.incompleteExtensions)return nullptr;
    for(const auto &entry:std::array<std::pair<bool,const char*>,3>{{{g.protectionExtension,"GL_EXT_protected_textures"},{g.storageExtension,"GL_ARB_texture_storage"},{g.viewExtension,g.desktop?"GL_ARB_texture_view":"GL_OES_texture_view"}}})
        if(entry.first){if(!index)return reinterpret_cast<const unsigned char*>(entry.second);--index;}
    require(false,"fixture extension count must match its actual list");return nullptr;
}
void GL_CALL getInteger(U name,I *out){
    switch(name){
    case 0x821d:*out=g.extensionCount;break;
    case 0x821e:++g.flagsQueries;if(!g.protectionExtension)++g.illegalQueries;*out=g.protectedContext?0x10:0;break;
    case 0x8b4d:case 0x8cdf:*out=4;break;
    case 0x0d33:*out=g.maximumTextureSize;break;
    case 0x8b8d:++g.shaderQueries;*out=g.program;break;
    case 0x0b74:++g.depthFunctionQueries;*out=g.depthFunction;break;
    case 0x84e0:*out=0x84c0+g.active;break;
    case 0x8069:*out=I(g.textures.at(std::size_t(g.active)));break;
    case 0x8919:*out=I(g.bindings.at(std::size_t(g.active)));break;
    case 0x8ca6:*out=g.framebuffer;break;case 0x8caa:*out=g.readFramebuffer;break;
    case 0x0c02:*out=g.readBufferNone?0:g.readFramebuffer?0x8ce0:0x0405;break;
    case 0x8825:*out=g.drawBufferNone?0:g.framebuffer?0x8ce0:0x0405;break;
    default:require(false,"unexpected integer query in sampler fixture");
    }
}
void GL_CALL getFloat(U name,F *out){
    require(g.desktop&&name==0x84fd,"MAX_TEXTURE_LOD_BIAS is queried only on identified desktop GL");
    ++g.biasLimitQueries;*out=g.maximumBias;
}
void GL_CALL getBoolean(U name,B *out){
    if(name==0x0c23){std::fill_n(out,4,B(g.colorWrite));return;}
    require(name==0x0b72,"only public color/depth write masks are inspected");++g.depthWriteQueries;*out=B(g.depthWrite);
}
B GL_CALL enabled(U name){return B(name==0x0b71?g.depth:name==0x0be2?g.blend:false);}
void GL_CALL active(U unit){g.active=I(unit-0x84c0);g.refresh();}
void GL_CALL program(U,U name,I *out){*out=name==0x8b86?2:128;}
const char *uniforms[]{"first_test_owned_color_texture","second_test_owned_color_texture"};
void GL_CALL uniform(U,U index,I,I *length,I *size,U *type,char *name){
    *length=I(std::strlen(uniforms[index]));*size=1;*type=0x8b5e;std::strcpy(name,uniforms[index]);
}
I GL_CALL location(U,const char *name){return std::strcmp(name,uniforms[0])==0?0:std::strcmp(name,uniforms[1])==0?1:-1;}
void GL_CALL uniformValue(U,I location,I *out){*out=location;}
void GL_CALL level(U,I level,U name,I *out){
    I maximum=0;for(I size=g.maximumTextureSize;size>1;size/=2)++maximum;
    require(level>=0&&level<=maximum,"never query an image above log2(MAX_TEXTURE_SIZE), including an incomplete texture's legal stored BASE_LEVEL");
    g.highestLevel=(std::max)(g.highestLevel,level);
    const bool defined=level<g.imageLevels&&(g.mips||!level);
    *out=!defined?0:name==0x1003?g.format:(std::max)(1,g.extent>>level)+(level==g.mismatchedLevel?1:0);
}
void samplerInteger(const Sampler &s,U name,I *out){
    switch(name){
    case 0x2801:*out=s.min;break;case 0x2800:*out=s.mag;break;
    case 0x2802:*out=s.s;break;case 0x2803:*out=s.t;break;case 0x8072:*out=s.r;break;
    case 0x884c:*out=s.compare;break;case 0x884d:*out=s.function;break;
    default:require(false,"unexpected sampler integer state");
    }
}
void GL_CALL textureInteger(U,U name,I *out){
    if(name==0x8bfa){
        ++g.textureProtectionQueries;if(!g.protectionExtension)++g.illegalQueries;
        *out=g.protectedTexture&&g.textures.at(std::size_t(g.active))==1;return;
    }
    if(name==0x813c){*out=g.baseLevel;return;}if(name==0x813d){*out=g.maximumMip;return;}
    if(name==0x912f){
        require(!g.desktop||g.desktopVersion>=42||g.storageExtension||g.viewExtension,"IMMUTABLE_FORMAT needs core/extension support, never speculative enums on GL3.3");
        ++g.immutableQueries;*out=g.immutable;return;
    }
    if(name==0x82df){
        require(!g.desktop||g.desktopVersion>=43||g.viewExtension,"texture_storage alone does NOT legalize IMMUTABLE_LEVELS on desktop");
        ++g.storageQueries;*out=g.storageLevels;return;
    }
    if(name==0x82dc){
        require((g.desktop&&g.desktopVersion>=43)||g.viewExtension,"VIEW_NUM_LEVELS is not a core GLES3.1 query");
        ++g.viewQueries;*out=g.viewLevels;return;
    }
    samplerInteger(Sampler{},name,out);
}
void GL_CALL samplerInt(U sampler,U name,I *out){samplerInteger(g.samplers.at(sampler),name,out);}
void GL_CALL samplerFloat(U sampler,U name,F *out){
    const auto &s=g.samplers.at(sampler);
    if(name==0x1004){require(g.desktop,"GLES border queries are not legal without a qualified extension");std::copy(s.border.begin(),s.border.end(),out);return;}
    if(name==0x8501){require(g.desktop,"never query desktop sampler bias on GLES");*out=s.bias;return;}
    *out=name==0x813a?s.low:name==0x813b?s.high:1;
}
void GL_CALL textureFloat(U,U name,F *out){samplerFloat(0,name,out);}
U GL_CALL framebufferStatus(U target){
    require(target==0x8ca8||target==0x8ca9,"only legal core READ/DRAW framebuffer completeness queries");
    return (target==0x8ca8?g.readComplete:g.drawComplete)?0x8cd5:0x8cd6;
}
void GL_CALL attachment(U target,U attachment,U name,I *out){
    const bool read=target==0x8ca8;
    if((target==0x8ca8||target==0x8ca9)&&(read?g.readFramebuffer:g.framebuffer)
       &&((attachment>=0x8ce0&&attachment<0x8ce4)||attachment==0x8d20)){
        require(name==0x8cd0||name==0x8cd1||name==0x8211||(name>=0x8212&&name<=0x8214),"public color resource type/name/component metadata only");
        *out=attachment!=0x8ce0?0:name==0x8cd0?(read?g.readColorType:g.colorType):name==0x8cd1?I(read?g.readColorAttachment:g.colorAttachment):name==0x8211?g.colorComponent:8;return;
    }
    if(!(read?g.readFramebuffer:g.framebuffer)&&((g.desktop&&attachment>=0x0400&&attachment<=0x0403)||(!g.desktop&&attachment==0x0405))){
        require(name==0x8211||(name>=0x8212&&name<=0x8214),"default-color transfer queries use legal API-specific attachment metadata");
        *out=name==0x8211?g.colorComponent:8;return;
    }
    require(target==0x8ca9&&attachment==(g.framebuffer?0x8d00u:0x1801u),"use the legal FBO/default depth attachment enum");
    require(name==0x8cd0||name==0x8216,"only public depth type/bit-size queries");
    ++g.depthAttachmentQueries;
    *out=name==0x8cd0?(g.depthBits?(g.framebuffer?0x8d41:0x8218):0):g.depthBits;
}
void GL_CALL gen(I count,U *out){
    for(I i=0;i<count;++i){U name=16;while(name<g.samplers.size()&&g.live[name])++name;require(name<g.samplers.size(),"private sampler fixture capacity");out[i]=name;g.samplers[name]={};g.live[name]=true;++g.creations;}
}
void GL_CALL remove(I count,const U *names){for(I i=0;i<count;++i){require(g.live.at(names[i]),"delete each private sampler exactly once");g.live[names[i]]=false;++g.deletions;}}
void GL_CALL parameterInt(U sampler,U name,I value){
    auto &s=g.samplers.at(sampler);
    switch(name){case 0x2801:s.min=value;break;case 0x2800:s.mag=value;break;case 0x2802:s.s=value;break;
    case 0x2803:s.t=value;break;case 0x8072:s.r=value;break;case 0x884c:s.compare=value;break;case 0x884d:s.function=value;break;
    default:require(false,"unexpected replacement sampler integer parameter");}
}
void GL_CALL parameterFloat(U sampler,U name,F value){
    if(name==0x813a)g.samplers.at(sampler).low=value;else if(name==0x813b)g.samplers.at(sampler).high=value;
    else if(name==0x8501){require(g.desktop,"never write desktop sampler bias on GLES");g.samplers.at(sampler).bias=value;}
    else require(false,"unexpected replacement sampler float parameter");
}
void GL_CALL parameterVector(U sampler,U name,const F *values){
    require(g.desktop&&name==0x1004,"GLES border-clamp state must be bypassed, not cloned incorrectly");
    std::copy(values,values+4,g.samplers.at(sampler).border.begin());
}
void GL_CALL bind(U unit,U sampler){g.bindings.at(unit)=sampler;g.refresh();}
B GL_CALL isTexture(U name){++g.namedQueries;return name<g.namedTextures.size()&&g.namedTextures[name].live;}
B GL_CALL isRenderbuffer(U name){++g.namedQueries;return name<g.namedRenderbuffers.size()&&g.namedRenderbuffers[name].live;}
void GL_CALL namedTexture(U name,U key,I *out){
    ++g.namedQueries;require(g.desktop&&g.desktopVersion>=45,"DSA queries must be capability-gated");const auto &v=g.namedTextures.at(name);require(v.live,"no DSA query on an uncreated name");
    switch(key){case 0x1006:*out=v.target;break;case 0x2801:*out=v.min;break;case 0x2800:*out=v.mag;break;
    case 0x813c:*out=v.base;break;case 0x813d:*out=v.max;break;case 0x912f:*out=v.immutable;break;
    case 0x82df:*out=v.storage;break;case 0x82dc:*out=v.view;break;default:require(false,"legal DSA texture parameter only");}
}
void GL_CALL namedLevel(U name,I mip,U key,I *out){
    ++g.namedQueries;const auto &v=g.namedTextures.at(name);require(v.live&&mip>=0&&mip<16,"legal existing texture level query");
    *out=mip>=v.levels?0:key==0x1000?(std::max)(1,v.width>>mip):key==0x1001?(std::max)(1,v.height>>mip):key==0x1003?v.format:0;
    require(key==0x1000||key==0x1001||key==0x1003,"only legal 2D image size/internal format queries");
}
void GL_CALL namedRenderbuffer(U name,U key,I *out){
    ++g.namedQueries;const auto &v=g.namedRenderbuffers.at(name);require(v.live,"no RBO query on uncreated name");
    switch(key){case 0x8d42:*out=v.width;break;case 0x8d43:*out=v.height;break;case 0x8d44:*out=v.format;break;case 0x8cab:*out=v.samples;break;default:require(false,"legal named RBO parameter only");}
}
void GL_CALL unexpected(){require(false,"protected GL pipeline must stop before allocating/compiling/drawing");}
void *resolve(const char *name,void*){
    if(g.missingBiasQuery&&std::strcmp(name,"glGetFloatv")==0)return nullptr;
    if(g.missingTransferQuery&&std::strcmp(name,"glCheckFramebufferStatus")==0)return nullptr;
    if(g.missingNamedQueries&&(std::strcmp(name,"glGetTextureParameteriv")==0||std::strcmp(name,"glGetNamedRenderbufferParameteriv")==0))return nullptr;
#define ENTRY(gl,fn) if(std::strcmp(name,"gl" #gl)==0)return reinterpret_cast<void*>(&fn)
    ENTRY(IsTexture,isTexture);ENTRY(IsRenderbuffer,isRenderbuffer);ENTRY(GetTextureParameteriv,namedTexture);ENTRY(GetTextureLevelParameteriv,namedLevel);ENTRY(GetNamedRenderbufferParameteriv,namedRenderbuffer);
    ENTRY(GetString,getString);ENTRY(GetStringi,getStringi);ENTRY(GetIntegerv,getInteger);ENTRY(GetFloatv,getFloat);ENTRY(GetBooleanv,getBoolean);
    ENTRY(IsEnabled,enabled);ENTRY(ActiveTexture,active);ENTRY(GetProgramiv,program);ENTRY(GetActiveUniform,uniform);
    ENTRY(GetUniformLocation,location);ENTRY(GetUniformiv,uniformValue);ENTRY(GetTexLevelParameteriv,level);
    ENTRY(GetTexParameteriv,textureInteger);ENTRY(GetTexParameterfv,textureFloat);ENTRY(GetSamplerParameteriv,samplerInt);
    ENTRY(GetSamplerParameterfv,samplerFloat);ENTRY(GetFramebufferAttachmentParameteriv,attachment);ENTRY(CheckFramebufferStatus,framebufferStatus);ENTRY(GenSamplers,gen);
    ENTRY(DeleteSamplers,remove);ENTRY(SamplerParameteri,parameterInt);ENTRY(SamplerParameterf,parameterFloat);
    ENTRY(SamplerParameterfv,parameterVector);ENTRY(BindSampler,bind);
#undef ENTRY
    // GlPipeline loads its complete API table before inspecting protection.
    // None of those other entry points may actually execute in rejection tests.
    return reinterpret_cast<void*>(&unexpected);
}
void unchanged(){require(g.active==3&&g.bindings==std::array<U,4>{{1,2,0,0}},"restore ACTIVE_TEXTURE and every original sampler BEFORE returning/throwing");}
}
void *operator new(std::size_t size){return allocate(size);}
void *operator new[](std::size_t size){return allocate(size);}
void operator delete(void *ptr)noexcept{std::free(ptr);}
void operator delete[](void *ptr)noexcept{std::free(ptr);}
#if defined(__cpp_sized_deallocation)
void operator delete(void *ptr,std::size_t)noexcept{std::free(ptr);}
void operator delete[](void *ptr,std::size_t)noexcept{std::free(ptr);}
#endif
int main(){
    using namespace BsiShaders;
    {
        g.reset();g.desktop=true;g.namedTextures[50].live=g.namedTextures[51].live=g.namedRenderbuffers[70].live=true;
        GlTextureOverride quality;std::string error;GlTextureUsage usage;require(quality.initialize(resolve,nullptr,&error),"initialize named-copy observer");
        auto copy=GlColorTransfer::image(70,0x8d41,0,0,0,0,50,0x0de1,0,0,0,0,64,64,1);
        require(quality.transfer(copy,&usage)&&usage.readCount==1&&usage.writeCount==1&&usage.reads[0]==GlColorResource::renderbuffer(70)&&usage.writes[0]==50,
                "a valid named RBO-to-texture CopyImageSubData must capture typed resources without depending on current FBO/bindings");
        unchanged();require(!g.creations&&!g.depthWriteQueries&&!g.depthAttachmentQueries,"named-copy capture changes no binding/sampler/depth state");
        copy.sourceTarget=0x0de1;copy.sourceName=51;require(quality.transfer(copy,&usage),"distinct defined same-format normalized 2D textures can copy");
        for(unsigned mutation=0;mutation<12;++mutation){
            auto bad=copy;
            switch(mutation){case 0:bad.sourceName=0;break;case 1:bad.sourceTarget=0x8513;break;case 2:bad.sourceLevel=1;break;case 3:bad.destinationLevel=1;break;
            case 4:bad.sourceZ=1;break;case 5:bad.depth=2;break;case 6:bad.source[0]=-1;break;case 7:bad.source[2]=65;break;case 8:bad.destination[0]=1;break;
            case 9:bad.sourceName=50;break;case 10:bad.destinationName=127;break;case 11:bad.source[3]=0;break;}
            require(!quality.transfer(bad,&usage)&&!usage.readCount&&!usage.writeCount,"invalid/unqualified named-copy packet cannot become a dependency");
        }
        auto unsupportedApi=copy;unsupportedApi.imageApis=2;const auto noApiQueries=g.namedQueries;
        require(!quality.transfer(unsupportedApi,&usage)&&g.namedQueries==noApiQueries,"a non-null EXT stub is not advertised EXT copy-image support");
        auto &src=g.namedTextures[51];src.target=0x8513;require(!quality.transfer(copy,&usage),"native texture target must match the supplied target, not a same-sized cubemap");src.target=0x0de1;
        src.format=0x8c43;require(!quality.transfer(copy,&usage),"view-compatible or sRGB reinterpretation is not guessed from byte size");src.format=0x8058;
        src.min=0x2703;src.levels=1;require(!quality.transfer(copy,&usage),"a mip-filtered incomplete source is not a successful image copy");
        src.immutable=1;src.storage=src.view=src.levels=4;require(quality.transfer(copy,&usage),"actual immutable storage bounds qualify a complete four-level source with stored MAX=1000");
        src.levels=3;require(!quality.transfer(copy,&usage),"a missing required immutable image still rejects the copy");src.levels=4;
        copy.sourceName=70;copy.sourceTarget=0x8d41;g.namedRenderbuffers[70].samples=4;require(!quality.transfer(copy,&usage),"CopyImage is NOT a multisample resolve into a single-sample texture");
        quality.release();
    }
    for(unsigned guard=0;guard<4;++guard){
        g.reset();g.desktop=guard!=0;g.desktopVersion=guard==1?33:45;g.missingNamedQueries=guard==2;
        if(guard==3){g.protectionExtension=true;g.extensionCount=1;}
        GlTextureOverride quality;std::string error;GlTextureUsage usage;require(quality.initialize(resolve,nullptr,&error),"older/incomplete-query contexts retain normal draw support");
        require(!quality.transfer(GlColorTransfer::image(1,0x0de1,0,0,0,0,2,0x0de1,0,0,0,0,1,1,1),&usage)&&!g.namedQueries,
                "GLES/older desktop/missing DSA/protection-possible contracts must reject named copies BEFORE any object query");quality.release();
    }
    {
        g.reset();g.depth=false;g.mips=false;GlTextureOverride quality;std::string error;GlTextureUsage usage;
        require(!quality.usage(&usage),"an uninitialized inspector cannot capture resource dependencies");
        require(quality.initialize(resolve,nullptr,&error)&&quality.usage(&usage),"read-only capture accepts a single-level compositor source without requiring scene depth/mips");
        require(usage.defaultFramebuffer&&usage.readCount==2&&usage.reads[0]==1&&usage.reads[1]==2&&!usage.writeCount,
                "capture only active sampler2D inputs and the actual default/FBO output kind");
        unchanged();require(!g.creations&&!g.depthWriteQueries&&!g.depthFunctionQueries&&!g.depthAttachmentQueries,
                "dependency observation neither creates/binds samplers nor invents/query scene depth");
        TextureDrawReason reason;
        require(!quality.begin(TextureQuality::Strong,true,4,&error,&reason,nullptr,TextureDrawPolicy::OwnedMipCompatibility)
                &&reason==TextureDrawReason::NoSampler&&error.find("bound textures=2")!=std::string::npos
                &&error.find("ineligible mip/color/protection=2")!=std::string::npos,
                "single-level capture is NOT permission to reduce it; explain the real no-mip/color bucket");
        unchanged();g.framebuffer=1;
        require(quality.usage(&usage)&&!usage.defaultFramebuffer&&usage.writeCount==1&&usage.writes[0]==99&&usage.readCount==2,
                "capture the actual COLOR_ATTACHMENT selected by draw buffer zero, not an arbitrary texture in the context");
        g.colorAttachment=1;
        require(quality.usage(&usage)&&usage.readCount==1&&usage.reads[0]==2,"attached feedback textures are not presented input evidence");
        g.colorWrite=false;require(!quality.usage(&usage),"a zero-color-mask draw cannot seed a color dependency");
        g.colorWrite=true;g.drawBufferNone=true;require(!quality.usage(&usage),"DRAW_BUFFER0 NONE cannot authorize a producer");
        g.drawBufferNone=false;g.blend=true;require(!quality.usage(&usage),"blended UI/transparent draws do not seed producer evidence");
        unchanged();quality.release();
    }
    for(bool desktop:{false,true}){
        g.reset();g.desktop=desktop;GlTextureOverride quality;std::string error;GlTextureBinding binding;
        require(!quality.textureBinding(&binding),"native binding cannot inspect an uninitialized GL provider");
        require(quality.initialize(resolve,nullptr,&error),"initialize native render-to-texture metadata capture");
        g.textures[3]=3;g.imageLevels=0;
        require(quality.textureBinding(&binding)&&binding.unit==3&&binding.texture==3&&!quality.textureBinding(&binding,true),
                "capture the real unit/name before native definition; undefined images cannot be accepted after native success");
        g.imageLevels=1;g.mips=false;g.blend=true;g.colorWrite=false;
        require(quality.textureBinding(&binding,true)&&binding.texture==3,"native texture definition is independent of draw blend/depth/write masks and does NOT need mips");
        failAfter=0;const bool allocationFree=quality.textureBinding(&binding,true);failAfter=-1;
        require(allocationFree&&!g.creations&&!g.deletions,"native metadata capture is allocation-free and never creates a GL object");
        g.baseLevel=1;require(!quality.textureBinding(&binding,true),"only an exact level-zero 2D native binding is supported");g.baseLevel=0;
        g.format=0x81a6;require(!quality.textureBinding(&binding,true),"depth/integer/HDR/noncolor native targets remain unsupported");
        g.format=0x8058;g.textures[3]=0;require(!quality.textureBinding(&binding),"default texture zero never becomes a native image dependency");
        quality.release();require(g.active==3&&g.bindings==std::array<U,4>{{1,2,0,0}},"no sampler/unit/binding change for any native metadata refusal");
    }
    for(I filter:{0x2600,0x2601}){
        g.reset();g.mips=false;g.samplers[1].min=g.samplers[2].min=filter;GlTextureOverride quality;std::string detail;
        require(quality.initialize(resolve,nullptr,&detail)&&!quality.begin(TextureQuality::Strong,true,4,&detail),"valid base-only sources stay native");
        require(detail.find("non-mip-filter=2")!=std::string::npos&&detail.find("incomplete-required-level=0")!=std::string::npos
                &&detail.find("missing higher mips are legal")!=std::string::npos&&g.highestLevel==0,
                "LINEAR/NEAREST does not require mip 1: report the effective SAMPLER filter, not an invented incomplete texture");
        unchanged();quality.release();
    }
    {
        g.reset();g.framebuffer=1;g.textures[3]=3;GlTextureOverride quality;std::string error;GlTextureUsage usage;
        require(quality.initialize(resolve,nullptr,&error),"initialize read-only color-transfer inspection");
        const auto blit=GlColorTransfer::blit(1,2,61,62,3,4,59,60,0x4000,0x2600);
        require(quality.transfer(blit,&usage)&&!usage.defaultFramebuffer&&usage.readCount==1&&usage.writeCount==1
                &&usage.reads[0]==88&&usage.writes[0]==99,"observe the actual READ_BUFFER -> DRAW_BUFFER0 color resources without a shader/depth predicate");
        g.colorWrite=false;g.blend=true;g.program=0;
        failAfter=0;const bool allocationFree=quality.transfer(blit,&usage);failAfter=-1;
        require(allocationFree,"copy/resolve evidence needs no allocation and ignores draw-only blend/color-mask/program state");
        g.readColorType=0x8d41;g.readColorAttachment=99;
        require(quality.transfer(blit,&usage)&&usage.reads[0]==GlColorResource::renderbuffer(99)&&usage.writes[0]==99,
                "equal numeric texture/renderbuffer names are DIFFERENT typed color resources");
        g.colorType=0x8d41;require(!quality.transfer(blit,&usage),"same actual typed read/write resource cannot self-authorize through a blit");
        g.colorType=0x1702;g.readColorType=0x1702;g.readColorAttachment=88;
        g.readComplete=false;require(!quality.transfer(blit,&usage),"incomplete read framebuffer is not transfer evidence");g.readComplete=true;
        g.drawComplete=false;require(!quality.transfer(blit,&usage),"incomplete draw framebuffer is not transfer evidence");g.drawComplete=true;
        g.readBufferNone=true;require(!quality.transfer(blit,&usage),"READ_BUFFER NONE is not a color source");g.readBufferNone=false;
        g.drawBufferNone=true;require(!quality.transfer(blit,&usage),"DRAW_BUFFER0 NONE is not a color destination");g.drawBufferNone=false;
        g.colorComponent=0x1406;require(!quality.transfer(blit,&usage),"HDR/float/integer attachment components are not qualified by color-copy capture");g.colorComponent=0x8c17;
        auto invalid=blit;invalid.mask|=0x0100;require(!quality.transfer(invalid,&usage),"mixed depth/stencil transfers stay native and untracked");
        invalid=blit;invalid.filter=0;require(!quality.transfer(invalid,&usage),"invalid blit filter is not evidence");
        invalid=blit;invalid.source[2]=invalid.source[0];require(!quality.transfer(invalid,&usage),"zero-area blit cannot propagate ownership");
        g.framebuffer=0;
        require(quality.transfer(blit,&usage)&&usage.defaultFramebuffer&&usage.writes[0].kind==GlColorResource::Pbuffer&&!usage.writes[0]
                &&usage.writes[0].colorBuffer==0x0405&&usage.nativeColorBuffer==0x0405,
                "native default DRAW blit must preserve its exact color aspect for native caller qualification");
        g.readFramebuffer=0;
        const auto copy=GlColorTransfer::copy(0x0de1,0,0,0,0,0,64,64);
        require(quality.transfer(copy,&usage)&&!usage.defaultFramebuffer&&usage.reads[0].kind==GlColorResource::Pbuffer
                &&!usage.reads[0]&&usage.writes[0]==3,"a default READ framebuffer copy leaves a zero-name native-pbuffer placeholder for the caller");
        g.baseLevel=1;require(!quality.transfer(copy,&usage),"nonzero destination base level is not the captured level-zero transfer path");g.baseLevel=0;
        for(auto bad:{GlColorTransfer::copy(0x8515,0,0,0,0,0,64,64),GlColorTransfer::copy(0x0de1,1,0,0,0,0,64,64),
                      GlColorTransfer::copy(0x0de1,0,1,0,0,0,64,64),GlColorTransfer::copy(0x0de1,0,0,0,-1,0,64,64),
                      GlColorTransfer::copy(0x0de1,0,0,0,0,0,0,64),GlColorTransfer::copy(0x0de1,0,0,0,0,0,std::numeric_limits<int>::max(),64)})
            require(!quality.transfer(bad,&usage)&&!usage.readCount&&!usage.writeCount,"invalid/unsupported copy arguments leave no partial proof and do not overflow");
        g.readFramebuffer=2;g.readColorAttachment=3;require(!quality.transfer(copy,&usage),"copy texture feedback is not a dependency");
        unchanged();require(!g.creations&&!g.deletions,"read-only transfers never allocate/bind/delete any GL object");quality.release();
    }
    {
        g.reset();g.protectionExtension=true;g.extensionCount=1;g.readFramebuffer=0;g.textures[3]=1;
        GlTextureOverride quality;std::string error;GlTextureUsage usage;
        require(quality.initialize(resolve,nullptr,&error),"initialize protected-resource-aware copy fixture");
        const auto copy=GlColorTransfer::copy(0x0de1,0,0,0,0,0,64,64);
        require(quality.transfer(copy,&usage),"an unprotected bound copy destination can be queried legally even when protected textures are supported");
        g.protectedTexture=true;require(!quality.transfer(copy,&usage)&&!g.illegalQueries,"a protected copy destination never becomes resource proof");
        g.protectedTexture=false;g.readFramebuffer=2;
        require(!quality.transfer(copy,&usage),"unqueryable framebuffer attachment protection fails closed, not inferred from a numeric name");
        unchanged();quality.release();
    }
    {
        g.reset();g.missingTransferQuery=true;GlTextureOverride quality;std::string error;GlTextureUsage usage;
        require(quality.initialize(resolve,nullptr,&error)&&!quality.transfer(GlColorTransfer::copy(0x0de1,0,0,0,0,0,64,64),&usage),
                "missing optional completeness inspection disables only transfer capture");
        require(quality.begin(TextureQuality::Strong,true,4,&error)==2,"older direct draw capture still works without optional transfer support");
        quality.end();unchanged();quality.release();
    }
    {
        g.reset();g.protectionExtension=true;g.extensionCount=1;g.protectedTexture=true;
        GlTextureOverride quality;std::string error;GlTextureUsage usage;
        require(quality.initialize(resolve,nullptr,&error)&&quality.usage(&usage)&&usage.readCount==1&&usage.reads[0]==2
                &&g.textureProtectionQueries==2&&!g.illegalQueries,"protected textures never become dependency roots, even in a mixed unprotected context");
        unchanged();quality.release();
    }
    {
        g.reset();GlTextureOverride quality;std::string error;GlTextureUsage usage;
        require(quality.initialize(resolve,nullptr,&error),"initialize read-only allocation rollback fixture");
        bool success=false;unsigned failures=0;
        for(int failure=0;failure<64;++failure){
            failAfter=failure;bool threw=false;
            try{success=quality.usage(&usage);}catch(const std::bad_alloc&){threw=true;}
            failAfter=-1;unchanged();require(!g.creations&&!g.deletions,"usage exception cannot allocate a GL object or modify a source sampler");
            if(threw)++failures;else break;
        }
        require(success&&failures>=4,"inject every read-only uniform-name allocation, including after changing the active unit");
        quality.release();
    }
    {
        g.reset();GlTextureOverride quality;std::string error;
        require(quality.initialize(resolve,nullptr,&error),"unprotected GLES with no protected-texture extension is supported without illegal enum queries");
        TextureDrawReason reason=TextureDrawReason::NotInspected;
        require(quality.begin(TextureQuality::Strong,true,4,&error,&reason)==2&&reason==TextureDrawReason::Prepared,"two ordinary test-owned mipmapped colors qualify with an explicit prepared outcome");
        require(g.active==3&&g.bindings[0]>=16&&g.bindings[1]>=16,"private bindings persist only until end, active unit restores before draw");
        require(g.flagsQueries==0&&g.textureProtectionQueries==0&&g.illegalQueries==0&&g.biasLimitQueries==0,
                "GLES never queries desktop bias or absent extension enums");
        quality.end();unchanged();quality.release();require(g.creations==g.deletions,"release all owned sampler names");
    }
    for(bool desktop:{false,true})for(I count:{2,4}){
        g.reset();g.desktop=desktop;g.immutable=true;g.storageLevels=g.viewLevels=g.imageLevels=count;
        GlTextureOverride quality;std::string error;
        require(quality.initialize(resolve,nullptr,&error)&&quality.begin(TextureQuality::Strong,true,4,&error)==2,
                "VALID truncated immutable mip chains qualify with the original MAX_LEVEL=1000; no demand for unallocated 1x1 tail");
        const auto floor=F((std::min)(3,count-1));
        require(g.samplers[g.bindings[0]].low==floor&&g.samplers[g.bindings[0]].bias==(desktop?floor:0)
                &&g.highestLevel==count-1&&g.maximumMip==1000&&g.storageQueries==2,
                "private LODs clamp to actual immutable storage and never query outside it or edit MAX_LEVEL");
        quality.end();unchanged();quality.release();
    }
    for(bool desktop:{false,true}){
        g.reset();g.desktop=desktop;g.maximumMip=std::numeric_limits<I>::max();
        GlTextureOverride quality;std::string error;
        require(quality.initialize(resolve,nullptr,&error)&&quality.begin(TextureQuality::Strong,true,4,&error)==2&&g.highestLevel==6,
                "a legal large stored MAX_LEVEL is not itself a bad chain; bounded actual image dimensions still terminate validation");
        quality.end();unchanged();quality.release();
    }
    {
        g.reset();g.maximumTextureSize=0;GlTextureOverride quality;std::string error;
        require(!quality.initialize(resolve,nullptr,&error)&&g.highestLevel==-1&&!g.creations,
                "an unavailable image-query limit fails before inspection instead of guessing a safe level bound");
        unchanged();quality.release();
    }
    for(bool desktop:{false,true}){
        g.reset();g.desktop=desktop;g.immutable=true;g.storageLevels=7;g.viewLevels=g.imageLevels=3;
        g.viewExtension=true;g.extensionCount=1;
        GlTextureOverride quality;std::string error;
        require(quality.initialize(resolve,nullptr,&error)&&quality.begin(TextureQuality::Strong,true,4,&error)==2
                &&g.samplers[g.bindings[0]].low==2&&g.highestLevel==2&&g.viewQueries==2,
                "a texture VIEW's exposed levels, not the parent's inherited IMMUTABLE_LEVELS, limit both validation and private LOD");
        quality.end();unchanged();quality.release();
    }
    for(I version:{33,42}){
        g.reset();g.desktop=true;g.desktopVersion=version;g.immutable=true;g.storageLevels=g.viewLevels=g.imageLevels=4;
        g.storageExtension=version==33;g.extensionCount=g.storageExtension?1:0;
        GlTextureOverride quality;std::string error;
        require(quality.initialize(resolve,nullptr,&error)&&quality.begin(TextureQuality::Strong,true,4,&error)==2
                &&g.immutableQueries==2&&!g.storageQueries&&!g.viewQueries&&g.highestLevel==4,
                "GL4.2/ARB_texture_storage-only uses its immutable defined prefix, never the illegal texture_view-only IMMUTABLE_LEVELS enum");
        quality.end();unchanged();quality.release();
    }
    {
        g.reset();g.desktop=true;g.desktopVersion=33;GlTextureOverride quality;std::string error;
        require(quality.initialize(resolve,nullptr,&error)&&quality.begin(TextureQuality::Strong,true,4,&error)==2
                &&!g.immutableQueries&&!g.storageQueries&&!g.viewQueries,
                "plain GL3.3 mutable chains still work without querying ANY absent immutable-storage enum");
        quality.end();unchanged();quality.release();
    }
    for(bool immutable:{false,true}){
        g.reset();g.immutable=immutable;g.storageLevels=g.viewLevels=g.imageLevels=4;
        g.baseLevel=2;g.extent=256;
        GlTextureOverride quality;std::string error;
        require(quality.initialize(resolve,nullptr,&error),"initialize a nonzero-base chain");
        if(immutable){
            require(quality.begin(TextureQuality::Strong,true,4,&error)==2&&g.samplers[g.bindings[0]].low==1&&g.highestLevel==3,
                    "immutable effective base is relative to storage; only the TWO exposed levels authorize floor1");
        }else{
            require(!quality.begin(TextureQuality::Strong,true,4,&error)&&error.find("incomplete-required-level=2")!=std::string::npos,
                    "an incomplete MUTABLE tail is still rejected, not reinterpreted as immutable storage");
            g.maximumMip=3;require(quality.begin(TextureQuality::Strong,true,4,&error)==2&&g.samplers[g.bindings[0]].low==1,
                    "a mutable chain explicitly bounded by MAX_LEVEL remains valid");
        }
        quality.end();unchanged();quality.release();
    }
    for(bool immutable:{false,true}){
        g.reset();g.immutable=immutable;g.storageLevels=g.viewLevels=g.imageLevels=4;
        g.baseLevel=std::numeric_limits<I>::max();g.extent=512;
        GlTextureOverride quality;std::string error;GlTextureUsage usage;
        require(quality.initialize(resolve,nullptr,&error),"initialize legal stored but non-queryable base level");
        require(quality.usage(&usage)&&!usage.readCount&&g.highestLevel==-1,"dependency observation never queries a non-queryable stored base image");
        require(!quality.begin(TextureQuality::Strong,true,4,&error),"a single/incomplete effective base cannot be reduced");
        require(immutable?(g.highestLevel==3&&error.find("single-level=2")!=std::string::npos)
                         :(g.highestLevel==-1&&error.find("range=2")!=std::string::npos),
                "immutable base clamps before querying; mutable out-of-limit base fails before injecting a GL error");
        unchanged();quality.release();
    }
    for(I invalid:{0,1,2,3}){
        g.reset();g.desktop=true;g.immutable=true;
        if(invalid==0)g.storageLevels=0;
        if(invalid==1)g.storageLevels=16;
        if(invalid==2)g.viewLevels=0;
        if(invalid==3)g.viewLevels=8;
        GlTextureOverride quality;std::string error;
        require(quality.initialize(resolve,nullptr,&error)&&!quality.begin(TextureQuality::Strong,true,4,&error)
                &&!g.creations&&g.highestLevel==-1&&error.find("storage-metadata=2")!=std::string::npos,
                "unqualified immutable/view metadata fails before image queries, never defaults to mutable or parent storage");
        unchanged();quality.release();
    }
    {
        g.reset();g.mismatchedLevel=3;GlTextureOverride quality;std::string error;
        require(quality.initialize(resolve,nullptr,&error)&&!quality.begin(TextureQuality::Strong,true,4,&error)
                &&error.find("incomplete-required-level=2")!=std::string::npos&&error.find("failed-level=3")!=std::string::npos,
                "mismatched mutable level dimensions retain an exact first blocker, distinct from protection and unsupported formats");
        g.mismatchedLevel=-1;g.format=0x81a6;
        require(!quality.begin(TextureQuality::Strong,true,4,&error)&&error.find("format=2")!=std::string::npos,
                "a depth format is not misreported as evidence of absent mipmaps");
        unchanged();quality.release();
    }
    for(bool desktop:{false,true}){
        g.reset();g.desktop=desktop;g.depth=false;
        GlTextureOverride quality;std::string error;TextureDrawReason reason;
        const auto original=g.samplers[1];require(quality.initialize(resolve,nullptr,&error),"initialize strength sweep");
        for(unsigned cycle=0;cycle<3;++cycle)for(int percent=0;percent<=100;++percent){
            const auto count=quality.begin(TextureQuality::Strong,true,4,&error,&reason,nullptr,TextureDrawPolicy::OwnedMipCompatibility,percent);
            require(count==(percent?2u:0u),"slider percentage reaches actual production begin, not a cosmetic property");
            if(percent){
                const auto &replacement=g.samplers[g.bindings[0]];const float wanted=3.f*float(percent)/100.f;
                require(replacement.low==wanted&&replacement.bias==(desktop?wanted:0.f),"actual bound sampler has requested percentage and API-legal bias");
                require(g.creations-g.deletions<=2,"obsolete strength variants must not exhaust the 128-entry private cache after repeated saves");
            }else require(reason==TextureDrawReason::NotInspected,"zero strength does no sampler inspection");
            quality.end();unchanged();require(g.samplers[1]==original,"every slider step preserves source sampler state and exact bindings");
        }
        const auto queries=g.shaderQueries;
        for(int invalid:{-1,101})require(!quality.begin(TextureQuality::Strong,true,4,&error,&reason,nullptr,TextureDrawPolicy::OwnedMipCompatibility,invalid)
                &&reason==TextureDrawReason::Policy&&g.shaderQueries==queries,"invalid strength fails before GL shader/texture inspection");
        quality.release();require(g.creations==g.deletions,"strength changes release every private sampler without leaks");
    }
    for(float limit:{1.f,16.f}){
        g.reset();g.desktop=true;g.maximumBias=limit;g.samplers[1].border={{.125f,.25f,.5f,1.f}};
        const auto first=g.samplers[1],second=g.samplers[2];
        GlTextureOverride quality;std::string error;
        require(quality.initialize(resolve,nullptr,&error)&&g.biasLimitQueries==1,"desktop bias support is queried once per context initialization");
        for(const auto mode:{TextureQuality::Light,TextureQuality::Strong}){
            const auto floor=mode==TextureQuality::Light?1.5f:3.f;
            auto wanted=first;wanted.low=floor;wanted.bias=(std::min)(floor,limit);
            require(quality.begin(mode,true,4,&error)==2&&g.samplers[g.bindings[0]]==wanted,
                    "actual production desktop replacement has bounded additive bias AND floor, with all source border/wrap/filter state cloned");
            quality.end();unchanged();
            require(g.samplers[1]==first&&g.samplers[2]==second,"production desktop substitution never edits source sampler descriptors");
            require(!quality.begin(TextureQuality::Native,true,4,&error),"desktop Off creates/binds no replacement");
            unchanged();
        }
        quality.release();require(g.creations==g.deletions,"desktop private bias samplers are released without leaks");
    }
    for(float invalid:{0.f,-1.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        g.reset();g.desktop=true;g.maximumBias=invalid;GlTextureOverride quality;std::string error;
        require(!quality.initialize(resolve,nullptr,&error)&&g.biasLimitQueries==1&&!g.creations,
                "unknown/nonfinite/nonpositive desktop bias limits fail closed before sampler creation");
        unchanged();
    }
    {
        g.reset();g.desktop=true;g.missingBiasQuery=true;GlTextureOverride quality;std::string error;
        require(!quality.initialize(resolve,nullptr,&error)&&!g.biasLimitQueries&&!g.creations,
                "missing desktop capability query is not permission to assume bias support");
        unchanged();
    }
    {
        g.reset();g.desktop=true;g.depth=false;GlTextureOverride quality;std::string error;
        TextureDrawReason reason=TextureDrawReason::NotInspected;TextureDepthFailure depth=TextureDepthFailure::None;
        require(quality.initialize(resolve,nullptr,&error)&&!quality.begin(TextureQuality::Strong,true,4,&error,&reason,&depth)
                &&reason==TextureDrawReason::Depth&&depth==TextureDepthFailure::TestDisabled&&!g.shaderQueries&&!g.creations,
                "depth-qualified control still refuses before sampler inspection; desktop bias support alone is not a new draw policy");
        unchanged();quality.release();
    }
    for(bool desktop:{false,true}){
        g.reset();g.desktop=desktop;g.depth=false;g.depthWrite=0;g.depthFunction=0x0207;g.depthBits=0;
        GlTextureOverride quality;std::string error;TextureDrawReason reason;TextureDepthFailure depth;
        const auto first=g.samplers[1],second=g.samplers[2];
        require(quality.initialize(resolve,nullptr,&error),"initialize explicit owned-mip compatibility");
        require(!quality.begin(TextureQuality::Strong,true,4,&error,&reason,&depth)
                &&reason==TextureDrawReason::Depth&&depth==TextureDepthFailure::TestDisabled,
                "legacy depth-qualified default remains a separately tested refusal");
        require(quality.begin(TextureQuality::Strong,true,4,&error,&reason,&depth,TextureDrawPolicy::OwnedMipCompatibility)==2
                &&reason==TextureDrawReason::Prepared&&depth==TextureDepthFailure::None&&g.shaderQueries==1,
                "actual production compatibility inspects and replaces known color mips with depth test/write/attachment ALL absent");
        require(g.depthWriteQueries==0&&g.depthFunctionQueries==0&&g.depthAttachmentQueries==0,
                "compatibility does not use depth state as a hidden second gate");
        require(g.samplers[g.bindings[0]].low==3&&g.samplers[g.bindings[0]].bias==(desktop?3.f:0.f),
                "depth-independent private sampling uses desktop bias+floor or legal GLES floor");
        quality.end();unchanged();require(g.samplers[1]==first&&g.samplers[2]==second,"compatibility restores Off without editing source objects");
        const auto inspected=g.shaderQueries;
        require(!quality.begin(TextureQuality::Strong,true,4,&error,&reason,&depth,static_cast<TextureDrawPolicy>(999))
                &&reason==TextureDrawReason::Policy&&g.shaderQueries==inspected,"unknown policy cannot authorize sampler inspection");
        quality.release();require(g.creations==g.deletions,"compatibility private samplers have explicit-release cleanup");
    }
    for(unsigned refusal=0;refusal<6;++refusal){
        g.reset();g.depth=false;
        if(refusal==0)g.blend=true;
        if(refusal==1)g.mips=false;
        if(refusal==2)g.format=0x81a6; // depth
        if(refusal==3)g.format=0x8d7c; // integer RGBA
        if(refusal==4)g.samplers[1].low=g.samplers[2].low=.5f;
        GlTextureOverride quality;std::string error;
        require(quality.initialize(resolve,nullptr,&error)
                &&!quality.begin(TextureQuality::Strong,refusal!=5,4,&error,nullptr,nullptr,TextureDrawPolicy::OwnedMipCompatibility)
                &&!g.creations,"compatibility still refuses blend/no-mip/depth-data/integer/existing-override/unreviewed resources");
        unchanged();quality.release();
    }
    {
        g.reset();g.depth=false;g.protectionExtension=true;g.extensionCount=1;g.protectedTexture=true;
        GlTextureOverride quality;std::string error;
        require(quality.initialize(resolve,nullptr,&error)
                &&quality.begin(TextureQuality::Strong,true,4,&error,nullptr,nullptr,TextureDrawPolicy::OwnedMipCompatibility)==1
                &&g.bindings[0]==1&&g.textureProtectionQueries==2,
                "depth-independent compatibility still leaves a protected texture native in a mixed unprotected context");
        quality.end();unchanged();quality.release();
    }
    {
        g.reset();g.desktop=true;g.samplers[1].bias=.5f;const auto original=g.samplers[1];
        GlTextureOverride quality;std::string error;
        require(quality.initialize(resolve,nullptr,&error)&&quality.begin(TextureQuality::Strong,true,4,&error)==1
                &&g.bindings[0]==1&&g.samplers[1]==original,
                "production desktop path preserves an existing application bias instead of stacking on it");
        quality.end();unchanged();quality.release();
    }
    {
        g.reset();GlTextureOverride quality;std::string error;
        require(!quality.initialize(resolve,nullptr,&error,true),
                "EGL protected resources possible without a GL per-texture query is UNKNOWN, never permission to modify them");
        unchanged();require(g.creations==0&&g.illegalQueries==0,"unknown protection fails closed without illegal extension queries");
    }
    {
        g.reset();g.protectionExtension=true;g.extensionCount=1;g.protectedTexture=true;
        GlTextureOverride quality;std::string error;require(quality.initialize(resolve,nullptr,&error),"regular protected-capable context can contain mixed protected/nonprotected resources");
        require(quality.begin(TextureQuality::Strong,true,4,&error)==1&&g.bindings[0]==1&&g.bindings[1]>=16,
                "a protected texture stays native EVEN IN AN UNPROTECTED context; only the other resource qualifies");
        require(g.flagsQueries==1&&g.textureProtectionQueries==2&&g.illegalQueries==0,"extension-aware context AND per-texture protection inspection");
        quality.end();unchanged();quality.release();
    }
    for(bool incomplete:{false,true}){
        g.reset();g.protectionExtension=true;g.extensionCount=1;g.protectedContext=!incomplete;g.incompleteExtensions=incomplete;
        GlTextureOverride quality;GlPipeline pipeline;std::string error;
        require(!quality.initialize(resolve,nullptr,&error)&&!pipeline.initialize(resolve,nullptr,&error),
                "protected contexts and incomplete protection inspection reject BOTH scene sampling and presentation grading");
        unchanged();require(g.creations==0&&g.illegalQueries==0,"protected refusal makes no resources and no illegal queries");
    }
    {
        g.reset();g.samplers[1].s=0x812d;GlTextureOverride quality;std::string error;
        require(quality.initialize(resolve,nullptr,&error)&&quality.begin(TextureQuality::Strong,true,4,&error)==1&&g.bindings[0]==1,
                "extension-only GLES border state is preserved by bypass, not silently replaced with default borders");
        quality.end();quality.release();unchanged();
    }
    for(bool framebuffer:{false,true}){
        for(const auto expected:{TextureDepthFailure::TestDisabled,TextureDepthFailure::WritesDisabled,TextureDepthFailure::Never,
                                 TextureDepthFailure::Always,TextureDepthFailure::MissingAttachment,TextureDepthFailure::InsufficientPrecision}){
            g.reset();g.framebuffer=framebuffer?7:0;
            switch(expected){
            case TextureDepthFailure::TestDisabled:g.depth=false;break;
            case TextureDepthFailure::WritesDisabled:g.depthWrite=0;break;
            case TextureDepthFailure::Never:g.depthFunction=0x0200;break;
            case TextureDepthFailure::Always:g.depthFunction=0x0207;break;
            case TextureDepthFailure::MissingAttachment:g.depthBits=0;break;
            case TextureDepthFailure::InsufficientPrecision:g.depthBits=8;break;
            default:require(false,"only explicit failing depth fixtures");
            }
            GlTextureOverride quality;std::string error;TextureDrawReason reason;
            TextureDepthFailure depth=TextureDepthFailure::None;
            require(quality.initialize(resolve,nullptr,&error)&&!quality.begin(TextureQuality::Strong,true,4,&error,&reason,&depth)
                    &&reason==TextureDrawReason::Depth&&depth==expected&&!error.empty(),
                    "all six precise depth failures stay refusals on default AND offscreen-FBO draw targets");
            require(!g.shaderQueries&&!g.creations,"depth rejection is BEFORE program/texture/sampler inspection or allocation");
            if(expected==TextureDepthFailure::TestDisabled)
                require(!g.depthWriteQueries&&!g.depthFunctionQueries&&!g.depthAttachmentQueries,"depth-disabled composition stops after its enabled-state query");
            else if(expected==TextureDepthFailure::WritesDisabled)
                require(g.depthWriteQueries==1&&!g.depthFunctionQueries&&!g.depthAttachmentQueries,"read-only depth stops before function/attachment queries");
            else if(expected==TextureDepthFailure::Never||expected==TextureDepthFailure::Always)
                require(g.depthWriteQueries==1&&g.depthFunctionQueries==1&&!g.depthAttachmentQueries,"NEVER/ALWAYS fail before attachment or sampler queries");
            else require(g.depthAttachmentQueries==(expected==TextureDepthFailure::MissingAttachment?1:2),"only query depth precision for an existing attachment");
            require(!quality.begin(TextureQuality::Native,true,4,&error,&reason,&depth)&&depth==TextureDepthFailure::None,
                    "a non-inspected call cannot retain the preceding depth subreason");
            unchanged();quality.release();
        }
        g.reset();g.framebuffer=framebuffer?7:0;g.depthBits=16;g.program=0;
        GlTextureOverride quality;std::string error;TextureDrawReason reason;TextureDepthFailure depth=TextureDepthFailure::Always;
        require(quality.initialize(resolve,nullptr,&error)&&!quality.begin(TextureQuality::Strong,true,4,&error,&reason,&depth)
                &&reason==TextureDrawReason::NoSampler&&depth==TextureDepthFailure::None&&g.depthAttachmentQueries==2&&g.shaderQueries==1,
                "legal 16-bit FBO/default scene depth passes the unchanged threshold, then separately rejects a missing program");
        unchanged();quality.release();
    }
    {
        g.reset();GlTextureOverride quality;std::string error;TextureDrawReason reason;
        require(quality.initialize(resolve,nullptr,&error),"initialize explicit inspection outcomes");
        require(!quality.begin(TextureQuality::Native,true,4,&error,&reason)&&reason==TextureDrawReason::NotInspected,"native/off does not pretend to inspect a game call");
        require(!quality.begin(TextureQuality::Strong,false,4,&error,&reason)&&reason==TextureDrawReason::Unreviewed,"unreviewed driver state remains a refusal");
        require(!quality.begin(TextureQuality::Strong,true,1,&error,&reason)&&reason==TextureDrawReason::Primitive,"non-triangle outcome is not stale prior scene evidence");
        g.blend=true;require(!quality.begin(TextureQuality::Strong,true,4,&error,&reason)&&reason==TextureDrawReason::Blended,"blend bypass is separate from depth/UI qualification");
        g.blend=false;g.program=0;
        require(!quality.begin(TextureQuality::Strong,true,4,&error,&reason)&&reason==TextureDrawReason::NoSampler&&!error.empty(),"a scene without an inspectable program cannot inherit a previous preparation detail");
        unchanged();quality.release();
    }
    for(bool desktop:{false,true})for(auto policy:{TextureDrawPolicy::DepthQualifiedScene,TextureDrawPolicy::OwnedMipCompatibility}){
        unsigned failures=0;bool unitFailure=false,bindingFailure=false,completed=false;
        for(int allocation=0;allocation<100&&!completed;++allocation){
            g.reset();g.desktop=desktop;g.depth=policy==TextureDrawPolicy::DepthQualifiedScene;
            GlTextureOverride quality;std::string detail;
            require(quality.initialize(resolve,nullptr,&detail),"initialize before targeted begin-only allocation failures");
            failedAfterUnitChange=failedAfterBinding=false;failAfter=allocation;
            try {
                require(quality.begin(TextureQuality::Strong,true,4,&detail,nullptr,nullptr,policy)==2,"uninterrupted inspection prepares two replacements");
                failAfter=-1;quality.end();completed=true;
            }catch(const std::bad_alloc&){
                failAfter=-1;++failures;unitFailure=unitFailure||failedAfterUnitChange;bindingFailure=bindingFailure||failedAfterBinding;
                unchanged(); // This is BEFORE end(): a failed draw must already be native.
            }
            failAfter=-1;quality.end();unchanged();quality.release();
            require(g.creations==g.deletions,"allocation failure cannot leak a newly generated sampler while inserting its cache entry");
        }
        require(completed&&failures>=4&&unitFailure&&bindingFailure,"inject every inspection allocation, including after unit change AND after a prior sampler replacement");
        std::printf("PASS: production %s %s sampler guards and %u injected allocation failures; active-unit/partial-binding rollback before the game draw and no sampler-name leaks\n",desktop?"desktop bias+floor":"GLES floor-only",policy==TextureDrawPolicy::OwnedMipCompatibility?"depth-independent compatibility":"depth-qualified control",failures);
    }
    std::puts("LIMIT: public-API doubles, not a protected driver, game-resource semantic proof, Windows interception or GPU certification");
}
