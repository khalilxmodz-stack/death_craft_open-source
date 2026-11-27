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

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"DeathCraft",__VA_ARGS__)
static constexpr int CS=16, HEIGHT=8, LOAD_RADIUS=4;
static constexpr float PI=3.14159265f;

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
static uint64_t chunkKey(int x,int z){return (uint64_t)(uint32_t)x<<32|(uint32_t)z;}

struct Chunk {
    int cx=0,cz=0;
    std::array<uint8_t,CS*CS*HEIGHT> blocks{};
    std::vector<float> mesh;
    bool dirty=true;
    uint8_t get(int x,int y,int z)const{return (x<0||x>=CS||z<0||z>=CS||y<0||y>=HEIGHT)?0:blocks[(y*CS+z)*CS+x];}
    void set(int x,int y,int z,uint8_t b){if(x>=0&&x<CS&&z>=0&&z<CS&&y>=0&&y<HEIGHT){blocks[(y*CS+z)*CS+x]=b;dirty=true;}}
};

class Game {
    EGLDisplay display=EGL_NO_DISPLAY; EGLSurface surface=EGL_NO_SURFACE; EGLContext context=EGL_NO_CONTEXT;
    GLuint program=0,vao=0,vbo=0; GLint mvpLoc=-1,cameraLoc=-1,fogLoc=-1;
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
    struct Item{uint8_t id=0;int count=0;};std::array<Item,20> inventory{};
    std::string status="DEATH_CRAFT  |  V0.1";
    int statusFrames=0;

    GLuint makeShader(GLenum type,const char*src){GLuint s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[1024];glGetShaderInfoLog(s,1024,nullptr,log);LOGE("Shader: %s",log);}return s;}
    bool setupGL(){
        const char*vs=R"(#version 300 es
        layout(location=0) in vec3 p; layout(location=1) in vec3 c;
        uniform mat4 mvp; uniform vec3 cameraPos; out vec3 color; out float fogDistance;
        void main(){gl_Position=mvp*vec4(p,1.0);color=c;fogDistance=length(p-cameraPos);})";
        const char*fs=R"(#version 300 es
        precision highp float; in vec3 color; in float fogDistance; uniform float fogEnabled; out vec4 outColor;
        void main(){vec3 sky=vec3(.48,.74,.96);float fog=fogEnabled*smoothstep(22.0,68.0,fogDistance);outColor=vec4(mix(color,sky,fog),1.0);})";
        GLuint a=makeShader(GL_VERTEX_SHADER,vs),b=makeShader(GL_FRAGMENT_SHADER,fs);
        program=glCreateProgram();glAttachShader(program,a);glAttachShader(program,b);glLinkProgram(program);glDeleteShader(a);glDeleteShader(b);
        GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok)return false;
        mvpLoc=glGetUniformLocation(program,"mvp");cameraLoc=glGetUniformLocation(program,"cameraPos");fogLoc=glGetUniformLocation(program,"fogEnabled");
        glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);
        glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);return true;
    }
    void addTri(std::vector<float>&v,V3 a,V3 b,V3 c,V3 color){
        for(V3 p:{a,b,c})v.insert(v.end(),{p.x,p.y,p.z,color.x,color.y,color.z});
    }
    void addFace(std::vector<float>&v,V3 a,V3 b,V3 c,V3 d,V3 color){
        addTri(v,a,b,c,color);addTri(v,a,c,d,color);
    }
    bool solid(int x,int y,int z){
        int cx=floorDiv(x,CS),cz=floorDiv(z,CS);auto it=chunks.find(chunkKey(cx,cz));
        if(it==chunks.end())return y<0;
        return it->second.get(modp(x,CS),y,modp(z,CS))!=0;
    }
    void generate(Chunk&c){
        for(int z=0;z<CS;z++)for(int x=0;x<CS;x++)c.set(x,0,z,1);
        int ox=c.cx*CS,oz=c.cz*CS;
        auto put=[&](int x,int y,int z,uint8_t id){if(floorDiv(x,CS)==c.cx&&floorDiv(z,CS)==c.cz)c.set(modp(x,CS),y,modp(z,CS),id);};
        for(int z=oz-2;z<oz+CS+2;z++)for(int x=ox-2;x<ox+CS+2;x++){
            if(modp(x+3,8)||modp(z+5,8))continue;uint32_t h=(uint32_t)x*374761393u+(uint32_t)z*668265263u;h=(h^(h>>13))*1274126177u;h^=h>>16;if(h%3==0)continue;
            int trunk=3+(int)(h%2);for(int y=1;y<=trunk;y++)put(x,y,z,4);
            for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++)for(int dy=-1;dy<=1;dy++){if(std::abs(dx)+std::abs(dz)==2&&dy==1)continue;put(x+dx,trunk+dy,z+dz,5);}put(x,trunk+1,z,5);
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
        if(b==1){if(face==2)return{.22f,.68f,.18f};return{.42f,.29f,.16f};}
        if(b==2)return{.48f,.31f,.17f};if(b==3)return{.43f,.45f,.48f};
        if(b==4)return{.43f,.27f,.12f};if(b==5)return{.12f,.48f,.13f};if(b==6)return{.67f,.46f,.24f};return{.72f,.62f,.35f};
    }
    int breakDuration(uint8_t b)const{switch(b){case 1:return 35;case 2:return 55;case 3:return 125;case 4:return 75;case 5:return 18;case 6:return 65;default:return 45;}}
    bool addItem(uint8_t id,int n){for(auto &i:inventory)if(i.id==id&&i.count){i.count+=n;return true;}for(auto &i:inventory)if(!i.count){i={id,n};return true;}status="Inventory full";statusFrames=90;return false;}
    uint8_t selectedBlock()const{return inventory[selected].count?inventory[selected].id:1;}
    void consumeSelected(){if(inventory[selected].count>0&&--inventory[selected].count==0)inventory[selected].id=0;}
    void craft(int r){uint8_t src=r==0?4:6;int n=r==0?1:(r==1?2:4);for(auto&i:inventory)if(i.id==src&&n){int k=std::min(n,i.count);i.count-=k;n-=k;if(!i.count)i.id=0;}if(n){status=r==0?"Need wood":"Need planks";statusFrames=90;return;}addItem(r==0?6:(r==1?7:8),r==0?4:(r==1?4:1));}
    int inventorySlotAt(float x,float y)const{for(int r=0;r<4;r++)for(int c=0;c<5;c++){float bx=-.58f+c*.235f,by=.30f-r*.245f;if(x>=bx&&x<=bx+.19f&&y>=by-.19f&&y<=by+.02f)return r*5+c;}return -1;}
    void rebuild(Chunk&c){
        c.mesh.clear();int ox=c.cx*CS,oz=c.cz*CS;
        for(int y=0;y<HEIGHT;y++)for(int z=0;z<CS;z++)for(int x=0;x<CS;x++){
            uint8_t b=c.get(x,y,z);if(!b)continue;
            float X=(float)(ox+x),Y=(float)y,Z=(float)(oz+z);
            V3 p000{X,Y,Z},p100{X+1,Y,Z},p110{X+1,Y+1,Z},p010{X,Y+1,Z};
            V3 p001{X,Y,Z+1},p101{X+1,Y,Z+1},p111{X+1,Y+1,Z+1},p011{X,Y+1,Z+1};
            if(!solid(ox+x,y,oz+z-1))addFace(c.mesh,p000,p100,p110,p010,blockColor(b,0));
            if(!solid(ox+x, y,oz+z+1))addFace(c.mesh,p101,p001,p011,p111,blockColor(b,1));
            if(!solid(ox+x,y+1,oz+z))addFace(c.mesh,p010,p110,p111,p011,blockColor(b,2));
            if(!solid(ox+x,y-1,oz+z))addFace(c.mesh,p001,p101,p100,p000,blockColor(b,3));
            if(!solid(ox+x-1,y,oz+z))addFace(c.mesh,p001,p000,p010,p011,blockColor(b,4));
            if(!solid(ox+x+1,y,oz+z))addFace(c.mesh,p100,p101,p111,p110,blockColor(b,5));
        }
        c.dirty=false;
    }
    void draw(const std::vector<float>&v,GLenum mode,const Mat4&m,float cx=0,float cy=0,float cz=0,bool fog=true){
        if(v.empty())return;glUseProgram(program);glUniformMatrix4fv(mvpLoc,1,GL_FALSE,m.m);glUniform3f(cameraLoc,cx,cy,cz);glUniform1f(fogLoc,fog?1.0f:0.0f);
        glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
        glEnableVertexAttribArray(1);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
        glDrawArrays(mode,0,(GLsizei)(v.size()/6));
    }
    void drawHud(){
        glDisable(GL_DEPTH_TEST);Mat4 m=identity();std::vector<float>v;
        auto t=[&](float x,float y,float X,float Y,float Z,float W,V3 c){addTri(v,{x,y,0},{X,Y,0},{Z,W,0},c);};
        auto rect=[&](float x,float y,float w,float h,V3 c){t(x,y,x+w,y,x+w,y+h,c);t(x,y,x+w,y+h,x,y+h,c);};
        auto circ=[&](float x,float y,float r,V3 c){for(int i=0;i<32;i++){float a=i*2*PI/32,b=(i+1)*2*PI/32;t(x,y,x+cosf(a)*r,y+sinf(a)*r,x+cosf(b)*r,y+sinf(b)*r,c);}};
        auto line=[&](float x,float y,float X,float Y,float q,V3 c){float dx=X-x,dy=Y-y,l=sqrtf(dx*dx+dy*dy);if(l<.001f)return;float ox=-dy/l*q,oy=dx/l*q;t(x+ox,y+oy,X+ox,Y+oy,X-ox,Y-oy,c);t(x+ox,y+oy,X-ox,Y-oy,x-ox,y-oy,c);};
        circ(joyOriginX,joyOriginY,.255f,{.01f,.01f,.015f});circ(joyOriginX,joyOriginY,.22f,{.14f,.14f,.16f});circ(joyOriginX+joyX*.16f,joyOriginY+joyY*.16f,.105f,{.01f,.01f,.015f});
        circ(.78f,-.62f,.14f,{.01f,.01f,.015f});circ(.78f,-.62f,.115f,{.17f,.17f,.19f});
        circ(.78f,-.10f,.14f,{.01f,.01f,.015f});circ(.78f,-.10f,.115f,{.17f,.17f,.19f});
        circ(.53f,-.40f,.14f,{.01f,.01f,.015f});circ(.53f,-.40f,.115f,{.17f,.17f,.19f});
        line(.78f,-.65f,.78f,-.56f,.018f,{1,1,1});line(.78f,-.56f,.74f,-.60f,.018f,{1,1,1});line(.78f,-.56f,.82f,-.60f,.018f,{1,1,1});
        line(.72f,-.12f,.84f,-.02f,.018f,{1,1,1});line(.73f,-.02f,.85f,-.02f,.018f,{1,1,1});
        line(.48f,-.45f,.58f,-.45f,.014f,{1,1,1});line(.48f,-.45f,.48f,-.35f,.014f,{1,1,1});line(.58f,-.45f,.58f,-.35f,.014f,{1,1,1});line(.48f,-.35f,.58f,-.35f,.014f,{1,1,1});
        rect(.84f,.79f,.13f,.15f,{.01f,.01f,.015f});rect(.86f,.81f,.09f,.11f,{.2f,.2f,.22f});line(.87f,.87f,.94f,.87f,.009f,{1,1,1});line(.87f,.83f,.94f,.83f,.009f,{1,1,1});
        for(int i=0;i<5;i++){float x=-.28f+i*.115f;rect(x,-.92f,.105f,.16f,i==selected?V3{1,.78f,.18f}:V3{.01f,.01f,.015f});rect(x+.008f,-.912f,.089f,.144f,{.2f,.2f,.22f});if(inventory[i].count){rect(x+.025f,-.88f,.055f,.075f,blockColor(inventory[i].id,2));drawText(std::to_string(inventory[i].count),x+.060f,-.89f,.010f,{1,1,1});}}
        draw(v,GL_TRIANGLES,m,0,0,0,false);if(statusFrames>0){drawText(status,-.85f,.72f,.018f,{1,1,1});statusFrames--;}glEnable(GL_DEPTH_TEST);
    }
    void drawInventory(){
        glDisable(GL_DEPTH_TEST);std::vector<float>v;auto r=[&](float x,float y,float w,float h,V3 c){addFace(v,{x,y,0},{x+w,y,0},{x+w,y+h,0},{x,y+h,0},c);};
        r(-.78f,-.73f,1.56f,1.43f,{.03f,.03f,.04f});r(-.75f,-.70f,1.50f,1.37f,{.13f,.14f,.16f});
        for(int i=0;i<20;i++){int row=i/5,col=i%5;float x=-.58f+col*.235f,y=.30f-row*.245f;r(x,y-.19f,.19f,.21f,i==selected?V3{.72f,.52f,.14f}:V3{.025f,.025f,.03f});r(x+.008f,y-.182f,.174f,.194f,{.24f,.24f,.26f});if(inventory[i].count){r(x+.055f,y-.135f,.075f,.09f,blockColor(inventory[i].id,2));drawText(std::to_string(inventory[i].count),x+.125f,y-.17f,.012f,{1,1,1});}}
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
        case 'U':return {17,17,17,17,17,17,14};case 'V':return {17,17,17,17,17,10,4};
        case 'Y':return {17,17,10,4,4,4,4};
        case '0':return {14,17,19,21,25,17,14};case '1':return {4,12,4,4,4,4,14};
        case '2':return {14,17,1,2,4,8,31};case '3':return {30,1,1,14,1,1,30};
        case '4':return {2,6,10,18,31,2,2};case '5':return {31,16,16,30,1,1,30};
        case '6':return {14,16,16,30,17,17,14};case '7':return {31,1,2,4,8,8,8};
        case '8':return {14,17,17,14,17,17,14};case '9':return {14,17,17,15,1,1,14};
        case ':':return {0,4,4,0,4,4,0};case '-':return {0,0,0,31,0,0,0};
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
    void movePlayer(float dx,float dz){
        float nx=px+dx;if(!overlaps(nx,feet,pz))px=nx;
        float nz=pz+dz;if(!overlaps(px,feet,nz))pz=nz;
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
        }else{
            uint8_t broken=0;auto it=chunks.find(chunkKey(floorDiv(x,CS),floorDiv(z,CS)));if(it!=chunks.end())broken=it->second.get(modp(x,CS),y,modp(z,CS));
            int cx=floorDiv(x,CS),cz=floorDiv(z,CS);Chunk&c=ensureChunk(cx,cz);c.set(modp(x,CS),y,modp(z,CS),0);edits[{x,y,z}]=0;
            if(broken==4)addItem(4,1);else if(broken==5)addItem(5,1);else if(broken==3)addItem(3,1);else if(broken==1||broken==2)addItem(2,1);else if(broken==6)addItem(6,1);
        }
        // Adjacent chunks may expose a newly visible border face.
        for(auto&entry:chunks)entry.second.dirty=true;
        status=place?"Block placed":"Block broken";statusFrames=60;
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
        float forward=-joyY,side=joyX,mag=std::sqrt(forward*forward+side*side);if(mag>1){forward/=mag;side/=mag;}
        float speed=5.2f;movePlayer((std::sin(yaw)*forward+std::cos(yaw)*side)*speed*dt,(-std::cos(yaw)*forward+std::sin(yaw)*side)*speed*dt);
        if(!grounded){vy-=22*dt;float next=feet+vy*dt;if(vy<=0){int bx=(int)std::floor(px),bz=(int)std::floor(pz),by=(int)std::floor(next-.01f);
            if(solid(bx,by,bz)||solid((int)std::floor(px-.25f),by,(int)std::floor(pz-.25f))||
               solid((int)std::floor(px+.25f),by,(int)std::floor(pz+.25f))||
               solid((int)std::floor(px-.25f),by,(int)std::floor(pz+.25f))||
               solid((int)std::floor(px+.25f),by,(int)std::floor(pz-.25f))){next=by+1;vy=0;grounded=true;}}
            else if(vy>0){int by=(int)std::floor(next+1.8f);if(solid((int)std::floor(px),by,(int)std::floor(pz))){next=by-1.8f;vy=0;}}
            feet=next;if(feet< -20){feet=5;vy=0;}}
        // Keep the feet on the surface of the generated grass layer.
        if(grounded&&feet<1.001f)feet=1.0f;
        if(breaking){int tx,ty,tz,ax,ay,az;if(raycast(tx,ty,tz,ax,ay,az)){if(tx!=breakX||ty!=breakY||tz!=breakZ){breakX=tx;breakY=ty;breakZ=tz;breakTimer=0;}auto it=chunks.find(chunkKey(floorDiv(tx,CS),floorDiv(tz,CS)));uint8_t q=it==chunks.end()?0:it->second.get(modp(tx,CS),ty,modp(tz,CS));breakTimer+=dt;if(q&&breakTimer>=breakDuration(q)/60.0f){editBlock(false);breaking=false;breakTimer=0;}}else breakTimer=0;}
        updateWorld();
        glViewport(0,0,width,height);glClearColor(.48f,.74f,.96f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        Mat4 vp=mul(perspective(1.02f,(float)width/height,.1f,180),camera(px,feet+1.62f,pz,yaw,pitch));
        for(auto&entry:chunks)draw(entry.second.mesh,GL_TRIANGLES,vp,px,feet+1.62f,pz,true);
        // Target outline around the block under the crosshair.
        int x,y,z,ax,ay,az;if(raycast(x,y,z,ax,ay,az)){
            float X=x,Y=y,Z=z;std::vector<float> edge;
            V3 white{1,.95f,.45f};
            V3 p[8]={{X,Y,Z},{X+1,Y,Z},{X+1,Y+1,Z},{X,Y+1,Z},{X,Y,Z+1},{X+1,Y,Z+1},{X+1,Y+1,Z+1},{X,Y+1,Z+1}};
            int e[24]={0,1,1,2,2,3,3,0,4,5,5,6,6,7,7,4,0,4,1,5,2,6,3,7};
            for(int i=0;i<24;i+=2){for(int j:{e[i],e[i+1]})edge.insert(edge.end(),{p[j].x,p[j].y,p[j].z,white.x,white.y,white.z});}
            glDisable(GL_CULL_FACE);draw(edge,GL_LINES,vp,px,feet+1.62f,pz,true);
        }
        if(inventoryOpen)drawInventory();else drawHud();eglSwapBuffers(display,surface);
    }
    void setJoy(float x,float y){joyX=std::clamp((x-joyOriginX)/.20f,-1.0f,1.0f);joyY=std::clamp((y-joyOriginY)/.20f,-1.0f,1.0f);}
public:
    bool init(ANativeWindow*w){
        const EGLint attrs[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,16,EGL_NONE};
        display=eglGetDisplay(EGL_DEFAULT_DISPLAY);if(display==EGL_NO_DISPLAY||!eglInitialize(display,nullptr,nullptr))return false;
        EGLConfig cfg;EGLint count=0;eglChooseConfig(display,attrs,&cfg,1,&count);if(!count)return false;
        const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};context=eglCreateContext(display,cfg,EGL_NO_CONTEXT,ca);surface=eglCreateWindowSurface(display,cfg,w,nullptr);
        if(context==EGL_NO_CONTEXT||surface==EGL_NO_SURFACE||!eglMakeCurrent(display,surface,surface,context))return false;
        eglQuerySurface(display,surface,EGL_WIDTH,&width);eglQuerySurface(display,surface,EGL_HEIGHT,&height);glViewport(0,0,width,height);
        if(!setupGL())return false;inventory[0]={1,32};inventory[1]={2,16};inventory[2]={3,12};inventory[3]={4,4};inventory[4]={6,8};selected=0;running=true;updateWorld();return true;
    }
    int input(AInputEvent*e){
        if(AInputEvent_getType(e)!=AINPUT_EVENT_TYPE_MOTION)return 0;int action=AMotionEvent_getAction(e)&AMOTION_EVENT_ACTION_MASK,idx=(AMotionEvent_getAction(e)&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;int id=AMotionEvent_getPointerId(e,idx);float x=AMotionEvent_getX(e,idx)/width*2-1,y=1-AMotionEvent_getY(e,idx)/height*2;
        if(action==AMOTION_EVENT_ACTION_DOWN||action==AMOTION_EVENT_ACTION_POINTER_DOWN){
            if(inventoryOpen){if(x>.62f&&y>.55f){inventoryOpen=false;dragIndex=-1;return 1;}if(x>.55f&&x<.80f&&y>.20f&&y<.43f){craft(0);return 1;}if(x>.55f&&x<.80f&&y>-.08f&&y<.16f){craft(1);return 1;}if(x>.55f&&x<.80f&&y>-.38f&&y<-.12f){craft(2);return 1;}int slot=inventorySlotAt(x,y);if(slot>=0){dragIndex=slot;dragPointer=id;}return 1;}
            if(x>.81f&&y>.72f){inventoryOpen=true;joyId=lookId=-1;joyX=joyY=0;breaking=false;return 1;}
            if(x<-.42f&&y<0){joyId=id;joyOriginX=x;joyOriginY=y;setJoy(x,y);}
            else if(x>.65f&&y<-.43f){if(grounded){vy=7.4f;grounded=false;}}
            else if(x>.65f&&y>-.34f&&y<.12f){int tx,ty,tz,ax,ay,az;if(raycast(tx,ty,tz,ax,ay,az)){breaking=true;breakPointer=id;breakX=tx;breakY=ty;breakZ=tz;breakTimer=0;}}
            else if(x>.40f&&x<.66f&&y>-.54f&&y<-.25f)editBlock(true);
            else if(y<-.72f&&x>-.31f&&x<.31f)selected=std::clamp((int)((x+.31f)/.62f*5),0,4);
            else{lookId=id;lastX=x;lastY=y;}return 1;
        }
        if(action==AMOTION_EVENT_ACTION_MOVE){for(size_t n=0;n<AMotionEvent_getPointerCount(e);n++){int pid=AMotionEvent_getPointerId(e,n);float tx=AMotionEvent_getX(e,n)/width*2-1,ty=1-AMotionEvent_getY(e,n)/height*2;if(pid==joyId)setJoy(tx,ty);if(pid==lookId){yaw+=(tx-lastX)*1.45f;pitch=std::clamp(pitch-(ty-lastY)*1.15f,-1.35f,1.35f);lastX=tx;lastY=ty;}}return 1;}
        if(action==AMOTION_EVENT_ACTION_UP||action==AMOTION_EVENT_ACTION_POINTER_UP||action==AMOTION_EVENT_ACTION_CANCEL){if(action==AMOTION_EVENT_ACTION_CANCEL){joyId=lookId=breakPointer=dragPointer=-1;joyX=joyY=0;breaking=false;dragIndex=-1;return 1;}if(id==joyId){joyId=-1;joyX=joyY=0;}if(id==lookId)lookId=-1;if(id==breakPointer){breakPointer=-1;breaking=false;breakTimer=0;}if(inventoryOpen&&id==dragPointer&&dragIndex>=0){int target=inventorySlotAt(x,y);if(target>=0&&target!=dragIndex)std::swap(inventory[target],inventory[dragIndex]);dragIndex=dragPointer=-1;}return 1;}return 1;
    }
    void shutdown(){running=false;if(display!=EGL_NO_DISPLAY){eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(context!=EGL_NO_CONTEXT)eglDestroyContext(display,context);if(surface!=EGL_NO_SURFACE)eglDestroySurface(display,surface);eglTerminate(display);}display=EGL_NO_DISPLAY;surface=EGL_NO_SURFACE;context=EGL_NO_CONTEXT;}
    void tick(float dt){frame(dt);}
    bool isRunning() const { return running; }
};
static Game game;
static void command(android_app*app,int32_t cmd){if(cmd==APP_CMD_INIT_WINDOW&&app->window){if(!game.init(app->window))LOGE("Game initialization failed");}if(cmd==APP_CMD_TERM_WINDOW)game.shutdown();}
void android_main(android_app*app){
    app->onAppCmd=command;app->onInputEvent=[](android_app*,AInputEvent*e)->int{return game.input(e);};int64_t previous=0;
    while(true){int events;android_poll_source*source;while(ALooper_pollOnce(game.isRunning()?0:-1,nullptr,&events,(void**)&source)>=0){if(source)source->process(app,source);if(app->destroyRequested){game.shutdown();return;}}
        timespec ts{};clock_gettime(CLOCK_MONOTONIC,&ts);int64_t now=(int64_t)ts.tv_sec*1000000000LL+ts.tv_nsec;float dt=previous?(now-previous)/1e9f:.016f;previous=now;if(game.isRunning())game.tick(dt);}
}
