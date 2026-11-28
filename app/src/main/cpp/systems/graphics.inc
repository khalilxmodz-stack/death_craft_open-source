// Subsystem: OpenGL ES shader setup, geometry helpers, and texture loading.
// Function comments describe each method's responsibility and its place in the game systems.

// Compiles one OpenGL ES shader and reports compilation errors.
GLuint makeShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, 1024, nullptr, log);
        LOGE("Shader: %s", log);
    }
    return s;
}
// Creates the OpenGL ES program and configures graphics resources.
bool setupGL() {
    const char* vs = R"(#version 300 es
        layout(location=0) in vec3 p; layout(location=1) in vec3 c; layout(location=2) in vec2 uvIn;
        uniform mat4 mvp; uniform vec3 cameraPos; uniform float animTime; uniform float waterAnimation; out vec3 color; out float fogDistance; out vec2 texCoord;
        void main(){vec3 pos=p;if(waterAnimation>.5&&uvIn.x>=(4.0/12.0)&&uvIn.x<(5.0/12.0)){pos.y+=sin(animTime*1.15+p.x*.72+p.z*.61)*.035;}gl_Position=mvp*vec4(pos,1.0);color=c;texCoord=uvIn;fogDistance=length(pos-cameraPos);})";
    const char* fs = R"(#version 300 es
        precision highp float; in vec3 color; in float fogDistance; in vec2 texCoord; uniform float fogEnabled; uniform float textured; uniform sampler2D blockAtlas; uniform vec3 skyColor; uniform float fogStart; uniform float fogEnd; uniform float worldLight; out vec4 outColor;
        void main(){vec4 texel=vec4(1.0);if(textured>.5)texel=texture(blockAtlas,texCoord);vec3 base=color*texel.rgb;base*=worldLight;base=pow(max(base,vec3(0.0)),vec3(0.96));base=clamp((base-vec3(0.5))*1.06+vec3(0.5),0.0,1.0);float fog=fogEnabled*smoothstep(fogStart,fogEnd,fogDistance);outColor=vec4(mix(base,skyColor,fog),textured>.5?texel.a:1.0);})";
    GLuint a = makeShader(GL_VERTEX_SHADER, vs), b = makeShader(GL_FRAGMENT_SHADER, fs);
    program = glCreateProgram();
    glAttachShader(program, a);
    glAttachShader(program, b);
    glLinkProgram(program);
    glDeleteShader(a);
    glDeleteShader(b);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
        return false;
    mvpLoc = glGetUniformLocation(program, "mvp");
    cameraLoc = glGetUniformLocation(program, "cameraPos");
    fogLoc = glGetUniformLocation(program, "fogEnabled");
    textureLoc = glGetUniformLocation(program, "textured");
    atlasLoc = glGetUniformLocation(program, "blockAtlas");
    skyColorLoc = glGetUniformLocation(program, "skyColor");
    animTimeLoc = glGetUniformLocation(program, "animTime");
    waterAnimationLoc = glGetUniformLocation(program, "waterAnimation");
    fogStartLoc = glGetUniformLocation(program, "fogStart");
    fogEndLoc = glGetUniformLocation(program, "fogEnd");
    worldLightLoc = glGetUniformLocation(program, "worldLight");
    glUseProgram(program);
    glUniform1i(atlasLoc, 0);
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    return true;
}
// Appends a triangle and its vertex attributes to a mesh buffer.
void addTri(std::vector<float>& v,
            V3 a,
            V3 b,
            V3 c,
            V3 color,
            float au = 0,
            float av = 0,
            float bu = 1,
            float bv = 0,
            float cu = 1,
            float cv = 1) {
    v.insert(v.end(), {a.x, a.y, a.z, color.x, color.y, color.z, au, av,
                       b.x, b.y, b.z, color.x, color.y, color.z, bu, bv,
                       c.x, c.y, c.z, color.x, color.y, color.z, cu, cv});
}
// Appends a colored quad as two triangles.
void addFace(std::vector<float>& v, V3 a, V3 b, V3 c, V3 d, V3 color) {
    addTri(v, a, b, c, color, 0, 0, 1, 0, 1, 1);
    addTri(v, a, c, d, color, 0, 0, 1, 1, 0, 1);
}

// Applies the selected texture filtering mode.
void applyGraphicsFiltering() {
    GLenum filter = smoothGraphics ? GL_LINEAR : GL_NEAREST;
    if (atlasTexture) {
        glBindTexture(GL_TEXTURE_2D, atlasTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D,
                        GL_TEXTURE_MIN_FILTER,
                        smoothGraphics ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
    }
    if (khalilTexture) {
        glBindTexture(GL_TEXTURE_2D, khalilTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    }
}
// Loads the texture used by the Khalil character.
void loadKhalilTexture(AAssetManager* manager) {
    std::vector<unsigned char> pixels;
    int w = 1, h = 1, channels = 4;
    unsigned char white[4] = {255, 255, 255, 255};
    AAsset* asset =
        manager ? AAssetManager_open(manager, "khalil_monster.png", AASSET_MODE_BUFFER) : nullptr;
    if (!asset && manager)
        asset = AAssetManager_open(manager, "textures/khalil_monster.png", AASSET_MODE_BUFFER);
    if (asset) {
        off_t length = AAsset_getLength(asset);
        std::vector<unsigned char> encoded((size_t)length);
        int read = (int)AAsset_read(asset, encoded.data(), (size_t)length);
        AAsset_close(asset);
        unsigned char* decoded =
            read > 0 ? stbi_load_from_memory(encoded.data(), read, &w, &h, &channels, 4) : nullptr;
        if (decoded) {
            pixels.assign(decoded, decoded + (size_t)w * h * 4);
            stbi_image_free(decoded);
            LOGE("Loaded khalil_monster.png: %dx%d", w, h);
        } else
            LOGE("khalil_monster.png missing or invalid; using solid white monster");
    } else
        LOGE("khalil_monster.png missing; using solid white monster");
    glGenTextures(1, &khalilTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, khalilTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 w,
                 h,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 pixels.empty() ? white : pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

// Loads the block atlas and other game textures.
bool loadTextures(AAssetManager* manager) {
    if (!manager)
        return false;
    constexpr int tile = 256, atlasTiles = 12, atlasW = tile * atlasTiles;
    std::vector<unsigned char> atlas((size_t)atlasW * tile * 4, 255);
    const char* names[] = {"textures/grass_texture.png",
                           "textures/dirt_texture.png",
                           "textures/stone_texture.png",
                           "textures/grass_tree_texture.png",
                           "textures/water_texture.png",
                           "textures/tnt.png",
                           "textures/sword.png",
                           "textures/wood_tree_texture.png",
                           "textures/iron_texture.png",
                           "textures/iron_block_texture.png",
                           "textures/crafting_table_texture.png",
                           "textures/stick_texture.png"};
    const unsigned char fallback[atlasTiles][4] = {{78, 156, 55, 255},
                                                   {126, 79, 43, 255},
                                                   {116, 122, 130, 255},
                                                   {49, 126, 48, 255},
                                                   {28, 112, 190, 210},
                                                   {255, 255, 255, 255},
                                                   {255, 255, 255, 255},
                                                   {104, 66, 34, 255},
                                                   {115, 105, 95, 255},
                                                   {180, 185, 190, 255},
                                                   {125, 83, 45, 255},
                                                   {160, 112, 55, 255}};
    for (int layer = 0; layer < atlasTiles; layer++)
        for (int y = 0; y < tile; y++)
            for (int x = 0; x < tile; x++) {
                size_t dst = ((size_t)y * atlasW + (size_t)layer * tile + x) * 4;
                std::copy(fallback[layer], fallback[layer] + 4, atlas.begin() + dst);
            }
    int loaded = 0;
    stbi_set_flip_vertically_on_load(1);
    for (int layer = 0; layer < atlasTiles; layer++) {
        AAsset* asset = AAssetManager_open(manager, names[layer], AASSET_MODE_BUFFER);
        if (!asset) {
            LOGE("Texture asset missing: %s", names[layer]);
            continue;
        }
        off_t length = AAsset_getLength(asset);
        std::vector<unsigned char> encoded((size_t)length);
        int read = (int)AAsset_read(asset, encoded.data(), (size_t)length);
        AAsset_close(asset);
        if (read <= 0)
            continue;
        int w = 0, h = 0, channels = 0;
        unsigned char* pixels = stbi_load_from_memory(encoded.data(), read, &w, &h, &channels, 4);
        if (!pixels) {
            LOGE("Texture decode failed: %s", names[layer]);
            if (layer == 5) {
                // tnt.png in the project is WebP data despite its .png extension;
                // stb_image cannot decode WebP, so build the same TNT tile directly.
                static const uint8_t letters[3][7] = {
                    {31, 4, 4, 4, 4, 4, 4}, {17, 27, 21, 21, 17, 17, 17}, {31, 4, 4, 4, 4, 4, 4}};
                for (int y = 0; y < tile; y++)
                    for (int x = 0; x < tile; x++) {
                        bool band = y >= 88 && y < 168;
                        int stripe = (x / 32) % 2;
                        unsigned char rr = band ? (y < 96 ? 218 : 245) : (stripe ? 205 : 224);
                        unsigned char gg = band ? (y < 96 ? 218 : 239) : (stripe ? 30 : 43);
                        unsigned char bb = band ? (y < 96 ? 218 : 225) : (stripe ? 23 : 32);
                        if (y % 32 < 3 || x % 32 < 2) {
                            rr = (unsigned char)(rr * 0.78f);
                            gg = (unsigned char)(gg * 0.78f);
                            bb = (unsigned char)(bb * 0.78f);
                        }
                        if (band) {
                            int col = x / 16 - 4, row = (165 - y) / 10;
                            if (col >= 0 && col < 17 && row >= 0 && row < 7) {
                                int letter = col < 5 ? 0 : (col < 11 ? 1 : 2);
                                int bit = col < 5 ? col : (col < 11 ? col - 6 : col - 12);
                                if (letters[letter][row] & (1 << (4 - bit))) {
                                    rr = 24;
                                    gg = 29;
                                    bb = 49;
                                }
                            }
                        }
                        size_t dst = ((size_t)y * atlasW + (size_t)layer * tile + x) * 4;
                        atlas[dst] = rr;
                        atlas[dst + 1] = gg;
                        atlas[dst + 2] = bb;
                        atlas[dst + 3] = 255;
                    }
                loaded++;
            }
            continue;
        }
        for (int y = 0; y < tile; y++)
            for (int x = 0; x < tile; x++) {
                int sx = x * w / tile, sy = y * h / tile;
                size_t src = ((size_t)sy * w + sx) * 4;
                size_t dst = ((size_t)y * atlasW + (size_t)layer * tile + x) * 4;
                std::copy(pixels + src, pixels + src + 4, atlas.begin() + dst);
            }
        stbi_image_free(pixels);
        loaded++;
    }
    glGenTextures(1, &atlasTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, atlasW, tile, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    loadKhalilTexture(manager);
    applyGraphicsFiltering();
    LOGE("Loaded block and item textures: %d/%d", loaded, atlasTiles);
    return loaded > 0;
}
