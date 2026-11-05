// SPDX-License-Identifier: BSD-3-Clause
#include "glyphs.h"

#include "ui/icon.h"

using namespace cutefish::ui;

namespace photos {

// 16 by 16 single-colour drawings, tinted by the toolkit like the built-in
// ones. Stroked shapes take the tint where they have coverage, so they draw
// as outlines.

static const char* zoomInGlyph =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>"
    "<g fill='none' stroke='#000' stroke-width='1.5' stroke-linecap='round'>"
    "<circle cx='6.75' cy='6.75' r='4.75'/>"
    "<path d='M10.3 10.3 15 15'/>"
    "<path d='M6.75 4.5v4.5M4.5 6.75h4.5'/>"
    "</g></svg>";

static const char* zoomOutGlyph =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>"
    "<g fill='none' stroke='#000' stroke-width='1.5' stroke-linecap='round'>"
    "<circle cx='6.75' cy='6.75' r='4.75'/>"
    "<path d='M10.3 10.3 15 15'/>"
    "<path d='M4.5 6.75h4.5'/>"
    "</g></svg>";

static const char* heartGlyph =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>"
    "<path fill='none' stroke='#000' stroke-width='1.4' stroke-linejoin='round' d='"
    "M8 13.6S2.35 10.13 1.42 6.94C.81 4.48 2.7 2.35 4.9 2.35c1.35 0 2.44.7 3.1 1.74.66-1.04 1.75-1.74 3.1-1.74 2.2 0 4.09 2.13 3.48 4.59C13.65 10.13 8 13.6 8 13.6Z'/>"
    "</svg>";

static const char* heartFilledGlyph =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>"
    "<path fill='#000' d='"
    "M8 13.6S2.35 10.13 1.42 6.94C.81 4.48 2.7 2.35 4.9 2.35c1.35 0 2.44.7 3.1 1.74.66-1.04 1.75-1.74 3.1-1.74 2.2 0 4.09 2.13 3.48 4.59C13.65 10.13 8 13.6 8 13.6Z'/>"
    "</svg>";

static const char* photoStackGlyph =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>"
    "<g fill='none' stroke='#000' stroke-width='1.3' stroke-linejoin='round'>"
    "<rect x='1.2' y='3.4' width='11.6' height='9.2' rx='2'/>"
    "<path d='M3.6 10.2 6.6 7.4a1.1 1.1 0 0 1 1.6 0l4 3.9M6.6 5.4h.01M10.2 5.2l3.6-2v7.4'/>"
    "</g></svg>";

static const char* rotateLeftGlyph =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>"
    "<g fill='none' stroke='#000' stroke-width='1.5' stroke-linecap='round' stroke-linejoin='round'>"
    "<path d='M3.5 6.5A5.5 5.5 0 1 1 3 9.5'/>"
    "<path d='M3.5 2.5v4h-4'/>"
    "</g></svg>";

static const char* rotateRightGlyph =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>"
    "<g fill='none' stroke='#000' stroke-width='1.5' stroke-linecap='round' stroke-linejoin='round'>"
    "<path d='M12.5 6.5A5.5 5.5 0 1 0 13 9.5'/>"
    "<path d='M12.5 2.5v4h4'/>"
    "</g></svg>";

static const char* fullscreenGlyph =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>"
    "<g fill='none' stroke='#000' stroke-width='1.5' stroke-linecap='round' stroke-linejoin='round'>"
    "<path d='M2 5.5V2h3.5M14 5.5V2h-3.5M2 10.5V14h3.5M14 10.5V14h-3.5'/>"
    "</g></svg>";

static const char* fullscreenExitGlyph =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>"
    "<g fill='none' stroke='#000' stroke-width='1.5' stroke-linecap='round' stroke-linejoin='round'>"
    "<path d='M5.5 2v3.5H2M10.5 2v3.5H14M5.5 14v-3.5H2M10.5 14v-3.5H14'/>"
    "</g></svg>";

static const char* editGlyph =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>"
    "<g fill='none' stroke='#000' stroke-width='1.4' stroke-linecap='round' stroke-linejoin='round'>"
    "<path d='M11.5 2.5 13.5 4.5 5 13H3v-2l8.5-8.5Z'/>"
    "</g></svg>";

static const char* addGlyph =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>"
    "<g fill='none' stroke='#000' stroke-width='1.5' stroke-linecap='round'>"
    "<path d='M8 3v10M3 8h10'/>"
    "</g></svg>";

void registerPhotoGlyphs()
{
    Icon::registerGlyph("photo-zoom-in", zoomInGlyph);
    Icon::registerGlyph("photo-zoom-out", zoomOutGlyph);
    Icon::registerGlyph("photo-heart", heartGlyph);
    Icon::registerGlyph("photo-heart-filled", heartFilledGlyph);
    Icon::registerGlyph("photo-stack", photoStackGlyph);
    Icon::registerGlyph("photo-rotate-left", rotateLeftGlyph);
    Icon::registerGlyph("photo-rotate-right", rotateRightGlyph);
    Icon::registerGlyph("photo-fullscreen", fullscreenGlyph);
    Icon::registerGlyph("photo-fullscreen-exit", fullscreenExitGlyph);
    Icon::registerGlyph("photo-edit", editGlyph);
    Icon::registerGlyph("photo-add", addGlyph);
}

} // namespace photos
