# Cutefish Photos

**Cutefish Photos** is a fast, elegant, privacy-first photo viewer and manager designed for CutefishOS and Wayland desktops using the Cutefish Toolkit (`cutefish-toolkit`).

Inspired by macOS Photos, it features a responsive thumbnail grid, persistent custom albums, single-image full-window view, in-memory preview rotation, wallpaper integration, image resizing, and format conversion.

---

## Features

- **Seamless Desktop Experience**: Built natively with the Cutefish Toolkit and Skia on Wayland.
- **Mac-Style Photo Viewer**: In single-image view, the sidebar automatically hides and `< Back` returns to the library.
- **Full Right-Click Context Menus**: Context menus across thumbnails, empty library space, sidebar albums, and full detail view.
- **Set as Desktop Background (`Ctrl+B`)**: Directly updates your Cutefish desktop background using `Cutefish::AppearanceClient`.
- **Non-Destructive In-Memory Rotation**:
  - `Ctrl+[`: Rotate left 90°
  - `Ctrl+]`: Rotate right 90°
  - `Ctrl+Shift+[`: Rotate 180°
  - Dedicated "Save" button and menu option to persist rotation to file only when explicitly requested.
  - "Reset Preview Rotation" (`Ctrl+R`) to undo changes.
- **Renaming, Resizing & Format Conversion**:
  - `F2`: Rename image directly from grid or detail view.
  - `Ctrl+Shift+R`: ImageMagick-powered aspect-locked resizing.
  - `Ctrl+Shift+E`: Convert to PNG, JPEG, WEBP, BMP, or TIFF.
- **Albums & Folder Support**: Add any directory as a persistent custom album (stored via `Settings("photos")`).
- **Smooth Zoom & Gestures**:
  - Keyboard: `Ctrl++`, `Ctrl+-`, `Ctrl+0` (fit), `Ctrl+1` (actual size).
  - Mouse: Wheel zoom while holding `Ctrl`.
  - Trackpad: Smooth two-finger pinch zoom.
- **Full Screen Mode (`F11`)**: Borderless distraction-free viewing.
- **Comprehensive Global Menu & D-Bus AppMenu**:
  - Application menu (`Photos`), File, Edit, Image, View (with Thumbnail Size switcher: Small/Medium/Large), Window, and Help (with interactive Shortcuts sheet).
- **Clean Modal About & Shortcuts Dialogs**: Beautifully styled `DialogWindow` modal dialogs with zero clipping.

---

## Shortcuts Reference

| Shortcut | Description |
|---|---|
| `Left` / `Right` | Previous / Next photo |
| `Return` / `Double Click` | Open photo in detail view |
| `Esc` / `Backspace` / `Ctrl+W` | Return to library grid |
| `Ctrl +` / `Ctrl -` | Zoom in / out (also mouse wheel & pinch) |
| `Ctrl 0` / `Ctrl 1` | Fit to window / Actual size |
| `F11` | Toggle full screen |
| `Ctrl \` | Toggle sidebar |
| `Ctrl I` | Toggle info panel |
| `Ctrl [` / `Ctrl ]` | Rotate preview left / right |
| `Ctrl S` | Save preview rotation to file |
| `Ctrl R` | Reset preview rotation |
| `Ctrl B` | Set as desktop background |
| `F2` | Rename photo |
| `Ctrl+Shift+R` | Resize image |
| `Ctrl+Shift+E` | Convert format |
| `Delete` | Move to trash |
| `Ctrl O` | Open library folder |
| `Ctrl+Shift+F` | Show in File Manager |

---

## Building & Installation

### Requirements

- `cmake` (>= 3.16)
- `g++` (C++20 support)
- `libcutefish-toolkit-dev`
- `imagemagick`
- `gio` / `glib2.0`

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Install

```bash
sudo cmake --install build
```
