# Death_Craft

A native Android voxel sandbox prototype built with C++20, Android NDK, CMake and OpenGL ES 3.0.

## V0.1 prototype features

- First-person camera and a mobile player controller.
- Touch joystick, swipe-to-look, jump, break and place controls.
- Flat grass terrain generated in 16x16 block chunks.
- Nearby chunks load around the player and distant chunks unload.
- Visible-face voxel meshing to avoid drawing hidden cube faces.
- Basic player collision with solid blocks and gravity/jumping.
- Five-slot block hotbar (grass, dirt and stone block types).
- Block edits are retained when chunks unload and reload during the current app session.

## Touch controls

- Left joystick: move.
- Drag on the right side: rotate the camera.
- Yellow round button: jump.
- Blue button: place the selected block.
- Red button: break the targeted block.
- Bottom hotbar: select a block type.
- Aim at a block with the center crosshair before placing or breaking.

## Build on GitHub

The workflow in `.github/workflows/android-apk.yml` builds a debug APK for arm64-v8a and armeabi-v7a. Open the repository's **Actions** tab, select **Android APK**, and run the workflow manually, or push a commit to `main`. Download the generated APK from the workflow run's Artifacts section.

## Current prototype limitations

- Terrain is an infinite-style flat plane generated in chunks, not yet varied terrain.
- Block edits are kept in memory only; there is no save file yet.
- No crafting, enemies, inventory persistence, audio, or multiplayer yet.
- Performance and device compatibility still need testing on physical Android phones.
