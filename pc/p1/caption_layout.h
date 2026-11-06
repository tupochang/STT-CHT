#pragma once
#include <algorithm>
namespace stt_caption {
struct Rect {int x,y,w,h;};
inline Rect fit_rect(int width,int height,int x,int y,int w,int h) {
    double scale=std::min(width/320.0,height/240.0);
    int ox=static_cast<int>((width-320*scale)/2),oy=static_cast<int>((height-240*scale)/2);
    return {ox+static_cast<int>(x*scale),oy+static_cast<int>((240-y-h)*scale),
        static_cast<int>(w*scale),static_cast<int>(h*scale)};
}
inline bool valid_text_origin(int x,int y,int lines) {
    return x>=0 && x<288 && y>=0 && lines>0 && y+lines*16<=240;
}
}
