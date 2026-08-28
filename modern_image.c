/* modern_image.c - Modern Terminal Graphics & Native macOS ImageIO Engine for w3m-vimium */
#include "modern_image.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

static unsigned int
modern_hash_str(const char *s)
{
    unsigned int h = 5381;
    while (s && *s) {
        h = ((h << 5) + h) + (unsigned char)(*s);
        s++;
    }
    return h;
}

static const char *
modern_basename(const char *path)
{
    const char *p = strrchr(path, '/');
    return p ? (p + 1) : path;
}

/* Detect modern terminal graphics protocol from environment */
int
modern_detect_graphics_proto(void)
{
    const char *term_prog = getenv("TERM_PROGRAM");
    const char *kitty_win = getenv("KITTY_WINDOW_ID");
    const char *term = getenv("TERM");

    if (kitty_win != NULL || (term && strstr(term, "kitty"))) {
        return MODERN_GRAPHICS_KITTY;
    }

    if (term_prog != NULL) {
        if (strcasecmp(term_prog, "iTerm.app") == 0 ||
            strcasecmp(term_prog, "WezTerm") == 0 ||
            strcasecmp(term_prog, "ghostty") == 0 ||
            strcasecmp(term_prog, "vscode") == 0) {
            return MODERN_GRAPHICS_ITERM2;
        }
        if (strcasecmp(term_prog, "Apple_Terminal") == 0) {
            /* Terminal.app does not support inline escape images; default to QuickLook */
            return MODERN_GRAPHICS_NONE;
        }
    }

    if (term != NULL) {
        if (strstr(term, "sixel") || strstr(term, "foot") || strstr(term, "mlterm")) {
            return MODERN_GRAPHICS_SIXEL;
        }
    }

    /* Modern default for macOS if running in modern multiplexer/emulator */
    if (term_prog != NULL) {
        return MODERN_GRAPHICS_ITERM2;
    }

    return MODERN_GRAPHICS_NONE;
}

/* Parse SVG width/height or viewBox from header */
static int
parse_svg_dimensions(const char *filepath, unsigned int *width, unsigned int *height)
{
    FILE *fp = fopen(filepath, "r");
    char buf[4096];
    size_t n;
    char *p;

    if (!fp)
        return 0;

    n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    if (n == 0)
        return 0;
    buf[n] = '\0';

    if (strstr(buf, "<svg") == NULL && strstr(buf, "<SVG") == NULL)
        return 0;

    /* Check viewBox="min-x min-y width height" */
    p = strstr(buf, "viewBox=\"");
    if (!p)
        p = strstr(buf, "viewbox=\"");
    if (p) {
        float min_x = 0, min_y = 0, vb_w = 0, vb_h = 0;
        if (sscanf(p + 9, "%f %f %f %f", &min_x, &min_y, &vb_w, &vb_h) == 4 ||
            sscanf(p + 9, "%f,%f,%f,%f", &min_x, &min_y, &vb_w, &vb_h) == 4) {
            if (vb_w > 0 && vb_h > 0) {
                *width = (unsigned int)(vb_w + 0.5f);
                *height = (unsigned int)(vb_h + 0.5f);
                return 1;
            }
        }
    }

    /* Check width="..." and height="..." */
    p = strstr(buf, "width=\"");
    if (p) {
        float w_val = 0;
        if (sscanf(p + 7, "%f", &w_val) == 1 && w_val > 0) {
            char *h_p = strstr(buf, "height=\"");
            if (h_p) {
                float h_val = 0;
                if (sscanf(h_p + 8, "%f", &h_val) == 1 && h_val > 0) {
                    *width = (unsigned int)(w_val + 0.5f);
                    *height = (unsigned int)(h_val + 0.5f);
                    return 1;
                }
            }
        }
    }

    /* Default fallback dimension for generic SVGs */
    *width = 800;
    *height = 600;
    return 1;
}

/* Direct in-process image dimension extractor using macOS ImageIO / CoreGraphics */
int
modern_get_image_size(const char *filepath, unsigned int *width, unsigned int *height)
{
    CFStringRef path_cf;
    CFURLRef url_cf;
    CGImageSourceRef image_source;
    CFDictionaryRef props;
    int success = 0;

    if (!filepath || !width || !height)
        return 0;

    /* Check for SVG */
    if (strstr(filepath, ".svg") || strstr(filepath, ".SVG")) {
        if (parse_svg_dimensions(filepath, width, height))
            return 1;
    }

    path_cf = CFStringCreateWithCString(kCFAllocatorDefault, filepath, kCFStringEncodingUTF8);
    if (!path_cf)
        return 0;

    url_cf = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path_cf, kCFURLPOSIXPathStyle, false);
    CFRelease(path_cf);
    if (!url_cf)
        return 0;

    image_source = CGImageSourceCreateWithURL(url_cf, NULL);
    CFRelease(url_cf);
    if (!image_source) {
        /* If ImageIO didn't recognize it, try SVG fallback check */
        return parse_svg_dimensions(filepath, width, height);
    }

    props = CGImageSourceCopyPropertiesAtIndex(image_source, 0, NULL);
    if (props) {
        CFNumberRef w_num = (CFNumberRef)CFDictionaryGetValue(props, kCGImagePropertyPixelWidth);
        CFNumberRef h_num = (CFNumberRef)CFDictionaryGetValue(props, kCGImagePropertyPixelHeight);

        if (w_num && h_num) {
            int w = 0, h = 0;
            CFNumberGetValue(w_num, kCFNumberIntType, &w);
            CFNumberGetValue(h_num, kCFNumberIntType, &h);
            if (w > 0 && h > 0) {
                *width = (unsigned int)w;
                *height = (unsigned int)h;
                success = 1;
            }
        }
        CFRelease(props);
    }

    CFRelease(image_source);
    return success;
}

/* Rasterize vector SVG to cached PNG for inline terminal rendering */
char *
modern_ensure_renderable_image(const char *filepath)
{
    static char cache_path[1024];
    struct stat st;
    char cmd[2048];

    if (!filepath)
        return NULL;

    /* If not SVG, return original directly */
    if (!strstr(filepath, ".svg") && !strstr(filepath, ".SVG")) {
        return (char *)filepath;
    }

    const char *tmp = getenv("TMPDIR");
    if (!tmp)
        tmp = "/tmp";

    snprintf(cache_path, sizeof(cache_path), "%s/w3m_svg_%u.png",
             tmp, modern_hash_str(filepath));

    if (stat(cache_path, &st) == 0 && st.st_size > 0) {
        return cache_path;
    }

    /* Convert SVG to PNG using macOS quicklook or sips/rsvg */
    snprintf(cmd, sizeof(cmd),
             "qlmanage -t -s 1000 -o '%s' '%s' >/dev/null 2>&1 && mv '%s/%s.png' '%s' 2>/dev/null",
             tmp, filepath, tmp, modern_basename(filepath), cache_path);

    if (system(cmd) != 0 || stat(cache_path, &st) != 0) {
        /* Fallback: try rsvg-convert or magick if present */
        snprintf(cmd, sizeof(cmd),
                 "rsvg-convert -o '%s' '%s' >/dev/null 2>&1 || magick '%s' '%s' >/dev/null 2>&1",
                 cache_path, filepath, filepath, cache_path);
        system(cmd);
    }

    if (stat(cache_path, &st) == 0 && st.st_size > 0)
        return cache_path;

    return (char *)filepath;
}

/* QuickLook fast popup preview for image under cursor */
void
modern_quicklook_preview(const char *filepath)
{
    pid_t pid;

    if (!filepath || access(filepath, R_OK) != 0)
        return;

    pid = fork();
    if (pid == 0) {
        /* Child process: non-blocking QuickLook preview */
        int fd = open("/dev/null", 0);
        if (fd >= 0) {
            dup2(fd, 0);
            dup2(fd, 1);
            dup2(fd, 2);
            close(fd);
        }
        execlp("qlmanage", "qlmanage", "-p", filepath, NULL);
        _exit(1);
    }
}
