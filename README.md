# Death Craft

Death Craft is a native Android voxel sandbox prototype built with **C++20**, the **Android NDK**, **CMake**, and **OpenGL ES 3.0**.

## Features

- First-person voxel sandbox with touch controls.
- Chunk-based terrain generation and configurable view distance.
- Visible-face block meshing to avoid rendering hidden cube faces.
- Block breaking and placement, with a five-slot hotbar.
- Player movement, jumping, collision, gravity, and survival fall damage.
- Health and armor HUD.
- Player name and FPS display.
- LAN multiplayer host/join prototype over UDP broadcast.
- Textured sky, terrain blocks, water, and mobile control buttons.
- World edits and player name are saved locally where supported by the current prototype.

## Touch controls

- **Left joystick:** move.
- **Drag on the right side:** rotate the camera.
- **Jump button:** jump.
- **Red X:** break the targeted block.
- **Blue button:** place the selected block.
- **Bottom hotbar:** select a block type.
- **Center crosshair:** aim at the block to edit.
- **LAN JOIN / HOST:** select a multiplayer role from the menu/game UI.

## Project structure

Native game code is organized by responsibility under `app/src/main/cpp/`:

- `main.cpp`: Android NativeActivity entry point, shared game state, and class assembly.
- `systems/lan.inc`: LAN socket setup and packet exchange.
- `systems/graphics.inc`: OpenGL setup, shaders, and texture loading.
- `systems/world.inc`: world data, save/load, terrain and block generation.
- `systems/inventory.inc`: inventory and crafting operations.
- `systems/chunk_render.inc`: chunk mesh generation and draw helpers.
- `systems/player.inc`: player collision, movement, targeting, and block editing.
- `systems/world_update.inc`: game simulation and frame update.
- `systems/menu.inc`: main menu and menu rendering.
- `stb_image.h`: image decoder used for texture assets.

The `.inc` files are included inside the `Game` class. This keeps the existing class layout and behavior intact while separating implementation into smaller, responsibility-based files.

## Build APK on GitHub

The workflow at `.github/workflows/android-apk.yml` builds debug APKs for **arm64-v8a** and **armeabi-v7a**.

1. Open the repository's **Actions** tab.
2. Select **Android APK**.
3. Run the workflow manually, or push a commit to `main`.
4. Download the APK from the completed run's **Artifacts** section.

## Current limitations

- LAN multiplayer is an early prototype and currently targets local-network discovery.
- World persistence and inventory persistence are still limited; test saving on the target device.
- Terrain generation and rendering are still being developed.
- Performance, multiplayer behavior, and device compatibility should be tested on physical Android devices.
- There is no crafting progression, enemy ecosystem, or audio system comparable to a full commercial sandbox game yet.

## Development

Keep new code in the matching `systems/` file where possible. Shared state and class fields remain in `main.cpp` until a later refactor introduces dedicated headers and independently compiled translation units.
