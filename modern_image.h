/* modern_image.h - Modern Terminal Graphics & Native macOS ImageIO Engine for w3m-vimium */
#ifndef MODERN_IMAGE_H
#define MODERN_IMAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Supported modern terminal graphic protocols */
#define MODERN_GRAPHICS_NONE      0
#define MODERN_GRAPHICS_ITERM2    1  /* OSC 1337 Inline Images (iTerm2, WezTerm, Ghostty, VSCode) */
#define MODERN_GRAPHICS_KITTY     2  /* APC G Kitty Graphics Protocol */
#define MODERN_GRAPHICS_SIXEL     3  /* DEC Sixel Graphics Protocol */
#define MODERN_GRAPHICS_OSC5379   4  /* OSC 5379 */

/* Detect modern terminal graphics protocol from environment */
int modern_detect_graphics_proto(void);

/* Direct in-process image dimension extractor using macOS ImageIO / CoreGraphics */
int modern_get_image_size(const char *filepath, unsigned int *width, unsigned int *height);

/* Rasterize vector SVG or unsupported format to PNG cache for inline display */
char *modern_ensure_renderable_image(const char *filepath);

/* QuickLook fast popup preview for image under cursor */
void modern_quicklook_preview(const char *filepath);

#ifdef __cplusplus
}
#endif

#endif /* MODERN_IMAGE_H */
