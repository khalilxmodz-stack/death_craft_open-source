// Android entry point and core game implementation.
// Shared constants and math helpers are used by the included game subsystems.
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <fstream>
#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Core world dimensions and shared mathematical constants.
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "DeathCraft", __VA_ARGS__)
static constexpr int CS = 16, HEIGHT = 64;
static constexpr float PI = 3.14159265f;
static constexpr int SEA_LEVEL = 46;

// Three-dimensional vector used for positions, directions, and colors.
struct V3 {
    float x, y, z;
};
// Column-major 4x4 matrix used by the OpenGL transformation pipeline.
struct Mat4 {
    float m[16]{};
};
// Returns an identity matrix with a unit diagonal.
static Mat4 identity() {
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
    return r;
}
// Multiplies two 4x4 transformation matrices.
static Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; c++)
        for (int y = 0; y < 4; y++)
            for (int k = 0; k < 4; k++)
                r.m[c * 4 + y] += a.m[k * 4 + y] * b.m[c * 4 + k];
    return r;
}
// Builds a perspective projection matrix for the camera.
static Mat4 perspective(float f, float a, float n, float z) {
    Mat4 r{};
    float t = 1 / std::tan(f * .5f);
    r.m[0] = t / a;
    r.m[5] = t;
    r.m[10] = (z + n) / (n - z);
    r.m[11] = -1;
    r.m[14] = 2 * z * n / (n - z);
    return r;
}
// Builds the view matrix from camera position and viewing angles.
static Mat4 camera(float x, float y, float z, float yaw, float pitch) {
    V3 f{std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
    V3 right{std::cos(yaw), 0, std::sin(yaw)};
    V3 up{-std::sin(yaw) * std::sin(pitch), std::cos(pitch), std::cos(yaw) * std::sin(pitch)};
    Mat4 r = identity();
    r.m[0] = right.x;
    r.m[4] = right.y;
    r.m[8] = right.z;
    r.m[1] = up.x;
    r.m[5] = up.y;
    r.m[9] = up.z;
    r.m[2] = -f.x;
    r.m[6] = -f.y;
    r.m[10] = -f.z;
    r.m[12] = -(right.x * x + right.y * y + right.z * z);
    r.m[13] = -(up.x * x + up.y * y + up.z * z);
    r.m[14] = f.x * x + f.y * y + f.z * z;
    return r;
}
// Performs mathematical floor division for negative world coordinates.
static int floorDiv(int a, int b) {
    int q = a / b, r = a % b;
    return r < 0 ? q - 1 : q;
}
// Returns a non-negative remainder for chunk-local coordinates.
static int modp(int a, int b) {
    int r = a % b;
    return r < 0 ? r + b : r;
}
// Produces a deterministic pseudo-random hash for terrain coordinates.
static uint32_t terrainHash(int x, int z) {
    uint32_t h = (uint32_t)x * 0x8da6b343u ^ (uint32_t)z * 0xd8163841u;
    h ^= h >> 13;
    h *= 0x85ebca6bu;
    h ^= h >> 16;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}
// Interpolates hashed samples to create smooth deterministic terrain noise.
static float terrainNoise(float x, float z) {
    int ix = (int)std::floor(x), iz = (int)std::floor(z);
    float fx = x - ix, fz = z - iz;
    fx = fx * fx * (3 - 2 * fx);
    fz = fz * fz * (3 - 2 * fz);
    auto sample = [](int a, int b) { return (terrainHash(a, b) & 0x00ffffffu) / 16777215.0f; };
    float a = sample(ix, iz), b = sample(ix + 1, iz), c = sample(ix, iz + 1),
          d = sample(ix + 1, iz + 1);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fz;
}
// Combines noise layers to calculate the height of the terrain surface.
static int terrainHeight(int x, int z) {
    float broad = terrainNoise(x * .014f + 17, z * .014f - 31);
    float hills = terrainNoise(x * .052f - 43, z * .052f + 11);
    float detail = terrainNoise(x * .145f + 71, z * .145f - 59);
    float mountain = terrainNoise(x * .010f + 107, z * .010f - 83);
    float peaks = terrainNoise(x * .031f - 37, z * .031f + 91);
    float ridgeNoise = terrainNoise(x * .061f + 129, z * .061f - 147);
    float valley = terrainNoise(x * .022f - 211, z * .022f + 173);
    float small = (broad - .5f) * 5.2f + (hills - .5f) * 5.8f + (detail - .5f) * 1.45f;
    float mask = std::clamp((mountain - .43f) * 2.25f, 0.0f, 1.0f);
    float ridge = 1.0f - std::abs(peaks * 2.0f - 1.0f);
    float high = mask * (std::pow(std::max(0.0f, (peaks - .40f) / .60f), 1.35f) * 18.0f +
                         std::pow(std::max(0.0f, (ridgeNoise - .68f) / .32f), 1.7f) * 8.0f +
                         std::pow(std::max(0.0f, (ridge - .72f) / .28f), 2.0f) * 5.0f);
    float valleyCut = std::pow(std::max(0.0f, (valley - .68f) / .32f), 1.4f) * 5.0f;
    return std::clamp((int)std::lround(45.0f + small + high - valleyCut), 38, HEIGHT - 7);
}
// Packs two signed chunk coordinates into one stable map key.
static uint64_t chunkKey(int x, int z) {
    return (uint64_t)(uint32_t)x << 32 | (uint32_t)z;
}

// Stores block data and cached GPU/CPU meshes for one terrain chunk.
struct Chunk {
    int cx = 0, cz = 0;
    std::array<uint8_t, CS * CS * HEIGHT> blocks{};
    std::vector<float> mesh;
    std::vector<float> shadowMesh;
    GLuint meshVbo = 0;
    GLsizei meshVertexCount = 0;
    bool dirty = true;
    uint8_t get(int x, int y, int z) const {
        return (x < 0 || x >= CS || z < 0 || z >= CS || y < 0 || y >= HEIGHT)
                   ? 0
                   : blocks[(y * CS + z) * CS + x];
    }
    void set(int x, int y, int z, uint8_t b) {
        if (x >= 0 && x < CS && z >= 0 && z < CS && y >= 0 && y < HEIGHT)
            blocks[(y * CS + z) * CS + x] = b;
    }
};

// Fixed-layout UDP packet shared by LAN multiplayer peers.
struct LanPacket {
    uint32_t magic = 0x44434654u;
    uint8_t type = 0;
    uint32_t player = 0;
    float x = 0, y = 0, z = 0, yaw = 0;
    int32_t bx = 0, by = 0, bz = 0;
    uint8_t block = 0;
    uint8_t flags = 0;
    char name[24]{};
};

// Owns game state, Android graphics/audio resources, and gameplay systems.
class Game {
    // EGL handles used to create and manage the Android OpenGL ES surface.
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    // OpenGL object IDs for shaders, vertex arrays, buffers, and loaded textures.
    GLuint program = 0, vao = 0, vbo = 0, atlasTexture = 0, khalilTexture = 0;
    // OpenSL ES engine and output-mix handles used by game audio.
    SLObjectItf slEngineObject = nullptr, slOutputMix = nullptr;
    SLEngineItf slEngine = nullptr;
    // Audio player interfaces, source assets, and file descriptors for sound effects.
    SLObjectItf slPlayers[3]{};
    SLPlayItf slPlay[3]{};
    SLSeekItf slSeek[3]{};
    AAsset* soundAssets[3]{};
    int soundFds[3]{-1, -1, -1};
    // Footstep timing and cached shader-uniform locations.
    float footstepTimer = 0.0f;
    bool footstepActive = false;
    GLint mvpLoc = -1, cameraLoc = -1, fogLoc = -1, textureLoc = -1, atlasLoc = -1,
          skyColorLoc = -1, animTimeLoc = -1, waterAnimationLoc = -1, fogStartLoc = -1,
          fogEndLoc = -1, worldLightLoc = -1;
    // Current sky tint and world-time animation clocks.
    V3 skyTint{.48f, .74f, .96f};
    float dayClock = 0.0f, animationTime = 0.0f;
    // Tracks whether night-specific world behavior is active.
    bool nightActive = false;
    // Temporary entity records for dropped items, particles, TNT, and arrows.
    struct DroppedItem {
        uint8_t id = 0;
        int count = 1;
        float x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0, age = 0;
    };
    struct BlockParticle {
        float x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0, life = 0;
        uint8_t id = 0;
    };
    struct PrimedTnt {
        float x = 0, y = 0, z = 0, fuse = 2.0f;
    };
    struct ArrowProjectile {
        float x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0, life = 0;
    };
    // Active skeleton projectiles and the skeleton's movement/combat state.
    std::vector<ArrowProjectile> skeletonArrows;
    float skeletonX = 8.0f, skeletonFeet = 1.0f, skeletonZ = 3.0f, skeletonYaw = 0,
          skeletonShootTimer = 1.4f, skeletonRespawn = 0, skeletonHitTimer = 0;
    int skeletonHealth = 8;
    bool skeletonAlive = true;
    // Active dropped items, block particles, primed TNT, and simulated water depths.
    std::vector<DroppedItem> droppedItems;
    std::vector<BlockParticle> blockParticles;
    std::vector<PrimedTnt> primedTnts;
    std::map<std::tuple<int, int, int>, uint8_t> waterFlowDepth;
    float waterSimulationTimer = 0.0f;
    // Khalil monster transform, fuse, respawn, and damage feedback state.
    float khalilX = 9.0f, khalilFeet = 1.0f, khalilZ = 0.0f, khalilFuse = 0.0f,
          khalilRespawn = 0.0f, khalilHitTimer = 0.0f;
    // Khalil's remaining health and alive/primed state flags.
    int khalilHealth = 8;
    bool khalilAlive = true, khalilPrimed = false;
    // Explosion animation timer and world-space explosion center.
    float explosionEffectTimer = 0.0f, explosionX = 0, explosionY = 0, explosionZ = 0;
    std::string deathMessage = "GRAFIK ATE YOU";
    // Current render-surface dimensions in pixels.
    int width = 1, height = 1;
    // Loaded chunks, queued mesh rebuilds, and player-edited block overrides.
    std::map<uint64_t, Chunk> chunks;
    std::deque<uint64_t> dirtyChunkQueue;
    std::map<std::tuple<int, int, int>, uint8_t> edits;
    // Player world position, camera angles, and vertical movement velocity.
    float px = 0, feet = 1.0f, pz = 0, yaw = 0, pitch = 0, vy = 0;
    // Fall tracking, grounded state, and current movement input.
    float fallStartFeet = 0, fallPeakFeet = 0;
    bool fallTracking = false;
    bool grounded = false, running = false;
    float joyX = 0, joyY = 0, lastX = 0, lastY = 0, joyOriginX = -.76f, joyOriginY = -.65f;
    // Active touch-pointer IDs for movement and camera look gestures.
    int joyId = -1, lookId = -1;
    // UI visibility and current block-breaking interaction state.
    bool inventoryOpen = false, craftingTableOpen = false, breaking = false;
    // Selected hotbar slot and touch/drag state for block and inventory actions.
    int selected = 0, breakPointer = -1, breakX = 0, breakY = 0, breakZ = 0, dragIndex = -1,
        dragPointer = -1;
    float dragX = 0, dragY = 0;
    float breakTimer = 0;
    // Player health, armor, and Grafik monster health values.
    int health = 10, armor = 10, grafikHealth = 6;
    float grafikHitTimer = 0.0f, swordCooldown = 0.0f;
    // Graphics preference and settings visibility.
    bool smoothGraphics = true, settingsOpen = false;
    // Number of chunks rendered around the player.
    int drawDistance = 7;
    // World mode flags controlling gameplay, terrain generation, and session state.
    bool creativeMode = true, spectatorMode = false, flatWorld = false, inWorld = false;
    // Lighting intensity and network synchronization timer for the skeleton.
    float worldBrightness = 1.0f, skeletonNetTimer = 0;
    // Save slot currently selected in the main menu.
    int selectedSlot = 0;
    // App-private directory used for world and player-name save files.
    std::string saveDirectory;
    float saveTimer = 0;
    // Grafik monster position, movement, respawn, and swallow-attack timers.
    float grafikX = 5.0f, grafikZ = -5.0f, grafikFeet = 0, grafikRespawn = 0, swallowTimer = 0;
    // Grafik alive state and whether the local player has died.
    bool grafikAlive = true, playerDead = false;
    // One inventory stack stores an item ID and its stack count.
    struct Item {
        uint8_t id = 0;
        int count = 0;
    };
    std::array<Item, 20> inventory{};
    // Status text, local/remote player names, and name-editing state.
    std::string status = "DEATH_CRAFT  |  V0.1";
    std::string playerName = "PLAYER", remoteName = "PLAYER";
    bool nameEditing = false;
    // FPS sampling accumulator, displayed FPS, and frame counter.
    float fpsAccumulator = 0, fpsValue = 0;
    int fpsFrames = 0;
    int statusFrames = 0;
    // UDP socket, host/client mode, and this device's network identifier.
    int lanSocket = -1, lanMode = 0;
    uint32_t networkId = 0;
    // Timers for player updates, host discovery, and remote-player timeout.
    float lanTimer = 0, helloTimer = 0, remoteTimer = 0;
    // Remote-player transform validity and replicated game-mode flags.
    bool remoteValid = false;
    float remoteX = 0, remoteY = 0, remoteZ = 0, remoteYaw = 0;
    bool remoteCreative = false, remoteSpectator = false;
    sockaddr_in lanBroadcast{};

    // Subsystem implementations are included inside Game to access shared state.
#include "systems/chunk_render.inc"
#include "systems/graphics.inc"
#include "systems/inventory.inc"
#include "systems/lan.inc"
#include "systems/menu.inc"
#include "systems/player.inc"
#include "systems/world.inc"
#include "systems/world_update.inc"

    // Converts a touch position into normalized joystick movement input.
    void setJoy(float x, float y) {
        joyX = std::clamp((x - joyOriginX) / .20f, -1.0f, 1.0f);
        joyY = std::clamp((y - joyOriginY) / .20f, -1.0f, 1.0f);
    }
    // Creates the OpenSL ES engine and loads the game's sound effects.
    void initAudio(AAssetManager* manager) {
        const SLInterfaceID ids[] = {SL_IID_ENGINE};
        const SLboolean req[] = {SL_BOOLEAN_TRUE};
        if (slCreateEngine(&slEngineObject, 0, nullptr, 1, ids, req) != SL_RESULT_SUCCESS)
            return;
        if ((*slEngineObject)->Realize(slEngineObject, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS)
            return;
        if ((*slEngineObject)->GetInterface(slEngineObject, SL_IID_ENGINE, &slEngine) !=
            SL_RESULT_SUCCESS)
            return;
        if ((*slEngine)->CreateOutputMix(slEngine, &slOutputMix, 0, nullptr, nullptr) !=
            SL_RESULT_SUCCESS)
            return;
        (*slOutputMix)->Realize(slOutputMix, SL_BOOLEAN_FALSE);
        const char* names[] = {"attack_sound.mp3", "move_sound.mp3", "tnt_explosive.mp3"};
        for (int i = 0; i < 3; i++) {
            std::string path = "textures/sound/" + std::string(names[i]);
            soundAssets[i] = AAssetManager_open(manager, path.c_str(), AASSET_MODE_UNKNOWN);
            if (!soundAssets[i])
                continue;
            off_t start = 0, length = 0;
            int fd = AAsset_openFileDescriptor(soundAssets[i], &start, &length);
            if (fd < 0)
                continue;
            soundFds[i] = fd;
            SLDataLocator_AndroidFD loc{SL_DATALOCATOR_ANDROIDFD, fd, start, length};
            SLDataFormat_MIME fmt{SL_DATAFORMAT_MIME, nullptr, SL_CONTAINERTYPE_MP3};
            SLDataSource source{&loc, &fmt};
            SLDataLocator_OutputMix out{SL_DATALOCATOR_OUTPUTMIX, slOutputMix};
            SLDataSink sink{&out, nullptr};
            const SLInterfaceID pids[] = {SL_IID_PLAY, SL_IID_SEEK};
            const SLboolean preq[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_FALSE};
            if ((*slEngine)->CreateAudioPlayer(
                    slEngine, &slPlayers[i], &source, &sink, 1, pids, preq) == SL_RESULT_SUCCESS &&
                (*slPlayers[i])->Realize(slPlayers[i], SL_BOOLEAN_FALSE) == SL_RESULT_SUCCESS) {
                (*slPlayers[i])->GetInterface(slPlayers[i], SL_IID_PLAY, &slPlay[i]);
                (*slPlayers[i])->GetInterface(slPlayers[i], SL_IID_SEEK, &slSeek[i]);
            }
            if (!slPlayers[i]) {
                close(fd);
                soundFds[i] = -1;
            }
        }
    }
    // Checks whether the selected sound effect is currently playing.
    bool isSoundPlaying(int id) {
        if (id < 0 || id >= 3 || !slPlay[id])
            return false;
        SLuint32 state = SL_PLAYSTATE_STOPPED;
        return (*slPlay[id])->GetPlayState(slPlay[id], &state) == SL_RESULT_SUCCESS &&
               state == SL_PLAYSTATE_PLAYING;
    }
    // Stops a sound effect and rewinds it to the beginning.
    void stopSound(int id) {
        if (id < 0 || id >= 3 || !slPlay[id])
            return;
        (*slPlay[id])->SetPlayState(slPlay[id], SL_PLAYSTATE_STOPPED);
        if (slSeek[id])
            (*slSeek[id])->SetPosition(slSeek[id], 0, SL_SEEKMODE_FAST);
    }
    // Restarts and plays the selected sound effect.
    void playSound(int id) {
        if (id < 0 || id >= 3 || !slPlay[id])
            return;
        stopSound(id);
        (*slPlay[id])->SetPlayState(slPlay[id], SL_PLAYSTATE_PLAYING);
    }
    // Releases all OpenSL ES objects, assets, and file descriptors.
    void shutdownAudio() {
        for (int i = 0; i < 3; i++) {
            if (slPlayers[i]) {
                (*slPlayers[i])->Destroy(slPlayers[i]);
                slPlayers[i] = nullptr;
                slPlay[i] = nullptr;
                slSeek[i] = nullptr;
            }
            if (soundFds[i] >= 0) {
                close(soundFds[i]);
                soundFds[i] = -1;
            }
            if (soundAssets[i]) {
                AAsset_close(soundAssets[i]);
                soundAssets[i] = nullptr;
            }
        }
        if (slOutputMix) {
            (*slOutputMix)->Destroy(slOutputMix);
            slOutputMix = nullptr;
        }
        if (slEngineObject) {
            (*slEngineObject)->Destroy(slEngineObject);
            slEngineObject = nullptr;
            slEngine = nullptr;
        }
    }

   public:
    // Initializes EGL, OpenGL ES, game assets, audio, and saved player settings.
    bool init(ANativeWindow* w, AAssetManager* manager, const char* dataDir) {
        const EGLint attrs[] = {EGL_RENDERABLE_TYPE,
                                EGL_OPENGL_ES3_BIT,
                                EGL_SURFACE_TYPE,
                                EGL_WINDOW_BIT,
                                EGL_RED_SIZE,
                                8,
                                EGL_GREEN_SIZE,
                                8,
                                EGL_BLUE_SIZE,
                                8,
                                EGL_ALPHA_SIZE,
                                8,
                                EGL_DEPTH_SIZE,
                                16,
                                EGL_NONE};
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr))
            return false;
        EGLConfig cfg;
        EGLint count = 0;
        eglChooseConfig(display, attrs, &cfg, 1, &count);
        if (!count)
            return false;
        const EGLint ca[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context = eglCreateContext(display, cfg, EGL_NO_CONTEXT, ca);
        surface = eglCreateWindowSurface(display, cfg, w, nullptr);
        if (context == EGL_NO_CONTEXT || surface == EGL_NO_SURFACE ||
            !eglMakeCurrent(display, surface, surface, context))
            return false;
        eglQuerySurface(display, surface, EGL_WIDTH, &width);
        eglQuerySurface(display, surface, EGL_HEIGHT, &height);
        glViewport(0, 0, width, height);
        if (!setupGL())
            return false;
        loadTextures(manager);
        initAudio(manager);
        saveDirectory = dataDir ? dataDir : ".";
        loadPlayerName();
        feet = 1;
        grafikFeet = 1;
        health = 10;
        armor = 10;
        grafikHealth = 2;
        selected = 0;
        running = true;
        inWorld = false;
        return true;
    }
    // Routes Android touch events to menus, movement, camera, and gameplay controls.
    int input(AInputEvent* e) {
        if (AInputEvent_getType(e) != AINPUT_EVENT_TYPE_MOTION)
            return 0;
        int action = AMotionEvent_getAction(e) & AMOTION_EVENT_ACTION_MASK,
            idx = (AMotionEvent_getAction(e) & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                  AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int id = AMotionEvent_getPointerId(e, idx);
        float x = AMotionEvent_getX(e, idx) / width * 2 - 1,
              y = 1 - AMotionEvent_getY(e, idx) / height * 2;
        if (action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            if (!inWorld) {
                if (settingsOpen) {
                    if (x > .62f && y > .55f) {
                        settingsOpen = false;
                        return 1;
                    }
                    if (y > .05f && y < .30f) {
                        smoothGraphics = !smoothGraphics;
                        applyGraphicsFiltering();
                        return 1;
                    }
                    if (y > -.35f && y < -.02f) {
                        drawDistance = std::clamp(drawDistance + (x < -.12f ? -1 : 1), 3, 10);
                        return 1;
                    }
                    return 1;
                }
                if (x > .68f && y > .70f) {
                    settingsOpen = true;
                    return 1;
                }
                if (nameEditing) {
                    if (y < -.43f) {
                        if (x > .25f) {
                            if (!playerName.empty())
                                playerName.pop_back();
                            savePlayerName();
                        } else {
                            nameEditing = false;
                            savePlayerName();
                        }
                        return 1;
                    }
                    int row = std::clamp((int)((.25f - y) / .20f), 0, 3);
                    const char* rows[] = {"ABCDEFG", "HIJKLMN", "OPQRSTU", "VWXYZ"};
                    int col = std::clamp((int)((x + .70f) / .19f), 0, 6);
                    if (rows[row][col] && playerName.size() < 16) {
                        playerName.push_back(rows[row][col]);
                        savePlayerName();
                    }
                    return 1;
                }
                if (y > .49f && y < .68f) {
                    nameEditing = true;
                    return 1;
                }
                if (y > .14f && y < .34f) {
                    creativeMode = x < 0;
                    return 1;
                }
                if (y > -.20f && y < .00f) {
                    flatWorld = x < 0;
                    return 1;
                }
                if (y > -.55f && y < -.33f) {
                    selectedSlot = std::clamp((int)((x + .62f) / .44f), 0, 2);
                    return 1;
                }
                if (y < -.68f) {
                    startWorld();
                    return 1;
                }
                return 1;
            }
            if (settingsOpen) {
                if (x > .62f && y > .55f) {
                    settingsOpen = false;
                    return 1;
                }
                if (y > .05f && y < .30f) {
                    smoothGraphics = !smoothGraphics;
                    applyGraphicsFiltering();
                    return 1;
                }
                if (y > -.35f && y < -.02f) {
                    drawDistance = std::clamp(drawDistance + (x < -.12f ? -1 : 1), 3, 10);
                    return 1;
                }
                return 1;
            }
            if (inventoryOpen) {
                if (x > .62f && y > .55f) {
                    inventoryOpen = false;
                    craftingTableOpen = false;
                    dragIndex = -1;
                    return 1;
                }
                if (x > .55f && x < .80f && y > .20f && y < .43f) {
                    craft(craftingTableOpen ? 3 : 0);
                    return 1;
                }
                if (x > .55f && x < .80f && y > -.08f && y < .16f) {
                    craft(craftingTableOpen ? 4 : 1);
                    return 1;
                }
                if (x > .55f && x < .80f && y > -.38f && y < -.12f) {
                    craft(craftingTableOpen ? 5 : 2);
                    return 1;
                }
                if (x > .55f && x < .80f && y > -.66f && y < -.48f) {
                    craft(6);
                    return 1;
                }
                int slot = inventorySlotAt(x, y);
                if (slot >= 0) {
                    dragIndex = slot;
                    dragPointer = id;
                    dragX = x;
                    dragY = y;
                }
                return 1;
            }
            if (x > -.95f && x < -.72f && y < -.90f) {
                setLanMode(2);
                return 1;
            }
            if (x > -.70f && x < -.47f && y < -.90f) {
                setLanMode(1);
                return 1;
            }
            if (x > .81f && y > .72f) {
                inventoryOpen = true;
                craftingTableOpen = false;
                joyId = lookId = -1;
                joyX = joyY = 0;
                breaking = false;
                return 1;
            }
            if (x > .80f && y > .27f && y < .48f) {
                cycleGameMode();
                return 1;
            }
            if (x > .82f && y > .48f && y < .68f) {
                settingsOpen = true;
                joyId = lookId = -1;
                joyX = joyY = 0;
                return 1;
            }
            if (x < -.42f && y < 0 && y > -.88f) {
                joyId = id;
                joyOriginX = x;
                joyOriginY = y;
                setJoy(x, y);
            } else if (x > .65f && y < -.43f) {
                if (spectatorMode) {
                    feet += 2.0f;
                    return 1;
                }
                if (inWater()) {
                    vy = 3.4f;
                    grounded = false;
                    fallTracking = false;
                } else if (grounded) {
                    vy = 7.4f;
                    grounded = false;
                    fallStartFeet = feet;
                    fallPeakFeet = feet;
                    fallTracking = true;
                }
            } else if (x > .65f && y > -.34f && y < .12f) {
                int tx, ty, tz, ax, ay, az;
                if (raycast(tx, ty, tz, ax, ay, az)) {
                    breaking = true;
                    breakPointer = id;
                    breakX = tx;
                    breakY = ty;
                    breakZ = tz;
                    breakTimer = 0;
                }
            } else if (x > .40f && x < .66f && y > -.54f && y < -.25f)
                editBlock(true);
            else if (x > .38f && x < .64f && y > -.22f && y < .03f)
                useTarget();
            else if (y < -.72f && x > -.31f && x < .31f)
                selected = std::clamp((int)((x + .31f) / .62f * 5), 0, 4);
            else {
                lookId = id;
                lastX = x;
                lastY = y;
            }
            return 1;
        }
        if (action == AMOTION_EVENT_ACTION_MOVE) {
            for (size_t n = 0; n < AMotionEvent_getPointerCount(e); n++) {
                int pid = AMotionEvent_getPointerId(e, n);
                float tx = AMotionEvent_getX(e, n) / width * 2 - 1,
                      ty = 1 - AMotionEvent_getY(e, n) / height * 2;
                if (pid == joyId)
                    setJoy(tx, ty);
                if (pid == dragPointer) {
                    dragX = tx;
                    dragY = ty;
                }
                if (pid == lookId) {
                    yaw += (tx - lastX) * 1.45f;
                    pitch = std::clamp(pitch - (ty - lastY) * 1.15f, -1.35f, 1.35f);
                    lastX = tx;
                    lastY = ty;
                }
            }
            return 1;
        }
        if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_POINTER_UP ||
            action == AMOTION_EVENT_ACTION_CANCEL) {
            if (action == AMOTION_EVENT_ACTION_CANCEL) {
                joyId = lookId = breakPointer = dragPointer = -1;
                joyX = joyY = 0;
                breaking = false;
                dragIndex = -1;
                return 1;
            }
            if (id == joyId) {
                joyId = -1;
                joyX = joyY = 0;
            }
            if (id == lookId)
                lookId = -1;
            if (id == breakPointer) {
                breakPointer = -1;
                breaking = false;
                breakTimer = 0;
            }
            if (inventoryOpen && id == dragPointer && dragIndex >= 0) {
                int target = inventorySlotAt(x, y);
                if (target >= 0 && target != dragIndex)
                    std::swap(inventory[target], inventory[dragIndex]);
                else if (target < 0) {
                    Item dropped = inventory[dragIndex];
                    if (dropped.count > 0) {
                        spawnDroppedItem(dropped.id,
                                         px + std::sin(yaw) * 1.0f,
                                         feet + .7f,
                                         pz - std::cos(yaw) * 1.0f,
                                         creativeMode ? 1 : dropped.count);
                        if (!creativeMode)
                            inventory[dragIndex] = {};
                    }
                }
                dragIndex = dragPointer = -1;
            }
            return 1;
        }
        return 1;
    }
    // Saves the active world and releases every game and platform resource.
    void shutdown() {
        if (inWorld)
            saveWorld();
        shutdownAudio();
        running = false;
        if (lanSocket >= 0) {
            close(lanSocket);
            lanSocket = -1;
        }
        lanMode = 0;
        remoteValid = false;
        for (auto& entry : chunks)
            if (entry.second.meshVbo) {
                glDeleteBuffers(1, &entry.second.meshVbo);
                entry.second.meshVbo = 0;
                entry.second.meshVertexCount = 0;
            }
        chunks.clear();
        dirtyChunkQueue.clear();
        if (display != EGL_NO_DISPLAY) {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (context != EGL_NO_CONTEXT)
                eglDestroyContext(display, context);
            if (surface != EGL_NO_SURFACE)
                eglDestroySurface(display, surface);
            eglTerminate(display);
        }
        display = EGL_NO_DISPLAY;
        surface = EGL_NO_SURFACE;
        context = EGL_NO_CONTEXT;
    }
    // Advances one simulation/render frame using the supplied elapsed time.
    void tick(float dt) {
        frame(dt);
    }
    // Public save hook used by Android lifecycle callbacks.
    void save() {
        saveWorld();
    }
    // Reports whether the game loop should continue running.
    bool isRunning() const {
        return running;
    }
};
// Single game instance used by Android lifecycle and input callbacks.
static Game game;
// Handles Android window and pause lifecycle events.
static void command(android_app* app, int32_t cmd) {
    if (cmd == APP_CMD_PAUSE)
        game.save();
    if (cmd == APP_CMD_INIT_WINDOW && app->window) {
        if (!game.init(app->window, app->activity->assetManager, app->activity->internalDataPath))
            LOGE("Game initialization failed");
    }
    if (cmd == APP_CMD_TERM_WINDOW)
        game.shutdown();
}
// Native Android application entry point and main event/render loop.
void android_main(android_app* app) {
    app->onAppCmd = command;
    app->onInputEvent = [](android_app*, AInputEvent* e) -> int { return game.input(e); };
    int64_t previous = 0;
    while (true) {
        int events;
        android_poll_source* source;
        while (ALooper_pollOnce(game.isRunning() ? 0 : -1, nullptr, &events, (void**)&source) >=
               0) {
            if (source)
                source->process(app, source);
            if (app->destroyRequested) {
                game.shutdown();
                return;
            }
        }
        timespec ts{};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        int64_t now = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
        float dt = previous ? (now - previous) / 1e9f : .016f;
        previous = now;
        if (game.isRunning())
            game.tick(dt);
    }
}
