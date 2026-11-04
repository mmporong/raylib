/*
 * ImageRotate() before/after check for raylib PR #6356.
 *
 * Uses CPU-side image functions only: no window, no GPU, no font.
 * Build it once against each raylib static library:
 *
 *   cc -std=c99 -O2 rotate_check.c -I<raylib>/src <raylib>/src/libraylib.a \
 *      -lGL -lm -lpthread -ldl -lrt -lX11 -o rotate_check
 *
 * Run:
 *
 *   ./rotate_check <tag> <output dir>
 *
 * Writes 1:1 PNGs named <tag>_<image>_rot<angle>.png and prints a summary.
 */

#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC_W 8
#define SRC_H 5

static const Color COL_TOP    = { 0, 121, 241, 255 };   // blue
static const Color COL_RIGHT  = { 0, 158, 47, 255 };    // green
static const Color COL_BOTTOM = { 255, 161, 0, 255 };   // orange
static const Color COL_LEFT   = { 230, 41, 55, 255 };   // red
static const Color COL_FILL   = { 40, 40, 40, 255 };    // dark gray

// 1-pixel border with a different color on each side
static Image MakeEdgeImage(void)
{
    Image image = GenImageColor(SRC_W, SRC_H, COL_FILL);

    for (int y = 1; y < SRC_H - 1; y++)
    {
        ImageDrawPixel(&image, 0, y, COL_LEFT);
        ImageDrawPixel(&image, SRC_W - 1, y, COL_RIGHT);
    }

    for (int x = 0; x < SRC_W; x++)
    {
        ImageDrawPixel(&image, x, 0, COL_TOP);
        ImageDrawPixel(&image, x, SRC_H - 1, COL_BOTTOM);
    }

    return image;
}

// Every pixel has a different color, used for the exact comparisons
static Image MakeUniqueImage(void)
{
    Image image = GenImageColor(SRC_W, SRC_H, BLACK);

    for (int y = 0; y < SRC_H; y++)
    {
        for (int x = 0; x < SRC_W; x++)
        {
            Color c = { (unsigned char)(20 + 25*x), (unsigned char)(20 + 50*y), (unsigned char)(200 - 10*x - 5*y), 255 };
            ImageDrawPixel(&image, x, y, c);
        }
    }

    return image;
}

static Color *Pixels(Image image)
{
    return (Color *)image.data;   // all images here are PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
}

static int ChannelDiff(Color a, Color b)
{
    int d = abs(a.r - b.r);
    if (abs(a.g - b.g) > d) d = abs(a.g - b.g);
    if (abs(a.b - b.b) > d) d = abs(a.b - b.b);
    if (abs(a.a - b.a) > d) d = abs(a.a - b.a);
    return d;
}

// Exact reference for right angles, built from ImageRotateCW()/ImageRotateCCW()
static Image RightAngleReference(Image source, int degrees)
{
    Image ref = ImageCopy(source);

    if (degrees < 0)
    {
        for (int i = 0; i < (-degrees)/90; i++) ImageRotateCCW(&ref);
    }
    else
    {
        for (int i = 0; i < degrees/90; i++) ImageRotateCW(&ref);
    }

    return ref;
}

static void AngleName(char *out, size_t size, int degrees)
{
    snprintf(out, size, "%s%03d", (degrees < 0)? "m" : "", abs(degrees));
}

static void ExportTagged(Image image, const char *dir, const char *tag, const char *name, const char *kind, const char *angle)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s_%s_%s%s.png", dir, tag, name, kind, angle);
    if (!ExportImage(image, path)) fprintf(stderr, "export failed: %s\n", path);
}

static void PrintCoverage(Image image)
{
    Color *p = Pixels(image);
    int n = image.width*image.height;
    int transparent = 0, partial = 0;
    double sumA = 0, sumX = 0, sumY = 0;

    for (int i = 0; i < n; i++)
    {
        if (p[i].a == 0) transparent++;
        else if (p[i].a < 255) partial++;

        double a = p[i].a;
        sumA += a;
        sumX += a*(i%image.width);
        sumY += a*(i/image.width);
    }

    double cx = (sumA > 0)? sumX/sumA : 0;
    double cy = (sumA > 0)? sumY/sumA : 0;

    printf(" size=%dx%d transparent=%d partial_alpha=%d alpha_centroid_offset=(%+.3f, %+.3f)",
        image.width, image.height, transparent, partial, cx - (image.width - 1)/2.0, cy - (image.height - 1)/2.0);
}

static void PrintRightAngleChecks(Image rotated, Image reference)
{
    if ((rotated.width != reference.width) || (rotated.height != reference.height))
    {
        printf(" size_mismatch_vs_ref(%dx%d)", reference.width, reference.height);
        return;
    }

    Color *a = Pixels(rotated);
    Color *b = Pixels(reference);
    int n = rotated.width*rotated.height;
    int differing = 0, off1 = 0, maxDiff = 0;

    for (int i = 0; i < n; i++)
    {
        int d = ChannelDiff(a[i], b[i]);
        if (d > 0) differing++;
        if (d == 1) off1++;
        if (d > maxDiff) maxDiff = d;
    }

    printf(" differing_from_ref=%d/%d (off_by_one_only=%d, max_channel_diff=%d)", differing, n, off1, maxDiff);
}

// Number of source pixels whose color does not appear anywhere in the output (tolerance 1)
static int MissingSourcePixels(Image source, Image rotated)
{
    Color *s = Pixels(source);
    Color *r = Pixels(rotated);
    int missing = 0;

    for (int i = 0; i < source.width*source.height; i++)
    {
        bool found = false;
        for (int j = 0; (j < rotated.width*rotated.height) && !found; j++) found = (ChannelDiff(s[i], r[j]) <= 1);
        if (!found) missing++;
    }

    return missing;
}

static int CountColor(Image image, Color c)
{
    Color *p = Pixels(image);
    int count = 0;
    for (int i = 0; i < image.width*image.height; i++) if (ChannelDiff(p[i], c) <= 1) count++;
    return count;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s <tag> <output dir>\n", argv[0]);
        return 2;
    }

    const char *tag = argv[1];
    const char *dir = argv[2];
    static const int angles[] = { 90, 180, 270, -90, 45, 30 };

    SetTraceLogLevel(LOG_WARNING);

    Image edge = MakeEdgeImage();
    Image unique = MakeUniqueImage();

    ExportTagged(edge, dir, tag, "edge", "source", "");
    ExportTagged(unique, dir, tag, "unique", "source", "");

    printf("[%s] source %dx%d, edge colors: top=%d right=%d bottom=%d left=%d\n", tag, SRC_W, SRC_H,
        CountColor(edge, COL_TOP), CountColor(edge, COL_RIGHT), CountColor(edge, COL_BOTTOM), CountColor(edge, COL_LEFT));

    for (size_t k = 0; k < sizeof(angles)/sizeof(angles[0]); k++)
    {
        int degrees = angles[k];
        bool rightAngle = (degrees%90 == 0);
        char angle[16];
        AngleName(angle, sizeof(angle), degrees);

        Image edgeRot = ImageCopy(edge);
        ImageRotate(&edgeRot, degrees);
        ExportTagged(edgeRot, dir, tag, "edge", "rot", angle);

        Image uniqueRot = ImageCopy(unique);
        ImageRotate(&uniqueRot, degrees);
        ExportTagged(uniqueRot, dir, tag, "unique", "rot", angle);

        printf("[%s] edge   ImageRotate(%4d):", tag, degrees);
        PrintCoverage(edgeRot);
        printf(" edge_pixels top=%d right=%d bottom=%d left=%d",
            CountColor(edgeRot, COL_TOP), CountColor(edgeRot, COL_RIGHT), CountColor(edgeRot, COL_BOTTOM), CountColor(edgeRot, COL_LEFT));

        if (rightAngle)
        {
            Image edgeRef = RightAngleReference(edge, degrees);
            ExportTagged(edgeRef, dir, tag, "edge", "ref", angle);
            PrintRightAngleChecks(edgeRot, edgeRef);
            UnloadImage(edgeRef);
        }
        printf("\n");

        printf("[%s] unique ImageRotate(%4d):", tag, degrees);
        PrintCoverage(uniqueRot);
        if (rightAngle)
        {
            Image uniqueRef = RightAngleReference(unique, degrees);
            ExportTagged(uniqueRef, dir, tag, "unique", "ref", angle);
            PrintRightAngleChecks(uniqueRot, uniqueRef);
            printf(" source_pixels_missing=%d/%d", MissingSourcePixels(unique, uniqueRot), SRC_W*SRC_H);
            UnloadImage(uniqueRef);
        }
        printf("\n");

        UnloadImage(edgeRot);
        UnloadImage(uniqueRot);
    }

    UnloadImage(edge);
    UnloadImage(unique);

    return 0;
}
