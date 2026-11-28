# Death Craft

Native Android voxel sandbox built with **C++20**, Android NDK, CMake, and OpenGL ES 3.0.

## Features

- First-person block world with touch controls, terrain chunks, block placement and breaking.
- Creative and Survival modes, flat and normal world types, and three save slots.
- Health and armor displayed as bars only.
- Classic pixel-style tree foliage texture at `app/src/main/assets/textures/grass_tree_texture.png`.
- A complete 20-minute day/night cycle (10 minutes daylight and 10 minutes night), with a fixed-world sun, moon, and yellow stars that disappear in daylight.
- A small in-game digital clock, black nighttime sky, and slow animated water surface (water vertices only; HUD and text remain stationary).
- KHALIL, a faster and more aggressive chasing monster that can kill the player with its close-range explosion; loads `khalil_monster.png` and uses a solid-white texture if absent.
- Placeable TNT: use a placed TNT block to start a 2-second fuse, then blast nearby blocks and damage or kill nearby players.
- Hostile mobs (Grafik, Khalil, and Skeleton) appear and attack only during nighttime; they despawn when daylight returns.
- Craftable bed (3 planks + 2 sticks): place it and use it at night to sleep until daytime.
- Expanded view distance and a slightly denser cloud field drifting slowly across the sky.
- Inventory and hotbar block icons use the same texture atlas as the world; `textures/tnt.png` is optional and defaults to solid white when missing.
- Block-breaking particle bursts, physical dropped block items, and automatic pickup at close range.
- Player name, FPS counter, hotbar, inventory, and LAN host/join prototype.
- World fog and view distance tuned for mobile rendering.
- Softer block ambient occlusion and brighter indirect lighting to prevent excessively dark shadows.

## Touch controls

- Left joystick: move.
- Drag on the right side: rotate the camera.
- Jump button: jump.
- Red X: break the targeted block.
- Place button: place the selected block.
- Bottom hotbar: select a block.
- JOIN / HOST: LAN multiplayer role.

## Project structure

- `app/src/main/cpp/main.cpp`: NativeActivity entry point and shared game state.
- `systems/lan.inc`: LAN sockets and packets.
- `systems/graphics.inc`: OpenGL shaders, texture loading, and graphics setup.
- `systems/world.inc`: terrain, blocks, save/load, and world generation.
- `systems/inventory.inc`: inventory and crafting.
- `systems/chunk_render.inc`: chunk meshes, rendering, and HUD.
- `systems/player.inc`: movement, collision, targeting, and block editing.
- `systems/world_update.inc`: simulation, day/night cycle, world-space sky, dropped items, block particles, and frame update.
- `systems/menu.inc`: main menu layout and name-entry screen.
- `app/src/main/assets/textures/`: block, foliage, sky, and control textures. Optional monster image: `app/src/main/assets/khalil_monster.png` (also accepts `textures/khalil_monster.png`).
- `stb_image.h`: image decoder.

The `.inc` files are included inside the `Game` class to keep shared state and existing behavior compatible while separating code by responsibility.

## Build

The GitHub Actions workflow `.github/workflows/android-apk.yml` builds the debug APK. Push to `main` or run **Android APK** manually from the Actions tab, then download the artifact from a successful run.

## Development notes

- The day/night cycle lasts exactly 1,200 seconds (20 minutes) per full cycle.
- Beds can be crafted from 3 planks and 2 sticks. Interact with a placed bed at night to skip directly to midday; only the LAN host can advance shared world time.
- Tree foliage texture is loaded as atlas layer 4 (zero-based index 3).
- Keep new code in the matching `systems/` file where possible.
- Test gameplay, lighting, and multiplayer on a physical Android device; CI confirms compilation and APK packaging, not device runtime behavior.

- World height expanded to 64 blocks. The surface has one grass layer, 10 dirt layers, deep stone layers, and an unbreakable bedrock floor.
- Block break/place confirmation popups have been removed to keep the HUD clean.
- Future audio assets are reserved in `app/src/main/assets/textures/sound/`.
