    // Subsystem: Chunk mesh generation, OpenGL submission, HUD, inventory, and text rendering.
    // Function comments describe each method's responsibility and its place in the game systems.

    // Builds the visible mesh and shadow mesh for a chunk.
    void rebuild(Chunk& c) {
        c.mesh.clear();
        c.shadowMesh.clear();
        int ox = c.cx * CS, oz = c.cz * CS;
        auto localSolid = [&](int x, int y, int z) {
            if (x >= 0 && x < CS && z >= 0 && z < CS) {
                uint8_t q = c.get(x, y, z);
                return q != 0 && q != 9 && !(q >= 24 && q <= 28);
            }
            return solid(ox + x, y, oz + z);
        };
        // Calculate ambient light once per block, not once per visible face.
        for (int y = 0; y < HEIGHT; y++)
            for (int z = 0; z < CS; z++)
                for (int x = 0; x < CS; x++) {
                    uint8_t b = c.get(x, y, z);
                    if (!b)
                        continue;
                    bool plantBlock = b >= 24 && b <= 28;
                    bool skyVisible = true;
                    for (int sy = y + 1; sy < HEIGHT; sy++)
                        if (localSolid(x, sy, z)) {
                            skyVisible = false;
                            break;
                        }
                    float ambient = skyVisible ? 1.0f : .24f;
                    int occlusion = 0;
                    const int ox4[4] = {-1, 1, 0, 0}, oz4[4] = {0, 0, -1, 1};
                    for (int n = 0; n < 4; n++)
                        if (localSolid(x + ox4[n], y, z + oz4[n]))
                            occlusion++;
                    float blockLight = std::max(.68f, ambient * (1.0f - occlusion * .045f));
                    if (b == 1) {
                        bool underTree = false;
                        for (int dz = -2; dz <= 2 && !underTree; dz++)
                            for (int dx = -2; dx <= 2 && !underTree; dx++)
                                for (int dy = 2; dy <= 5; dy++) {
                                    int tx = x + dx, ty = y + dy, tz = z + dz;
                                    uint8_t q = (tx >= 0 && tx < CS && tz >= 0 && tz < CS)
                                                    ? c.get(tx, ty, tz)
                                                    : blockAt(ox + tx, ty, oz + tz);
                                    if (q == 5) {
                                        underTree = true;
                                        break;
                                    }
                                }
                        if (underTree)
                            blockLight *= .88f;
                    }
                    float X = (float)(ox + x), Y = (float)y, Z = (float)(oz + z);
                    V3 p000{X, Y, Z}, p100{X + 1, Y, Z}, p110{X + 1, Y + 1, Z}, p010{X, Y + 1, Z};
                    V3 p001{X, Y, Z + 1}, p101{X + 1, Y, Z + 1}, p111{X + 1, Y + 1, Z + 1},
                        p011{X, Y + 1, Z + 1};
                    if (plantBlock) {
                        auto blade =
                            [&](float x0, float z0, float x1, float z1, float height, V3 color) {
                                V3 a{x0, Y, z0}, bb{x1, Y, z1}, cc{x1, Y + height, z1},
                                    d{x0, Y + height, z0};
                                addTri(c.mesh, a, bb, cc, color);
                                addTri(c.mesh, a, cc, d, color);
                                V3 e{x1, Y, z0}, f{x0, Y, z1}, g{x0, Y + height, z1},
                                    hh{x1, Y + height, z0};
                                addTri(c.mesh, e, f, g, color);
                                addTri(c.mesh, e, g, hh, color);
                            };
                        if (b == 28) {
                            blade(X + .12f, Z + .20f, X + .42f, Z + .34f, .62f, {.22f, .62f, .16f});
                            blade(X + .52f, Z + .12f, X + .82f, Z + .38f, .48f, {.30f, .72f, .19f});
                            blade(X + .18f, Z + .68f, X + .48f, Z + .84f, .54f, {.18f, .55f, .14f});
                            blade(X + .58f, Z + .60f, X + .86f, Z + .78f, .68f, {.35f, .76f, .20f});
                        } else {
                            V3 stem{.18f, .48f, .13f};
                            blade(X + .46f, Z + .46f, X + .54f, Z + .54f, .46f, stem);
                            V3 petal = b == 24 ? V3{1.0f, .24f, .30f}
                                               : (b == 25 ? V3{1.0f, .78f, .16f}
                                                          : (b == 26 ? V3{.78f, .42f, .94f}
                                                                     : V3{1.0f, .92f, .88f}));
                            float cx = X + .5f, cz = Z + .5f, py = Y + .48f;
                            addFace(c.mesh,
                                    {cx - .22f, py, cz},
                                    {cx + .22f, py, cz},
                                    {cx + .12f, py + .25f, cz},
                                    {cx - .12f, py + .25f, cz},
                                    petal);
                            addFace(c.mesh,
                                    {cx, py, cz - .22f},
                                    {cx, py, cz + .22f},
                                    {cx, py + .25f, cz + .12f},
                                    {cx, py + .25f, cz - .12f},
                                    petal);
                            addFace(c.mesh,
                                    {cx - .09f, py + .22f, cz - .09f},
                                    {cx + .09f, py + .22f, cz - .09f},
                                    {cx + .09f, py + .38f, cz + .09f},
                                    {cx - .09f, py + .38f, cz + .09f},
                                    {1.0f, .82f, .22f});
                        }
                    } else if (b == 22 || b == 23) {
                        continue;
                    } else if (b == 20 || b == 21) {
                        float x0 = X + .06f, x1 = X + .94f, z0 = b == 20 ? Z + .43f : Z + .06f,
                              z1 = b == 20 ? Z + .57f : Z + .94f;
                        auto quad = [&](float ax,
                                        float ay,
                                        float az,
                                        float bx,
                                        float by,
                                        float bz,
                                        float cx,
                                        float cy,
                                        float cz,
                                        float dx,
                                        float dy,
                                        float dz,
                                        uint8_t mat) {
                            addBlockFace(c.mesh,
                                         {ax, ay, az},
                                         {bx, by, bz},
                                         {cx, cy, cz},
                                         {dx, dy, dz},
                                         mat,
                                         0,
                                         ox + x,
                                         y,
                                         oz + z,
                                         blockLight);
                        };
                        // Light oak door slab with a darker inset frame and four small square
                        // peephole panels.
                        quad(x0, Y, z0, x1, Y, z0, x1, Y + 2, z0, x0, Y + 2, z0, 20);
                        quad(x0, Y + 2, z1, x1, Y + 2, z1, x1, Y, z1, x0, Y, z1, 20);
                        quad(x0, Y, z0, x0, Y, z1, x0, Y + 2, z1, x0, Y + 2, z0, 20);
                        quad(x1, Y, z1, x1, Y, z0, x1, Y + 2, z0, x1, Y + 2, z1, 20);
                        for (int row = 0; row < 2; row++)
                            for (int col = 0; col < 2; col++) {
                                float hx = x0 + .24f + col * .25f, hy = Y + .78f + row * .34f,
                                      hz = b == 20 ? z0 - .004f : z1 + .004f;
                                float hs = .075f;
                                if (b == 20)
                                    quad(hx,
                                         hy,
                                         hz,
                                         hx + hs,
                                         hy,
                                         hz,
                                         hx + hs,
                                         hy + hs,
                                         hz,
                                         hx,
                                         hy + hs,
                                         hz,
                                         19);
                                else
                                    quad(hx,
                                         hy,
                                         hz,
                                         hx,
                                         hy,
                                         hz - hs,
                                         hx,
                                         hy + hs,
                                         hz - hs,
                                         hx,
                                         hy + hs,
                                         hz,
                                         19);
                            }
                    } else if (b == 18) {
                        auto box = [&](float x0,
                                       float y0,
                                       float z0,
                                       float x1,
                                       float y1,
                                       float z1,
                                       uint8_t material) {
                            V3 a{x0, y0, z0}, bb{x1, y0, z0}, cc{x1, y1, z0}, dd{x0, y1, z0},
                                e{x0, y0, z1}, f{x1, y0, z1}, g{x1, y1, z1}, h{x0, y1, z1};
                            addBlockFace(
                                c.mesh, a, bb, cc, dd, material, 0, ox + x, y, oz + z, blockLight);
                            addBlockFace(
                                c.mesh, f, e, h, g, material, 1, ox + x, y, oz + z, blockLight);
                            addBlockFace(
                                c.mesh, dd, cc, g, h, material, 2, ox + x, y, oz + z, blockLight);
                            addBlockFace(
                                c.mesh, e, f, bb, a, material, 3, ox + x, y, oz + z, blockLight);
                            addBlockFace(
                                c.mesh, e, a, dd, h, material, 4, ox + x, y, oz + z, blockLight);
                            addBlockFace(
                                c.mesh, bb, f, g, cc, material, 5, ox + x, y, oz + z, blockLight);
                        };
                        // Taller, fuller classic bed: wooden base, thick mattress and raised white
                        // pillow.
                        box(X + .035f, Y + .08f, Z + .035f, X + .965f, Y + .22f, Z + .965f, 4);
                        box(X + .045f, Y + .22f, Z + .045f, X + .955f, Y + .64f, Z + .955f, 18);
                        box(X + .07f, Y + .64f, Z + .70f, X + .93f, Y + .82f, Z + .95f, 14);
                        box(X + .045f, Y + .78f, Z + .045f, X + .955f, Y + .84f, Z + .12f, 4);
                    } else {
                        if (!localSolid(x, y, z - 1) && c.get(x, y, z - 1) != 9)
                            addBlockFace(c.mesh,
                                         p000,
                                         p100,
                                         p110,
                                         p010,
                                         b,
                                         0,
                                         ox + x,
                                         y,
                                         oz + z,
                                         blockLight);
                        if (!localSolid(x, y, z + 1) && c.get(x, y, z + 1) != 9)
                            addBlockFace(c.mesh,
                                         p101,
                                         p001,
                                         p011,
                                         p111,
                                         b,
                                         1,
                                         ox + x,
                                         y,
                                         oz + z,
                                         blockLight);
                        if (!localSolid(x, y + 1, z) && c.get(x, y + 1, z) != 9)
                            addBlockFace(c.mesh,
                                         p010,
                                         p110,
                                         p111,
                                         p011,
                                         b,
                                         2,
                                         ox + x,
                                         y,
                                         oz + z,
                                         blockLight);
                        if (!localSolid(x, y - 1, z) && c.get(x, y - 1, z) != 9)
                            addBlockFace(c.mesh,
                                         p001,
                                         p101,
                                         p100,
                                         p000,
                                         b,
                                         3,
                                         ox + x,
                                         y,
                                         oz + z,
                                         blockLight);
                        if (!localSolid(x - 1, y, z) && c.get(x - 1, y, z) != 9)
                            addBlockFace(c.mesh,
                                         p001,
                                         p000,
                                         p010,
                                         p011,
                                         b,
                                         4,
                                         ox + x,
                                         y,
                                         oz + z,
                                         blockLight);
                        if (!localSolid(x + 1, y, z) && c.get(x + 1, y, z) != 9)
                            addBlockFace(c.mesh,
                                         p100,
                                         p101,
                                         p111,
                                         p110,
                                         b,
                                         5,
                                         ox + x,
                                         y,
                                         oz + z,
                                         blockLight);
                    }
                }
        c.meshVertexCount = (GLsizei)(c.mesh.size() / 8);
        if (!c.mesh.empty()) {
            if (!c.meshVbo)
                glGenBuffers(1, &c.meshVbo);
            glBindBuffer(GL_ARRAY_BUFFER, c.meshVbo);
            glBufferData(
                GL_ARRAY_BUFFER, c.mesh.size() * sizeof(float), c.mesh.data(), GL_STATIC_DRAW);
        } else if (c.meshVbo) {
            glDeleteBuffers(1, &c.meshVbo);
            c.meshVbo = 0;
        }
        c.mesh.clear();
        c.mesh.shrink_to_fit();
        c.dirty = false;
    }
    // Submits a chunk's cached mesh to the graphics pipeline.
    void drawChunk(const Chunk& c, const Mat4& m, float cx, float cy, float cz) {
        if (!c.meshVbo || c.meshVertexCount <= 0)
            return;
        glUseProgram(program);
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, m.m);
        glUniform3f(cameraLoc, cx, cy, cz);
        glUniform1f(fogLoc, 1.0f);
        glUniform1f(textureLoc, atlasTexture ? 1.0f : 0.0f);
        glUniform3f(skyColorLoc, skyTint.x, skyTint.y, skyTint.z);
        glUniform1f(animTimeLoc, animationTime);
        glUniform1f(waterAnimationLoc, 1.0f);
        glUniform1f(fogStartLoc, (float)drawDistance * CS * 1.05f);
        glUniform1f(fogEndLoc, (float)drawDistance * CS * 1.65f);
        glUniform1f(worldLightLoc, worldBrightness);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlasTexture);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, c.meshVbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glDrawArrays(GL_TRIANGLES, 0, c.meshVertexCount);
    }
    // Uploads and draws a vertex buffer with the requested render state.
    void draw(const std::vector<float>& v,
              GLenum mode,
              const Mat4& m,
              float cx = 0,
              float cy = 0,
              float cz = 0,
              bool fog = true,
              bool textured = false,
              GLuint textureOverride = 0,
              bool animateWater = true) {
        if (v.empty())
            return;
        glUseProgram(program);
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, m.m);
        glUniform3f(cameraLoc, cx, cy, cz);
        glUniform1f(fogLoc, fog ? 1.0f : 0.0f);
        glUniform1f(textureLoc, textured && (textureOverride || atlasTexture) ? 1.0f : 0.0f);
        glUniform3f(skyColorLoc, skyTint.x, skyTint.y, skyTint.z);
        glUniform1f(animTimeLoc, animationTime);
        glUniform1f(waterAnimationLoc, textured && !textureOverride && animateWater ? 1.0f : 0.0f);
        glUniform1f(fogStartLoc, (float)drawDistance * CS * 1.05f);
        glUniform1f(fogEndLoc, (float)drawDistance * CS * 1.65f);
        glUniform1f(worldLightLoc, worldBrightness);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureOverride ? textureOverride : atlasTexture);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glDrawArrays(mode, 0, (GLsizei)(v.size() / 8));
    }
    // Renders a textured inventory icon for a block or item ID.
    void drawBlockIcon(uint8_t id, float x, float y, float w, float h) {
        std::vector<float> icon;
        V3 white{1, 1, 1};
        if (id == 15 || id == 16) {
            float cx = x + w * .5f;
            addTri(icon,
                   {cx - w * .06f, y + h * .05f, 0},
                   {cx + w * .06f, y + h * .05f, 0},
                   {cx + w * .045f, y + h * .67f, 0},
                   {.48f, .28f, .12f});
            addTri(icon,
                   {cx - w * .06f, y + h * .05f, 0},
                   {cx + w * .045f, y + h * .67f, 0},
                   {cx - w * .045f, y + h * .67f, 0},
                   {.60f, .36f, .16f});
            if (id == 15) {
                addTri(icon,
                       {cx - w * .02f, y + h * .68f, 0},
                       {x + w * .14f, y + h * .96f, 0},
                       {x + w * .48f, y + h * .78f, 0},
                       {.57f, .34f, .14f});
                addTri(icon,
                       {cx - w * .02f, y + h * .68f, 0},
                       {x + w * .48f, y + h * .78f, 0},
                       {x + w * .84f, y + h * .96f, 0},
                       {.57f, .34f, .14f});
            } else {
                addTri(icon,
                       {x + w * .12f, y + h * .68f, 0},
                       {x + w * .88f, y + h * .68f, 0},
                       {x + w * .78f, y + h * .84f, 0},
                       {.62f, .39f, .17f});
                addTri(icon,
                       {x + w * .12f, y + h * .68f, 0},
                       {x + w * .78f, y + h * .84f, 0},
                       {x + w * .22f, y + h * .84f, 0},
                       {.72f, .48f, .22f});
            }
            draw(icon, GL_TRIANGLES, identity(), 0, 0, 0, false, false, 0, false);
            return;
        }
        if (id == 20) {
            float l = x + w * .18f, r = x + w * .82f, b = y + h * .04f, t = y + h * .96f;
            addFace(icon, {l, b, 0}, {r, b, 0}, {r, t, 0}, {l, t, 0}, {.58f, .34f, .15f});
            addFace(icon,
                    {l + .035f, b + .025f, 0},
                    {r - .035f, b + .025f, 0},
                    {r - .035f, t - .025f, 0},
                    {l + .035f, t - .025f, 0},
                    {.76f, .49f, .25f});
            for (int row = 0; row < 2; row++)
                for (int col = 0; col < 2; col++) {
                    float hx = l + .16f + col * w * .17f, hy = b + .48f * h + row * h * .17f;
                    addFace(icon,
                            {hx, hy, 0},
                            {hx + w * .10f, hy, 0},
                            {hx + w * .10f, hy + h * .09f, 0},
                            {hx, hy + h * .09f, 0},
                            {.22f, .12f, .065f});
                }
            draw(icon, GL_TRIANGLES, identity(), 0, 0, 0, false, false, 0, false);
            return;
        }
        if (id == 18) {
            addTri(icon,
                   {x + w * .08f, y + h * .22f, 0},
                   {x + w * .92f, y + h * .22f, 0},
                   {x + w * .92f, y + h * .62f, 0},
                   {.55f, .16f, .18f});
            addTri(icon,
                   {x + w * .08f, y + h * .22f, 0},
                   {x + w * .92f, y + h * .62f, 0},
                   {x + w * .08f, y + h * .62f, 0},
                   {.88f, .22f, .25f});
            addTri(icon,
                   {x + w * .12f, y + h * .62f, 0},
                   {x + w * .88f, y + h * .62f, 0},
                   {x + w * .78f, y + h * .90f, 0},
                   {.86f, .88f, .92f});
            addTri(icon,
                   {x + w * .12f, y + h * .62f, 0},
                   {x + w * .78f, y + h * .90f, 0},
                   {x + w * .22f, y + h * .90f, 0},
                   {1, 1, 1});
            draw(icon, GL_TRIANGLES, identity(), 0, 0, 0, false, false, 0, false);
            return;
        }
        if (id == 12) {
            float cx = x + w * .5f, hy = y + h * .63f;
            addTri(icon,
                   {cx - w * .055f, y + h * .04f, 0},
                   {cx + w * .055f, y + h * .04f, 0},
                   {cx + w * .04f, hy, 0},
                   {.42f, .23f, .09f});
            addTri(icon,
                   {cx - w * .055f, y + h * .04f, 0},
                   {cx + w * .04f, hy, 0},
                   {cx - w * .04f, hy, 0},
                   {.54f, .32f, .13f});
            addTri(icon,
                   {x + w * .13f, hy, 0},
                   {x + w * .87f, hy, 0},
                   {x + w * .77f, y + h * .80f, 0},
                   {.72f, .77f, .84f});
            addTri(icon,
                   {x + w * .13f, hy, 0},
                   {x + w * .77f, y + h * .80f, 0},
                   {x + w * .23f, y + h * .80f, 0},
                   {.88f, .91f, .96f});
            addTri(icon,
                   {x + w * .23f, y + h * .80f, 0},
                   {x + w * .77f, y + h * .80f, 0},
                   {cx, y + h * .98f, 0},
                   {.58f, .64f, .73f});
            draw(icon, GL_TRIANGLES, identity(), 0, 0, 0, false, false, 0, false);
            return;
        }
        if (id == 11) {
            float cx = x + w * .5f;
            addTri(icon,
                   {cx - w * .11f, y + h * .22f, 0},
                   {cx + w * .11f, y + h * .22f, 0},
                   {cx, y + h * .98f, 0},
                   {.88f, .92f, .98f});
            addTri(icon,
                   {cx - w * .11f, y + h * .22f, 0},
                   {cx, y + h * .98f, 0},
                   {cx - w * .02f, y + h * .22f, 0},
                   {.98f, .99f, 1});
            addTri(icon,
                   {x + w * .18f, y + h * .18f, 0},
                   {x + w * .82f, y + h * .18f, 0},
                   {x + w * .76f, y + h * .27f, 0},
                   {.72f, .76f, .82f});
            addTri(icon,
                   {x + w * .18f, y + h * .18f, 0},
                   {x + w * .76f, y + h * .27f, 0},
                   {x + w * .24f, y + h * .27f, 0},
                   {.72f, .76f, .82f});
            addTri(icon,
                   {cx - w * .07f, y + h * .18f, 0},
                   {cx + w * .07f, y + h * .18f, 0},
                   {cx + w * .06f, y + h * .02f, 0},
                   {.38f, .21f, .09f});
            addTri(icon,
                   {cx - w * .07f, y + h * .18f, 0},
                   {cx + w * .06f, y + h * .02f, 0},
                   {cx - w * .06f, y + h * .02f, 0},
                   {.48f, .28f, .12f});
            draw(icon, GL_TRIANGLES, identity(), 0, 0, 0, false, false, 0, false);
            return;
        }
        int tile =
            id == 10
                ? 5
                : (id == 9
                       ? 4
                       : (id == 3
                              ? 2
                              : (id == 5
                                     ? 3
                                     : (id == 4
                                            ? 7
                                            : (id == 13
                                                   ? 8
                                                   : (id == 14
                                                          ? 9
                                                          : (id == 8
                                                                 ? 10
                                                                 : (id == 7
                                                                        ? 11
                                                                        : (id == 17
                                                                               ? 8
                                                                               : ((id == 1)
                                                                                      ? 0
                                                                                      : 1))))))))));
        float u0 = (tile + .035f) / 12.0f, u1 = (tile + .965f) / 12.0f, v0 = .035f, v1 = .965f;
        addTri(icon, {x, y, 0}, {x + w, y, 0}, {x + w, y + h, 0}, white, u0, v0, u1, v0, u1, v1);
        addTri(icon, {x, y, 0}, {x + w, y + h, 0}, {x, y + h, 0}, white, u0, v0, u1, v1, u0, v1);
        draw(icon, GL_TRIANGLES, identity(), 0, 0, 0, false, true, 0, false);
    }
    // Renders the in-game heads-up display and touch controls.
    void drawHud() {
        glDisable(GL_DEPTH_TEST);
        Mat4 m = identity();
        std::vector<float> v;
        auto t = [&](float x, float y, float X, float Y, float Z, float W, V3 c) {
            addTri(v, {x, y, 0}, {X, Y, 0}, {Z, W, 0}, c);
        };
        auto rect = [&](float x, float y, float w, float h, V3 c) {
            t(x, y, x + w, y, x + w, y + h, c);
            t(x, y, x + w, y + h, x, y + h, c);
        };
        auto circ = [&](float x, float y, float r, V3 c) {
            for (int i = 0; i < 32; i++) {
                float a = i * 2 * PI / 32, b = (i + 1) * 2 * PI / 32;
                t(x, y, x + cosf(a) * r, y + sinf(a) * r, x + cosf(b) * r, y + sinf(b) * r, c);
            }
        };
        auto line = [&](float x, float y, float X, float Y, float q, V3 c) {
            float dx = X - x, dy = Y - y, l = sqrtf(dx * dx + dy * dy);
            if (l < .001f)
                return;
            float ox = -dy / l * q, oy = dx / l * q;
            t(x + ox, y + oy, X + ox, Y + oy, X - ox, Y - oy, c);
            t(x + ox, y + oy, X - ox, Y - oy, x - ox, y - oy, c);
        };
        rect(
            -.94f, -.985f, .20f, .075f, lanMode == 2 ? V3{.12f, .58f, .22f} : V3{.05f, .28f, .11f});
        rect(
            -.70f, -.985f, .20f, .075f, lanMode == 1 ? V3{.12f, .40f, .78f} : V3{.06f, .20f, .42f});
        // Renders a string using the game's bitmap font.
        drawText("JOIN", -.91f, -.925f, .012f, {1, 1, 1});
        drawText("HOST", -.67f, -.925f, .012f, {1, 1, 1});
        drawText("FPS:" + std::to_string((int)fpsValue), .63f, .68f, .018f, {1, 1, 1});
        rect(.84f, .57f, .13f, .10f, {.04f, .05f, .07f});
        drawText("SET", .875f, .60f, .012f, {1, 1, 1});
        rect(.82f, .30f, .16f, .12f, {.10f, .16f, .22f});
        drawText(spectatorMode ? "SPEC" : (creativeMode ? "CREA" : "SURV"),
                 .835f,
                 .335f,
                 .011f,
                 {1, 1, 1});
        circ(joyOriginX, joyOriginY, .255f, {.01f, .01f, .015f});
        circ(joyOriginX, joyOriginY, .22f, {.14f, .14f, .16f});
        circ(joyOriginX + joyX * .16f, joyOriginY + joyY * .16f, .105f, {.01f, .01f, .015f});
        circ(.78f, -.62f, .14f, {.01f, .01f, .015f});
        circ(.78f, -.62f, .115f, {.17f, .17f, .19f});
        circ(.78f, -.10f, .14f, {.01f, .01f, .015f});
        circ(.78f, -.10f, .115f, {.17f, .17f, .19f});
        circ(.53f, -.40f, .14f, {.01f, .01f, .015f});
        circ(.53f, -.40f, .115f, {.17f, .17f, .19f});
        circ(.53f, -.10f, .14f, {.01f, .01f, .015f});
        circ(.53f, -.10f, .115f, {.17f, .17f, .19f});
        line(.78f, -.65f, .78f, -.56f, .018f, {1, 1, 1});
        line(.78f, -.56f, .74f, -.60f, .018f, {1, 1, 1});
        line(.78f, -.56f, .82f, -.60f, .018f, {1, 1, 1});
        line(.72f, -.12f, .84f, -.02f, .018f, {1, 1, 1});
        line(.73f, -.02f, .85f, -.02f, .018f, {1, 1, 1});
        line(.48f, -.45f, .58f, -.45f, .014f, {1, 1, 1});
        line(.48f, -.45f, .48f, -.35f, .014f, {1, 1, 1});
        line(.58f, -.45f, .58f, -.35f, .014f, {1, 1, 1});
        line(.48f, -.35f, .58f, -.35f, .014f, {1, 1, 1});
        line(.49f, -.14f, .57f, -.06f, .018f, {1, .35f, .25f});
        line(.50f, -.07f, .56f, -.13f, .018f, {1, .35f, .25f});
        line(-.034f, 0, -.009f, 0, .006f, {.015f, .015f, .02f});
        line(.009f, 0, .034f, 0, .006f, {.015f, .015f, .02f});
        line(0, -.034f, 0, -.009f, .006f, {.015f, .015f, .02f});
        line(0, .009f, 0, .034f, .006f, {.015f, .015f, .02f});
        line(-.032f, 0, -.009f, 0, .0025f, {1, 1, 1});
        line(.009f, 0, .032f, 0, .0025f, {1, 1, 1});
        line(0, -.032f, 0, -.009f, .0025f, {1, 1, 1});
        line(0, .009f, 0, .032f, .0025f, {1, 1, 1});
        if (breaking) {
            int tx, ty, tz, ax, ay, az;
            if (raycast(tx, ty, tz, ax, ay, az)) {
                auto it = chunks.find(chunkKey(floorDiv(tx, CS), floorDiv(tz, CS)));
                uint8_t block =
                    it == chunks.end() ? 0 : it->second.get(modp(tx, CS), ty, modp(tz, CS));
                if (block) {
                    float progress = std::clamp(breakTimer / breakDuration(block), 0.0f, 1.0f);
                    rect(-.125f, .145f, .25f, .022f, {.015f, .015f, .02f});
                    rect(-.122f, .148f, .244f * progress, .016f, {1.0f, .72f, .16f});
                    drawText(std::to_string((int)(progress * 100)) + "%",
                             -.035f,
                             .195f,
                             .014f,
                             {1, 1, 1});
                }
            }
        }
        const int hpPercent = std::clamp(health, 0, 10) * 10,
                  armorPercent = std::clamp(armor, 0, 10) * 10;
        rect(-.94f, .855f, .43f, .025f, {.10f, .035f, .045f});
        if (hpPercent > 0)
            rect(-.94f, .855f, .43f * (hpPercent / 100.0f), .025f, {.88f, .10f, .14f});
        rect(-.94f, .735f, .43f, .022f, {.045f, .075f, .10f});
        if (armorPercent > 0)
            rect(-.94f, .735f, .43f * (armorPercent / 100.0f), .022f, {.25f, .62f, .92f});
        rect(.84f, .79f, .13f, .15f, {.01f, .01f, .015f});
        rect(.86f, .81f, .09f, .11f, {.2f, .2f, .22f});
        line(.87f, .87f, .94f, .87f, .009f, {1, 1, 1});
        line(.87f, .83f, .94f, .83f, .009f, {1, 1, 1});
        for (int i = 0; i < 5; i++) {
            float x = -.28f + i * .115f;
            rect(x, -.92f, .105f, .16f, i == selected ? V3{1, .78f, .18f} : V3{.01f, .01f, .015f});
            rect(x + .008f, -.912f, .089f, .144f, {.2f, .2f, .22f});
            if (inventory[i].count) {
                rect(x + .025f, -.88f, .055f, .075f, blockColor(inventory[i].id, 2));
                drawText(inventory[i].count >= 999999 ? "INF" : std::to_string(inventory[i].count),
                         x + .060f,
                         -.89f,
                         .010f,
                         {1, 1, 1});
            }
        }
        draw(v, GL_TRIANGLES, m, 0, 0, 0, false);
        for (int i = 0; i < 5; i++)
            if (inventory[i].count)
                drawBlockIcon(inventory[i].id, -.28f + i * .115f + .025f, -.88f, .055f, .075f);
        if (statusFrames > 0) {
            drawText(status, -.85f, .66f, .018f, {1, 1, 1});
            statusFrames--;
        }
        if (spectatorMode)
            drawText("SPECTATOR", -.90f, .68f, .015f, {.75f, .88f, 1});
        if (playerDead) {
            float k = std::clamp(swallowTimer / 2.8f, 0.0f, 1.0f);
            std::vector<float> overlay;
            addFace(overlay,
                    {-1, -1, 0},
                    {1, -1, 0},
                    {1, 1, 0},
                    {-1, 1, 0},
                    {.015f + .035f * k, .005f, .018f + .025f * k});
            draw(overlay, GL_TRIANGLES, identity(), 0, 0, 0, false);
            std::vector<float> mouth;
            for (int i = 0; i < 48; i++) {
                float a = i * 2 * PI / 48, b = (i + 1) * 2 * PI / 48;
                addTri(mouth,
                       {0, 0, 0},
                       {std::cos(a) * (.10f + .70f * k), std::sin(a) * (.10f + .70f * k), 0},
                       {std::cos(b) * (.10f + .70f * k), std::sin(b) * (.10f + .70f * k), 0},
                       {.005f, .002f, .008f});
            }
            draw(mouth, GL_TRIANGLES, identity(), 0, 0, 0, false);
            drawText(deathMessage.empty() ? std::string("YOU DIED") : deathMessage,
                     -.52f,
                     -.72f,
                     .035f,
                     {1, .25f, .30f});
        }
        glEnable(GL_DEPTH_TEST);
    }
    // Renders inventory slots, item icons, and crafting controls.
    void drawInventory() {
        glDisable(GL_DEPTH_TEST);
        std::vector<float> v;
        auto r = [&](float x, float y, float w, float h, V3 c) {
            addFace(v, {x, y, 0}, {x + w, y, 0}, {x + w, y + h, 0}, {x, y + h, 0}, c);
        };
        r(-.78f, -.73f, 1.56f, 1.43f, {.03f, .03f, .04f});
        r(-.75f, -.70f, 1.50f, 1.37f, {.13f, .14f, .16f});
        for (int i = 0; i < 20; i++) {
            int row = i / 5, col = i % 5;
            float x = -.58f + col * .235f, y = .30f - row * .245f;
            r(x,
              y - .19f,
              .19f,
              .21f,
              i == selected ? V3{.72f, .52f, .14f} : V3{.025f, .025f, .03f});
            r(x + .008f, y - .182f, .174f, .194f, {.24f, .24f, .26f});
            if (inventory[i].count && i != dragIndex) {
                r(x + .055f, y - .135f, .075f, .09f, blockColor(inventory[i].id, 2));
                drawText(inventory[i].count >= 999999 ? "INF" : std::to_string(inventory[i].count),
                         x + .125f,
                         y - .17f,
                         .008f,
                         {1, 1, 1});
            }
        }
        r(.60f, .27f, .15f, .15f, {.02f, .02f, .025f});
        r(.60f, -.02f, .15f, .15f, {.02f, .02f, .025f});
        r(.60f, -.31f, .15f, .15f, {.02f, .02f, .025f});
        r(.60f, -.60f, .15f, .12f, {.02f, .02f, .025f});
        draw(v, GL_TRIANGLES, identity(), 0, 0, 0, false);
        for (int i = 0; i < 20; i++)
            if (inventory[i].count && i != dragIndex) {
                int row = i / 5, col = i % 5;
                float x = -.58f + col * .235f, y = .30f - row * .245f;
                drawBlockIcon(inventory[i].id, x + .055f, y - .135f, .075f, .09f);
            }
        if (dragIndex >= 0 && inventory[dragIndex].count)
            drawBlockIcon(inventory[dragIndex].id, dragX - .035f, dragY - .035f, .075f, .09f);
        drawText(craftingTableOpen ? "CRAFTING TABLE" : "INVENTORY",
                 -.25f,
                 .62f,
                 .025f,
                 {1, .85f, .35f});
        drawText("INVENTORY", -.22f, .47f, .012f, {.8f, .85f, .9f});
        drawText("RECIPES", .62f, .38f, .011f, {.8f, .85f, .9f});
        drawText("DRAG TO MOVE - OUTSIDE TO DROP", -.52f, -.66f, .012f, {1, 1, 1});
        drawText(craftingTableOpen ? "WOOD AXE" : "PLANKS", .57f, .48f, .015f, {1, 1, 1});
        drawText(craftingTableOpen ? "WOOD PICK" : "STICKS", .58f, .19f, .015f, {1, 1, 1});
        drawText(craftingTableOpen ? "IRON BLOCK" : "TABLE", .59f, -.10f, .015f, {1, 1, 1});
        drawText("BED", .63f, -.55f, .015f, {1, 1, 1});
        drawText("X", .66f, .62f, .025f, {1, 1, 1});
        glEnable(GL_DEPTH_TEST);
    }
    // Maps a supported character to its bitmap-font glyph data.
    std::array<uint8_t, 7> glyph(char c) {
        switch (c) {
            case 'A':
                return {14, 17, 17, 31, 17, 17, 17};
            case 'B':
                return {30, 17, 17, 30, 17, 17, 30};
            case 'C':
                return {14, 17, 16, 16, 16, 17, 14};
            case 'D':
                return {30, 17, 17, 17, 17, 17, 30};
            case 'E':
                return {31, 16, 16, 30, 16, 16, 31};
            case 'F':
                return {31, 16, 16, 30, 16, 16, 16};
            case 'G':
                return {14, 17, 16, 23, 17, 17, 14};
            case 'H':
                return {17, 17, 17, 31, 17, 17, 17};
            case 'I':
                return {14, 4, 4, 4, 4, 4, 14};
            case 'K':
                return {17, 18, 20, 24, 20, 18, 17};
            case 'L':
                return {16, 16, 16, 16, 16, 16, 31};
            case 'M':
                return {17, 27, 21, 21, 17, 17, 17};
            case 'N':
                return {17, 25, 25, 21, 19, 19, 17};
            case 'O':
                return {14, 17, 17, 17, 17, 17, 14};
            case 'P':
                return {30, 17, 17, 30, 16, 16, 16};
            case 'R':
                return {30, 17, 17, 30, 20, 18, 17};
            case 'S':
                return {15, 16, 16, 14, 1, 1, 30};
            case 'T':
                return {31, 4, 4, 4, 4, 4, 4};
            case 'U':
                return {17, 17, 17, 17, 17, 17, 14};
            case 'V':
                return {17, 17, 17, 17, 17, 10, 4};
            case 'W':
                return {17, 17, 17, 21, 21, 21, 10};
            case 'Y':
                return {17, 17, 10, 4, 4, 4, 4};
            case '0':
                return {14, 17, 19, 21, 25, 17, 14};
            case '1':
                return {4, 12, 4, 4, 4, 4, 14};
            case '2':
                return {14, 17, 1, 2, 4, 8, 31};
            case '3':
                return {30, 1, 1, 14, 1, 1, 30};
            case '4':
                return {2, 6, 10, 18, 31, 2, 2};
            case '5':
                return {31, 16, 16, 30, 1, 1, 30};
            case '6':
                return {14, 16, 16, 30, 17, 17, 14};
            case '7':
                return {31, 1, 2, 4, 8, 8, 8};
            case '8':
                return {14, 17, 17, 14, 17, 17, 14};
            case '9':
                return {14, 17, 17, 15, 1, 1, 14};
            case ':':
                return {0, 4, 4, 0, 4, 4, 0};
            case '-':
                return {0, 0, 0, 31, 0, 0, 0};
            case '%':
                return {17, 2, 4, 8, 17, 0, 0};
            default:
                return {0, 0, 0, 0, 0, 0, 0};
        }
    }
    void drawText(const std::string& str, float x, float y, float scale, V3 color) {
        if (str.empty())
            return;
        const float maxWidth = 1.72f;
        float effective = std::min(std::min(scale, .016f), maxWidth / (6.0f * str.size()));
        float textWidth = effective * 6.0f * str.size();
        x = std::clamp(x, -.97f, .97f - textWidth);
        y = std::clamp(y, -.82f + 6.0f * effective, .96f);
        scale = effective;
        std::vector<float> v;
        float cursor = x;
        for (char c : str) {
            if (c >= 'a' && c <= 'z')
                c = (char)(c - 'a' + 'A');
            auto g = glyph(c);
            for (int row = 0; row < 7; row++)
                for (int col = 0; col < 5; col++)
                    if (g[row] & (1 << (4 - col))) {
                        float px0 = cursor + col * scale, py0 = y - row * scale;
                        addFace(v,
                                {px0, py0, 0},
                                {px0 + scale * .72f, py0, 0},
                                {px0 + scale * .72f, py0 + scale * .72f, 0},
                                {px0, py0 + scale * .72f, 0},
                                color);
                    }
            cursor += scale * 6;
        }
        Mat4 ortho = identity();
        GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
        glDisable(GL_DEPTH_TEST);
        draw(v, GL_TRIANGLES, ortho, 0, 0, 0, false);
        if (depthWasEnabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
    }
