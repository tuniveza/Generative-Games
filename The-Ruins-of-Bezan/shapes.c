#include "shapes.h"
#include "meshgen.h"

#include <string.h>

/* up to 6 materials per shape, one builder each */
typedef struct {
    MeshBuilder mb[6];
    Material mat[6];
    int n;
} Kit;

static int kit_add(Kit *k, const Material *m)
{
    mb_init(&k->mb[k->n]);
    k->mat[k->n] = *m;
    return k->n++;
}

static int kit_color(Kit *k, float r, float g, float b, float rough, float metal, const float glow[3])
{
    Material m;
    material_color(&m, r, g, b, rough, metal);
    if (glow)
        glm_vec3_copy((float *)glow, m.emissive);
    return kit_add(k, &m);
}

static void kit_done(Kit *k, Model *m)
{
    model_from_builders(m, k->mb, k->mat, k->n);
    for (int i = 0; i < k->n; i++)
        mb_free(&k->mb[i]);
}

static void ball(MeshBuilder *mb, vec3 c, vec3 r, int seg)
{
    mat4 xf;
    glm_translate_make(xf, c);
    glm_scale(xf, r);
    mb_capsule(mb, xf, 1.0f, 0.0f, seg);
}

static void box(MeshBuilder *mb, vec3 c, vec3 half)
{
    mat4 xf;
    glm_translate_make(xf, c);
    mb_box(mb, xf, half, 1.0f);
}

static void cyl(MeshBuilder *mb, vec3 base, float r0, float r1, float h, int seg)
{
    mat4 xf;
    glm_translate_make(xf, base);
    mb_cylinder(mb, xf, r0, r1, h, seg, 1.0f);
}

void shape_fish(Model *m, vec3 back, vec3 belly, float length, float fat, vec3 glow)
{
    Kit k = {0};
    const float none[3] = { 0, 0, 0 };
    int body = kit_color(&k, back[0], back[1], back[2], 0.3f, 0.15f, glow ? glow : none);
    int under = kit_color(&k, belly[0], belly[1], belly[2], 0.3f, 0.1f, NULL);
    int eye = kit_color(&k, 0.01f, 0.01f, 0.01f, 0.1f, 0.0f, NULL);
    int fin = kit_color(&k, back[0] * 0.8f, back[1] * 0.8f, back[2] * 0.8f, 0.5f, 0.0f, NULL);
    float L = length, H = length * fat, W = H * 0.55f;
    /* a darker back over a pale belly */
    ball(&k.mb[body], (vec3){0, H * 0.08f, 0}, (vec3){W * 0.5f, H * 0.46f, L * 0.42f}, 16);
    ball(&k.mb[under], (vec3){0, -H * 0.1f, L * 0.02f}, (vec3){W * 0.46f, H * 0.36f, L * 0.36f}, 14);
    for (int s = -1; s <= 1; s += 2)
        ball(&k.mb[eye], (vec3){s * W * 0.36f, H * 0.12f, L * 0.3f}, (vec3){W * 0.1f, W * 0.1f, W * 0.1f}, 8);
    /* tail fin, dorsal fin */
    mat4 xf;
    glm_translate_make(xf, (vec3){0, 0, -L * 0.5f});
    glm_rotate_x(xf, GLM_PI_2f, xf);
    mb_wedge(&k.mb[fin], xf, (vec3){H * 0.42f, L * 0.13f, 0.004f + L * 0.01f}, 1.0f);
    glm_translate_make(xf, (vec3){0, H * 0.52f, -L * 0.02f});
    glm_rotate_y(xf, GLM_PI_2f, xf);
    mb_wedge(&k.mb[fin], xf, (vec3){L * 0.18f, H * 0.14f, 0.003f + L * 0.008f}, 1.0f);
    if (glow) {     /* a lantern fish's lure on a stalk */
        int bulb = kit_color(&k, 0.9f, 0.95f, 0.7f, 0.2f, 0.0f, (float[3]){ glow[0] * 3, glow[1] * 3, glow[2] * 3 });
        ball(&k.mb[bulb], (vec3){0, H * 0.75f, L * 0.48f}, (vec3){W * 0.18f, W * 0.18f, W * 0.18f}, 8);
    }
    kit_done(&k, m);
}

void shape_scallop(Model *m, vec3 color, vec3 glow)
{
    Kit k = {0};
    int shell = kit_color(&k, color[0], color[1], color[2], 0.35f, 0.0f, glow);
    int rib = kit_color(&k, color[0] * 0.8f, color[1] * 0.8f, color[2] * 0.8f, 0.35f, 0.0f, glow);
    /* a half disc of ribs fanning from the hinge */
    ball(&k.mb[shell], (vec3){0, 0.008f, 0.012f}, (vec3){0.04f, 0.01f, 0.034f}, 14);
    for (int i = 0; i < 9; i++) {
        float a = (i - 4) * 0.3f;
        mat4 xf;
        glm_translate_make(xf, (vec3){sinf(a) * 0.02f, 0.012f, -0.012f + cosf(a) * 0.02f});
        glm_rotate_y(xf, a, xf);
        mb_box(&k.mb[rib], xf, (vec3){0.0035f, 0.006f, 0.022f}, 1.0f);
    }
    box(&k.mb[rib], (vec3){0, 0.006f, -0.018f}, (vec3){0.014f, 0.005f, 0.006f});   /* the hinge's ears */
    kit_done(&k, m);
}

void shape_bell(Model *m, float height, vec3 color)
{
    Kit k = {0};
    int bronze = kit_color(&k, color[0], color[1], color[2], 0.28f, 1.0f, NULL);
    int dark = kit_color(&k, 0.08f, 0.06f, 0.04f, 0.5f, 1.0f, NULL);
    float h = height;
    /* hanging down from the origin: crown loop, shoulder, waist, flared lip */
    cyl(&k.mb[bronze], (vec3){0, -h * 0.35f, 0}, h * 0.28f, h * 0.2f, h * 0.3f, 24);
    cyl(&k.mb[bronze], (vec3){0, -h * 0.8f, 0}, h * 0.42f, h * 0.28f, h * 0.45f, 24);
    cyl(&k.mb[bronze], (vec3){0, -h * 0.95f, 0}, h * 0.5f, h * 0.42f, h * 0.15f, 24);
    cyl(&k.mb[dark], (vec3){0, -h * 0.9f, 0}, h * 0.46f, h * 0.46f, h * 0.02f, 24);
    ball(&k.mb[bronze], (vec3){0, -h * 0.05f, 0}, (vec3){h * 0.2f, h * 0.08f, h * 0.2f}, 12);
    ball(&k.mb[dark], (vec3){0, -h * 0.9f, 0}, (vec3){h * 0.1f, h * 0.1f, h * 0.1f}, 10);     /* the clapper */
    kit_done(&k, m);
}

void shape_crystal(Model *m, float height, vec3 color, vec3 glow)
{
    Kit k = {0};
    int c = kit_color(&k, color[0], color[1], color[2], 0.1f, 0.0f, glow);
    cyl(&k.mb[c], (vec3){0, 0, 0}, 0.0f, height * 0.28f, height * 0.35f, 6);
    cyl(&k.mb[c], (vec3){0, height * 0.35f, 0}, height * 0.28f, 0.0f, height * 0.65f, 6);
    kit_done(&k, m);
}

void shape_rod(Model *m, int tier)
{
    static const float blank[5][3] = { { 0 }, { 0.35f, 0.22f, 0.12f }, { 0.75f, 0.55f, 0.25f },
                                       { 0.85f, 0.38f, 0.42f }, { 0.05f, 0.2f, 0.22f } };
    Kit k = {0};
    int cork = kit_color(&k, 0.55f, 0.4f, 0.25f, 0.9f, 0.0f, NULL);
    int shaft = kit_color(&k, blank[tier][0], blank[tier][1], blank[tier][2], tier == 2 ? 0.3f : 0.5f, tier == 2 ? 1.0f : 0.0f, NULL);
    int metal = kit_color(&k, 0.6f, 0.6f, 0.62f, 0.3f, 1.0f, NULL);
    cyl(&k.mb[cork], (vec3){0, -0.08f, 0}, 0.018f, 0.016f, 0.34f, 10);
    cyl(&k.mb[shaft], (vec3){0, 0.26f, 0}, 0.012f, 0.004f, 1.55f, 8);
    /* the reel hangs under the handle */
    mat4 xf;
    glm_translate_make(xf, (vec3){-0.03f, 0.05f, 0.0f});
    glm_rotate_z(xf, GLM_PI_2f, xf);
    mb_cylinder(&k.mb[metal], xf, 0.035f, 0.035f, 0.025f, 14, 1.0f);
    box(&k.mb[metal], (vec3){-0.018f, 0.05f, 0}, (vec3){0.012f, 0.006f, 0.004f});
    for (int g = 0; g < 4; g++)
        box(&k.mb[metal], (vec3){-0.012f, 0.45f + g * 0.35f, 0}, (vec3){0.008f, 0.004f, 0.004f});
    if (tier == 4) {        /* leviathan runes glowing along it */
        int rune = kit_color(&k, 0.1f, 0.3f, 0.3f, 0.3f, 0.0f, (float[3]){ 0.3f, 2.0f, 2.2f });
        for (int g = 0; g < 6; g++)
            box(&k.mb[rune], (vec3){0.0f, 0.4f + g * 0.2f, 0.009f}, (vec3){0.004f, 0.02f, 0.002f});
    }
    kit_done(&k, m);
}

void shape_bobber(Model *m)
{
    Kit k = {0};
    int red = kit_color(&k, 0.8f, 0.06f, 0.04f, 0.3f, 0.0f, NULL);
    int white = kit_color(&k, 0.9f, 0.9f, 0.88f, 0.3f, 0.0f, NULL);
    ball(&k.mb[red], (vec3){0, 0.025f, 0}, (vec3){0.035f, 0.03f, 0.035f}, 12);
    ball(&k.mb[white], (vec3){0, -0.005f, 0}, (vec3){0.034f, 0.028f, 0.034f}, 12);
    cyl(&k.mb[red], (vec3){0, 0.04f, 0}, 0.005f, 0.004f, 0.05f, 6);
    kit_done(&k, m);
}

void shape_bottle(Model *m)
{
    Kit k = {0};
    Material glass;
    material_color(&glass, 0.3f, 0.6f, 0.45f, 0.05f, 0.0f);
    glass.base_color[3] = 0.45f;
    glass.alpha_mode = ALPHA_BLEND;
    int paper = kit_color(&k, 0.85f, 0.8f, 0.65f, 0.9f, 0.0f, NULL);
    int cork = kit_color(&k, 0.5f, 0.35f, 0.2f, 0.9f, 0.0f, NULL);
    int g = kit_add(&k, &glass);
    cyl(&k.mb[paper], (vec3){0, 0.03f, 0}, 0.018f, 0.018f, 0.12f, 8);
    cyl(&k.mb[g], (vec3){0, 0, 0}, 0.04f, 0.04f, 0.16f, 14);
    cyl(&k.mb[g], (vec3){0, 0.16f, 0}, 0.04f, 0.015f, 0.04f, 14);
    cyl(&k.mb[g], (vec3){0, 0.2f, 0}, 0.015f, 0.015f, 0.05f, 10);
    cyl(&k.mb[cork], (vec3){0, 0.24f, 0}, 0.016f, 0.017f, 0.025f, 8);
    kit_done(&k, m);
}

void shape_pearl(Model *m, float radius, vec3 color, vec3 glow)
{
    Kit k = {0};
    int p = kit_color(&k, color[0], color[1], color[2], 0.12f, 0.1f, glow);
    ball(&k.mb[p], (vec3){0, 0, 0}, (vec3){radius, radius, radius}, 16);
    kit_done(&k, m);
}

void shape_harpoon(Model *m)
{
    Kit k = {0};
    int wood = kit_color(&k, 0.4f, 0.26f, 0.14f, 0.7f, 0.0f, NULL);
    int iron = kit_color(&k, 0.2f, 0.2f, 0.22f, 0.4f, 1.0f, NULL);
    int rope = kit_color(&k, 0.5f, 0.42f, 0.28f, 0.95f, 0.0f, NULL);
    cyl(&k.mb[wood], (vec3){0, -0.3f, 0}, 0.018f, 0.016f, 1.45f, 8);
    cyl(&k.mb[iron], (vec3){0, 1.15f, 0}, 0.012f, 0.0f, 0.3f, 8);
    for (int s = -1; s <= 1; s += 2) {      /* barbs */
        mat4 xf;
        glm_translate_make(xf, (vec3){s * 0.02f, 1.2f, 0});
        glm_rotate_z(xf, s * 0.5f, xf);
        mb_box(&k.mb[iron], xf, (vec3){0.004f, 0.05f, 0.004f}, 1.0f);
    }
    cyl(&k.mb[rope], (vec3){0, 0.1f, 0}, 0.022f, 0.022f, 0.12f, 8);
    kit_done(&k, m);
}

void shape_trident(Model *m)
{
    Kit k = {0};
    int shaft = kit_color(&k, 0.05f, 0.25f, 0.26f, 0.3f, 0.6f, NULL);
    int gold = kit_color(&k, 0.9f, 0.68f, 0.25f, 0.2f, 1.0f, (float[3]){ 0.2f, 0.5f, 0.5f });
    cyl(&k.mb[shaft], (vec3){0, -0.35f, 0}, 0.017f, 0.015f, 1.5f, 10);
    box(&k.mb[gold], (vec3){0, 1.15f, 0}, (vec3){0.12f, 0.018f, 0.018f});
    for (int p = -1; p <= 1; p++) {
        cyl(&k.mb[gold], (vec3){p * 0.11f, 1.15f, 0}, 0.012f, 0.004f, p ? 0.28f : 0.36f, 8);
        mat4 xf;
        glm_translate_make(xf, (vec3){p * 0.11f + 0.015f, 1.36f + (p ? 0.0f : 0.07f), 0});
        glm_rotate_z(xf, 0.6f, xf);
        mb_box(&k.mb[gold], xf, (vec3){0.004f, 0.03f, 0.004f}, 1.0f);
    }
    ball(&k.mb[gold], (vec3){0, 1.1f, 0}, (vec3){0.035f, 0.035f, 0.035f}, 10);
    kit_done(&k, m);
}

void shape_ember_blade(Model *m)
{
    Kit k = {0};
    int grip = kit_color(&k, 0.12f, 0.06f, 0.04f, 0.8f, 0.0f, NULL);
    int guard = kit_color(&k, 0.15f, 0.12f, 0.12f, 0.35f, 1.0f, NULL);
    int blade = kit_color(&k, 0.2f, 0.05f, 0.02f, 0.4f, 0.3f, (float[3]){ 4.0f, 1.0f, 0.12f });
    cyl(&k.mb[grip], (vec3){0, -0.12f, 0}, 0.016f, 0.016f, 0.22f, 8);
    box(&k.mb[guard], (vec3){0, 0.11f, 0}, (vec3){0.09f, 0.015f, 0.02f});
    ball(&k.mb[guard], (vec3){0, -0.13f, 0}, (vec3){0.025f, 0.025f, 0.025f}, 8);
    mat4 xf;
    glm_translate_make(xf, (vec3){0, 0.5f, 0});
    mb_box(&k.mb[blade], xf, (vec3){0.028f, 0.37f, 0.004f}, 1.0f);
    glm_translate_make(xf, (vec3){0, 0.93f, 0});
    mb_wedge(&k.mb[blade], xf, (vec3){0.028f, 0.06f, 0.004f}, 1.0f);
    kit_done(&k, m);
}

void shape_diving_helm(Model *m)
{
    Kit k = {0};
    int brass = kit_color(&k, 0.78f, 0.55f, 0.24f, 0.25f, 1.0f, NULL);
    int copper = kit_color(&k, 0.6f, 0.3f, 0.15f, 0.35f, 1.0f, NULL);
    Material glass;
    material_color(&glass, 0.5f, 0.75f, 0.8f, 0.05f, 0.0f);
    glass.base_color[3] = 0.4f;
    glass.alpha_mode = ALPHA_BLEND;
    int g = kit_add(&k, &glass);
    ball(&k.mb[brass], (vec3){0, 0.2f, 0}, (vec3){0.19f, 0.2f, 0.19f}, 20);
    cyl(&k.mb[copper], (vec3){0, 0.0f, 0}, 0.24f, 0.2f, 0.06f, 20);            /* the collar */
    /* portholes: front and both sides, each a glass disc in a brass ring */
    for (int i = 0; i < 3; i++) {
        float a = (i - 1) * GLM_PI_2f;
        mat4 xf;
        glm_translate_make(xf, (vec3){sinf(a) * 0.18f, 0.21f, cosf(a) * 0.18f});
        glm_rotate_y(xf, a, xf);
        glm_rotate_x(xf, GLM_PI_2f, xf);
        mb_cylinder(&k.mb[copper], xf, 0.085f, 0.085f, 0.03f, 16, 1.0f);
        glm_translate(xf, (vec3){0, 0.005f, 0});
        mb_cylinder(&k.mb[g], xf, 0.07f, 0.07f, 0.03f, 16, 1.0f);
    }
    for (int b = 0; b < 8; b++)
        ball(&k.mb[copper], (vec3){cosf(b * 0.785f) * 0.22f, 0.04f, sinf(b * 0.785f) * 0.22f}, (vec3){0.015f, 0.015f, 0.015f}, 6);
    kit_done(&k, m);
}

void shape_worm(Model *m, vec3 color, vec3 glow)
{
    Kit k = {0};
    int w = kit_color(&k, color[0], color[1], color[2], 0.4f, 0.0f, glow);
    for (int i = 0; i < 7; i++) {
        float a = i * 0.6f;
        ball(&k.mb[w], (vec3){sinf(a) * 0.012f, 0.004f, i * 0.008f - 0.024f}, (vec3){0.005f, 0.005f, 0.006f}, 8);
    }
    kit_done(&k, m);
}

void shape_shrimp(Model *m)
{
    Kit k = {0};
    int s = kit_color(&k, 0.9f, 0.45f, 0.3f, 0.35f, 0.0f, NULL);
    for (int i = 0; i < 6; i++) {
        float a = i * 0.5f;
        float r = 0.009f - i * 0.001f;
        ball(&k.mb[s], (vec3){0, cosf(a) * 0.015f, sinf(a) * 0.015f}, (vec3){r, r, r * 1.2f}, 8);
    }
    kit_done(&k, m);
}

void shape_lure(Model *m)
{
    Kit k = {0};
    int spoon = kit_color(&k, 0.9f, 0.4f, 0.1f, 0.2f, 1.0f, (float[3]){ 3.5f, 1.0f, 0.1f });
    int hook = kit_color(&k, 0.5f, 0.5f, 0.52f, 0.3f, 1.0f, NULL);
    ball(&k.mb[spoon], (vec3){0, 0, 0}, (vec3){0.014f, 0.004f, 0.028f}, 10);
    cyl(&k.mb[hook], (vec3){0, 0, -0.03f}, 0.002f, 0.002f, 0.02f, 6);
    kit_done(&k, m);
}

void shape_seaweed(Model *m)
{
    Kit k = {0};
    int w = kit_color(&k, 0.12f, 0.32f, 0.1f, 0.5f, 0.0f, NULL);
    for (int s = 0; s < 4; s++) {
        float bend = 0.0f;
        vec3 p = { s * 0.02f - 0.03f, 0, 0 };
        for (int i = 0; i < 5; i++) {
            mat4 xf;
            glm_translate_make(xf, p);
            glm_rotate_z(xf, bend, xf);
            mb_box(&k.mb[w], xf, (vec3){0.012f, 0.025f, 0.002f}, 1.0f);
            p[1] += 0.045f * cosf(bend);
            p[0] -= 0.045f * sinf(bend);
            bend += 0.25f * (s % 2 ? 1 : -1);
        }
    }
    kit_done(&k, m);
}

void shape_charm(Model *m, vec3 color, vec3 glow)
{
    Kit k = {0};
    int c = kit_color(&k, color[0], color[1], color[2], 0.15f, 0.3f, glow);
    int cord = kit_color(&k, 0.3f, 0.2f, 0.1f, 0.9f, 0.0f, NULL);
    ball(&k.mb[c], (vec3){0, 0, 0}, (vec3){0.03f, 0.04f, 0.008f}, 12);
    cyl(&k.mb[cord], (vec3){0, 0.035f, 0}, 0.004f, 0.004f, 0.02f, 6);
    kit_done(&k, m);
}

void shape_boat(Model *m)
{
    Kit k = {0};
    Material planks;
    material_from_dir(&planks, "assets/textures/weathered_planks");
    int wood = kit_add(&k, &planks);
    int paint = kit_color(&k, 0.1f, 0.42f, 0.4f, 0.6f, 0.0f, NULL);
    int dark = kit_color(&k, 0.18f, 0.12f, 0.07f, 0.8f, 0.0f, NULL);
    const float L = 3.6f, beam = 1.35f, depth = 0.6f;
    /* clinker planks: three strakes a side, each a run of short boards that follow the
     * hull's curve (narrow at the bow, fuller aft) and flare outward as they rise */
    int segs = 10;
    for (int strake = 0; strake < 3; strake++) {
        float y = 0.1f + strake * (depth - 0.1f) / 3.0f;
        float flare = 0.25f + strake * 0.22f;
        for (int i = 0; i < segs; i++) {
            float z0 = -L * 0.5f + L * i / segs, z1 = z0 + L / segs;
            for (int side = -1; side <= 1; side += 2) {
                float wa, wb;
                {
                    float t0 = (z0 / (L * 0.5f)), t1 = (z1 / (L * 0.5f));
                    float s0 = t0 > 0 ? t0 * t0 * 1.25f : t0 * t0 * 0.45f;
                    float s1 = t1 > 0 ? t1 * t1 * 1.25f : t1 * t1 * 0.45f;
                    float grow = 0.55f + strake * 0.22f;
                    wa = beam * 0.5f * grow * fmaxf(0.03f, 1.0f - s0);
                    wb = beam * 0.5f * grow * fmaxf(0.03f, 1.0f - s1);
                }
                float sheer = 0.08f * (z0 + z1 > 0 ? ((z0 + z1) / L) * ((z0 + z1) / L) * 4.0f : 0.3f * ((z0 + z1) / L) * ((z0 + z1) / L) * 4.0f);
                vec3 c = { side * (wa + wb) * 0.5f, y + sheer, (z0 + z1) * 0.5f };
                float len = sqrtf((z1 - z0) * (z1 - z0) + (wb - wa) * (wb - wa));
                mat4 xf;
                glm_translate_make(xf, c);
                glm_rotate_y(xf, atan2f(side * (wb - wa), z1 - z0), xf);
                glm_rotate_z(xf, -side * flare, xf);
                mb_box(&k.mb[strake == 2 ? paint : wood], xf, (vec3){0.02f, (depth - 0.1f) / 6.0f + 0.02f, len * 0.5f + 0.01f}, 1.0f);
            }
        }
    }
    /* keel and floor, stern board, bow post, two seats */
    box(&k.mb[dark], (vec3){0, 0.03f, 0}, (vec3){0.05f, 0.04f, L * 0.47f});
    box(&k.mb[wood], (vec3){0, 0.1f, -0.1f}, (vec3){0.32f, 0.02f, L * 0.36f});
    box(&k.mb[paint], (vec3){0, 0.36f, -L * 0.5f + 0.02f}, (vec3){0.42f, 0.24f, 0.03f});
    box(&k.mb[dark], (vec3){0, 0.36f, L * 0.5f - 0.02f}, (vec3){0.04f, 0.3f, 0.05f});
    box(&k.mb[wood], (vec3){0, 0.42f, -0.9f}, (vec3){0.5f, 0.025f, 0.14f});
    box(&k.mb[wood], (vec3){0, 0.42f, 0.25f}, (vec3){0.55f, 0.025f, 0.14f});
    /* oarlocks */
    for (int s = -1; s <= 1; s += 2)
        box(&k.mb[dark], (vec3){s * 0.62f, 0.62f, 0.25f}, (vec3){0.02f, 0.05f, 0.02f});
    kit_done(&k, m);
}
