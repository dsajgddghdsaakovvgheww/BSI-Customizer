#pragma once
#include "GlDeferredCopyAudit.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace BsiShaders {
// Typed identities: GL textures, renderbuffers and native pbuffer generations
// can have the SAME numeric name without denoting the same color storage.
// Names are still namespace-qualified below, never compared across providers.
struct GlColorResource {
    enum Kind { Texture, Renderbuffer, Pbuffer } kind=Texture;
    std::uintptr_t name=0;
    unsigned colorBuffer=0; // native default-color aspect; texture/RBO names use zero
    constexpr GlColorResource()=default;
    constexpr GlColorResource(unsigned texture):name(texture){}
    static constexpr GlColorResource renderbuffer(unsigned name){return {Renderbuffer,name};}
    static constexpr GlColorResource pbuffer(std::uintptr_t generation,unsigned buffer=0){return {Pbuffer,generation,buffer};}
    constexpr explicit operator bool()const{return name!=0;}
    constexpr bool operator==(GlColorResource other)const{return kind==other.kind&&name==other.name&&colorBuffer==other.colorBuffer;}
private:
    constexpr GlColorResource(Kind kind,std::uintptr_t name,unsigned buffer=0):kind(kind),name(name),colorBuffer(buffer){}
};
// Read-only GL observations made on the CURRENT context. A zero-name Pbuffer
// is only a placeholder for a default framebuffer; the native caller must
// qualify it using an observed live, unprotected pbuffer (or the exact selected
// presentation OUTPUT), or discard the capture.
struct GlTextureUsage {
    static constexpr std::size_t capacity=32;
    bool defaultFramebuffer=false;
    unsigned nativeColorBuffer=0;
    std::array<GlColorResource,capacity> reads{},writes{};
    std::size_t readCount=0,writeCount=0;
};
// Invalidation ONLY: a conservative superset of the current draw's enabled
// raster-color targets. It carries no sampled inputs, native-image identity or
// ownership, and cannot be passed to producer()/observe(). Blend, primitive,
// program and color masks do not make another target an unknown namespace write.
// A native output still needs the caller's selected/live-pbuffer route check;
// its only policy effect here is to revoke an in-flight capture, never a link.
struct GlDrawWrites {
    static constexpr std::size_t capacity=GlTextureUsage::capacity;
    bool defaultFramebuffer=false,nativeColorWrite=false;
    std::array<GlColorResource,capacity> writes{};
    std::size_t writeCount=0;
};
struct GlContextIdentity {
    unsigned provider=0;
    std::uintptr_t display=0,context=0;
    bool operator==(const GlContextIdentity &other)const{
        return provider==other.provider&&display==other.display&&context==other.context;
    }
};
// Deferred metadata is NOT ownership. Only an exact later consumer can resolve
// a submitted base-level copy; the original capture's namespace/TTL still bind it.
enum class GlDeferredCopyResult { Refused, Queued, Capacity };
struct GlDeferredCopyResolution {unsigned copies=0,inputs=0;};
struct GlDeferredCopyCapture {std::uint64_t expires=0,epoch=0;};
// Bounded, allocation-free dependency evidence, serialized by renderMutex.
// Only a real selected-drawable/default-FBO draw or submitted color blit can
// seed consumed color resources. A nonpresenting producer must write an exact
// recent dependency (including an observed copy/resolve predecessor). GL names
// require the SAME context or a SUCCESSFUL native sharing call. An explicitly
// linked native pbuffer lifetime/color aspect is independent of GL namespaces;
// its material inputs remain in the producer's separate namespace.
// Sharing alone never authorizes a producer. No GL names are compared across
// unrelated contexts, providers or EGL displays; no foreign HWND is adopted.
class GlTextureProvenance {
public:
    static constexpr std::size_t contextCapacity=64,textureCapacity=128,pendingCopyCapacity=32;
    static constexpr unsigned maximumHops=8;
    static constexpr std::uint64_t frameLifetime=2;
    void synchronize(std::uintptr_t source,std::uint64_t revision){
        if(source_==source&&revision_==revision)return;
        source_=source;revision_=revision;clearEvidence();
        audit_={};auditChanged(); // old-source retirements must not enter the new interval
    }
    void clearEvidence(GlDeferredCopyExit reason=GlDeferredCopyExit::EvidenceReset){
        advancePendingEpoch();for(auto &item:textures_)item={};
        for(auto &copy:pending_)if(copy.output)erase(copy,reason);
        pendingCount_=0;frame_=1;
    }
    bool hasPendingCopies()const{return pendingCount_!=0;} // caller serializes; not ownership
    const GlDeferredCopyAudit &deferredCopyAudit()const{return audit_;} // caller serializes, as for the resource graph
    bool takeDeferredCopyAudit(GlDeferredCopyAudit *out,std::uint64_t *revision){
        if(!auditDirty_||!out||!revision)return false;
        *out=audit_;*revision=revision_;auditDirty_=false;return true;
    }
    void nextFrame(){
        if(frame_==std::numeric_limits<std::uint64_t>::max()){clearEvidence();return;}
        ++frame_;
        for(auto &item:textures_)if(item.texture&&item.expires<frame_)item={};
        for(auto &copy:pending_)if(copy.output&&copy.expires<frame_)erase(copy,GlDeferredCopyExit::Expired);
    }
    bool remember(GlContextIdentity key){return space(key,true)!=0;}
    bool known(GlContextIdentity key)const{return knownSpace(key)!=0;}
    bool hasDependencies(GlContextIdentity key)const{
        if(!source_)return false;
        const auto id=knownSpace(key);if(!id)return false;
        return std::any_of(textures_.begin(),textures_.end(),[&](const Texture &item){return live(item,id);});
    }
    // Native pbuffers are NOT GL object names. An exact observed native
    // image binding/copy can link that lifetime across non-sharing contexts.
    // This prefilter never authorizes a texture/RBO or a different pbuffer.
    bool hasNativeOutput(GlContextIdentity key,GlColorResource output)const{
        if(!source_||!key.context||output.kind!=GlColorResource::Pbuffer||!output)return false;
        return std::any_of(textures_.begin(),textures_.end(),[&](const Texture &item){return nativeMatch(item,key,output);});
    }
    bool hasNativePbuffer(GlContextIdentity key,std::uintptr_t generation)const{
        if(!source_||!key.context||!generation)return false;
        for(const auto &item:textures_)if(item.texture.kind==GlColorResource::Pbuffer&&item.texture.name==generation
            &&nativeMatch(item,key,item.texture))return true;
        return false;
    }
    // Call only AFTER native context creation/sharing succeeded. An unknown
    // pre-existing relationship is deliberately not reconstructed from names.
    bool shared(GlContextIdentity parent,GlContextIdentity child){
        if(!parent.context||!child.context||parent.provider!=child.provider||parent.display!=child.display)return false;
        const auto a=space(parent,true),b=space(child,true);if(!a||!b)return false;
        if(a==b)return true;
        for(auto &entry:contexts_)if(entry.space==b)entry.space=a;
        // A late namespace merge invalidates old name evidence. Re-learn only
        // from subsequent selected presentation draws, not pre-merge guesses.
        clearEvidence(GlDeferredCopyExit::ShareChanged);return true;
    }
    void forget(GlContextIdentity key){
        advancePendingEpoch();const auto id=knownSpace(key);
        for(auto &copy:pending_)if(copy.output&&copy.key==key)erase(copy,GlDeferredCopyExit::ContextRetired);
        for(auto &entry:contexts_)if(entry.space&&entry.key==key)entry={};
        if(id&&std::none_of(contexts_.begin(),contexts_.end(),[&](const Context &c){return c.space==id;}))
            for(auto &item:textures_)if(item.space==id)item={};
    }
    void forgetProvider(unsigned provider){
        for(auto &entry:contexts_)if(entry.space&&entry.key.provider==provider)entry={};
        invalidate(provider,GlDeferredCopyExit::ProviderRetired);
    }
    // Invalidate by provider even if deletion came through an untracked shared
    // context: conservative over-invalidation is preferable to a reused name.
    void deleted(unsigned provider,int count,const unsigned *names,GlColorResource::Kind kind=GlColorResource::Texture){
        if(count<=0)return;
        advancePendingEpoch();bool invalid=!names||count>256;
        if(!invalid)for(const auto &item:textures_)if(item.texture&&item.provider==provider&&item.texture.kind==kind)
            for(int i=0;i<count;++i)if(item.texture.name==names[i]){invalid=true;break;}
        if(!invalid)for(const auto &copy:pending_)if(copy.output&&copy.key.provider==provider)
            for(int i=0;i<count;++i)if((copy.output.kind==kind&&copy.output.name==names[i])
                ||(copy.input.kind==kind&&copy.input.name==names[i])){invalid=true;break;}
        if(invalid)invalidate(provider);
    }
    void retiredPbuffer(unsigned provider,std::uintptr_t generation){
        advancePendingEpoch();for(const auto &item:textures_)if(item.provider==provider&&item.texture.kind==GlColorResource::Pbuffer&&item.texture.name==generation){
            invalidate(provider,GlDeferredCopyExit::NativeSourceRetired);return;
        }
        for(const auto &copy:pending_)if(copy.output&&copy.key.provider==provider
            &&copy.input.kind==GlColorResource::Pbuffer&&copy.input.name==generation){invalidate(provider,GlDeferredCopyExit::NativeSourceRetired);return;}
    }
    // Drop inherited ancestors too: they no longer have an intact observed
    // path if a downstream/root resource was retired. Keep valid namespaces.
    void discardPendingProvider(unsigned provider,GlDeferredCopyExit reason=GlDeferredCopyExit::MissedCallback){
        advancePendingEpoch();if(!pendingCount_)return;
        for(auto &copy:pending_)if(copy.output&&copy.key.provider==provider)erase(copy,reason);
    }
    void invalidate(unsigned provider,GlDeferredCopyExit reason=GlDeferredCopyExit::ResourceRetired){
        for(auto &item:textures_)if(item.provider==provider)item={};
        discardPendingProvider(provider,reason);
    }
    // An observed later write replaces the earlier copy's destination. If an
    // already-qualified callback cannot capture its output, revoke that space's
    // pending metadata without probing more GL state. Revocation is not adoption.
    void discardPendingWrites(GlContextIdentity key,const GlTextureUsage *usage,GlDeferredDiscardSite site=GlDeferredDiscardSite::Unspecified){
        discardPendingColorWrites(key,usage?&usage->writes:nullptr,usage?usage->writeCount:0,false,site);
    }
    // Deliberately separate from sampled dependency evidence. Even a complete
    // footprint may ONLY remove pending copies; it never seeds/renews a graph.
    // Null/oversized capture remains namespace-wide conservative revocation.
    void discardPendingDrawWrites(GlContextIdentity key,const GlDrawWrites *writes,GlDeferredDiscardSite site=GlDeferredDiscardSite::Unspecified){
        discardPendingColorWrites(key,writes?&writes->writes:nullptr,writes?writes->writeCount:0,writes&&writes->nativeColorWrite,site);
    }
    // Called only AFTER a native CopyTexSubImage2D returns with its existing
    // descriptor/current/route/resource checks intact. A queued edge cannot
    // seed a namespace, authorize a producer or refresh any dependency. The
    // exact destination must be consumed later in an observed SAME/share space.
    // The native wrapper takes this value BEFORE releasing its inspection lock;
    // elapsed presentation frames and intervening writes still count at return.
    GlDeferredCopyCapture captureDeferredCopy()const{return {deadline(),pendingEpoch_};}
    GlDeferredCopyResult deferCopy(GlContextIdentity key,const GlTextureUsage &usage){
        return deferCopy(key,usage,captureDeferredCopy()); // already-submitted policy fixtures
    }
    GlDeferredCopyResult deferCopy(GlContextIdentity key,const GlTextureUsage &usage,GlDeferredCopyCapture capture){
        if(!capture.epoch||capture.epoch!=pendingEpoch_||capture.expires<frame_||capture.expires>deadline())return GlDeferredCopyResult::Refused;
        const auto id=knownSpace(key);
        if(!source_||!id||!hasDependencies(key)||usage.readCount!=1||usage.writeCount!=1)return GlDeferredCopyResult::Refused;
        const auto input=usage.reads[0],output=usage.writes[0];
        if(!input||!output||output.kind!=GlColorResource::Texture||input==output
           ||(input.kind==GlColorResource::Pbuffer&&!input.colorBuffer)||producer(key,usage))return GlDeferredCopyResult::Refused;
        // Keep only the latest captured copy into a given output, not an
        // obsolete predecessor overwritten by a later observed copy.
        for(auto &copy:pending_)if(copy.output&&copy.space==id&&copy.output==output){
            copy={key,input,output,id,capture.expires};
            ++audit_.exits[static_cast<std::size_t>(GlDeferredCopyExit::Superseded)];noteQueued(input);
            return GlDeferredCopyResult::Queued;
        }
        for(auto &copy:pending_)if(!copy.output){
            copy={key,input,output,id,capture.expires};++pendingCount_;noteQueued(input);return GlDeferredCopyResult::Queued;
        }
        return GlDeferredCopyResult::Capacity;
    }
    bool producer(GlContextIdentity key,const GlTextureUsage &usage)const{
        if(!source_||usage.readCount>usage.capacity||usage.writeCount>usage.capacity)return false;
        const auto id=knownSpace(key);
        for(const auto &item:textures_)if((live(item,id)||nativeMatch(item,key,item.texture))
            &&contains(usage.writes,usage.writeCount,item.texture))return true;
        return false;
    }
    // Returns the number of consumed color bindings observed, not pixels/FPS.
    unsigned observe(GlContextIdentity key,const GlTextureUsage &usage,bool selectedDrawable,GlDeferredCopyResolution *resolved=nullptr){
        if(resolved)*resolved={};
        if(!source_||usage.readCount>usage.capacity||usage.writeCount>usage.capacity)return 0;
        discardPendingWrites(key,&usage);
        bool nativeOutput=false;
        for(std::size_t i=0;i<usage.writeCount;++i)nativeOutput=nativeOutput||hasNativeOutput(key,usage.writes[i]);
        const auto id=space(key,selectedDrawable||nativeOutput);if(!id)return 0;
        unsigned hops=0;std::uint64_t expires=0;
        if(selectedDrawable&&usage.defaultFramebuffer){
            noteConsumer(id,usage);expires=deadline();
        }else{
            hops=maximumHops;
            for(const auto &item:textures_)if((live(item,id)||nativeMatch(item,key,item.texture))&&contains(usage.writes,usage.writeCount,item.texture)){
                if(item.expires>expires){expires=item.expires;hops=item.hops+1;}
                else if(item.expires==expires)hops=(std::min)(hops,item.hops+1);
            }
            if(!expires||hops>=maximumHops)return 0;
        }
        unsigned count=0;
        for(std::size_t i=0;i<usage.readCount;++i){
            const auto texture=usage.reads[i];
            if(!texture||contains(usage.writes,usage.writeCount,texture))continue;
            if(note(key,id,texture,expires,hops))++count;
            else if(pendingCount_)for(const auto &copy:pending_)if(copy.output==texture&&copy.space==id){
                ++audit_.resourceBlocked;auditChanged();break; // matching consumer could not store its proof
            }
        }
        if(count)resolvePending(resolved);
        return count;
    }
private:
    void discardPendingColorWrites(GlContextIdentity key,const std::array<GlColorResource,GlTextureUsage::capacity> *writes,
                                   std::size_t count,bool nativeWrite,GlDeferredDiscardSite site){
        // Also revoke an IN-FLIGHT capture not yet queued. Interleaved callback
        // writes remain conservatively unordered, INCLUDING unrelated/native
        // writes; the more precise queued-output match does not relax that rule.
        if(!writes||count||nativeWrite)advancePendingEpoch();
        if(!pendingCount_)return;
        const auto id=knownSpace(key);if(!id)return;
        const bool unknown=!writes||count>writes->size();
        for(auto &copy:pending_)if(copy.output&&copy.space==id
            &&(unknown||contains(*writes,count,copy.output)))
                erase(copy,unknown?GlDeferredCopyExit::UnknownWrite:GlDeferredCopyExit::DestinationWrite,site);
    }
    struct Context {GlContextIdentity key{};std::uint64_t space=0;};
    struct PendingCopy {GlContextIdentity key{};GlColorResource input{},output{};std::uint64_t space=0,expires=0;};
    struct Texture {unsigned provider=0;GlColorResource texture{};unsigned hops=0;std::uint64_t space=0,expires=0;std::uintptr_t display=0;};
    std::array<Context,contextCapacity> contexts_{};
    std::array<Texture,textureCapacity> textures_{};
    std::array<PendingCopy,pendingCopyCapacity> pending_{};
    unsigned pendingCount_=0;
    GlDeferredCopyAudit audit_{};
    bool auditDirty_=false;
    std::uintptr_t source_=0;
    std::uint64_t revision_=0,frame_=1,nextSpace_=1,pendingEpoch_=1;
    void advancePendingEpoch(){if(pendingEpoch_)++pendingEpoch_;} // wrap disables new deferred captures, never aliases an old token
    std::uint64_t deadline()const{
        return frame_>std::numeric_limits<std::uint64_t>::max()-frameLifetime
            ?std::numeric_limits<std::uint64_t>::max():frame_+frameLifetime;
    }
    void auditChanged(){
        if(audit_.sequence!=std::numeric_limits<std::uint64_t>::max())++audit_.sequence;
        audit_.pending=pendingCount_;audit_.peak=(std::max)(audit_.peak,pendingCount_);auditDirty_=true;
    }
    void noteQueued(GlColorResource input){
        ++audit_.queued;
        const auto kind=static_cast<unsigned>(input.kind);
        ++audit_.sources[kind<3?kind:3];auditChanged();
    }
    void noteConsumer(std::uint64_t id,const GlTextureUsage &usage){
        auto stop=GlDeferredConsumerStop::NoPending;
        if(pendingCount_){
            stop=GlDeferredConsumerStop::OtherNamespace;
            for(const auto &copy:pending_)if(copy.output&&copy.space==id){
                stop=GlDeferredConsumerStop::NoDestination;
                if(contains(usage.reads,usage.readCount,copy.output)){stop=GlDeferredConsumerStop::ExactDestination;break;}
            }
        }
        ++audit_.consumers[static_cast<std::size_t>(stop)];auditChanged();
    }
    void erase(PendingCopy &copy,GlDeferredCopyExit reason,GlDeferredDiscardSite site=GlDeferredDiscardSite::Unspecified){
        copy={};--pendingCount_;
        const auto why=static_cast<std::size_t>(reason),where=static_cast<std::size_t>(site);
        ++audit_.exits[why<audit_.exits.size()?why:static_cast<std::size_t>(GlDeferredCopyExit::EvidenceReset)];
        if(reason==GlDeferredCopyExit::UnknownWrite)++audit_.discardSites[where<audit_.discardSites.size()?where:0];
        auditChanged();
    }
    void resolvePending(GlDeferredCopyResolution *resolved){
        // No GL queries, allocation or ownership from a pending edge. A real
        // consumer has just seeded/refreshed an exact destination proof. Resolve
        // at most the existing hop bound, retaining BOTH the root and capture
        // deadlines. Copy cycles cannot renew themselves or replay indefinitely.
        for(unsigned pass=0;pendingCount_&&pass<maximumHops;++pass){
            bool progress=false;
            for(auto &copy:pending_)if(copy.output){
                if(copy.expires<frame_||knownSpace(copy.key)!=copy.space){
                    erase(copy,copy.expires<frame_?GlDeferredCopyExit::Expired:GlDeferredCopyExit::NamespaceLost);continue;
                }
                unsigned hops=maximumHops;std::uint64_t expires=0;
                for(const auto &item:textures_)if(live(item,copy.space)&&item.texture==copy.output){
                    const auto until=(std::min)(item.expires,copy.expires);
                    if(until>expires){expires=until;hops=item.hops+1;}
                    else if(until==expires)hops=(std::min)(hops,item.hops+1);
                }
                if(!expires)continue;
                if(hops>=maximumHops){++audit_.hopBlocked;auditChanged();continue;}
                if(!note(copy.key,copy.space,copy.input,expires,hops)){++audit_.resourceBlocked;auditChanged();continue;}
                if(resolved){++resolved->copies;++resolved->inputs;}
                erase(copy,GlDeferredCopyExit::Reconciled);progress=true;
            }
            if(!progress)break;
        }
    }
    static bool contains(const std::array<GlColorResource,GlTextureUsage::capacity> &items,std::size_t count,GlColorResource name){
        return count<=items.size()&&std::find(items.begin(),items.begin()+count,name)!=items.begin()+count;
    }
    std::uint64_t knownSpace(GlContextIdentity key)const{
        if(!key.context)return 0;
        for(const auto &entry:contexts_)if(entry.space&&entry.key==key)return entry.space;
        return 0;
    }
    std::uint64_t space(GlContextIdentity key,bool create){
        if(const auto id=knownSpace(key))return id;
        if(!create||!key.context||!nextSpace_)return 0;
        for(auto &entry:contexts_)if(!entry.space){entry={key,nextSpace_++};return entry.space;}
        return 0;
    }
    bool live(const Texture &item,std::uint64_t id)const{return id&&item.texture&&item.space==id&&item.expires>=frame_;}
    bool nativeMatch(const Texture &item,GlContextIdentity key,GlColorResource output)const{
        return key.context&&output.kind==GlColorResource::Pbuffer&&output&&item.texture==output
            &&item.provider==key.provider&&item.display==key.display&&item.expires>=frame_;
    }
    bool note(GlContextIdentity key,std::uint64_t id,GlColorResource name,std::uint64_t expires,unsigned hops){
        for(auto &item:textures_)if((item.space==id&&item.texture==name)||nativeMatch(item,key,name)){
            if(expires>item.expires){item.expires=expires;item.hops=hops;}
            else if(expires==item.expires)item.hops=(std::min)(item.hops,hops);
            return true;
        }
        for(auto &item:textures_)if(!item.texture||item.expires<frame_){item={key.provider,name,hops,id,expires,key.display};return true;}
        return false;
    }
};
} // namespace BsiShaders
