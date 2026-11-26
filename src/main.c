
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <setjmp.h>

PSP_MODULE_INFO("Crinmison", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define STRIDE 512
#define TEX 128
#define FLOOR_Y 218.0f
#define PLAYER_W 64.0f
#define PLAYER_H 80.0f

typedef struct {
    float u, v, x, y, z;
} Vertex;

typedef struct {
    unsigned int *data;
    int loaded;
} Sprite;

enum {
    IDLE, WALK, RUN, JUMP, FALL,
    LAND, ATTACK, HURT, MAGIC, ANIM_COUNT
};

static const char *files[ANIM_COUNT][4] = {
    {"idle.png", NULL, NULL, NULL},
    {"walk_1.png", "walk_2.png", NULL, NULL},
    {"run_1.png", "run_2.png", "run_3.png", "run_4.png"},
    {"jump_1.png", "jump_2.png", NULL, NULL},
    {"fall.png", NULL, NULL, NULL},
    {"landing.png", NULL, NULL, NULL},
    {"attack_1.png", "attack_2.png", NULL, NULL},
    {"hurt.png", NULL, NULL, NULL},
    {"magic.png", NULL, NULL, NULL}
};

static const int frames[ANIM_COUNT] =
    {1, 2, 4, 2, 1, 1, 2, 1, 1};

static Sprite sprites[ANIM_COUNT][4];
static unsigned int __attribute__((aligned(16))) list[262144];

static FILE *open_asset(const char *name)
{
    char path[256];

    const char *roots[] = {
        "ms0:/PSP/GAME/Crinmison/assets/sprites/",
        "ef0:/PSP/GAME/Crinmison/assets/sprites/",
        "assets/sprites/"
    };

    int i;

    for (i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "%s%s", roots[i], name);

        FILE *f = fopen(path, "rb");

        if (f) return f;
    }

    return NULL;
}

static int load_png(Sprite *out, const char *name)
{
    FILE *fp = open_asset(name);

    if (!fp) return 0;

    png_structp png =
        png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png) {
        fclose(fp);
        return 0;
    }

    png_infop info = png_create_info_struct(png);

    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return 0;
    }

    unsigned char *raw = NULL;
    png_bytep *rows = NULL;

    if (setjmp(png_jmpbuf(png))) {
        free(raw);
        free(rows);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    png_uint_32 w, h;
    int depth, color;

    png_get_IHDR(
        png, info, &w, &h,
        &depth, &color, NULL, NULL, NULL
    );

    if (w != TEX || h != TEX)
        png_error(png, "Invalid sprite size");

    if (depth == 16)
        png_set_strip_16(png);

    if (color == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (color == PNG_COLOR_TYPE_GRAY && depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if (color == PNG_COLOR_TYPE_GRAY ||
        color == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    if (!(color & PNG_COLOR_MASK_ALPHA))
        png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER);

    png_read_update_info(png, info);

    png_size_t rowbytes = png_get_rowbytes(png, info);

    raw = malloc(rowbytes * h);
    rows = malloc(sizeof(png_bytep) * h);

    if (!raw || !rows)
        png_error(png, "Memory error");

    for (unsigned int y = 0; y < h; y++)
        rows[y] = raw + y * rowbytes;

    png_read_image(png, rows);

    out->data = memalign(16, TEX * TEX * 4);

    if (!out->data)
        png_error(png, "Memory error");

    for (unsigned int y = 0; y < h; y++) {
        unsigned char *s = rows[y];
        unsigned int *d = out->data + y * TEX;

        for (unsigned int x = 0; x < w; x++) {
            unsigned char r = s[x * 4];
            unsigned char g = s[x * 4 + 1];
            unsigned char b = s[x * 4 + 2];
            unsigned char a = s[x * 4 + 3];

            d[x] = ((unsigned int)a << 24) |
                   ((unsigned int)b << 16) |
                   ((unsigned int)g << 8) | r;
        }
    }

    out->loaded = 1;

    free(raw);
    free(rows);

    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    return 1;
}

static void load_sprites(void)
{
    memset(sprites, 0, sizeof(sprites));

    for (int a = 0; a < ANIM_COUNT; a++) {
        for (int f = 0; f < frames[a]; f++) {
            load_png(&sprites[a][f], files[a][f]);
        }
    }
}

static void draw_sprite(Sprite *s, float x, float y, int flip)
{
    if (!s->loaded) return;

    sceGuEnable(GU_TEXTURE_2D);

    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, TEX, TEX, TEX, s->data);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);

    sceGuColor(0xffffffff);

    Vertex *v = sceGuGetMemory(2 * sizeof(Vertex));

    v[0] = (Vertex) {
        flip ? 128.0f : 0.0f,
        0, x, y, 0
    };

    v[1] = (Vertex) {
        flip ? 0.0f : 128.0f,
        128, x + PLAYER_W, y + PLAYER_H, 0
    };

    sceGuDrawArray(
        GU_SPRITES,
        GU_TEXTURE_32BITF |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2, 0, v
    );

    sceGuDisable(GU_TEXTURE_2D);
}

int main(void)
{
    float px = 100;
    float py = FLOOR_Y - PLAYER_H;
    float vx = 0;
    float vy = 0;

    float anim_time = 0;
    float action_time = 0;

    int grounded = 1;
    int facing = 1;
    int anim = IDLE;
    int frame = 0;
    int old_buttons = 0;

    unsigned int previous_time;
    SceCtrlData pad;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    sceGuInit();

    sceGuStart(GU_DIRECT, list);

    sceGuDrawBuffer(GU_PSM_8888, (void *)0, STRIDE);
    sceGuDispBuffer(SW, SH, (void *)0x88000, STRIDE);
    sceGuDepthBuffer((void *)0x110000, STRIDE);

    sceGuOffset(2048 - SW / 2, 2048 - SH / 2);
    sceGuViewport(2048, 2048, SW, SH);
    sceGuScissor(0, 0, SW, SH);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);

    sceGuClearColor(0xff21172e);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    load_sprites();

    previous_time = sceKernelGetSystemTimeLow();

    while (1) {
        unsigned int now = sceKernelGetSystemTimeLow();

        float dt = (now - previous_time) / 1000000.0f;

        previous_time = now;

        if (dt > 0.05f) dt = 0.05f;

        sceCtrlReadBufferPositive(&pad, 1);

        vx = 0;

        if (pad.Buttons & PSP_CTRL_LEFT) {
            vx = -110;
            facing = -1;
        }

        if (pad.Buttons & PSP_CTRL_RIGHT) {
            vx = 110;
            facing = 1;
        }

        if ((pad.Buttons & PSP_CTRL_RTRIGGER) && vx != 0)
            vx *= 1.6f;

        if ((pad.Buttons & PSP_CTRL_CROSS) &&
            !(old_buttons & PSP_CTRL_CROSS) && grounded) {
            vy = -350;
            grounded = 0;
        }

        if ((pad.Buttons & PSP_CTRL_SQUARE) &&
            !(old_buttons & PSP_CTRL_SQUARE)) {
            action_time = 0.28f;
            anim = ATTACK;
            frame = 0;
        }

        if ((pad.Buttons & PSP_CTRL_CIRCLE) &&
            !(old_buttons & PSP_CTRL_CIRCLE)) {
            action_time = 0.4f;
            anim = MAGIC;
            frame = 0;
        }

        old_buttons = pad.Buttons;

        px += vx * dt;

        if (px < 0) px = 0;

        if (!grounded) {
            vy += 850 * dt;
            py += vy * dt;

            if (py >= FLOOR_Y - PLAYER_H) {
                py = FLOOR_Y - PLAYER_H;
                vy = 0;
                grounded = 1;
                action_time = 0.1f;
                anim = LAND;
                frame = 0;
            }
        }

        if (action_time > 0)
            action_time -= dt;

        int next = IDLE;

        if (action_time > 0 &&
            (anim == ATTACK || anim == MAGIC || anim == LAND))
            next = anim;
        else if (!grounded)
            next = vy < 0 ? JUMP : FALL;
        else if (vx != 0)
            next = (pad.Buttons & PSP_CTRL_RTRIGGER) ? RUN : WALK;

        if (next != anim) {
            anim = next;
            frame = 0;
            anim_time = 0;
        }

        anim_time += dt;

        if (anim_time > 0.14f) {
            anim_time = 0;
            frame = (frame + 1) % frames[anim];
        }

        sceGuStart(GU_DIRECT, list);

        sceGuClearColor(0xff21172e);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        sceGuDisable(GU_TEXTURE_2D);

        sceGuColor(0xff47304f);

        Vertex *ground = sceGuGetMemory(2 * sizeof(Vertex));

        ground[0] = (Vertex){0, 0, 0, FLOOR_Y, 0};
        ground[1] = (Vertex){0, 0, SW, SH, 0};

        sceGuDrawArray(
            GU_SPRITES,
            GU_VERTEX_32BITF | GU_TRANSFORM_2D,
            2, 0, ground
        );

        Sprite *current = &sprites[anim][frame];

        if (!current->loaded)
            current = &sprites[IDLE][0];

        draw_sprite(current, px, py, facing < 0);

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    return 0;
}
