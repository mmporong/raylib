/*
 * Side-by-side composite for the ImageRotate() before/after PNGs.
 *
 * Uses CPU-side image functions only (no window, no GPU, no font) and
 * does not call ImageRotate(), so it can link against either raylib build.
 *
 *   ./compose <out.png> <scale> <in1.png> [in2.png ...]
 *
 * Each input is scaled with ImageResizeNN() and drawn left to right over a
 * checkerboard, so transparent pixels show the checkerboard. Gray lines mark
 * the pixel grid of each input image.
 */

#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>

#define MARGIN 24
#define GAP 40

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "usage: %s <out.png> <scale> <in1.png> [in2.png ...]\n", argv[0]);
        return 2;
    }

    SetTraceLogLevel(LOG_WARNING);

    const char *outPath = argv[1];
    int scale = atoi(argv[2]);
    int count = argc - 3;
    Image *panels = (Image *)calloc(count, sizeof(Image));
    int totalWidth = 2*MARGIN + GAP*(count - 1);
    int maxHeight = 0;

    for (int i = 0; i < count; i++)
    {
        panels[i] = LoadImage(argv[3 + i]);
        if (panels[i].data == NULL)
        {
            fprintf(stderr, "could not load %s\n", argv[3 + i]);
            return 1;
        }

        ImageFormat(&panels[i], PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        ImageResizeNN(&panels[i], panels[i].width*scale, panels[i].height*scale);

        totalWidth += panels[i].width;
        if (panels[i].height > maxHeight) maxHeight = panels[i].height;
    }

    Image canvas = GenImageColor(totalWidth + 1, maxHeight + 2*MARGIN + 1, WHITE);
    Color grid = { 150, 150, 150, 255 };
    int x0 = MARGIN;

    for (int i = 0; i < count; i++)
    {
        int w = panels[i].width;
        int h = panels[i].height;

        Image checker = GenImageChecked(w, h, scale/4, scale/4, (Color){ 255, 255, 255, 255 }, (Color){ 200, 200, 200, 255 });
        ImageDrawImage(&canvas, checker, x0, MARGIN, WHITE);
        UnloadImage(checker);

        ImageDrawImage(&canvas, panels[i], x0, MARGIN, WHITE);

        for (int gx = 0; gx <= w; gx += scale) ImageDrawRectangle(&canvas, x0 + gx, MARGIN, 1, h + 1, grid);
        for (int gy = 0; gy <= h; gy += scale) ImageDrawRectangle(&canvas, x0, MARGIN + gy, w + 1, 1, grid);

        x0 += w + GAP;
        UnloadImage(panels[i]);
    }

    bool ok = ExportImage(canvas, outPath);
    UnloadImage(canvas);
    free(panels);

    if (!ok)
    {
        fprintf(stderr, "export failed: %s\n", outPath);
        return 1;
    }

    return 0;
}
