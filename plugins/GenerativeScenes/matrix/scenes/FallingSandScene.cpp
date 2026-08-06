#include "FallingSandScene.h"
#include <algorithm>
#include <cmath>

namespace {
void hsv_to_rgb(float h, float s, float v, uint8_t &r, uint8_t &g, uint8_t &b) {
    h=std::fmod(h,360.0f); if(h<0)h+=360; const float c=v*s, x=c*(1-std::fabs(std::fmod(h/60,2)-1)), m=v-c;
    float rf=0,gf=0,bf=0; if(h<60){rf=c;gf=x;}else if(h<120){rf=x;gf=c;}else if(h<180){gf=c;bf=x;}else if(h<240){gf=x;bf=c;}else if(h<300){rf=x;bf=c;}else{rf=c;bf=x;}
    r=static_cast<uint8_t>((rf+m)*255);g=static_cast<uint8_t>((gf+m)*255);b=static_cast<uint8_t>((bf+m)*255);
}
}
using namespace GenerativeScenes;

void FallingSandScene::register_properties(){ add_property(emitters_); add_property(spawn_rate_); add_property(water_); add_property(reset_fill_percent_); }
void FallingSandScene::initialize(int width,int height){ Scene::initialize(width,height); set_target_fps(45); reset(); }
void FallingSandScene::reset(){ cells_.assign(static_cast<size_t>(matrix_width*matrix_height),{}); frame_=0; }

bool FallingSandScene::render(rgb_matrix::FrameCanvas *canvas){
    if(cells_.size()!=static_cast<size_t>(matrix_width*matrix_height)) reset();
    ++frame_; std::uniform_int_distribution<int> chance(0,99);
    const int emitters=std::clamp(emitters_->get(),1,16), rate=std::clamp(spawn_rate_->get(),1,40);
    for(int e=0;e<emitters;++e){
        const int cx=((e+1)*matrix_width)/(emitters+1)+static_cast<int>(std::sin((frame_+e*37)*0.025f)*std::max(2,matrix_width/(emitters*3)));
        for(int n=0;n<rate;++n){ const int x=std::clamp(cx+(chance(rng_)%7)-3,0,matrix_width-1); Cell &c=cells_[index(x,0)]; if(c.type==0){c.type=(water_->get()&&chance(rng_)<20)?2:1;c.hue=static_cast<uint8_t>((frame_/2+e*37+n*3)%255);} }
    }

    for(int y=matrix_height-2;y>=0;--y){
        const bool left_first=((frame_+y)&1)==0;
        for(int xi=0;xi<matrix_width;++xi){ const int x=left_first?xi:(matrix_width-1-xi); Cell &c=cells_[index(x,y)]; if(c.type==0)continue;
            auto move=[&](int nx,int ny){ cells_[index(nx,ny)]=c; c={}; };
            if(cells_[index(x,y+1)].type==0){move(x,y+1);continue;}
            if(c.type==1){ int d=((x+y+frame_)&1)?1:-1; if(x+d>=0&&x+d<matrix_width&&cells_[index(x+d,y+1)].type==0){move(x+d,y+1);continue;} d=-d; if(x+d>=0&&x+d<matrix_width&&cells_[index(x+d,y+1)].type==0){move(x+d,y+1);continue;} }
            else { int d=((x+frame_)&1)?1:-1; bool moved=false; for(int dist=1;dist<=4;++dist){int nx=x+d*dist;if(nx<0||nx>=matrix_width)break;if(cells_[index(nx,y)].type==0){move(nx,y);moved=true;break;}} if(!moved){d=-d;for(int dist=1;dist<=4;++dist){int nx=x+d*dist;if(nx<0||nx>=matrix_width)break;if(cells_[index(nx,y)].type==0){move(nx,y);break;}}} }
        }
    }

    int occupied=0; canvas->Clear();
    for(int y=0;y<matrix_height;++y)for(int x=0;x<matrix_width;++x){ const Cell &c=cells_[index(x,y)]; if(!c.type)continue; ++occupied; uint8_t r,g,b; if(c.type==2){r=20;g=90;b=255;}else hsv_to_rgb(c.hue*360.0f/255.0f,0.82f,1.0f,r,g,b); canvas->SetPixel(x,y,r,g,b); }
    const int threshold=std::clamp(reset_fill_percent_->get(),25,95); if(occupied*100>=matrix_width*matrix_height*threshold) reset();
    wait_until_next_frame(); return true;
}
std::unique_ptr<Scenes::Scene> FallingSandSceneWrapper::create(){return std::make_unique<FallingSandScene>();}
