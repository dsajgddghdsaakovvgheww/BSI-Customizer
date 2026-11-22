#pragma once
#include <array>

namespace BsiShaders {
// Arguments only, never pixels or a command buffer. Both operations retain the
// native ABI/arguments; observations are not a driver-success/pixel oracle.
struct GlColorTransfer {
    enum Kind { BlitFramebuffer, CopyTexSubImage2D } kind=BlitFramebuffer;
    std::array<int,4> source{},destination{};
    unsigned mask=0,filter=0,target=0;
    int level=0;
    static GlColorTransfer blit(int sx0,int sy0,int sx1,int sy1,int dx0,int dy0,int dx1,int dy1,unsigned mask,unsigned filter){
        return {BlitFramebuffer,{sx0,sy0,sx1,sy1},{dx0,dy0,dx1,dy1},mask,filter,0,0};
    }
    static GlColorTransfer copy(unsigned target,int level,int xoffset,int yoffset,int x,int y,int width,int height){
        return {CopyTexSubImage2D,{x,y,width,height},{xoffset,yoffset,0,0},0,0,target,level};
    }
};
} // namespace BsiShaders
