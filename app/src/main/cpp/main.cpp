#include <android_native_app_glue.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <vector>
#include <algorithm>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "DeathCraft", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "DeathCraft", __VA_ARGS__)

static const char* VS = R"(#version 300 es
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main(){ gl_Position=uMVP*vec4(aPos,1.0); })";

static const char* FS = R"(#version 300 es
precision mediump float;
uniform vec4 uColor;
out vec4 fragColor;
void main(){ fragColor=uColor; })";

struct Mat4 { float m[16]{}; };
static Mat4 identity() { Mat4 r{}; r.m[0]=r.m[5]=r.m[10]=r.m[15]=1; return r; }
static Mat4 multiply(const Mat4&a,const Mat4&b) {
    Mat4 r{};
    for(int c=0;c<4;c++) for(int row=0;row<4;row++)
        for(int k=0;k<4;k++) r.m[c*4+row]+=a.m[k*4+row]*b.m[c*4+k];
    return r;
}
static Mat4 perspective(float f,float aspect,float n,float z) {
    Mat4 r{}; float t=1.0f/std::tan(f*0.5f);
    r.m[0]=t/aspect; r.m[5]=t; r.m[10]=(z+n)/(n-z);
    r.m[11]=-1; r.m[14]=(2*z*n)/(n-z); return r;
}
static Mat4 view(float x,float y,float z,float yaw,float pitch) {
    float cy=std::cos(yaw), sy=std::sin(yaw), cp=std::cos(pitch), sp=std::sin(pitch);
    Mat4 r=identity();
    r.m[0]=cy; r.m[4]=sy*sp; r.m[8]=sy*cp;
    r.m[1]=0;  r.m[5]=cp;    r.m[9]=-sp;
    r.m[2]=-sy; r.m[6]=cy*sp; r.m[10]=cy*cp;
    r.m[12]=-(r.m[0]*x+r.m[4]*y+r.m[8]*z);
    r.m[13]=-(r.m[1]*x+r.m[5]*y+r.m[9]*z);
    r.m[14]=-(r.m[2]*x+r.m[6]*y+r.m[10]*z);
    return r;
}

class Game {
public:
    EGLDisplay display=EGL_NO_DISPLAY;
    EGLSurface surface=EGL_NO_SURFACE;
    EGLContext context=EGL_NO_CONTEXT;
    GLuint program=0, vao=0, vbo=0;
    GLint mvpLoc=-1, colorLoc=-1;
    int width=1,height=1;
    float px=0,py=1.7f,pz=0,yaw=0,pitch=0;
    float vy=0;
    bool grounded=true, running=false;
    bool joy=false, look=false;
    float joyX=0,joyY=0,lastX=0,lastY=0;
    int joyPointer=-1,lookPointer=-1;

    bool init(ANativeWindow* window) {
        const EGLint attrs[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
            EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,16,EGL_NONE};
        display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if(display==EGL_NO_DISPLAY || !eglInitialize(display,nullptr,nullptr)) return false;
        EGLConfig config; EGLint count;
        eglChooseConfig(display,attrs,&config,1,&count);
        if(!count) return false;
        const EGLint ctxAttrs[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
        context=eglCreateContext(display,config,EGL_NO_CONTEXT,ctxAttrs);
        surface=eglCreateWindowSurface(display,config,window,nullptr);
        if(context==EGL_NO_CONTEXT || surface==EGL_NO_SURFACE || !eglMakeCurrent(display,surface,surface,context)) return false;
        eglQuerySurface(display,surface,EGL_WIDTH,&width);
        eglQuerySurface(display,surface,EGL_HEIGHT,&height);
        glViewport(0,0,width,height);
        glEnable(GL_DEPTH_TEST);
        GLuint vs=shader(GL_VERTEX_SHADER,VS), fs=shader(GL_FRAGMENT_SHADER,FS);
        program=glCreateProgram(); glAttachShader(program,vs); glAttachShader(program,fs); glLinkProgram(program);
        glDeleteShader(vs); glDeleteShader(fs);
        GLint ok=0; glGetProgramiv(program,GL_LINK_STATUS,&ok); if(!ok) return false;
        mvpLoc=glGetUniformLocation(program,"uMVP"); colorLoc=glGetUniformLocation(program,"uColor");
        glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo);
        running=true; return true;
    }

    GLuint shader(GLenum type,const char* src) {
        GLuint s=glCreateShader(type); glShaderSource(s,1,&src,nullptr); glCompileShader(s);
        GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
        if(!ok) LOGE("Shader compilation failed"); return s;
    }

    void draw(const std::vector<float>& vertices, GLenum mode, const Mat4& mvp, float r,float g,float b,float a=1) {
        glUseProgram(program); glUniformMatrix4fv(mvpLoc,1,GL_FALSE,mvp.m);
        glUniform4f(colorLoc,r,g,b,a);
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);
        glDrawArrays(mode,0,(GLsizei)(vertices.size()/3));
    }

    void frame(float dt) {
        if(!running || !eglMakeCurrent(display,surface,surface,context)) return;
        dt=std::min(dt,0.04f);
        float forward=-joyY, side=joyX;
        float speed=5.0f;
        px+=(std::sin(yaw)*forward+std::cos(yaw)*side)*speed*dt;
        pz+=(-std::cos(yaw)*forward+std::sin(yaw)*side)*speed*dt;
        if(!grounded) { vy-=18.0f*dt; py+=vy*dt; if(py<=1.7f){py=1.7f;vy=0;grounded=true;} }

        glViewport(0,0,width,height);
        glClearColor(0.48f,0.75f,0.96f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        Mat4 proj=perspective(1.15f,(float)width/(float)height,0.1f,500.0f);
        Mat4 vp=multiply(proj,view(px,py,pz,yaw,pitch));
        // A large green ground tile follows the player, creating an endless flat field.
        std::vector<float> ground={
            px-250,0,pz-250, px+250,0,pz-250, px+250,0,pz+250,
            px-250,0,pz-250, px+250,0,pz+250, px-250,0,pz+250
        };
        draw(ground,GL_TRIANGLES,vp,0.20f,0.62f,0.19f);
        // Subtle repeating grid for a clear sense of scale.
        std::vector<float> lines;
        int cx=(int)std::floor(px/10.0f), cz=(int)std::floor(pz/10.0f);
        for(int i=-12;i<=12;i++){
            float x=(cx+i)*10.0f, z=(cz+i)*10.0f;
            lines.insert(lines.end(),{x,0.012f,pz-120,x,0.012f,pz+120});
            lines.insert(lines.end(),{px-120,0.012f,z,px+120,0.012f,z});
        }
        draw(lines,GL_LINES,vp,0.16f,0.52f,0.16f);
        // Simple touch HUD: joystick base and jump button.
        glDisable(GL_DEPTH_TEST);
        Mat4 hud=identity();
        std::vector<float> circles;
        auto circle=[&](float x,float y,float radius){
            for(int i=0;i<40;i++){ float a=i*6.2831853f/40, b=(i+1)*6.2831853f/40;
                circles.insert(circles.end(),{x,y,0,x+std::cos(a)*radius,y+std::sin(a)*radius,0,x+std::cos(b)*radius,y+std::sin(b)*radius,0});}
        };
        circle(-0.72f,-0.62f,0.19f); draw(circles,GL_TRIANGLES,hud,0.08f,0.12f,0.10f,0.42f);
        circles.clear(); circle(0.78f,-0.62f,0.12f); draw(circles,GL_TRIANGLES,hud,0.95f,0.82f,0.22f,0.8f);
        glEnable(GL_DEPTH_TEST);
        eglSwapBuffers(display,surface);
    }

    int input(AInputEvent* e) {
        if(AInputEvent_getType(e)!=AINPUT_EVENT_TYPE_MOTION) return 0;
        int action=AMotionEvent_getAction(e)&AMOTION_EVENT_ACTION_MASK;
        int index=(AMotionEvent_getAction(e)&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int id=AMotionEvent_getPointerId(e,index);
        float x=AMotionEvent_getX(e,index)/(float)width*2.0f-1.0f;
        float y=1.0f-AMotionEvent_getY(e,index)/(float)height*2.0f;
        if(action==AMOTION_EVENT_ACTION_DOWN || action==AMOTION_EVENT_ACTION_POINTER_DOWN){
            if(x < -0.35f && y < 0.05f){ joy=true; joyPointer=id; updateJoy(x,y); }
            else if(x>0.58f && y < -0.35f){ if(grounded){vy=7.2f;grounded=false;} }
            else {look=true;lookPointer=id;lastX=x;lastY=y;}
            return 1;
        }
        if(action==AMOTION_EVENT_ACTION_MOVE){
            size_t n=AMotionEvent_getPointerCount(e);
            for(size_t i=0;i<n;i++){
                int pid=AMotionEvent_getPointerId(e,(size_t)i);
                float tx=AMotionEvent_getX(e,(size_t)i)/(float)width*2.0f-1.0f;
                float ty=1.0f-AMotionEvent_getY(e,(size_t)i)/(float)height*2.0f;
                if(pid==joyPointer) updateJoy(tx,ty);
                if(pid==lookPointer){ yaw-=(tx-lastX)*2.8f; pitch=std::clamp(pitch-(ty-lastY)*2.2f,-1.35f,1.35f); lastX=tx;lastY=ty; }
            }
            return 1;
        }
        if(action==AMOTION_EVENT_ACTION_UP || action==AMOTION_EVENT_ACTION_POINTER_UP || action==AMOTION_EVENT_ACTION_CANCEL){
            if(id==joyPointer){joy=false;joyPointer=-1;joyX=joyY=0;}
            if(id==lookPointer){look=false;lookPointer=-1;}
            return 1;
        }
        return 1;
    }
    void updateJoy(float x,float y){joyX=std::clamp((x+0.72f)/0.19f,-1.0f,1.0f);joyY=std::clamp((y+0.62f)/0.19f,-1.0f,1.0f);}
    void shutdown(){
        running=false;
        if(display!=EGL_NO_DISPLAY){eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
            if(context!=EGL_NO_CONTEXT)eglDestroyContext(display,context);
            if(surface!=EGL_NO_SURFACE)eglDestroySurface(display,surface);
            eglTerminate(display);}
        display=EGL_NO_DISPLAY;surface=EGL_NO_SURFACE;context=EGL_NO_CONTEXT;
    }
};

static Game game;
static void handle_cmd(android_app* app,int32_t cmd) {
    if(cmd==APP_CMD_INIT_WINDOW && app->window){ if(!game.init(app->window)) LOGE("OpenGL initialization failed"); }
    if(cmd==APP_CMD_TERM_WINDOW) game.shutdown();
}
void android_main(android_app* app) {
    app->onAppCmd=handle_cmd; app->onInputEvent=[](android_app*,AInputEvent* e)->int{return game.input(e);};
    int64_t previous=0;
    while(true){
        int events; android_poll_source* source;
        while(ALooper_pollOnce(game.running?0:-1,nullptr,&events,(void**)&source)>=0){
            if(source) source->process(app,source);
            if(app->destroyRequested){game.shutdown();return;}
        }
        if(game.running){
            timespec ts{}; clock_gettime(CLOCK_MONOTONIC,&ts);
            int64_t now=(int64_t)ts.tv_sec*1000000000LL+ts.tv_nsec;
            float dt=previous? (now-previous)/1000000000.0f:0.016f; previous=now;
            game.frame(dt);
        }
    }
}
