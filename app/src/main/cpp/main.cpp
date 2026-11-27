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
    enum class Screen { MENU, PLAYING, PAUSED };
    Screen screen=Screen::MENU;
    float fpsElapsed=0; int fpsFrames=0,fpsValue=0;
    bool breaking=false,placing=false;
    int selected=1;
    float breakTimer=0;
    std::array<uint8_t,5> hotbar{{1,2,3,1,2}};
    std::string status="DEATH_CRAFT  |  V0.1";
    int statusFrames=0;

    GLuint makeShader(GLenum type,const char*src){GLuint s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[1024];glGetShaderInfoLog(s,1024,nullptr,log);LOGE("Shader: %s",log);}return s;}
    bool setupGL(){
        const char*vs=R"(#version 300 es
        layout(location=0) in vec3 p; layout(location=1) in vec3 c;
        uniform mat4 mvp; uniform vec3 cameraPos; out vec3 color; out float fogDistance;
        void main(){gl_Position=mvp*vec4(p,1.0);color=c;fogDistance=length(p-cameraPos);})";
        const char*fs=R"(#version 300 es
        precision mediump float; in vec3 color; in float fogDistance; uniform float fogEnabled; out vec4 outColor;
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
        for(int z=0;z<CS;z++)for(int x=0;x<CS;x++){
            c.set(x,0,z,1);
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
        if(b==2)return{.48f,.31f,.17f};
        return{.43f,.45f,.48f};
    }
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
        glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
        Mat4 ortho=identity();std::vector<float> v;
        auto tri=[&](float x1,float y1,float x2,float y2,float x3,float y3,V3 c){addTri(v,{x1,y1,0},{x2,y2,0},{x3,y3,0},c);};
        auto rect=[&](float x,float y,float w,float h,V3 c){tri(x,y,x+w,y,x+w,y+h,c);tri(x,y,x+w,y+h,x,y+h,c);};
        auto circle=[&](float x,float y,float r,V3 c){for(int i=0;i<32;i++){float a=i*2*PI/32,b=(i+1)*2*PI/32;tri(x,y,x+std::cos(a)*r,y+std::sin(a)*r,x+std::cos(b)*r,y+std::sin(b)*r,c);}};
        // Semi-transparent-looking dark control pads and bright action controls.
        circle(joyOriginX,joyOriginY,.22f,{.10f,.14f,.13f});circle(joyOriginX+joyX*.14f,joyOriginY+joyY*.14f,.085f,{.72f,.78f,.70f});
        circle(.78f,-.62f,.105f,{.94f,.77f,.20f});
        circle(.58f,-.30f,.075f,{.20f,.72f,.90f});
        circle(.82f,.02f,.075f,{.85f,.35f,.20f});
        auto line=[&](float x1,float y1,float x2,float y2,float t,V3 col){
            float dx=x2-x1,dy=y2-y1,len=std::sqrt(dx*dx+dy*dy);if(len<.0001f)return;
            float ox=-dy/len*t,oy=dx/len*t;
            tri(x1+ox,y1+oy,x2+ox,y2+oy,x2-ox,y2-oy,col);
            tri(x1+ox,y1+oy,x2-ox,y2-oy,x1-ox,y1-oy,col);
        };
        line(.78f,-.66f,.78f,-.57f,.012f,{.12f,.13f,.10f});
        line(.78f,-.57f,.75f,-.60f,.012f,{.12f,.13f,.10f});
        line(.78f,-.57f,.81f,-.60f,.012f,{.12f,.13f,.10f});
        line(.54f,-.31f,.62f,-.31f,.009f,{1,1,1});
        line(.54f,-.31f,.58f,-.26f,.009f,{1,1,1});
        line(.58f,-.26f,.62f,-.31f,.009f,{1,1,1});
        line(.79f,.02f,.85f,.08f,.012f,{1,1,1});
        line(.85f,.08f,.88f,.02f,.012f,{1,1,1});
        line(.79f,.02f,.88f,.02f,.012f,{1,1,1});
        // Five selectable hotbar slots.
        float start=-.24f,slot=.105f;
        for(int i=0;i<5;i++){V3 c=i==selected?V3{1,.82f,.25f}:V3{.18f,.22f,.20f};rect(start+i*.11f,-.91f,slot,.15f,c);
            V3 bc=blockColor(hotbar[i],2);rect(start+i*.11f+.025f,-.875f,.055f,.075f,bc);}
        // FPS counter and pause control
        rect(-.97f,.82f,.23f,.13f,{.08f,.12f,.13f});
        // Pause icon
        rect(.88f,.82f,.035f,.12f,{1,1,1});rect(.94f,.82f,.035f,.12f,{1,1,1});
        // Crosshair
        rect(-.018f,-.008f,.036f,.016f,{1,1,1});rect(-.006f,-.025f,.012f,.05f,{1,1,1});
        draw(v,GL_TRIANGLES,ortho,0,0,0,false);
        drawText("FPS:"+std::to_string(fpsValue),-.93f,.865f,.018f,{1,1,1});
        glDisable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);
    }
    std::array<uint8_t,7> glyph(char c){
        switch(c){
        case 'A':return {14,17,17,31,17,17,17};case 'B':return {30,17,17,30,17,17,30};
        case 'C':return {14,17,16,16,16,17,14};case 'D':return {30,17,17,17,17,17,30};
        case 'E':return {31,16,16,30,16,16,31};case 'F':return {31,16,16,30,16,16,16};
        case 'H':return {17,17,17,31,17,17,17};case 'I':return {14,4,4,4,4,4,14};
        case 'L':return {16,16,16,16,16,16,31};case 'M':return {17,27,21,21,17,17,17};
        case 'N':return {17,25,25,21,19,19,17};case 'O':return {14,17,17,17,17,17,14};
        case 'P':return {30,17,17,30,16,16,16};case 'R':return {30,17,17,30,20,18,17};
        case 'S':return {15,16,16,14,1,1,30};case 'T':return {31,4,4,4,4,4,4};
        case 'U':return {17,17,17,17,17,17,14};case 'V':return {17,17,17,17,17,10,4};
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
        for(char c:str){auto g=glyph(c);for(int row=0;row<7;row++)for(int col=0;col<5;col++)if(g[row]&(1<<(4-col))){
            float px0=cursor+col*scale,py0=y-row*scale;
            addFace(v,{px0,py0,0},{px0+scale*.82f,py0,0},{px0+scale*.82f,py0+scale*.82f,0},{px0,py0+scale*.82f,0},color);
        }cursor+=scale*6;}
        Mat4 ortho=identity();draw(v,GL_TRIANGLES,ortho,0,0,0,false);
    }
    void drawMenu(){
        std::vector<float> v;auto rect=[&](float x,float y,float w,float h,V3 c){addFace(v,{x,y,0},{x+w,y,0},{x+w,y+h,0},{x,y+h,0},c);};
        rect(-.62f,-.48f,1.24f,.98f,{.14f,.21f,.23f});
        rect(-.36f,-.20f,.72f,.22f,{.20f,.52f,.30f});
        rect(-.36f,-.53f,.72f,.22f,{.25f,.31f,.34f});
        draw(v,GL_TRIANGLES,identity(),0,0,0,false);
        if(screen==Screen::MENU){drawText("DEATH CRAFT",-.39f,.30f,.040f,{1,.88f,.35f});drawText("TAP TO PLAY",-.26f,-.055f,.027f,{1,1,1});}
        else {drawText("PAUSED",-.17f,.30f,.045f,{1,.88f,.35f});drawText("RESUME",-.17f,-.055f,.030f,{1,1,1});drawText("MAIN MENU",-.24f,-.385f,.026f,{1,1,1});}
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
            if(ay<0||ay>=HEIGHT){status="Build height limit";statusFrames=90;return;}
            if(overlaps(ax+.5f,ay,az+.5f)){status="Cannot place inside player";statusFrames=90;return;}
            int cx=floorDiv(ax,CS),cz=floorDiv(az,CS);
            Chunk&c=ensureChunk(cx,cz);c.set(modp(ax,CS),ay,modp(az,CS),hotbar[selected]);
            edits[{ax,ay,az}]=hotbar[selected];
        } else {
            int cx=floorDiv(x,CS),cz=floorDiv(z,CS);
            Chunk&c=ensureChunk(cx,cz);c.set(modp(x,CS),y,modp(z,CS),0);
            edits[{x,y,z}]=0;
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
        fpsElapsed+=dt;fpsFrames++;if(fpsElapsed>=.5f){fpsValue=(int)(fpsFrames/fpsElapsed);fpsFrames=0;fpsElapsed=0;}
        if(screen!=Screen::PLAYING){glViewport(0,0,width,height);glClearColor(.10f,.16f,.19f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);drawMenu();eglSwapBuffers(display,surface);return;}
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
        updateWorld();
        glViewport(0,0,width,height);glClearColor(.48f,.74f,.96f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        Mat4 vp=mul(perspective(1.12f,(float)width/height,.1f,150),camera(px,feet+1.62f,pz,yaw,pitch));
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
        drawHud();eglSwapBuffers(display,surface);
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
        if(!setupGL())return false;running=true;updateWorld();return true;
    }
    int input(AInputEvent*e){
        if(AInputEvent_getType(e)!=AINPUT_EVENT_TYPE_MOTION)return 0;
        int action=AMotionEvent_getAction(e)&AMOTION_EVENT_ACTION_MASK;
        int idx=(AMotionEvent_getAction(e)&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int id=AMotionEvent_getPointerId(e,idx);float x=AMotionEvent_getX(e,idx)/width*2-1,y=1-AMotionEvent_getY(e,idx)/height*2;
        if(action==AMOTION_EVENT_ACTION_DOWN||action==AMOTION_EVENT_ACTION_POINTER_DOWN){
            if(screen==Screen::MENU){if(y>-.35f&&y<.35f){screen=Screen::PLAYING;}return 1;}
            if(screen==Screen::PAUSED){if(y>-.15f&&y<.35f){screen=Screen::PLAYING;}else if(y<-.25f&&y>-.75f){screen=Screen::MENU;}return 1;}
            if(x>.80f&&y>.78f){screen=Screen::PAUSED;joyId=lookId=-1;joyX=joyY=0;return 1;}
            if(x<-.42f&&y<.0f){joyId=id;joyOriginX=x;joyOriginY=y;setJoy(x,y);}
            else if(x>.67f&&y<-.40f){if(grounded){vy=7.4f;grounded=false;}}
            else if(x>.68f&&y>-.42f&&y<.20f){editBlock(false);}
            else if(x>.43f&&x<.68f&&y>-.48f&&y<.05f){editBlock(true);}
            else if(y<-.72f&&x>-.30f&&x<.30f){int s=(int)((x+.30f)/.60f*5);selected=std::clamp(s,0,4);}
            else {lookId=id;lastX=x;lastY=y;}
            return 1;
        }
        if(action==AMOTION_EVENT_ACTION_MOVE){
            for(size_t i=0;i<AMotionEvent_getPointerCount(e);i++){
                int pid=AMotionEvent_getPointerId(e,i);float tx=AMotionEvent_getX(e,i)/width*2-1,ty=1-AMotionEvent_getY(e,i)/height*2;
                if(pid==joyId)setJoy(tx,ty);
                if(pid==lookId){yaw-=(tx-lastX)*2.7f;pitch=std::clamp(pitch-(ty-lastY)*2.2f,-1.35f,1.35f);lastX=tx;lastY=ty;}
            }return 1;
        }
        if(action==AMOTION_EVENT_ACTION_UP||action==AMOTION_EVENT_ACTION_POINTER_UP||action==AMOTION_EVENT_ACTION_CANCEL){
            if(action==AMOTION_EVENT_ACTION_CANCEL){joyId=lookId=-1;joyX=joyY=0;return 1;}
            if(id==joyId){joyId=-1;joyX=joyY=0;}if(id==lookId)lookId=-1;return 1;
        }return 1;
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
