#pragma once
#include <array>

namespace BsiShaders {
// Arguments only, never pixels or a command buffer. All operations retain the
// native ABI/arguments; observations are not a driver-success/pixel oracle.
struct GlColorTransfer {
    enum Kind { BlitFramebuffer, CopyTexSubImage2D, CopyImage } kind=BlitFramebuffer;
    std::array<int,4> source{},destination{};
    unsigned mask=0,filter=0,target=0;
    int level=0;
    unsigned sourceName=0,sourceTarget=0,destinationName=0,destinationTarget=0;
    int sourceLevel=0,destinationLevel=0,sourceZ=0,destinationZ=0,depth=0;
    unsigned imageApis=1; // core/ARB=1, EXT=2, OES=4, NV=8; exact observed address contracts
    static GlColorTransfer blit(int sx0,int sy0,int sx1,int sy1,int dx0,int dy0,int dx1,int dy1,unsigned mask,unsigned filter){
        return {BlitFramebuffer,{sx0,sy0,sx1,sy1},{dx0,dy0,dx1,dy1},mask,filter,0,0};
    }
    static GlColorTransfer copy(unsigned target,int level,int xoffset,int yoffset,int x,int y,int width,int height){
        return {CopyTexSubImage2D,{x,y,width,height},{xoffset,yoffset,0,0},0,0,target,level};
    }
    static GlColorTransfer image(unsigned src,unsigned srcTarget,int srcLevel,int sx,int sy,int sz,
                                 unsigned dst,unsigned dstTarget,int dstLevel,int dx,int dy,int dz,int width,int height,int depth){
        GlColorTransfer call;call.kind=CopyImage;call.source={sx,sy,width,height};call.destination={dx,dy,0,0};
        call.sourceName=src;call.sourceTarget=srcTarget;call.sourceLevel=srcLevel;call.sourceZ=sz;
        call.destinationName=dst;call.destinationTarget=dstTarget;call.destinationLevel=dstLevel;call.destinationZ=dz;call.depth=depth;return call;
    }
};
} // namespace BsiShaders
