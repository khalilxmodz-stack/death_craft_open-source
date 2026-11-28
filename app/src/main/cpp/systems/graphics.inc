    GLuint makeShader(GLenum type,const char*src){GLuint s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[1024];glGetShaderInfoLog(s,1024,nullptr,log);LOGE("Shader: %s",log);}return s;}
    bool setupGL(){
        const char*vs=R"(#version 300 es
        layout(location=0) in vec3 p; layout(location=1) in vec3 c; layout(location=2) in vec2 uvIn;
        uniform mat4 mvp; uniform vec3 cameraPos; uniform float animTime; out vec3 color; out float fogDistance; out vec2 texCoord;
        void main(){vec3 pos=p;if(uvIn.x>.80){pos.y+=sin(animTime*1.15+p.x*.72+p.z*.61)*.035;}gl_Position=mvp*vec4(pos,1.0);color=c;texCoord=uvIn;fogDistance=length(pos-cameraPos);})";
        const char*fs=R"(#version 300 es
        precision highp float; in vec3 color; in float fogDistance; in vec2 texCoord; uniform float fogEnabled; uniform float textured; uniform sampler2D blockAtlas; uniform vec3 skyColor; out vec4 outColor;
        void main(){vec3 base=color;if(textured>.5)base*=texture(blockAtlas,texCoord).rgb;float fog=fogEnabled*smoothstep(78.0,126.0,fogDistance);outColor=vec4(mix(base,skyColor,fog),1.0);})";
        GLuint a=makeShader(GL_VERTEX_SHADER,vs),b=makeShader(GL_FRAGMENT_SHADER,fs);
        program=glCreateProgram();glAttachShader(program,a);glAttachShader(program,b);glLinkProgram(program);glDeleteShader(a);glDeleteShader(b);
        GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok)return false;
        mvpLoc=glGetUniformLocation(program,"mvp");cameraLoc=glGetUniformLocation(program,"cameraPos");fogLoc=glGetUniformLocation(program,"fogEnabled");textureLoc=glGetUniformLocation(program,"textured");atlasLoc=glGetUniformLocation(program,"blockAtlas");skyColorLoc=glGetUniformLocation(program,"skyColor");animTimeLoc=glGetUniformLocation(program,"animTime");
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
        constexpr int tile=256, atlasW=tile*5;
        std::vector<unsigned char> atlas((size_t)atlasW*tile*4,255);
        const char* names[]={"textures/grass_texture.png","textures/dirt_texture.png","textures/stone_texture.png","textures/grass_tree_texture.png","textures/water_texture.png"};
        const unsigned char fallback[5][4]={{78,156,55,255},{126,79,43,255},{116,122,130,255},{49,126,48,255},{28,112,190,210}};
        for(int layer=0;layer<5;layer++)for(int y=0;y<tile;y++)for(int x=0;x<tile;x++){
            size_t dst=((size_t)y*atlasW+(size_t)layer*tile+x)*4;
            std::copy(fallback[layer],fallback[layer]+4,atlas.begin()+dst);
        }
        int loaded=0;stbi_set_flip_vertically_on_load(1);
        for(int layer=0;layer<5;layer++){
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
        LOGE("Loaded block textures: %d/5",loaded);return loaded>0;
    }
