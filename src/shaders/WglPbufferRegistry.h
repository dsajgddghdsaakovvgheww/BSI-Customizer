#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace BsiShaders {
// Call only AFTER successful public native Get/Release/DestroyPbuffer calls,
// with the complete get/release/destroy hook family installed. A successful
// GetPbufferDC is native evidence of the exact DC, even for a pre-existing
// pbuffer. This is route classification ONLY, never presentation/ownership.
// Serialized by surfaceMutex; no GL calls, allocation or renderMutex waits.
class WglPbufferRegistry {
public:
    static constexpr std::size_t capacity=128;
    static constexpr unsigned maximumLeases=1024;
    bool acquired(std::uintptr_t pbuffer,std::uintptr_t dc){
        if(!pbuffer||!dc)return false;
        for(auto &entry:entries_)if(entry.pbuffer&&entry.pbuffer!=pbuffer&&entry.dc==dc){
            // Ambiguous/reused DC without observed release: neither route may
            // retain this identity. Generations are never recycled.
            entry={};for(auto &other:entries_)if(other.pbuffer==pbuffer)other={};return false;
        }
        for(auto &entry:entries_)if(entry.pbuffer==pbuffer){
            if((entry.leases&&entry.dc!=dc)||entry.leases>=maximumLeases){entry={};return false;}
            entry.dc=dc;++entry.leases;return true;
        }
        if(!nextGeneration_)return false;
        for(auto &entry:entries_)if(!entry.pbuffer){entry={pbuffer,dc,nextGeneration_++,1};return true;}
        return false;
    }
    bool released(std::uintptr_t pbuffer,std::uintptr_t dc){
        for(auto &entry:entries_)if(entry.pbuffer==pbuffer&&entry.dc==dc&&entry.leases){
            if(!--entry.leases)entry.dc=0;
            return true;
        }
        return false;
    }
    std::uintptr_t destroyed(std::uintptr_t pbuffer){
        for(auto &entry:entries_)if(entry.pbuffer==pbuffer){const auto generation=entry.generation;entry={};return generation;}
        return 0;
    }
    std::uintptr_t generation(std::uintptr_t pbuffer)const{
        if(pbuffer)for(const auto &entry:entries_)if(entry.pbuffer==pbuffer)return entry.generation;
        return 0;
    }
    std::uintptr_t resource(std::uintptr_t dc)const{
        if(dc)for(const auto &entry:entries_)if(entry.dc==dc&&entry.leases)return entry.generation;
        return 0;
    }
private:
    struct Entry {std::uintptr_t pbuffer=0,dc=0,generation=0;unsigned leases=0;};
    std::array<Entry,capacity> entries_{};
    std::uintptr_t nextGeneration_=1;
};
} // namespace BsiShaders
