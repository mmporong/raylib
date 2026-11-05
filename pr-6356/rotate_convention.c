/*
 * Rotation convention check for raylib PR #6356.
 *
 * Rotates the same image with ImageRotate(), DrawTexturePro() and DrawTextureEx(),
 * puts the three results on the same canvas and writes a side-by-side PNG.
 * The two GPU panels are compared with the ImageRotate() panel pixel by pixel.
 *
 *   cc -std=c99 -O2 rotate_convention.c -I<raylib>/src <raylib>/src/libraylib.a \
 *      -lGL -lm -lpthread -ldl -lrt -lX11 -o rotate_convention
 *   ./rotate_convention <output dir>
 */

#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define SRC_W 8
#define SRC_H 5
#define SCALE 24
#define PANEL 300
#define LABEL 26
#define GAP 12

static const Color COL_TOP    = { 0, 121, 241, 255 };   // blue
static const Color COL_RIGHT  = { 0, 158, 47, 255 };    // green
static const Color COL_BOTTOM = { 255, 161, 0, 255 };   // orange
static const Color COL_LEFT   = { 230, 41, 55, 255 };   // red
static const Color COL_FILL   = { 40, 40, 40, 255 };    // dark gray

// Same test image as the before/after samples: 1-pixel border, one color per side
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

    ImageResizeNN(&image, SRC_W*SCALE, SRC_H*SCALE);
    return image;
}

// Draws a texture on a PANEL x PANEL transparent canvas and reads it back
static Image DrawPanel(Texture2D texture, float angle, int useEx)
{
    RenderTexture2D target = LoadRenderTexture(PANEL, PANEL);
    float w = (float)texture.width;
    float h = (float)texture.height;
    float c = PANEL/2.0f;

    BeginTextureMode(target);
    ClearBackground(BLANK);

    if (useEx)
    {
        // DrawTextureEx() pivots on the top-left corner, so place that corner
        // where it lands when the image rotates around the canvas center
        float rad = angle*DEG2RAD;
        Vector2 corner = {
            c - (w/2.0f)*cosf(rad) + (h/2.0f)*sinf(rad),
            c - (w/2.0f)*sinf(rad) - (h/2.0f)*cosf(rad)
        };
        DrawTextureEx(texture, corner, angle, 1.0f, WHITE);
    }
    else
    {
        Rectangle source = { 0, 0, w, h };
        Rectangle dest = { c, c, w, h };
        Vector2 origin = { w/2.0f, h/2.0f };
        DrawTexturePro(texture, source, dest, origin, angle, WHITE);
    }

    EndTextureMode();

    Image result = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&result);
    ImageFormat(&result, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    UnloadRenderTexture(target);
    return result;
}

// Runs ImageRotate() and centers the result on a PANEL x PANEL transparent canvas
static Image RotatePanel(Image source, int angle)
{
    Image rotated = ImageCopy(source);
    ImageRotate(&rotated, angle);
    ImageFormat(&rotated, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    Image result = GenImageColor(PANEL, PANEL, BLANK);
    int x = (PANEL - rotated.width)/2;
    int y = (PANEL - rotated.height)/2;
    ImageDrawImage(&result, rotated, x, y, WHITE);
    UnloadImage(rotated);
    return result;
}

// Mean and 99th-percentile channel difference over pixels opaque in both panels,
// plus the share of pixels that are opaque in one panel only
static void Compare(const char *name, int angle, Image a, Image b)
{
    Color *pa = LoadImageColors(a);
    Color *pb = LoadImageColors(b);
    int histogram[256] = { 0 };
    long long sum = 0;
    int both = 0;
    int onlyOne = 0;

    for (int i = 0; i < PANEL*PANEL; i++)
    {
        int oa = pa[i].a > 127;
        int ob = pb[i].a > 127;

        if (oa != ob) onlyOne++;
        if (!(pa[i].a == 255 && pb[i].a == 255)) continue;

        int d = abs(pa[i].r - pb[i].r);
        int dg = abs(pa[i].g - pb[i].g);
        int db = abs(pa[i].b - pb[i].b);
        if (dg > d) d = dg;
        if (db > d) d = db;

        histogram[d]++;
        sum += d;
        both++;
    }

    int nonzero = both - histogram[0];
    int maxDiff = 0;
    for (int d = 255; d > 0; d--) { if (histogram[d] > 0) { maxDiff = d; break; } }

    printf("%-16s angle %4d: compared %6d px, differing %5d px, mean diff %.3f/255, max diff %d/255, coverage mismatch %d px\n",
        name, angle, both, nonzero, (double)sum/both, maxDiff, onlyOne);

    UnloadImageColors(pa);
    UnloadImageColors(pb);
}

static void Checker(Image *canvas, int x, int y)
{
    for (int cy = 0; cy < PANEL; cy += 12)
    {
        for (int cx = 0; cx < PANEL; cx += 12)
        {
            Color c = (((cx + cy)/12) % 2) ? (Color){ 255, 255, 255, 255 } : (Color){ 225, 225, 225, 255 };
            ImageDrawRectangle(canvas, x + cx, y + cy, 12, 12, c);
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <output dir>\n", argv[0]);
        return 2;
    }

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "rotate_convention");

    Image source = MakeEdgeImage();
    Texture2D texture = LoadTextureFromImage(source);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);

    int angles[] = { 45, -45, 30, 90 };

    for (int i = 0; i < 4; i++)
    {
        int angle = angles[i];
        Image cpu = RotatePanel(source, angle);
        Image pro = DrawPanel(texture, (float)angle, 0);
        Image ex = DrawPanel(texture, (float)angle, 1);

        Compare("DrawTexturePro", angle, cpu, pro);
        Compare("DrawTextureEx", angle, cpu, ex);
        {
            Image opposite = DrawPanel(texture, (float)-angle, 0);
            Compare("CONTROL opposite", angle, cpu, opposite);
            UnloadImage(opposite);
        }

        if (angle == 45)
        {
            Image unrotated = GenImageColor(PANEL, PANEL, BLANK);
            ImageDrawImage(&unrotated, source, (PANEL - source.width)/2, (PANEL - source.height)/2, WHITE);

            Image panels[4] = { unrotated, cpu, pro, ex };
            const char *labels[4] = {
                "Source image",
                "ImageRotate(&image, 45)",
                "DrawTexturePro(..., 45, WHITE)",
                "DrawTextureEx(..., 45, 1, WHITE)"
            };

            int width = 4*PANEL + 5*GAP;
            int height = PANEL + LABEL + 2*GAP;
            Image canvas = GenImageColor(width, height, WHITE);

            for (int p = 0; p < 4; p++)
            {
                int x = GAP + p*(PANEL + GAP);

                ImageDrawText(&canvas, labels[p], x, GAP, 10, BLACK);
                Checker(&canvas, x, GAP + LABEL);
                ImageDrawImage(&canvas, panels[p], x, GAP + LABEL, WHITE);
            }

            char path[512];
            snprintf(path, sizeof(path), "%s/rotate_045_convention.png", argv[1]);
            ExportImage(canvas, path);
            UnloadImage(canvas);
            UnloadImage(unrotated);
        }

        UnloadImage(cpu);
        UnloadImage(pro);
        UnloadImage(ex);
    }

    UnloadTexture(texture);
    UnloadImage(source);
    CloseWindow();
    return 0;
}
