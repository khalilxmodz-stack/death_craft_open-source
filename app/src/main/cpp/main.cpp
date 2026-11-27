#include <android_native_app_glue.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <algorithm>
#include <cstdint>
#include <string>
#include <initializer_list>
#include <tuple>
#include <time.h>
#include <android/asset_manager.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"DeathCraft",__VA_ARGS__)
static constexpr int CS=16, HEIGHT=32, LOAD_RADIUS=4;
static constexpr float PI=3.14159265f;
static constexpr int SEA_LEVEL=6;

struct V3 { float x,y,z; };
struct Mat4 { float m[16]{}; };
static Mat4 identity(){Mat4 r{};r.m[0]=r.m[5]=r.m[10]=r.m[15]=1;return r;}
static Mat4 mul(const Mat4&a,const Mat4&b){Mat4 r{};for(int c=0;c<4;c++)for(int y=0;y<4;y++)for(int k=0;k<4;k++)r.m[c*4+y]+=a.m[k*4+y]*b.m[c*4+k];return r;}
static Mat4 perspective(float f,float a,float n,float z){Mat4 r{};float t=1/std::tan(f*.5f);r.m[0]=t/a;r.m[5]=t;r.m[10]=(z+n)/(n-z);r.m[11]=-1;r.m[14]=2*z*n/(n-z);return r;}
static Mat4 camera(float x,float y,float z,float yaw,float pitch){
    V3 f{std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
    V3 right{std::cos(yaw),0,std::sin(yaw)};
    V3 up{-std::sin(yaw)*std::sin(pitch),std::cos(pitch),std::cos(yaw)*std::sin(pitch)};
    Mat4 r=identity();
    r.m[0]=right.x;r.m[4]=right.y;r.m[8]=right.z;
    r.m[1]=up.x;r.m[5]=up.y;r.m[9]=up.z;
    r.m[2]=-f.x;r.m[6]=-f.y;r.m[10]=-f.z;
    r.m[12]=-(right.x*x+right.y*y+right.z*z);
    r.m[13]=-(up.x*x+up.y*y+up.z*z);
    r.m[14]=f.x*x+f.y*y+f.z*z;return r;
}
static int floorDiv(int a,int b){int q=a/b,r=a%b;return r<0?q-1:q;}
static int modp(int a,int b){int r=a%b;return r<0?r+b:r;}
static uint32_t terrainHash(int x,int z){
    uint32_t h=(uint32_t)x*0x8da6b343u^(uint32_t)z*0xd8163841u;
    h^=h>>13;h*=0x85ebca6bu;h^=h>>16;h*=0xc2b2ae35u;h^=h>>16;return h;
}
static float terrainNoise(float x,float z){
    int ix=(int)std::floor(x),iz=(int)std::floor(z);float fx=x-ix,fz=z-iz;
    fx=fx*fx*(3-2*fx);fz=fz*fz*(3-2*fz);
    auto sample=[](int a,int b){return (terrainHash(a,b)&0x00ffffffu)/16777215.0f;};
    float a=sample(ix,iz),b=sample(ix+1,iz),c=sample(ix,iz+1),d=sample(ix+1,iz+1);
    return (a+(b-a)*fx)+((c+(d-c)*fx)-(a+(b-a)*fx))*fz;
}
static int terrainHeight(int x,int z){
    float broad=terrainNoise(x*.018f+17,z*.018f-31);
    float hills=terrainNoise(x*.075f-43,z*.075f+11);
    float detail=terrainNoise(x*.16f+71,z*.16f-59);
    float mountain=terrainNoise(x*.012f+107,z*.012f-83);
    float peaks=terrainNoise(x*.038f-37,z*.038f+91);
    float small=(broad-.5f)*3.4f+(hills-.5f)*3.8f+(detail-.5f)*1.2f;
    float high=std::pow(std::max(0.0f,(mountain-.60f)/.40f),1.45f)*15.0f+
               std::pow(std::max(0.0f,(peaks-.70f)/.30f),2.0f)*5.0f;
    return std::clamp((int)std::lround(3.0f+small+high),1,HEIGHT-7);
}
static uint64_t chunkKey(int x,int z){return (uint64_t)(uint32_t)x<<32|(uint32_t)z;}

struct Chunk {
    int cx=0,cz=0;
    std::array<uint8_t,CS*CS*HEIGHT> blocks{};
    std::vector<float> mesh;
    bool dirty=true;
    uint8_t get(int x,int y,int z)const{return (x<0||x>=CS||z<0||z>=CS||y<0||y>=HEIGHT)?0:blocks[(y*CS+z)*CS+x];}
    void set(int x,int y,int z,uint8_t b){if(x>=0&&x<CS&&z>=0&&z<CS&&y>=0&&y<HEIGHT){blocks[(y*CS+z)*CS+x]=b;dirty=true;}}
};

struct LanPacket { uint32_t magic=0x44434654u; uint8_t type=0; uint32_t player=0; float x=0,y=0,z=0,yaw=0; int32_t bx=0,by=0,bz=0; uint8_t block=0; };

class Game {
    EGLDisplay display=EGL_NO_DISPLAY; EGLSurface surface=EGL_NO_SURFACE; EGLContext context=EGL_NO_CONTEXT;
    GLuint program=0,vao=0,vbo=0,atlasTexture=0; GLint mvpLoc=-1,cameraLoc=-1,fogLoc=-1,textureLoc=-1,atlasLoc=-1;
    int width=1,height=1;
    std::map<uint64_t,Chunk> chunks;
    std::map<std::tuple<int,int,int>,uint8_t> edits;
    float px=0,feet=1.0f,pz=0,yaw=0,pitch=0,vy=0;
    bool grounded=false,running=false;
    float joyX=0,joyY=0,lastX=0,lastY=0,joyOriginX=-.76f,joyOriginY=-.65f;
    int joyId=-1,lookId=-1;
    bool inventoryOpen=false,breaking=false;
    int selected=0,breakPointer=-1,breakX=0,breakY=0,breakZ=0,dragIndex=-1,dragPointer=-1;
    float breakTimer=0;
    int health=10,armor=10,grafikHealth=2;
    bool creativeMode=true,flatWorld=false,inWorld=false;
    int selectedSlot=0;
    std::string saveDirectory;
    float saveTimer=0;
    float grafikX=5.0f,grafikZ=-5.0f,grafikFeet=0,grafikRespawn=0,swallowTimer=0;
    bool grafikAlive=true,playerDead=false;
    struct Item{uint8_t id=0;int count=0;};std::array<Item,20> inventory{};
    std::string status="DEATH_CRAFT  |  V0.1";
    int statusFrames=0;
    int lanSocket=-1,lanMode=0;
    uint32_t networkId=0;
    float lanTimer=0,helloTimer=0,remoteTimer=0;
    bool remoteValid=false;
    float remoteX=0,remoteY=0,remoteZ=0,remoteYaw=0;
    sockaddr_in lanBroadcast{};

    void setLanMode(int mode){
        if(lanSocket<0){
            lanSocket=socket(AF_INET,SOCK_DGRAM,0);
            if(lanSocket<0){status="LAN SOCKET ERROR";statusFrames=120;return;}
            int yes=1;setsockopt(lanSocket,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
#ifdef SO_REUSEPORT
            setsockopt(lanSocket,SOL_SOCKET,SO_REUSEPORT,&yes,sizeof(yes));
#endif
            setsockopt(lanSocket,SOL_SOCKET,SO_BROADCAST,&yes,sizeof(yes));
            sockaddr_in local{};local.sin_family=AF_INET;local.sin_port=htons(27015);local.sin_addr.s_addr=htonl(INADDR_ANY);
            if(bind(lanSocket,(sockaddr*)&local,sizeof(local))<0){close(lanSocket);lanSocket=-1;status="LAN PORT BUSY";statusFrames=120;return;}
            fcntl(lanSocket,F_SETFL,fcntl(lanSocket,F_GETFL,0)|O_NONBLOCK);
            networkId=(uint32_t)getpid()^(uint32_t)time(nullptr);if(!networkId)networkId=1;
            lanBroadcast.sin_family=AF_INET;lanBroadcast.sin_port=htons(27015);lanBroadcast.sin_addr.s_addr=htonl(INADDR_BROADCAST);
        }
        lanMode=mode;remoteValid=false;remoteTimer=0;lanTimer=0;helloTimer=0;
        status=mode==1?"LAN HOSTING - WAITING":"LAN SEARCHING FOR HOST";statusFrames=180;
    }
    void sendLan(const LanPacket&p){if(lanSocket>=0)sendto(lanSocket,&p,sizeof(p),0,(sockaddr*)&lanBroadcast,sizeof(lanBroadcast));}
    void updateLan(float dt){
        if(lanSocket<0||lanMode==0)return;
        LanPacket p{};sockaddr_in from{};socklen_t fromLen=sizeof(from);
        while(recvfrom(lanSocket,&p,sizeof(p),0,(sockaddr*)&from,&fromLen)==sizeof(p)){
            if(p.magic!=0x44434654u||p.player==networkId)continue;
            if(p.type==1&&lanMode==1){LanPacket reply{};reply.type=2;reply.player=networkId;reply.x=px;reply.y=feet;reply.z=pz;reply.yaw=yaw;sendLan(reply);}
            else if(p.type==2){remoteValid=true;remoteTimer=0;remoteX=p.x;remoteY=p.y;remoteZ=p.z;remoteYaw=p.yaw;}
            else if(p.type==3){int cx=floorDiv(p.bx,CS),cz=floorDiv(p.bz,CS);Chunk&c=ensureChunk(cx,cz);c.set(modp(p.bx,CS),p.by,modp(p.bz,CS),p.block);edits[{p.bx,p.by,p.bz}]=p.block;for(auto&entry:chunks)entry.second.dirty=true;}
            fromLen=sizeof(from);
        }
        lanTimer+=dt;helloTimer+=dt;remoteTimer+=dt;
        if(lanTimer>=.10f){lanTimer=0;LanPacket state{};state.type=2;state.player=networkId;state.x=px;state.y=feet;state.z=pz;state.yaw=yaw;sendLan(state);}
        if(lanMode==2&&helloTimer>=.50f){helloTimer=0;LanPacket hello{};hello.type=1;hello.player=networkId;sendLan(hello);}
        if(remoteTimer>3.0f)remoteValid=false;
    }

    GLuint makeShader(GLenum type,const char*src){GLuint s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[1024];glGetShaderInfoLog(s,1024,nullptr,log);LOGE("Shader: %s",log);}return s;}
    bool setupGL(){
        const char*vs=R"(#version 300 es
        layout(location=0) in vec3 p; layout(location=1) in vec3 c; layout(location=2) in vec2 uvIn;
        uniform mat4 mvp; uniform vec3 cameraPos; out vec3 color; out float fogDistance; out vec2 texCoord;
        void main(){gl_Position=mvp*vec4(p,1.0);color=c;texCoord=uvIn;fogDistance=length(p-cameraPos);})";
        const char*fs=R"(#version 300 es
        precision highp float; in vec3 color; in float fogDistance; in vec2 texCoord; uniform float fogEnabled; uniform float textured; uniform sampler2D blockAtlas; out vec4 outColor;
        void main(){vec3 base=color;if(textured>.5)base*=texture(blockAtlas,texCoord).rgb;vec3 sky=vec3(.48,.74,.96);float fog=fogEnabled*smoothstep(22.0,68.0,fogDistance);outColor=vec4(mix(base,sky,fog),1.0);})";
        GLuint a=makeShader(GL_VERTEX_SHADER,vs),b=makeShader(GL_FRAGMENT_SHADER,fs);
        program=glCreateProgram();glAttachShader(program,a);glAttachShader(program,b);glLinkProgram(program);glDeleteShader(a);glDeleteShader(b);
        GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok)return false;
        mvpLoc=glGetUniformLocation(program,"mvp");cameraLoc=glGetUniformLocation(program,"cameraPos");fogLoc=glGetUniformLocation(program,"fogEnabled");textureLoc=glGetUniformLocation(program,"textured");atlasLoc=glGetUniformLocation(program,"blockAtlas");
        glUseProgram(program);glUniform1i(atlasLoc,0);
        glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);
        glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);return true;
    }
    void addTri(std::vector<float>&v,V3 a,V3 b,V3 c,V3 color,float au=0,float av=0,float bu=1,float bv=0,float cu=1,float cv=1){
        v.insert(v.end(),{a.x,a.y,a.z,color.x,color.y,color.z,au,av,b.x,b.y,b.z,color.x,color.y,color.z,bu,bv,c.x,c.y,c.z,color.x,color.y,color.z,cu,cv});
    }
    void addFace(std::vector<float>&v,V3 a,V3 b,V3 c,V3 d,V3 color){
        addTri(v,a,b,c,color,0,0,1,0,1,1);addTri(v,a,c,d,color,0,0,1,1,0,1);
    }
    bool loadTextures(AAssetManager* manager){
        if(!manager)return false;
        constexpr int tile=256, atlasW=tile*4;
        std::vector<unsigned char> atlas((size_t)atlasW*tile*4,255);
        const char* names[]={"textures/grass_texture.png","textures/dirt_texture.png","textures/stone_texture.png","textures/grass_tree_texture.png"};
        const unsigned char fallback[4][4]={{78,156,55,255},{126,79,43,255},{116,122,130,255},{49,126,48,255}};
        for(int layer=0;layer<4;layer++)for(int y=0;y<tile;y++)for(int x=0;x<tile;x++){
            size_t dst=((size_t)y*atlasW+(size_t)layer*tile+x)*4;
            std::copy(fallback[layer],fallback[layer]+4,atlas.begin()+dst);
        }
        int loaded=0;stbi_set_flip_vertically_on_load(1);
        for(int layer=0;layer<4;layer++){
            AAsset* asset=AAssetManager_open(manager,names[layer],AASSET_MODE_BUFFER);
            if(!asset){LOGE("Texture asset missing: %s",names[layer]);continue;}
            off_t length=AAsset_getLength(asset);std::vector<unsigned char> encoded((size_t)length);
            int read=(int)AAsset_read(asset,encoded.data(),(size_t)length);AAsset_close(asset);
            if(read<=0)continue;
            int w=0,h=0,channels=0;unsigned char* pixels=stbi_load_from_memory(encoded.data(),read,&w,&h,&channels,4);
            if(!pixels){LOGE("Texture decode failed: %s",names[layer]);continue;}
            for(int y=0;y<tile;y++)for(int x=0;x<tile;x++){
                int sx=x*w/tile,sy=y*h/tile;size_t src=((size_t)sy*w+sx)*4;
                size_t dst=((size_t)y*atlasW+(size_t)layer*tile+x)*4;
                std::copy(pixels+src,pixels+src+4,atlas.begin()+dst);
            }
            stbi_image_free(pixels);loaded++;
        }
        glGenTextures(1,&atlasTexture);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,atlasTexture);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,atlasW,tile,0,GL_RGBA,GL_UNSIGNED_BYTE,atlas.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glGenerateMipmap(GL_TEXTURE_2D);
        LOGE("Loaded block textures: %d/3",loaded);return loaded>0;
    }
    int groundHeight(int x,int z)const{return flatWorld?8:terrainHeight(x,z);}
    std::string savePath()const{return saveDirectory+"/deathcraft_world_"+std::to_string(selectedSlot)+".dat";}
    void saveWorld(){
        if(!inWorld||saveDirectory.empty())return;
        std::ofstream out(savePath(),std::ios::binary|std::ios::trunc);if(!out)return;
        const char magic[8]={'D','C','W','O','R','L','D','1'};out.write(magic,8);
        int32_t version=1,mode=creativeMode?1:0,flat=flatWorld,slot=selectedSlot,hp=health,ap=armor,sel=selected;uint32_t count=(uint32_t)edits.size();
        out.write((char*)&version,4);out.write((char*)&mode,4);out.write((char*)&flat,4);out.write((char*)&slot,4);
        out.write((char*)&px,sizeof(px));out.write((char*)&feet,sizeof(feet));out.write((char*)&pz,sizeof(pz));out.write((char*)&yaw,sizeof(yaw));out.write((char*)&pitch,sizeof(pitch));
        out.write((char*)&hp,4);out.write((char*)&ap,4);out.write((char*)&sel,4);
        for(const auto&i:inventory){out.write((char*)&i.id,1);int32_t n=i.count;out.write((char*)&n,4);}
        out.write((char*)&count,4);
        for(const auto&e:edits){int32_t x=std::get<0>(e.first),y=std::get<1>(e.first),z=std::get<2>(e.first);uint8_t b=e.second;out.write((char*)&x,4);out.write((char*)&y,4);out.write((char*)&z,4);out.write((char*)&b,1);}
    }
    bool loadWorld(){
        std::ifstream in(savePath(),std::ios::binary);if(!in)return false;
        char magic[8]{};in.read(magic,8);if(!in||std::memcmp(magic,"DCWORLD1",8)!=0)return false;
        int32_t version=0,mode=1,flat=0,slot=0,hp=10,ap=10,sel=0;uint32_t count=0;
        in.read((char*)&version,4);in.read((char*)&mode,4);in.read((char*)&flat,4);in.read((char*)&slot,4);
        if(!in||version!=1)return false;
        in.read((char*)&px,sizeof(px));in.read((char*)&feet,sizeof(feet));in.read((char*)&pz,sizeof(pz));in.read((char*)&yaw,sizeof(yaw));in.read((char*)&pitch,sizeof(pitch));
        in.read((char*)&hp,4);in.read((char*)&ap,4);in.read((char*)&sel,4);
        for(auto&i:inventory){int32_t n=0;in.read((char*)&i.id,1);in.read((char*)&n,4);i.count=std::clamp(n,0,999999);}
        in.read((char*)&count,4);if(!in||count>2000000)return false;
        edits.clear();for(uint32_t i=0;i<count;i++){int32_t x,y,z;uint8_t b;in.read((char*)&x,4);in.read((char*)&y,4);in.read((char*)&z,4);in.read((char*)&b,1);if(!in)return false;edits[{x,y,z}]=b;}
        creativeMode=mode!=0;flatWorld=flat!=0;health=std::clamp(hp,0,10);armor=std::clamp(ap,0,10);selected=std::clamp(sel,0,4);return true;
    }
    void startWorld(){
        chunks.clear();edits.clear();inventory={};health=10;armor=10;grafikHealth=2;grafikAlive=true;playerDead=false;swallowTimer=0;
        px=0;pz=0;yaw=0;pitch=0;vy=0;feet=(float)groundHeight(0,0);grounded=true;
        bool loaded=loadWorld();
        if(!loaded){
            px=0;pz=0;
            if(!flatWorld){for(int radius=0;radius<80&&groundHeight((int)px,(int)pz)<=SEA_LEVEL;radius++){px=(float)(radius+1);pz=(float)((radius%2)?radius/2:-radius/2);}}
            feet=(float)groundHeight((int)px,(int)pz);
            inventory[0]=creativeMode?Item{1,999999}:Item{1,32};inventory[1]=creativeMode?Item{2,999999}:Item{2,16};
            inventory[2]=creativeMode?Item{3,999999}:Item{3,12};inventory[3]=creativeMode?Item{4,999999}:Item{4,4};
            inventory[4]=creativeMode?Item{5,999999}:Item{5,8};inventory[5]=creativeMode?Item{6,999999}:Item{6,8};
            inventory[6]=creativeMode?Item{7,999999}:Item{7,0};inventory[7]=creativeMode?Item{8,999999}:Item{8,0};
        }
        if(creativeMode)for(auto&i:inventory)if(i.id)i.count=999999;
        grafikX=6;grafikZ=-5;grafikFeet=(float)groundHeight(6,-5);selected=0;inWorld=true;saveTimer=0;updateWorld();status=loaded?"WORLD LOADED":"WORLD CREATED";statusFrames=120;
    }
    uint8_t blockAt(int x,int y,int z)const{
        int cx=floorDiv(x,CS),cz=floorDiv(z,CS);auto it=chunks.find(chunkKey(cx,cz));
        return it==chunks.end()?0:it->second.get(modp(x,CS),y,modp(z,CS));
    }
    void addBlockFace(std::vector<float>&v,V3 a,V3 b,V3 c,V3 d,uint8_t block,int face,int gx,int gy,int gz){
        int tile=block==3?2:(block==5?3:((block==1&&face==2)?0:1));
        float u0=(tile+.012f)/4.0f,u1=(tile+.988f)/4.0f,v0=.012f,v1=.988f;
        const float shade[6]={.72f,.84f,1.0f,.38f,.62f,.78f};float light=shade[face];
        if(block==1&&face==2){
            bool underTree=false;
            for(int dz=-2;dz<=2&&!underTree;dz++)for(int dx=-2;dx<=2&&!underTree;dx++)
                for(int dy=2;dy<=5;dy++)if(blockAt(gx+dx,gy+dy,gz+dz)==5){underTree=true;break;}
            if(underTree)light*=.52f;
        }
        V3 color=block==9?V3{.22f*light,.52f*light,.88f*light}:V3{light,light,light};
        addTri(v,a,b,c,color,u0,v0,u1,v0,u1,v1);addTri(v,a,c,d,color,u0,v0,u1,v1,u0,v1);
    }
    bool solid(int x,int y,int z){
        int cx=floorDiv(x,CS),cz=floorDiv(z,CS);auto it=chunks.find(chunkKey(cx,cz));
        if(it==chunks.end())return y<0;
        uint8_t b=it->second.get(modp(x,CS),y,modp(z,CS));return b!=0&&b!=9;
    }
    void generate(Chunk&c){
        int ox=c.cx*CS,oz=c.cz*CS;
        for(int z=0;z<CS;z++)for(int x=0;x<CS;x++){
            int h=groundHeight(ox+x,oz+z);
            for(int y=0;y<h;y++)c.set(x,y,z,y==h-1?1:(y>=h-4?2:3));
            if(h<SEA_LEVEL)for(int y=h;y<SEA_LEVEL;y++)c.set(x,y,z,9);
        }
        auto put=[&](int x,int y,int z,uint8_t id){if(floorDiv(x,CS)==c.cx&&floorDiv(z,CS)==c.cz)c.set(modp(x,CS),y,modp(z,CS),id);};
        for(int z=oz-2;z<oz+CS+2;z++)for(int x=ox-2;x<ox+CS+2;x++){
            if(modp(x+3,8)||modp(z+5,8))continue;
            uint32_t h=terrainHash(x,z);if(h%3==0)continue;
            int ground=groundHeight(x,z);if(ground>7||ground<SEA_LEVEL)continue;
            int trunk=3+(int)(h%2);
            for(int y=0;y<trunk;y++)put(x,ground+y,z,4);
            int top=ground+trunk;
            for(int dy=-1;dy<=1;dy++)for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
                if(std::abs(dx)==1&&std::abs(dz)==1&&dy!=1)continue;
                if(dx==0&&dz==0&&dy<=0)continue;
                put(x+dx,top+dy,z+dz,5);
            }
            put(x,top+1,z,5);
        }
        for(const auto& e:edits){
            int x=std::get<0>(e.first), y=std::get<1>(e.first), z=std::get<2>(e.first);
            if(floorDiv(x,CS)==c.cx && floorDiv(z,CS)==c.cz)
                c.set(modp(x,CS),y,modp(z,CS),e.second);
        }
        c.dirty=true;
    }
    Chunk& ensureChunk(int cx,int cz){
        uint64_t k=chunkKey(cx,cz);auto it=chunks.find(k);
        if(it==chunks.end()){Chunk c;c.cx=cx;c.cz=cz;generate(c);it=chunks.emplace(k,std::move(c)).first;}
        return it->second;
    }
    V3 blockColor(uint8_t b,int face){
        if(b==9)return{.12f,.42f,.82f};if(b==1){if(face==2)return{.22f,.68f,.18f};return{.42f,.29f,.16f};}
        if(b==2)return{.48f,.31f,.17f};if(b==3)return{.43f,.45f,.48f};
        if(b==4)return{.43f,.27f,.12f};if(b==5)return{.12f,.48f,.13f};if(b==6)return{.67f,.46f,.24f};return{.72f,.62f,.35f};
    }
    float breakDuration(uint8_t b)const{switch(b){case 1:return .65f;case 2:return .90f;case 3:return 2.10f;case 4:return 1.25f;case 5:return .30f;case 6:return 1.10f;default:return .85f;}}
    bool addItem(uint8_t id,int n){
        if(!id||n<=0)return true;
        for(auto &i:inventory)if(i.id==id&&i.count>0&&i.count<64&&n>0){int add=std::min(64-i.count,n);i.count+=add;n-=add;}
        while(n>0){auto it=std::find_if(inventory.begin(),inventory.end(),[](const Item&i){return i.count<=0;});if(it==inventory.end()){status="Inventory full";statusFrames=90;return false;}int put=std::min(64,n);*it={id,put};n-=put;}
        return true;
    }
    uint8_t selectedBlock()const{return inventory[selected].count?inventory[selected].id:1;}
    void consumeSelected(){if(!creativeMode&&inventory[selected].count>0&&--inventory[selected].count==0)inventory[selected].id=0;}
    void craft(int r){uint8_t src=r==0?4:6;int n=r==0?1:(r==1?2:4);for(auto&i:inventory)if(i.id==src&&n){int k=std::min(n,i.count);i.count-=k;n-=k;if(!i.count)i.id=0;}if(n){status=r==0?"Need wood":"Need planks";statusFrames=90;return;}addItem(r==0?6:(r==1?7:8),r==0?4:(r==1?4:1));}
    int inventorySlotAt(float x,float y)const{for(int r=0;r<4;r++)for(int c=0;c<5;c++){float bx=-.58f+c*.235f,by=.30f-r*.245f;if(x>=bx&&x<=bx+.19f&&y>=by-.19f&&y<=by+.02f)return r*5+c;}return -1;}
    void rebuild(Chunk&c){
        c.mesh.clear();int ox=c.cx*CS,oz=c.cz*CS;
        for(int y=0;y<HEIGHT;y++)for(int z=0;z<CS;z++)for(int x=0;x<CS;x++){
            uint8_t b=c.get(x,y,z);if(!b)continue;
            float X=(float)(ox+x),Y=(float)y,Z=(float)(oz+z);
            V3 p000{X,Y,Z},p100{X+1,Y,Z},p110{X+1,Y+1,Z},p010{X,Y+1,Z};
            V3 p001{X,Y,Z+1},p101{X+1,Y,Z+1},p111{X+1,Y+1,Z+1},p011{X,Y+1,Z+1};
            if(!solid(ox+x,y,oz+z-1)&&c.get(x,y,z-1)!=9)addBlockFace(c.mesh,p000,p100,p110,p010,b,0,ox+x,y,oz+z);
            if(!solid(ox+x,y,oz+z+1)&&c.get(x,y,z+1)!=9)addBlockFace(c.mesh,p101,p001,p011,p111,b,1,ox+x,y,oz+z);
            if(!solid(ox+x,y+1,oz+z)&&c.get(x,y+1,z)!=9)addBlockFace(c.mesh,p010,p110,p111,p011,b,2,ox+x,y,oz+z);
            if(!solid(ox+x,y-1,oz+z)&&c.get(x,y-1,z)!=9)addBlockFace(c.mesh,p001,p101,p100,p000,b,3,ox+x,y,oz+z);
            if(!solid(ox+x-1,y,oz+z)&&c.get(x-1,y,z)!=9)addBlockFace(c.mesh,p001,p000,p010,p011,b,4,ox+x,y,oz+z);
            if(!solid(ox+x+1,y,oz+z)&&c.get(x+1,y,z)!=9)addBlockFace(c.mesh,p100,p101,p111,p110,b,5,ox+x,y,oz+z);
        }
        c.dirty=false;
    }
    void draw(const std::vector<float>&v,GLenum mode,const Mat4&m,float cx=0,float cy=0,float cz=0,bool fog=true,bool textured=false){
        if(v.empty())return;glUseProgram(program);glUniformMatrix4fv(mvpLoc,1,GL_FALSE,m.m);glUniform3f(cameraLoc,cx,cy,cz);glUniform1f(fogLoc,fog?1.0f:0.0f);glUniform1f(textureLoc,textured&&atlasTexture?1.0f:0.0f);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,atlasTexture);
        glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);
        glEnableVertexAttribArray(1);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float)));
        glEnableVertexAttribArray(2);glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(6*sizeof(float)));
        glDrawArrays(mode,0,(GLsizei)(v.size()/8));
    }
    void drawHud(){
        glDisable(GL_DEPTH_TEST);Mat4 m=identity();std::vector<float>v;
        auto t=[&](float x,float y,float X,float Y,float Z,float W,V3 c){addTri(v,{x,y,0},{X,Y,0},{Z,W,0},c);};
        auto rect=[&](float x,float y,float w,float h,V3 c){t(x,y,x+w,y,x+w,y+h,c);t(x,y,x+w,y+h,x,y+h,c);};
        auto circ=[&](float x,float y,float r,V3 c){for(int i=0;i<32;i++){float a=i*2*PI/32,b=(i+1)*2*PI/32;t(x,y,x+cosf(a)*r,y+sinf(a)*r,x+cosf(b)*r,y+sinf(b)*r,c);}};
        auto line=[&](float x,float y,float X,float Y,float q,V3 c){float dx=X-x,dy=Y-y,l=sqrtf(dx*dx+dy*dy);if(l<.001f)return;float ox=-dy/l*q,oy=dx/l*q;t(x+ox,y+oy,X+ox,Y+oy,X-ox,Y-oy,c);t(x+ox,y+oy,X-ox,Y-oy,x-ox,y-oy,c);};
        rect(-.94f,.57f,.20f,.12f,lanMode==1?V3{.12f,.55f,.20f}:V3{.04f,.04f,.05f});
        rect(-.70f,.57f,.20f,.12f,lanMode==2?V3{.12f,.35f,.65f}:V3{.04f,.04f,.05f});
        drawText("HOST",-.91f,.61f,.012f,{1,1,1});drawText("JOIN",-.67f,.61f,.012f,{1,1,1});
        circ(joyOriginX,joyOriginY,.255f,{.01f,.01f,.015f});circ(joyOriginX,joyOriginY,.22f,{.14f,.14f,.16f});circ(joyOriginX+joyX*.16f,joyOriginY+joyY*.16f,.105f,{.01f,.01f,.015f});
        circ(.78f,-.62f,.14f,{.01f,.01f,.015f});circ(.78f,-.62f,.115f,{.17f,.17f,.19f});
        circ(.78f,-.10f,.14f,{.01f,.01f,.015f});circ(.78f,-.10f,.115f,{.17f,.17f,.19f});
        circ(.53f,-.40f,.14f,{.01f,.01f,.015f});circ(.53f,-.40f,.115f,{.17f,.17f,.19f});
        circ(.53f,-.10f,.14f,{.01f,.01f,.015f});circ(.53f,-.10f,.115f,{.17f,.17f,.19f});
        line(.78f,-.65f,.78f,-.56f,.018f,{1,1,1});line(.78f,-.56f,.74f,-.60f,.018f,{1,1,1});line(.78f,-.56f,.82f,-.60f,.018f,{1,1,1});
        line(.72f,-.12f,.84f,-.02f,.018f,{1,1,1});line(.73f,-.02f,.85f,-.02f,.018f,{1,1,1});
        line(.48f,-.45f,.58f,-.45f,.014f,{1,1,1});line(.48f,-.45f,.48f,-.35f,.014f,{1,1,1});line(.58f,-.45f,.58f,-.35f,.014f,{1,1,1});line(.48f,-.35f,.58f,-.35f,.014f,{1,1,1});
        line(.49f,-.14f,.57f,-.06f,.018f,{1,.35f,.25f});line(.50f,-.07f,.56f,-.13f,.018f,{1,.35f,.25f});
        line(-.034f,0,-.009f,0,.006f,{.015f,.015f,.02f});line(.009f,0,.034f,0,.006f,{.015f,.015f,.02f});
        line(0,-.034f,0,-.009f,.006f,{.015f,.015f,.02f});line(0,.009f,0,.034f,.006f,{.015f,.015f,.02f});
        line(-.032f,0,-.009f,0,.0025f,{1,1,1});line(.009f,0,.032f,0,.0025f,{1,1,1});
        line(0,-.032f,0,-.009f,.0025f,{1,1,1});line(0,.009f,0,.032f,.0025f,{1,1,1});
        if(breaking){
            int tx,ty,tz,ax,ay,az;
            if(raycast(tx,ty,tz,ax,ay,az)){
                auto it=chunks.find(chunkKey(floorDiv(tx,CS),floorDiv(tz,CS)));
                uint8_t block=it==chunks.end()?0:it->second.get(modp(tx,CS),ty,modp(tz,CS));
                if(block){float progress=std::clamp(breakTimer/breakDuration(block),0.0f,1.0f);
                    rect(-.125f,.145f,.25f,.022f,{.015f,.015f,.02f});
                    rect(-.122f,.148f,.244f*progress,.016f,{1.0f,.72f,.16f});
                    drawText(std::to_string((int)(progress*100))+"%",-.035f,.195f,.014f,{1,1,1});
                }
            }
        }
        for(int i=0;i<5;i++){
            float hx=-.91f+i*.075f,hy=.88f;V3 hc=health>=((i+1)*2)?V3{.92f,.10f,.14f}:(health==i*2+1?V3{.92f,.42f,.12f}:V3{.20f,.08f,.09f});
            circ(hx-.012f,hy+.006f,.020f,hc);circ(hx+.012f,hy+.006f,.020f,hc);t(hx-.032f,hy,hx+.032f,hy,hx,hy-.050f,hc);
            float sx=-.91f+i*.075f,sy=.77f;V3 sc=armor>=((i+1)*2)?V3{.30f,.67f,.95f}:V3{.13f,.19f,.24f};
            t(sx-.026f,sy+.025f,sx+.026f,sy+.025f,sx,sy-.042f,sc);t(sx-.026f,sy+.025f,sx,sy-.042f,sx-.022f,sy-.005f,sc);t(sx+.026f,sy+.025f,sx+.022f,sy-.005f,sx,sy-.042f,sc);
        }
        rect(.84f,.79f,.13f,.15f,{.01f,.01f,.015f});rect(.86f,.81f,.09f,.11f,{.2f,.2f,.22f});line(.87f,.87f,.94f,.87f,.009f,{1,1,1});line(.87f,.83f,.94f,.83f,.009f,{1,1,1});
        for(int i=0;i<5;i++){float x=-.28f+i*.115f;rect(x,-.92f,.105f,.16f,i==selected?V3{1,.78f,.18f}:V3{.01f,.01f,.015f});rect(x+.008f,-.912f,.089f,.144f,{.2f,.2f,.22f});if(inventory[i].count){rect(x+.025f,-.88f,.055f,.075f,blockColor(inventory[i].id,2));drawText(inventory[i].count>=999999?"INF":std::to_string(inventory[i].count),x+.060f,-.89f,.010f,{1,1,1});}}
        draw(v,GL_TRIANGLES,m,0,0,0,false);if(statusFrames>0){drawText(status,-.85f,.66f,.018f,{1,1,1});statusFrames--;}
        if(playerDead){float k=std::clamp(swallowTimer/2.8f,0.0f,1.0f);std::vector<float> overlay;addFace(overlay,{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0},{.015f+.035f*k,.005f,.018f+.025f*k});draw(overlay,GL_TRIANGLES,identity(),0,0,0,false);std::vector<float>mouth;for(int i=0;i<48;i++){float a=i*2*PI/48,b=(i+1)*2*PI/48;addTri(mouth,{0,0,0},{std::cos(a)*(.10f+.70f*k),std::sin(a)*(.10f+.70f*k),0},{std::cos(b)*(.10f+.70f*k),std::sin(b)*(.10f+.70f*k),0},{.005f,.002f,.008f});}draw(mouth,GL_TRIANGLES,identity(),0,0,0,false);drawText("GRAFIK ATE YOU",-.52f,-.72f,.035f,{1,.25f,.30f});}
        glEnable(GL_DEPTH_TEST);
    }
    void drawInventory(){
        glDisable(GL_DEPTH_TEST);std::vector<float>v;auto r=[&](float x,float y,float w,float h,V3 c){addFace(v,{x,y,0},{x+w,y,0},{x+w,y+h,0},{x,y+h,0},c);};
        r(-.78f,-.73f,1.56f,1.43f,{.03f,.03f,.04f});r(-.75f,-.70f,1.50f,1.37f,{.13f,.14f,.16f});
        for(int i=0;i<20;i++){int row=i/5,col=i%5;float x=-.58f+col*.235f,y=.30f-row*.245f;r(x,y-.19f,.19f,.21f,i==selected?V3{.72f,.52f,.14f}:V3{.025f,.025f,.03f});r(x+.008f,y-.182f,.174f,.194f,{.24f,.24f,.26f});if(inventory[i].count){r(x+.055f,y-.135f,.075f,.09f,blockColor(inventory[i].id,2));drawText(inventory[i].count>=999999?"INF":std::to_string(inventory[i].count),x+.125f,y-.17f,.012f,{1,1,1});}}
        r(.60f,.27f,.15f,.15f,{.02f,.02f,.025f});r(.60f,-.02f,.15f,.15f,{.02f,.02f,.025f});r(.60f,-.31f,.15f,.15f,{.02f,.02f,.025f});draw(v,GL_TRIANGLES,identity(),0,0,0,false);
        drawText("INVENTORY",-.25f,.62f,.025f,{1,.85f,.35f});drawText("DRAG ITEMS",-.43f,-.66f,.016f,{1,1,1});drawText("PLANKS",.57f,.48f,.015f,{1,1,1});drawText("STICKS",.58f,.19f,.015f,{1,1,1});drawText("TABLE",.59f,-.10f,.015f,{1,1,1});drawText("X",.66f,.62f,.025f,{1,1,1});glEnable(GL_DEPTH_TEST);
    }
    std::array<uint8_t,7> glyph(char c){
        switch(c){
        case 'A':return {14,17,17,31,17,17,17};case 'B':return {30,17,17,30,17,17,30};
        case 'C':return {14,17,16,16,16,17,14};case 'D':return {30,17,17,17,17,17,30};
        case 'E':return {31,16,16,30,16,16,31};case 'F':return {31,16,16,30,16,16,16};
        case 'G':return {14,17,16,23,17,17,14};case 'H':return {17,17,17,31,17,17,17};case 'I':return {14,4,4,4,4,4,14};case 'K':return {17,18,20,24,20,18,17};
        case 'L':return {16,16,16,16,16,16,31};case 'M':return {17,27,21,21,17,17,17};
        case 'N':return {17,25,25,21,19,19,17};case 'O':return {14,17,17,17,17,17,14};
        case 'P':return {30,17,17,30,16,16,16};case 'R':return {30,17,17,30,20,18,17};
        case 'S':return {15,16,16,14,1,1,30};case 'T':return {31,4,4,4,4,4,4};
        case 'U':return {17,17,17,17,17,17,14};case 'V':return {17,17,17,17,17,10,4};case 'W':return {17,17,17,21,21,21,10};
        case 'Y':return {17,17,10,4,4,4,4};
        case '0':return {14,17,19,21,25,17,14};case '1':return {4,12,4,4,4,4,14};
        case '2':return {14,17,1,2,4,8,31};case '3':return {30,1,1,14,1,1,30};
        case '4':return {2,6,10,18,31,2,2};case '5':return {31,16,16,30,1,1,30};
        case '6':return {14,16,16,30,17,17,14};case '7':return {31,1,2,4,8,8,8};
        case '8':return {14,17,17,14,17,17,14};case '9':return {14,17,17,15,1,1,14};
        case ':':return {0,4,4,0,4,4,0};case '-':return {0,0,0,31,0,0,0};case '%':return {17,2,4,8,17,0,0};
        default:return {0,0,0,0,0,0,0};
        }
    }
    void drawText(const std::string&str,float x,float y,float scale,V3 color){
        std::vector<float> v;float cursor=x;
        for(char c:str){if(c>='a'&&c<='z')c=(char)(c-'a'+'A');auto g=glyph(c);for(int row=0;row<7;row++)for(int col=0;col<5;col++)if(g[row]&(1<<(4-col))){
            float px0=cursor+col*scale,py0=y-row*scale;
            addFace(v,{px0,py0,0},{px0+scale*.82f,py0,0},{px0+scale*.82f,py0+scale*.82f,0},{px0,py0+scale*.82f,0},color);
        }cursor+=scale*6;}
        Mat4 ortho=identity();draw(v,GL_TRIANGLES,ortho,0,0,0,false);
    }
    bool overlaps(float x,float y,float z){
        const float radius=.30f;
        for(int bx=(int)std::floor(x-radius);bx<=(int)std::floor(x+radius);bx++)
        for(int by=(int)std::floor(y);by<=(int)std::floor(y+1.8f);by++)
        for(int bz=(int)std::floor(z-radius);bz<=(int)std::floor(z+radius);bz++)
            if(solid(bx,by,bz))return true;
        return false;
    }
    float groundAt(float x,float z,float reference){
        int bx=(int)std::floor(x),bz=(int)std::floor(z);
        for(int y=(int)std::floor(reference+1.05f);y>=(int)std::floor(reference-.20f);y--)
            if(solid(bx,y,bz))return (float)y+1.0f;
        return reference;
    }
    bool supported(){
        const float r=.23f;float samples[5][2]={{px,pz},{px-r,pz-r},{px+r,pz-r},{px-r,pz+r},{px+r,pz+r}};
        for(auto&q:samples)if(solid((int)std::floor(q[0]),(int)std::floor(feet-.06f),(int)std::floor(q[1])))return true;
        return false;
    }
    bool tryStep(float nx,float nz){
        const float r=.23f;float samples[5][2]={{nx,nz},{nx-r,nz-r},{nx+r,nz-r},{nx-r,nz+r},{nx+r,nz+r}};
        float target=feet;
        for(auto&q:samples)target=std::max(target,groundAt(q[0],q[1],feet));
        if(target>feet+.02f&&target<=feet+1.01f&&!overlaps(nx,target,nz)){
            px=nx;pz=nz;feet=target;vy=0;grounded=true;return true;
        }
        return false;
    }
    void movePlayer(float dx,float dz){
        float distance=std::max(std::abs(dx),std::abs(dz));
        int steps=std::max(1,(int)std::ceil(distance/.14f));
        float sx=dx/steps,sz=dz/steps;
        for(int i=0;i<steps;i++){
            float nx=px+sx;if(!overlaps(nx,feet,pz))px=nx;else tryStep(nx,pz);
            float nz=pz+sz;if(!overlaps(px,feet,nz))pz=nz;else tryStep(px,nz);
        }
    }
    bool raycast(int&hx,int&hy,int&hz,int&px0,int&py0,int&pz0){
        V3 d{std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
        int lastX=0,lastY=0,lastZ=0;
        for(float t=.15f;t<6.0f;t+=.06f){
            int x=(int)std::floor(px+0.0f+d.x*t),y=(int)std::floor(feet+1.62f+d.y*t),z=(int)std::floor(pz+d.z*t);
            if(solid(x,y,z)){hx=x;hy=y;hz=z;px0=lastX;py0=lastY;pz0=lastZ;return true;}
            lastX=x;lastY=y;lastZ=z;
        }return false;
    }
    void editBlock(bool place){
        int x,y,z,ax,ay,az;if(!raycast(x,y,z,ax,ay,az)){status="No block in reach";statusFrames=90;return;}
        if(place){
            if(!inventory[selected].count){status="Select a block";statusFrames=90;return;}uint8_t item=selectedBlock();if(ay<0||ay>=HEIGHT)return;
            if(overlaps(ax+.5f,ay,az+.5f)){status="Cannot place inside player";statusFrames=90;return;}
            int cx=floorDiv(ax,CS),cz=floorDiv(az,CS);Chunk&c=ensureChunk(cx,cz);c.set(modp(ax,CS),ay,modp(az,CS),item);edits[{ax,ay,az}]=item;consumeSelected();
            if(lanMode){LanPacket packet{};packet.type=3;packet.player=networkId;packet.bx=ax;packet.by=ay;packet.bz=az;packet.block=item;sendLan(packet);}
        }else{
            uint8_t broken=0;auto it=chunks.find(chunkKey(floorDiv(x,CS),floorDiv(z,CS)));if(it!=chunks.end())broken=it->second.get(modp(x,CS),y,modp(z,CS));
            int cx=floorDiv(x,CS),cz=floorDiv(z,CS);Chunk&c=ensureChunk(cx,cz);c.set(modp(x,CS),y,modp(z,CS),0);edits[{x,y,z}]=0;
            if(broken==4)addItem(4,1);else if(broken==5)addItem(5,1);else if(broken==3)addItem(3,1);else if(broken==1||broken==2)addItem(2,1);else if(broken==6)addItem(6,1);
            if(lanMode){LanPacket packet{};packet.type=3;packet.player=networkId;packet.bx=x;packet.by=y;packet.bz=z;packet.block=0;sendLan(packet);}
        }
        // Adjacent chunks may expose a newly visible border face.
        for(auto&entry:chunks)entry.second.dirty=true;
        status=place?"Block placed":"Block broken";statusFrames=60;
    }
    void attackGrafik(){
        if(!grafikAlive||playerDead)return;
        float dx=grafikX-px,dz=grafikZ-pz,dist=std::sqrt(dx*dx+dz*dz);
        float fx=std::sin(yaw),fz=-std::cos(yaw);
        float facing=dist>.001f?(dx*fx+dz*fz)/dist:1.0f;
        if(dist<=3.4f&&facing>.35f){
            grafikHealth--;status=grafikHealth<=0?"GRAFIK DEFEATED":"GRAFIK HIT";statusFrames=90;
            if(grafikHealth<=0){grafikAlive=false;grafikRespawn=0;}
        }else{status="GRAFIK OUT OF REACH";statusFrames=70;}
    }
    void updateGrafik(float dt){
        if(!grafikAlive){grafikRespawn+=dt;if(grafikRespawn>18.0f){grafikAlive=true;grafikHealth=2;grafikX=px+6.0f;grafikZ=pz-5.0f;grafikFeet=(float)groundHeight((int)std::floor(grafikX),(int)std::floor(grafikZ));}return;}
        if(playerDead||creativeMode)return;
        float dx=px-grafikX,dz=pz-grafikZ,dist=std::sqrt(dx*dx+dz*dz);
        if(dist<.82f){playerDead=true;swallowTimer=.001f;return;}
        if(dist>1.0f){float step=std::min(1.65f*dt,dist-.72f);float nx=grafikX+dx/dist*step,nz=grafikZ+dz/dist*step;
            int gh=groundHeight((int)std::floor(nx),(int)std::floor(nz));
            if(std::abs((float)gh-grafikFeet)<=1.1f){grafikX=nx;grafikZ=nz;grafikFeet=(float)gh;}
        }
    }
    void updateWorld(){
        int cx=floorDiv((int)std::floor(px),CS),cz=floorDiv((int)std::floor(pz),CS);
        for(int z=-LOAD_RADIUS;z<=LOAD_RADIUS;z++)for(int x=-LOAD_RADIUS;x<=LOAD_RADIUS;x++)ensureChunk(cx+x,cz+z);
        for(auto it=chunks.begin();it!=chunks.end();){
            if(std::abs(it->second.cx-cx)>LOAD_RADIUS+1||std::abs(it->second.cz-cz)>LOAD_RADIUS+1)it=chunks.erase(it);else ++it;
        }
        for(auto&entry:chunks)if(entry.second.dirty)rebuild(entry.second);
    }
    void frame(float dt){
        if(!running||!eglMakeCurrent(display,surface,surface,context))return;dt=std::min(dt,.04f);
        if(!inWorld){glViewport(0,0,width,height);glClearColor(.10f,.18f,.25f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);drawMenu();eglSwapBuffers(display,surface);return;}
        if(playerDead){swallowTimer+=dt;if(swallowTimer>2.8f){playerDead=false;swallowTimer=0;health=10;armor=10;px=0;pz=0;feet=(float)groundHeight(0,0);vy=0;grounded=true;grafikX=6;grafikZ=-5;grafikFeet=(float)groundHeight(6,-5);grafikAlive=true;grafikHealth=2;} }
        if(!playerDead){
        float forward=joyY,side=joyX,mag=std::sqrt(forward*forward+side*side);if(mag>1){forward/=mag;side/=mag;}
        bool swimming=inWater();float speed=swimming?2.8f:5.2f;movePlayer((std::sin(yaw)*forward+std::cos(yaw)*side)*speed*dt,(-std::cos(yaw)*forward+std::sin(yaw)*side)*speed*dt);
        if(swimming){grounded=false;vy-=2.0f*dt;if(feet+1.2f<SEA_LEVEL)vy+=5.0f*dt;vy*=.94f;vy=std::clamp(vy,-2.0f,3.0f);feet+=vy*dt;}
        else {if(grounded&&!supported())grounded=false;}
        if(!grounded&&!swimming){vy-=22*dt;float next=feet+vy*dt;if(vy<=0){int bx=(int)std::floor(px),bz=(int)std::floor(pz),by=(int)std::floor(next-.01f);
            if(solid(bx,by,bz)||solid((int)std::floor(px-.25f),by,(int)std::floor(pz-.25f))||
               solid((int)std::floor(px+.25f),by,(int)std::floor(pz+.25f))||
               solid((int)std::floor(px-.25f),by,(int)std::floor(pz+.25f))||
               solid((int)std::floor(px+.25f),by,(int)std::floor(pz-.25f))){next=by+1;vy=0;grounded=true;}}
            else if(vy>0){int by=(int)std::floor(next+1.8f);if(solid((int)std::floor(px),by,(int)std::floor(pz))){next=by-1.8f;vy=0;}}
            feet=next;if(feet< -20){feet=5;vy=0;}}
        // Keep the feet on the surface of the generated grass layer.
        if(grounded&&feet<1.001f)feet=1.0f;
        if(breaking){int tx,ty,tz,ax,ay,az;if(raycast(tx,ty,tz,ax,ay,az)){if(tx!=breakX||ty!=breakY||tz!=breakZ){breakX=tx;breakY=ty;breakZ=tz;breakTimer=0;}auto it=chunks.find(chunkKey(floorDiv(tx,CS),floorDiv(tz,CS)));uint8_t q=it==chunks.end()?0:it->second.get(modp(tx,CS),ty,modp(tz,CS));breakTimer+=dt;if(q&&breakTimer>=breakDuration(q)){editBlock(false);breaking=false;breakTimer=0;}}else breakTimer=0;}
        }
        updateGrafik(dt);
        updateLan(dt);
        if(inWorld){saveTimer+=dt;if(saveTimer>=12.0f){saveWorld();saveTimer=0;}}
        updateWorld();
        glViewport(0,0,width,height);glClearColor(.48f,.74f,.96f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        Mat4 vp=mul(perspective(1.02f,(float)width/height,.1f,180),camera(px,feet+1.62f,pz,yaw,pitch));
        for(auto&entry:chunks)draw(entry.second.mesh,GL_TRIANGLES,vp,px,feet+1.62f,pz,true,true);
        if(remoteValid){
            std::vector<float> remoteSphere;auto rp=[&](float lat,float lon){return V3{remoteX+.42f*std::cos(lat)*std::sin(lon),remoteY+.72f+.42f*std::sin(lat),remoteZ+.42f*std::cos(lat)*std::cos(lon)};};
            for(int iy=0;iy<8;iy++){float a0=-PI*.5f+PI*iy/8.0f,a1=-PI*.5f+PI*(iy+1)/8.0f;for(int ix=0;ix<12;ix++){float b0=2*PI*ix/12.0f,b1=2*PI*(ix+1)/12.0f;V3 p0=rp(a0,b0),p1=rp(a0,b1),p2=rp(a1,b1),p3=rp(a1,b0);V3 c{.12f,.48f,.95f};addTri(remoteSphere,p0,p1,p2,c);addTri(remoteSphere,p0,p2,p3,c);}}
            draw(remoteSphere,GL_TRIANGLES,vp,px,feet+1.62f,pz,true,false);
        }
        if(grafikAlive){
            std::vector<float> sphere;auto point=[&](float lat,float lon){return V3{grafikX+.58f*std::cos(lat)*std::sin(lon),grafikFeet+.62f+.58f*std::sin(lat),grafikZ+.58f*std::cos(lat)*std::cos(lon)};};
            for(int iy=0;iy<10;iy++){float a0=-PI*.5f+PI*iy/10.0f,a1=-PI*.5f+PI*(iy+1)/10.0f;for(int ix=0;ix<16;ix++){float b0=2*PI*ix/16.0f,b1=2*PI*(ix+1)/16.0f;V3 p0=point(a0,b0),p1=point(a0,b1),p2=point(a1,b1),p3=point(a1,b0);float shade=.72f+.28f*std::max(0.0f,std::sin((a0+a1)*.5f));V3 c{.48f*shade,.12f*shade,.68f*shade};addTri(sphere,p0,p1,p2,c);addTri(sphere,p0,p2,p3,c);}}
            draw(sphere,GL_TRIANGLES,vp,px,feet+1.62f,pz,true,false);
        }
        // Target outline around the block under the crosshair.
        int x,y,z,ax,ay,az;if(raycast(x,y,z,ax,ay,az)){
            float X=x,Y=y,Z=z;std::vector<float> edge;
            V3 white{1,.95f,.45f};
            V3 p[8]={{X,Y,Z},{X+1,Y,Z},{X+1,Y+1,Z},{X,Y+1,Z},{X,Y,Z+1},{X+1,Y,Z+1},{X+1,Y+1,Z+1},{X,Y+1,Z+1}};
            int e[24]={0,1,1,2,2,3,3,0,4,5,5,6,6,7,7,4,0,4,1,5,2,6,3,7};
            for(int i=0;i<24;i+=2){for(int j:{e[i],e[i+1]})edge.insert(edge.end(),{p[j].x,p[j].y,p[j].z,white.x,white.y,white.z,0,0});}
            glDisable(GL_CULL_FACE);draw(edge,GL_LINES,vp,px,feet+1.62f,pz,true);
        }
        if(inventoryOpen)drawInventory();else drawHud();eglSwapBuffers(display,surface);
    }
    void drawMenu(){
        glDisable(GL_DEPTH_TEST);std::vector<float>v;
        addFace(v,{-.78f,-.86f,0},{.78f,-.86f,0},{.78f,.86f,0},{-.78f,.86f,0},{.035f,.055f,.075f});
        addFace(v,{-.74f,-.82f,0},{.74f,-.82f,0},{.74f,.82f,0},{-.74f,.82f,0},{.10f,.15f,.19f});
        draw(v,GL_TRIANGLES,identity(),0,0,0,false);drawText("DEATH CRAFT",-.37f,.68f,.045f,{.45f,.88f,1});
        std::vector<float>b;auto button=[&](float x,float y,float w,float h,V3 c){addFace(b,{x,y,0},{x+w,y,0},{x+w,y+h,0},{x,y+h,0},c);};
        button(-.62f,.34f,.58f,.20f,creativeMode?V3{.16f,.52f,.25f}:V3{.08f,.10f,.12f});button(.04f,.34f,.58f,.20f,!creativeMode?V3{.16f,.52f,.25f}:V3{.08f,.10f,.12f});
        button(-.62f,.04f,.58f,.20f,flatWorld?V3{.16f,.52f,.25f}:V3{.08f,.10f,.12f});button(.04f,.04f,.58f,.20f,!flatWorld?V3{.16f,.52f,.25f}:V3{.08f,.10f,.12f});
        for(int i=0;i<3;i++)button(-.62f+i*.44f,-.36f,.38f,.18f,selectedSlot==i?V3{.55f,.37f,.12f}:V3{.08f,.10f,.12f});
        button(-.24f,-.70f,.48f,.22f,{.12f,.42f,.68f});draw(b,GL_TRIANGLES,identity(),0,0,0,false);
        drawText("CREATIVE",-.55f,.405f,.017f,{1,1,1});drawText("SURVIVAL",.10f,.405f,.017f,{1,1,1});drawText("FLAT",-.48f,.105f,.022f,{1,1,1});drawText("NORMAL",.10f,.105f,.018f,{1,1,1});
        for(int i=0;i<3;i++)drawText("SLOT "+std::to_string(i+1),-.59f+i*.44f,-.30f,.014f,{1,1,1});
        drawText("START",-.15f,-.63f,.025f,{1,1,1});drawText("MODE",-.08f,.60f,.018f,{.8f,.85f,.9f});drawText("WORLD TYPE",-.20f,.28f,.018f,{.8f,.85f,.9f});drawText("SAVE SLOT",-.18f,-.12f,.018f,{.8f,.85f,.9f});glEnable(GL_DEPTH_TEST);
    }
    void setJoy(float x,float y){joyX=std::clamp((x-joyOriginX)/.20f,-1.0f,1.0f);joyY=std::clamp((y-joyOriginY)/.20f,-1.0f,1.0f);}
public:
    bool init(ANativeWindow*w,AAssetManager* manager,const char* dataDir){
        const EGLint attrs[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,16,EGL_NONE};
        display=eglGetDisplay(EGL_DEFAULT_DISPLAY);if(display==EGL_NO_DISPLAY||!eglInitialize(display,nullptr,nullptr))return false;
        EGLConfig cfg;EGLint count=0;eglChooseConfig(display,attrs,&cfg,1,&count);if(!count)return false;
        const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};context=eglCreateContext(display,cfg,EGL_NO_CONTEXT,ca);surface=eglCreateWindowSurface(display,cfg,w,nullptr);
        if(context==EGL_NO_CONTEXT||surface==EGL_NO_SURFACE||!eglMakeCurrent(display,surface,surface,context))return false;
        eglQuerySurface(display,surface,EGL_WIDTH,&width);eglQuerySurface(display,surface,EGL_HEIGHT,&height);glViewport(0,0,width,height);
        if(!setupGL())return false;loadTextures(manager);saveDirectory=dataDir?dataDir:".";feet=1;grafikFeet=1;health=10;armor=10;grafikHealth=2;selected=0;running=true;inWorld=false;return true;
    }
    int input(AInputEvent*e){
        if(AInputEvent_getType(e)!=AINPUT_EVENT_TYPE_MOTION)return 0;int action=AMotionEvent_getAction(e)&AMOTION_EVENT_ACTION_MASK,idx=(AMotionEvent_getAction(e)&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;int id=AMotionEvent_getPointerId(e,idx);float x=AMotionEvent_getX(e,idx)/width*2-1,y=1-AMotionEvent_getY(e,idx)/height*2;
        if(action==AMOTION_EVENT_ACTION_DOWN||action==AMOTION_EVENT_ACTION_POINTER_DOWN){
            if(!inWorld){if(y>.32f){creativeMode=x<0;return 1;}if(y>.02f&&y<.27f){flatWorld=x<0;return 1;}if(y>-.40f&&y<-.12f){selectedSlot=std::clamp((int)((x+.62f)/.44f),0,2);return 1;}if(y<-.52f){startWorld();return 1;}return 1;}
            if(inventoryOpen){if(x>.62f&&y>.55f){inventoryOpen=false;dragIndex=-1;return 1;}if(x>.55f&&x<.80f&&y>.20f&&y<.43f){craft(0);return 1;}if(x>.55f&&x<.80f&&y>-.08f&&y<.16f){craft(1);return 1;}if(x>.55f&&x<.80f&&y>-.38f&&y<-.12f){craft(2);return 1;}int slot=inventorySlotAt(x,y);if(slot>=0){dragIndex=slot;dragPointer=id;}return 1;}
            if(x>-.95f&&x<-.72f&&y>.55f&&y<.73f){setLanMode(1);return 1;}
            if(x>-.72f&&x<-.47f&&y>.55f&&y<.73f){setLanMode(2);return 1;}
            if(x>.81f&&y>.72f){inventoryOpen=true;joyId=lookId=-1;joyX=joyY=0;breaking=false;return 1;}
            if(x<-.42f&&y<0){joyId=id;joyOriginX=x;joyOriginY=y;setJoy(x,y);}
            else if(x>.65f&&y<-.43f){if(inWater()){vy=3.4f;grounded=false;}else if(grounded){vy=7.4f;grounded=false;}}
            else if(x>.65f&&y>-.34f&&y<.12f){int tx,ty,tz,ax,ay,az;if(raycast(tx,ty,tz,ax,ay,az)){breaking=true;breakPointer=id;breakX=tx;breakY=ty;breakZ=tz;breakTimer=0;}}
            else if(x>.40f&&x<.66f&&y>-.54f&&y<-.25f)editBlock(true);
            else if(x>.38f&&x<.64f&&y>-.22f&&y<.03f)attackGrafik();
            else if(y<-.72f&&x>-.31f&&x<.31f)selected=std::clamp((int)((x+.31f)/.62f*5),0,4);
            else{lookId=id;lastX=x;lastY=y;}return 1;
        }
        if(action==AMOTION_EVENT_ACTION_MOVE){for(size_t n=0;n<AMotionEvent_getPointerCount(e);n++){int pid=AMotionEvent_getPointerId(e,n);float tx=AMotionEvent_getX(e,n)/width*2-1,ty=1-AMotionEvent_getY(e,n)/height*2;if(pid==joyId)setJoy(tx,ty);if(pid==lookId){yaw+=(tx-lastX)*1.45f;pitch=std::clamp(pitch-(ty-lastY)*1.15f,-1.35f,1.35f);lastX=tx;lastY=ty;}}return 1;}
        if(action==AMOTION_EVENT_ACTION_UP||action==AMOTION_EVENT_ACTION_POINTER_UP||action==AMOTION_EVENT_ACTION_CANCEL){if(action==AMOTION_EVENT_ACTION_CANCEL){joyId=lookId=breakPointer=dragPointer=-1;joyX=joyY=0;breaking=false;dragIndex=-1;return 1;}if(id==joyId){joyId=-1;joyX=joyY=0;}if(id==lookId)lookId=-1;if(id==breakPointer){breakPointer=-1;breaking=false;breakTimer=0;}if(inventoryOpen&&id==dragPointer&&dragIndex>=0){int target=inventorySlotAt(x,y);if(target>=0&&target!=dragIndex)std::swap(inventory[target],inventory[dragIndex]);dragIndex=dragPointer=-1;}return 1;}return 1;
    }
    void shutdown(){if(inWorld)saveWorld();running=false;if(lanSocket>=0){close(lanSocket);lanSocket=-1;}lanMode=0;remoteValid=false;if(display!=EGL_NO_DISPLAY){eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(context!=EGL_NO_CONTEXT)eglDestroyContext(display,context);if(surface!=EGL_NO_SURFACE)eglDestroySurface(display,surface);eglTerminate(display);}display=EGL_NO_DISPLAY;surface=EGL_NO_SURFACE;context=EGL_NO_CONTEXT;}
    void tick(float dt){frame(dt);}
    void save(){saveWorld();}
    bool isRunning() const { return running; }
};
static Game game;
static void command(android_app*app,int32_t cmd){if(cmd==APP_CMD_PAUSE)game.save();if(cmd==APP_CMD_INIT_WINDOW&&app->window){if(!game.init(app->window,app->activity->assetManager,app->activity->internalDataPath))LOGE("Game initialization failed");}if(cmd==APP_CMD_TERM_WINDOW)game.shutdown();}
void android_main(android_app*app){
    app->onAppCmd=command;app->onInputEvent=[](android_app*,AInputEvent*e)->int{return game.input(e);};int64_t previous=0;
    while(true){int events;android_poll_source*source;while(ALooper_pollOnce(game.isRunning()?0:-1,nullptr,&events,(void**)&source)>=0){if(source)source->process(app,source);if(app->destroyRequested){game.shutdown();return;}}
        timespec ts{};clock_gettime(CLOCK_MONOTONIC,&ts);int64_t now=(int64_t)ts.tv_sec*1000000000LL+ts.tv_nsec;float dt=previous?(now-previous)/1e9f:.016f;previous=now;if(game.isRunning())game.tick(dt);}
}
