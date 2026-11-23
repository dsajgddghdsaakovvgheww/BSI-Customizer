#pragma once
#include "GlDeferredCopyAudit.h"
#include "GlTransferAudit.h"
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
    static constexpr std::size_t correlationCapacity=32,copyOutputCapacity=16;
    static constexpr unsigned maximumHops=8;
    static constexpr std::size_t edgeCapacity=4;
    static constexpr std::uint64_t frameLifetime=2;
    void synchronize(std::uintptr_t source,std::uint64_t revision){
        if(source_==source&&revision_==revision)return;
        source_=source;revision_=revision;clearEvidence();resetCorrelationNames();
        audit_={};auditChanged(); // old-source retirements must not enter the new interval
    }
    void clearEvidence(GlDeferredCopyExit reason=GlDeferredCopyExit::EvidenceReset){
        advancePendingEpoch();clearCorrelationNames();for(auto &item:textures_)item={};
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
        advancePendingEpoch();clearCorrelationNames();const auto id=knownSpace(key);
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
        clearCorrelationNames();
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
    // Option-2 invalidation: distinguish a genuinely attempted but unavailable
    // footprint from an intentional caller gate skip. Known writes supersede
    // only matching graph resources; unknown writes kill this observed namespace's edges.
    void invalidateNamespace(GlContextIdentity key){
        const auto id=knownSpace(key);if(!id)return;
        for(auto &item:textures_)if(item.space==id)++item.epoch;
        for(auto &item:textures_)for(auto &edge:item.edges)if(edge.used&&edge.space==id)edge.captured=0;
    }
    void invalidateWrites(GlContextIdentity key,const GlDrawWrites *writes){
        if(!writes){invalidateNamespace(key);return;}
        if(writes->nativeColorWrite){invalidateNamespace(key);return;}
        const auto id=knownSpace(key);if(!id)return;
        GlTextureUsage usage;usage.writes=writes->writes;usage.writeCount=writes->writeCount;
        supersede(key,id,usage);
    }
    void invalidateProviderNamespaces(unsigned provider){
        std::array<std::uint64_t,contextCapacity> spaces{};std::size_t count=0;
        for(const auto &context:contexts_)if(context.space&&context.key.provider==provider
            &&std::find(spaces.begin(),spaces.begin()+count,context.space)==spaces.begin()+count)spaces[count++]=context.space;
        for(std::size_t n=0;n<count;++n)for(const auto &context:contexts_)
            if(context.space==spaces[n]&&context.key.provider==provider){invalidateNamespace(context.key);break;}
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
        const auto id=knownSpace(key);
        if(!source_||!id||usage.readCount>usage.capacity||usage.writeCount>usage.capacity)return GlDeferredCopyResult::Refused;
        noteCopyCorrelation(id,usage);
        if(!capture.epoch||capture.epoch!=pendingEpoch_||capture.expires<frame_||capture.expires>deadline()
           ||usage.readCount!=1||usage.writeCount!=1||!hasDependencies(key))return GlDeferredCopyResult::Refused;
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
    // replacesWrites: the caller is a native color TRANSFER (copy/blit/image copy). Like the
    // deferred-copy policy ("keep only the latest copy into an output") it supersedes every
    // earlier recorded link INTO its destination. Draws accumulate and never set it.
    unsigned observe(GlContextIdentity key,const GlTextureUsage &usage,bool selectedDrawable,GlDeferredCopyResolution *resolved=nullptr,bool replacesWrites=false,GlObserveStop *stop=nullptr){
        if(resolved)*resolved={};
        if(stop)*stop=GlObserveStop::InvalidUsage;
        if(!source_||usage.readCount>usage.capacity||usage.writeCount>usage.capacity)return 0;
        if(stop)*stop=GlObserveStop::UnknownNamespace;
        discardPendingWrites(key,&usage);
        bool nativeOutput=false;
        for(std::size_t i=0;i<usage.writeCount;++i)nativeOutput=nativeOutput||hasNativeOutput(key,usage.writes[i]);
        const auto id=space(key,selectedDrawable||nativeOutput);if(!id)return 0;
        if(stop)*stop=GlObserveStop::NoRootedProducer;
        const bool root=selectedDrawable&&usage.defaultFramebuffer;
        if(replacesWrites)noteCopyCorrelation(id,usage);
        if(root)notePresentedInputs(id,usage);
        if(replacesWrites&&!root)supersede(key,id,usage);
        unsigned hops=0;std::uint64_t expires=0;
        std::array<std::size_t,GlTextureUsage::capacity> parents{};std::size_t parentCount=0;
        if(root){
            noteConsumer(id,usage);expires=deadline();
        }else{
            hops=maximumHops;
            for(std::size_t p=0;p<textures_.size();++p){
                const auto &item=textures_[p];
                if((live(item,id)||nativeMatch(item,key,item.texture))&&contains(usage.writes,usage.writeCount,item.texture)){
                    if(parentCount<parents.size())parents[parentCount++]=p;
                    if(item.expires>expires){expires=item.expires;hops=item.hops+1;}
                    else if(item.expires==expires)hops=(std::min)(hops,item.hops+1);
                }
            }
            if(!expires)return 0;
            if(hops>=maximumHops){if(stop)*stop=GlObserveStop::HopLimit;return 0;}
        }
        unsigned count=0;bool blockedByCapacity=false;
        for(std::size_t i=0;i<usage.readCount;++i){
            const auto texture=usage.reads[i];
            if(!texture||contains(usage.writes,usage.writeCount,texture))continue;
            if(Texture *child=noteItem(key,id,texture,expires,hops)){
                ++count;
                // A root mark comes ONLY from real selected-presentation consumption.
                if(root)child->rootUntil=(std::max)(child->rootUntil,expires);
                else for(std::size_t p=0;p<parentCount;++p)link(*child,parents[p],deadline(),id);
            }
            else if(pendingCount_)for(const auto &copy:pending_)if(copy.output==texture&&copy.space==id){
                ++audit_.resourceBlocked;auditChanged();blockedByCapacity=true;break; // matching consumer could not store its proof
            }
        }
        if(stop)*stop=count?GlObserveStop::Observed:blockedByCapacity?GlObserveStop::ResourceCapacity:GlObserveStop::NoUsableInputs;
        if(count)resolvePending(resolved);
        if(count&&root)refreshFromRoots();
        return count;
    }
    // Links dropped because a resource already had edgeCapacity distinct live consumers.
    // Dropping only withholds renewal; it never grants proof.
    std::uint64_t edgeDrops()const{return edgeDrops_;}
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
    // A recorded consumer link child->parent ("child's color flowed into parent"). It names the
    // parent by slot AND unique serial AND write epoch, so a reused fixed-array slot or an
    // overwritten destination can never be reconnected by index alone. `captured` is the
    // capture deadline of the OBSERVATION that created it; walking never extends it.
    struct Edge {bool used=false;std::size_t parent=0;std::uint64_t parentSerial=0,parentEpoch=0,captured=0,space=0;};
    struct Texture {unsigned provider=0;GlColorResource texture{};unsigned hops=0;std::uint64_t space=0,expires=0;std::uintptr_t display=0;
        std::uint64_t serial=0,epoch=0,rootUntil=0;std::array<Edge,edgeCapacity> edges{};};
    std::array<Context,contextCapacity> contexts_{};
    std::array<Texture,textureCapacity> textures_{};
    std::array<PendingCopy,pendingCopyCapacity> pending_{};
    struct CorrelationName {bool used=false;std::uint64_t space=0;GlColorResource resource{};std::uint64_t lastFrame=0,distinctFrames=0;};
    std::array<CorrelationName,correlationCapacity> presentedNames_{},copyInputNames_{};
    std::array<CorrelationName,copyOutputCapacity> copyOutputNames_{};
    std::uint64_t lastCopyOutputFrame_=0;
    unsigned pendingCount_=0;
    GlDeferredCopyAudit audit_{};
    bool auditDirty_=false;
    std::uintptr_t source_=0;
    std::uint64_t revision_=0,frame_=1,nextSpace_=1,pendingEpoch_=1,nextSerial_=1,edgeDrops_=0;
    void advancePendingEpoch(){if(pendingEpoch_)++pendingEpoch_;} // wrap disables new deferred captures, never aliases an old token
    std::uint64_t deadline()const{
        return frame_>std::numeric_limits<std::uint64_t>::max()-frameLifetime
            ?std::numeric_limits<std::uint64_t>::max():frame_+frameLifetime;
    }
    void auditChanged(){
        if(audit_.sequence!=std::numeric_limits<std::uint64_t>::max())++audit_.sequence;
        audit_.pending=pendingCount_;audit_.peak=(std::max)(audit_.peak,pendingCount_);auditDirty_=true;
    }
    void resetCorrelationNames(){presentedNames_={};copyInputNames_={};copyOutputNames_={};lastCopyOutputFrame_=0;}
    void clearCorrelationNames(){
        for(auto &name:presentedNames_)name.lastFrame=0;
        for(auto &name:copyInputNames_)name.lastFrame=0;
        for(auto &name:copyOutputNames_)name.lastFrame=0;
        lastCopyOutputFrame_=0;
    }
    bool recentlySeen(const std::array<CorrelationName,correlationCapacity> &names,std::uint64_t id,GlColorResource resource)const{
        for(const auto &name:names)if(name.used&&name.lastFrame&&name.space==id&&name.resource==resource
            &&frame_>=name.lastFrame&&frame_-name.lastFrame<=frameLifetime)return true;
        return false;
    }
    bool recentlyPresented(std::uint64_t id,GlColorResource resource)const{return recentlySeen(presentedNames_,id,resource);}
    bool recentlyCopied(const std::array<CorrelationName,correlationCapacity> &names,std::uint64_t id,GlColorResource resource)const{
        return recentlySeen(names,id,resource);
    }
    bool recentlyCopiedOutput(std::uint64_t id,GlColorResource resource)const{
        for(const auto &name:copyOutputNames_)if(name.used&&name.lastFrame&&name.space==id&&name.resource==resource
            &&frame_>=name.lastFrame&&frame_-name.lastFrame<=frameLifetime)return true;
        return false;
    }
    bool noteRecentName(std::array<CorrelationName,correlationCapacity> &names,std::uint64_t id,GlColorResource resource){
        for(auto &name:names)if(name.used&&name.space==id&&name.resource==resource){name.lastFrame=frame_;return true;}
        for(auto &name:names)if(!name.used){name={true,id,resource,frame_,0};return true;}
        return false;
    }
    void notePresentedInputs(std::uint64_t id,const GlTextureUsage &usage){
        bool changed=false;
        for(std::size_t i=0;i<usage.readCount;++i){
            const auto resource=usage.reads[i];if(!resource)continue;
            ++audit_.presentedInputObservations;changed=true;
            if(recentlyCopied(copyInputNames_,id,resource))++audit_.presentationInputsRecentCopyInputMatches;
            if(recentlyCopiedOutput(id,resource))++audit_.presentationInputsRecentCopyOutputMatches;
            CorrelationName *slot=nullptr;
            for(auto &name:presentedNames_)if(name.used&&name.space==id&&name.resource==resource){slot=&name;break;}
            if(slot){slot->lastFrame=frame_;continue;}
            for(auto &name:presentedNames_)if(!name.used){slot=&name;break;}
            if(!slot){++audit_.presentedInputUntracked;continue;}
            *slot={true,id,resource,frame_,0};++audit_.presentedInputDistinct;
        }
        if(changed)auditChanged();
    }
    void noteCopyCorrelation(std::uint64_t id,const GlTextureUsage &usage){
        ++audit_.copyObservations;bool inputMatch=false,outputMatch=false,hasOutput=false;
        for(std::size_t i=0;i<usage.readCount;++i)if(usage.reads[i]){
            if(recentlyPresented(id,usage.reads[i]))inputMatch=true;
            if(!noteRecentName(copyInputNames_,id,usage.reads[i]))++audit_.copyInputUntracked;
        }
        for(std::size_t i=0;i<usage.writeCount;++i){
            const auto resource=usage.writes[i];if(!resource)continue;hasOutput=true;
            if(recentlyPresented(id,resource))outputMatch=true;
            CorrelationName *slot=nullptr;
            for(auto &name:copyOutputNames_)if(name.used&&name.space==id&&name.resource==resource){slot=&name;break;}
            if(!slot){
                for(auto &name:copyOutputNames_)if(!name.used){slot=&name;break;}
                if(!slot){++audit_.copyOutputUntracked;continue;}
                *slot={true,id,resource,0,0};++audit_.copyOutputDistinct;
            }
            if(slot->lastFrame!=frame_){slot->lastFrame=frame_;++slot->distinctFrames;
                audit_.copyOutputMaxFramesPerName=(std::max)(audit_.copyOutputMaxFramesPerName,slot->distinctFrames);}
        }
        if(inputMatch)++audit_.copyInputRecentPresentedMatches;
        if(outputMatch)++audit_.copyOutputRecentPresentedMatches;
        if(hasOutput&&lastCopyOutputFrame_!=frame_){lastCopyOutputFrame_=frame_;++audit_.copyOutputFrames;}
        auditChanged();
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
                std::array<std::size_t,4> parents{};std::size_t parentCount=0;
                for(std::size_t p=0;p<textures_.size();++p){
                    const auto &item=textures_[p];
                    if(!(live(item,copy.space)&&item.texture==copy.output))continue;
                    if(parentCount<parents.size())parents[parentCount++]=p;
                    const auto until=(std::min)(item.expires,copy.expires);
                    if(until>expires){expires=until;hops=item.hops+1;}
                    else if(until==expires)hops=(std::min)(hops,item.hops+1);
                }
                if(!expires)continue;
                if(hops>=maximumHops){++audit_.hopBlocked;auditChanged();continue;}
                Texture *child=noteItem(copy.key,copy.space,copy.input,expires,hops);
                if(!child){++audit_.resourceBlocked;auditChanged();continue;}
                // Later consumption never extends the capture: the edge keeps copy.expires.
                for(std::size_t p=0;p<parentCount;++p)link(*child,parents[p],copy.expires,copy.space);
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
    Texture *noteItem(GlContextIdentity key,std::uint64_t id,GlColorResource name,std::uint64_t expires,unsigned hops){
        for(auto &item:textures_)if((item.space==id&&item.texture==name)||nativeMatch(item,key,name)){
            if(expires>item.expires){item.expires=expires;item.hops=hops;}
            else if(expires==item.expires)item.hops=(std::min)(item.hops,hops);
            return &item;
        }
        for(auto &item:textures_)if(!item.texture||item.expires<frame_){
            item={key.provider,name,hops,id,expires,key.display};
            item.serial=nextSerial_++;if(!nextSerial_)nextSerial_=1;
            return &item;
        }
        return nullptr;
    }
    bool note(GlContextIdentity key,std::uint64_t id,GlColorResource name,std::uint64_t expires,unsigned hops){
        return noteItem(key,id,name,expires,hops)!=nullptr;
    }
    // The parent a still-valid edge names, or null. Slot index alone is never trusted: serial
    // (slot reuse), epoch (destination superseded by a later transfer) and the same
    // typed-namespace rule used at observation time (the observing GL namespace recorded in the
    // edge, or the same native provider/display for a Pbuffer parent) must all still hold. A
    // native child item is shared across namespaces, so its own `space` is NOT the rule.
    const Texture *parentOf(const Texture &child,const Edge &edge)const{
        if(!edge.used||edge.parent>=textures_.size())return nullptr;
        const auto &parent=textures_[edge.parent];
        if(&parent==&child||!parent.texture||parent.serial!=edge.parentSerial||parent.epoch!=edge.parentEpoch)return nullptr;
        if(parent.texture.kind==GlColorResource::Pbuffer){
            if(parent.provider!=child.provider||parent.display!=child.display)return nullptr;
        }else if(!edge.space||parent.space!=edge.space)return nullptr; // the OBSERVING namespace, not the child's first-seen one
        return &parent;
    }
    void link(Texture &child,std::size_t parentIndex,std::uint64_t captured,std::uint64_t space){
        const auto &parent=textures_[parentIndex];
        if(&parent==&child||!parent.texture)return;
        Edge *free=nullptr,*weakest=nullptr;
        for(auto &edge:child.edges){
            if(edge.used&&edge.parent==parentIndex&&edge.parentSerial==parent.serial&&edge.space==space){
                // Same observed epoch: a re-observation can only lengthen the capture. A new epoch
                // replaces the invalid old capture instead of inheriting its deadline.
                edge.captured=edge.parentEpoch==parent.epoch?(std::max)(edge.captured,captured):captured;
                edge.parentEpoch=parent.epoch;return;
            }
            if(!edge.used||edge.captured<frame_||!parentOf(child,edge)){if(!free)free=&edge;}
            else if(!weakest||edge.captured<weakest->captured)weakest=&edge;
        }
        Edge *target=free;
        if(!target&&weakest&&weakest->captured<captured)target=weakest;
        if(!target){++edgeDrops_;return;}
        *target={true,parentIndex,parent.serial,parent.epoch,captured,space};
    }
    void supersede(GlContextIdentity key,std::uint64_t id,const GlTextureUsage &usage){
        for(auto &item:textures_)
            if((live(item,id)||nativeMatch(item,key,item.texture))&&contains(usage.writes,usage.writeCount,item.texture))++item.epoch;
    }
    // Producer-first order means a producer only ever saw its consumer's PREVIOUS-frame expiry.
    // After a REAL selected consumption, recompute each linked item's proof as the best bounded
    // walk (at most maximumHops-1 edges) from a live root where EVERY edge is still valid and the
    // proof never exceeds min(root deadline, each edge's own capture deadline). It only raises
    // an item that is still live, with the hop count of the walk that justifies it, and uses the
    // same "later expiry wins, equal expiry keeps fewer hops" rule as direct observation.
    void refreshFromRoots(){
        std::array<std::uint64_t,textureCapacity> previous{},current{},best{};
        std::array<unsigned,textureCapacity> bestHops{};
        bool any=false;
        for(std::size_t i=0;i<textures_.size();++i)
            if(textures_[i].texture&&textures_[i].rootUntil>=frame_&&textures_[i].expires>=frame_)previous[i]=(std::min)(textures_[i].rootUntil,textures_[i].expires);
        for(unsigned depth=1;depth<maximumHops;++depth){
            current.fill(0);bool progress=false;
            for(std::size_t i=0;i<textures_.size();++i){
                const auto &child=textures_[i];if(!child.texture)continue;
                for(const auto &edge:child.edges){
                    const auto *parent=parentOf(child,edge);if(!parent)continue;
                    const auto proof=previous[static_cast<std::size_t>(parent-textures_.data())];
                    const auto until=(std::min)(proof,edge.captured);
                    if(until>=frame_&&until>current[i]){current[i]=until;progress=true;}
                }
            }
            if(!progress)break;
            for(std::size_t i=0;i<textures_.size();++i)if(current[i]>best[i]){best[i]=current[i];bestHops[i]=depth;any=true;}
            previous=current;
        }
        if(!any)return;
        for(std::size_t i=0;i<textures_.size();++i){
            auto &item=textures_[i];if(!best[i]||!item.texture||item.expires<frame_)continue;
            if(best[i]>item.expires){item.expires=best[i];item.hops=bestHops[i];}
            else if(best[i]==item.expires)item.hops=(std::min)(item.hops,bestHops[i]);
        }
    }
};
} // namespace BsiShaders
