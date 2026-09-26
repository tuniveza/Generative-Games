#include "items.h"
#include "meshgen.h"

#include <stdio.h>
#include <string.h>

#define PH "assets/models/"

ItemDef ITEMS[ITEM_COUNT] = {
    [ITEM_TORCH] = { "Torch", "Pitch-soaked rags on a stick.\nT: raise a light in your off hand.",
                     NULL, KIND_GADGET, SWING_PUNCH, 0, 0, 0.5f, 0, 0, true, 0.4f, 0.2f },

    [ITEM_DAGGER] = { "Ornate Dagger", "Quick and nasty. Stabs twice before\nanything else swings once.",
                      PH "ornate_medieval_dagger/ornate_medieval_dagger.gltf",
                      KIND_WEAPON, SWING_STAB, 14, 1.9f, 0.38f, 1.5f, 0, false, 0.28f, 0.2f },
    [ITEM_AXE] = { "Wooden Axe", "A woodcutter's axe. Fine for\nsplitting logs, and other things.",
                   PH "wooden_axe/wooden_axe.gltf",
                   KIND_WEAPON, SWING_SLASH, 24, 2.2f, 0.7f, 3.0f, 0, false, 0.46f, 0.1f },
    [ITEM_MACE] = { "Ornate Mace", "Flanged steel on a gilded haft.\nArmor is merely a suggestion.",
                    PH "ornate_medieval_mace/ornate_medieval_mace.gltf",
                    KIND_WEAPON, SWING_SLASH, 30, 2.2f, 0.8f, 4.0f, 0, false, 0.46f, 0.12f },
    [ITEM_WARHAMMER] = { "Ornate War Hammer", "Heavy, balanced, and very,\nvery persuasive.",
                         PH "ornate_war_hammer/ornate_war_hammer.gltf",
                         KIND_WEAPON, SWING_OVERHEAD, 38, 2.3f, 0.95f, 5.0f, 0, false, 0.5f, 0.1f },
    [ITEM_SLEDGEHAMMER] = { "Sledgehammer", "Slow. Enormous. Sends rats\ninto the next valley.",
                            PH "sledgehammer_01/sledgehammer_01.gltf",
                            KIND_WEAPON, SWING_OVERHEAD, 70, 2.5f, 1.45f, 12.0f, 0, false, 0.62f, 0.08f },
    [ITEM_SHIELD] = { "Kite Shield", "Hold right mouse to block most\nof a bite. Click to bash.",
                      PH "kite_shield/kite_shield.gltf",
                      KIND_WEAPON, SWING_BASH, 8, 1.8f, 0.6f, 9.0f, 0, false, 0.55f, 0.5f },

    [ITEM_GRENADE] = { "Stick Grenade", "Where did THIS come from?\nClick to throw. Stand well back.",
                       PH "stick_grenade/stick_grenade.gltf",
                       KIND_THROWN, SWING_PUNCH, 90, 5.0f, 0.9f, 14.0f, 0, true, 0.24f, 0.15f },

    [ITEM_SWEET_POTATO] = { "Sweet Potato", "Raw, cold, and somehow\ndelicious. Heals 20.",
                            PH "sweet_potato/sweet_potato.gltf",
                            KIND_FOOD, SWING_PUNCH, 0, 0, 1.0f, 0, 20, true, 0.12f, 0.5f },
    [ITEM_CHEESE] = { "Wheel of Cheese", "In a little wooden box.\nHeals 35.",
                      PH "CheeseBox_01/CheeseBox_01.gltf",
                      KIND_FOOD, SWING_PUNCH, 0, 0, 1.0f, 0, 35, true, 0.2f, 0.5f },
    [ITEM_GOBLET] = { "Brass Goblet", "Still holds a splash of very\nold wine. Heals 50.",
                      PH "brass_goblets/brass_goblets.gltf",
                      KIND_FOOD, SWING_PUNCH, 0, 0, 1.0f, 0, 50, true, 0.16f, 0.5f },
    [ITEM_AVOCADO] = { "Sacred Avocado", "The relic of the shrine. Eating it\nheals fully and raises max health.",
                       "assets/avocado/Avocado.gltf",
                       KIND_FOOD, SWING_PUNCH, 0, 0, 1.2f, 0, 999, false, 0.14f, 0.5f },

    [ITEM_RUBBER_DUCK] = { "Rubber Duck", "SQUEAK. Every creature nearby\nruns for its life.",
                           PH "rubber_duck_toy/rubber_duck_toy.gltf",
                           KIND_GADGET, SWING_PUNCH, 0, 0, 2.0f, 0, 0, false, 0.14f, 0.5f },
    [ITEM_POCKET_WATCH] = { "Pocket Watch", "Wind it and the world crawls\nfor six seconds. You don't.",
                            PH "pocket_watch/pocket_watch.gltf",
                            KIND_GADGET, SWING_PUNCH, 0, 0, 14.0f, 0, 0, false, 0.08f, 0.5f },
    [ITEM_BOOMBOX] = { "Boombox", "Nobody can resist the groove.\nCreatures nearby dance helplessly.",
                       PH "boombox/boombox.gltf",
                       KIND_GADGET, SWING_PUNCH, 0, 0, 16.0f, 0, 0, false, 0.4f, 0.5f },
    [ITEM_COMPASS] = { "Sea-dog's Compass", "Its needle ignores north and\npoints at unopened treasure.",
                       PH "seadogs_compass/seadogs_compass.gltf",
                       KIND_GADGET, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.1f, 0.5f },
    [ITEM_BINOCULARS] = { "Binoculars", "Hold the mouse button to\nlook far away.",
                          PH "vintage_binocular/vintage_binocular.gltf",
                          KIND_GADGET, SWING_PUNCH, 0, 0, 0.2f, 0, 0, false, 0.17f, 0.5f },
    [ITEM_GAS_MASK] = { "Old Gas Mask", "Click to wear. Creatures can't\nsmell you: they notice you later.",
                        PH "old_gas_mask/old_gas_mask.gltf",
                        KIND_GADGET, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.28f, 0.5f },
    [ITEM_SPECTACLES] = { "Round Spectacles", "Click to wear. Reveals how\nhurt every creature is.",
                          PH "round_spectacles/round_spectacles.gltf",
                          KIND_GADGET, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.14f, 0.5f },

    [ITEM_KEY] = { "Rusty Key", "Heavy iron. It must open\nsomething important.",
                   NULL, KIND_KEY, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.16f, 0.5f },

    [ITEM_LANTERN] = { "Lantern", "A steady light that doesn't mind the rain.\nT: carry it. G: set it down.",
                       PH "wooden_lantern_01/wooden_lantern_01.gltf",
                       KIND_GADGET, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.3f, 0.5f },
    [ITEM_MANA_POTION] = { "Blue Vial", "Tastes of thunderstorms.\nRestores 60 mana.",
                           NULL, KIND_FOOD, SWING_PUNCH, 0, 0, 1.0f, 0, 0, true, 0.12f, 0.5f, .mana = 60 },

    [ITEM_TOME_FIREBALL] = { "Tome of Fireball", "Hurls a ball of fire that bursts\non whatever it hits. 20 mana.",
        NULL, KIND_SPELL, SWING_PUNCH, 45, 3.5f, 0.8f, 6.0f, 0, false, 0.16f, 0.5f,
        .mana = 20, .spell = SPELL_FIREBALL, .glow = { 6.0f, 2.2f, 0.4f } },
    [ITEM_TOME_FROST] = { "Tome of Frost", "A ring of ice around you. Everything\nnearby freezes solid. 30 mana.",
        NULL, KIND_SPELL, SWING_PUNCH, 20, 6.0f, 2.0f, 4.0f, 0, false, 0.16f, 0.5f,
        .mana = 30, .spell = SPELL_FROST, .glow = { 0.8f, 2.5f, 6.0f } },
    [ITEM_TOME_LIGHTNING] = { "Tome of Storms", "Lightning leaps to the nearest foe\nand onward to two more. 25 mana.",
        NULL, KIND_SPELL, SWING_PUNCH, 38, 22.0f, 1.2f, 3.0f, 0, false, 0.16f, 0.5f,
        .mana = 25, .spell = SPELL_LIGHTNING, .glow = { 3.0f, 3.5f, 8.0f } },
    [ITEM_TOME_HEAL] = { "Tome of Mending", "Warm light that closes wounds.\nHeals 45. 25 mana.",
        NULL, KIND_SPELL, SWING_PUNCH, 0, 0, 1.5f, 0, 45, false, 0.16f, 0.5f,
        .mana = 25, .spell = SPELL_HEAL, .glow = { 2.0f, 5.0f, 1.5f } },
    [ITEM_TOME_WISP] = { "Tome of the Wisp", "Summons a floating light that follows\nyou for a minute. 15 mana.",
        NULL, KIND_SPELL, SWING_PUNCH, 0, 0, 2.0f, 0, 0, false, 0.16f, 0.5f,
        .mana = 15, .spell = SPELL_WISP, .glow = { 4.0f, 4.5f, 6.0f } },
    [ITEM_TOME_BLINK] = { "Tome of Blinking", "Step through the air to where you're\nlooking, eight paces on. 15 mana.",
        NULL, KIND_SPELL, SWING_PUNCH, 0, 8.0f, 1.0f, 0, 0, false, 0.16f, 0.5f,
        .mana = 15, .spell = SPELL_BLINK, .glow = { 5.0f, 1.5f, 6.0f } },
};

static void build_torch(Model *m)
{
    /* wooden handle, iron collar, and a tarred rag head */
    MeshBuilder mbs[3];
    Material mats[3];
    for (int i = 0; i < 3; i++)
        mb_init(&mbs[i]);
    material_from_dir(&mats[0], "assets/textures/old_planks_02");
    material_color(&mats[1], 0.12f, 0.11f, 0.1f, 0.45f, 1.0f);
    material_color(&mats[2], 0.05f, 0.035f, 0.02f, 0.95f, 0.0f);

    mat4 xf = GLM_MAT4_IDENTITY_INIT;
    mb_cylinder(&mbs[0], xf, 0.022f, 0.028f, 0.5f, 10, 0.5f);
    glm_translate_make(xf, (vec3){0, 0.36f, 0});
    mb_cylinder(&mbs[1], xf, 0.034f, 0.034f, 0.03f, 12, 1.0f);
    glm_translate_make(xf, (vec3){0, 0.4f, 0});
    mb_cylinder(&mbs[2], xf, 0.04f, 0.035f, 0.13f, 12, 1.0f);

    model_from_builders(m, mbs, mats, 3);
    for (int i = 0; i < 3; i++)
        mb_free(&mbs[i]);
}

static void build_key(Model *m)
{
    MeshBuilder mb;
    mb_init(&mb);
    Material iron;
    material_color(&iron, 0.18f, 0.12f, 0.08f, 0.6f, 1.0f);

    /* ring bow, shaft, and bit, lying along +Y */
    for (int i = 0; i < 10; i++) {
        float a = i / 10.0f * GLM_PIf * 2.0f;
        mat4 xf;
        glm_translate_make(xf, (vec3){cosf(a) * 0.03f, 0.13f + sinf(a) * 0.03f, 0});
        glm_rotate(xf, a, (vec3){0, 0, 1});
        mb_box(&mb, xf, (vec3){0.006f, 0.011f, 0.006f}, 1.0f);
    }
    mat4 xf = GLM_MAT4_IDENTITY_INIT;
    mb_cylinder(&mb, xf, 0.007f, 0.007f, 0.1f, 8, 1.0f);
    glm_translate_make(xf, (vec3){0.015f, 0.012f, 0});
    mb_box(&mb, xf, (vec3){0.015f, 0.006f, 0.004f}, 1.0f);
    glm_translate_make(xf, (vec3){0.012f, 0.03f, 0});
    mb_box(&mb, xf, (vec3){0.012f, 0.005f, 0.004f}, 1.0f);

    model_from_builders(m, &mb, &iron, 1);
    mb_free(&mb);
}

/* a leather-bound book with a glowing rune on the cover */
static void build_tome(Model *m, vec3 cover, vec3 glow)
{
    MeshBuilder mbs[3];
    Material mats[3];
    for (int i = 0; i < 3; i++)
        mb_init(&mbs[i]);
    material_color(&mats[0], cover[0], cover[1], cover[2], 0.7f, 0.0f);     /* leather */
    material_color(&mats[1], 0.85f, 0.8f, 0.68f, 0.9f, 0.0f);                /* pages */
    material_color(&mats[2], 0.1f, 0.1f, 0.1f, 0.4f, 0.0f);                  /* rune */
    glm_vec3_copy(glow, mats[2].emissive);

    mat4 xf;
    glm_translate_make(xf, (vec3){0, 0.02f, 0});
    mb_box(&mbs[1], xf, (vec3){0.085f, 0.02f, 0.115f}, 1.0f);
    glm_translate_make(xf, (vec3){0, 0.043f, 0});
    mb_box(&mbs[0], xf, (vec3){0.095f, 0.005f, 0.125f}, 1.0f);
    glm_translate_make(xf, (vec3){0, -0.003f, 0});
    mb_box(&mbs[0], xf, (vec3){0.095f, 0.005f, 0.125f}, 1.0f);
    glm_translate_make(xf, (vec3){-0.092f, 0.02f, 0});
    mb_box(&mbs[0], xf, (vec3){0.008f, 0.026f, 0.125f}, 1.0f);
    /* the rune: a diamond and a bar */
    glm_translate_make(xf, (vec3){0.01f, 0.049f, 0});
    glm_rotate(xf, GLM_PI_4f, (vec3){0, 1, 0});
    mb_box(&mbs[2], xf, (vec3){0.03f, 0.002f, 0.03f}, 1.0f);
    glm_translate_make(xf, (vec3){0.01f, 0.049f, 0.075f});
    mb_box(&mbs[2], xf, (vec3){0.05f, 0.002f, 0.006f}, 1.0f);

    model_from_builders(m, mbs, mats, 3);
    for (int i = 0; i < 3; i++)
        mb_free(&mbs[i]);
}

/* a small glass vial of glowing blue */
static void build_vial(Model *m)
{
    MeshBuilder mbs[3];
    Material mats[3];
    for (int i = 0; i < 3; i++)
        mb_init(&mbs[i]);
    material_color(&mats[0], 0.05f, 0.2f, 0.9f, 0.1f, 0.0f);
    glm_vec3_copy((vec3){0.2f, 0.8f, 3.0f}, mats[0].emissive);
    material_color(&mats[1], 0.8f, 0.9f, 1.0f, 0.05f, 0.0f);
    mats[1].base_color[3] = 0.35f;
    mats[1].alpha_mode = ALPHA_BLEND;
    material_color(&mats[2], 0.35f, 0.2f, 0.1f, 0.9f, 0.0f);

    mat4 xf = GLM_MAT4_IDENTITY_INIT;
    mb_cylinder(&mbs[0], xf, 0.028f, 0.028f, 0.07f, 14, 1.0f);
    mb_cylinder(&mbs[1], xf, 0.032f, 0.032f, 0.09f, 14, 1.0f);
    glm_translate_make(xf, (vec3){0, 0.09f, 0});
    mb_cylinder(&mbs[1], xf, 0.012f, 0.012f, 0.03f, 10, 1.0f);
    glm_translate_make(xf, (vec3){0, 0.115f, 0});
    mb_cylinder(&mbs[2], xf, 0.014f, 0.012f, 0.02f, 10, 1.0f);

    model_from_builders(m, mbs, mats, 3);
    for (int i = 0; i < 3; i++)
        mb_free(&mbs[i]);
}

const Model *items_torch_model(void)
{
    return &ITEMS[ITEM_TORCH].model;
}

/* grip space: the fist is at the origin and the handle runs along +Y */
static void compute_hold(ItemDef *it)
{
    const Model *m = &it->model;
    vec3 ext, c;
    glm_vec3_sub((float *)m->max, (float *)m->min, ext);
    glm_vec3_center((float *)m->min, (float *)m->max, c);
    float s = it->hold_size / fmaxf(glm_vec3_max(ext), 1e-4f);

    glm_scale_make(it->hold, (vec3){s, s, s});
    if (it->kind == KIND_WEAPON || it->kind == KIND_THROWN || it == &ITEMS[ITEM_TORCH])
        glm_translate(it->hold, (vec3){-c[0], -(m->min[1] + ext[1] * it->grip), -c[2]});
    else
        glm_translate(it->hold, (vec3){-c[0], -c[1], -c[2]});
}

void items_load(void)
{
    for (int i = 1; i < ITEM_COUNT; i++) {
        ItemDef *it = &ITEMS[i];
        if (i == ITEM_TORCH) {
            build_torch(&it->model);
        } else if (i == ITEM_KEY) {
            build_key(&it->model);
        } else if (i == ITEM_MANA_POTION) {
            build_vial(&it->model);
        } else if (it->kind == KIND_SPELL) {
            vec3 cover;
            glm_vec3_scale(it->glow, 0.06f, cover);
            glm_vec3_adds(cover, 0.08f, cover);
            build_tome(&it->model, cover, it->glow);
        } else if (!model_load(&it->model, it->model_path)) {
            fprintf(stderr, "item %s: missing model\n", it->name);
            continue;
        }
        it->loaded = true;
    }

    /* a few models come with extras we don't want in hand */
    model_hide_node(&ITEMS[ITEM_DAGGER].model, "ornate_medieval_dagger_scabbard");
    model_hide_node(&ITEMS[ITEM_GOBLET].model, "brass_goblet_02");
    model_hide_node(&ITEMS[ITEM_GOBLET].model, "brass_goblet_03");

    for (int i = 1; i < ITEM_COUNT; i++)
        if (ITEMS[i].loaded)
            compute_hold(&ITEMS[i]);
}

void items_free(void)
{
    for (int i = 1; i < ITEM_COUNT; i++) {
        if (ITEMS[i].loaded)
            model_free(&ITEMS[i].model);
        glDeleteTextures(1, &ITEMS[i].icon);
    }
}

void items_make_icons(Renderer *r, GLuint model_prog)
{
    const int size = 192;
    FrameUniforms saved = r->frame;

    GLuint fbo, depth;
    glCreateFramebuffers(1, &fbo);
    glCreateRenderbuffers(1, &depth);
    glNamedRenderbufferStorage(depth, GL_DEPTH_COMPONENT24, size, size);
    glNamedFramebufferRenderbuffer(fbo, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);

    /* studio lighting: warm key light from the upper left, cool fill from the sky */
    mat4 view, proj;
    vec3 eye = { 0.9f, 0.7f, 1.6f };
    glm_lookat(eye, (vec3){0, 0, 0}, (vec3){0, 1, 0}, view);
    glm_perspective(glm_rad(32.0f), 1.0f, 0.05f, 20.0f, proj);
    glm_mat4_mul(proj, view, r->frame.view_proj);
    glm_mat4_inv(r->frame.view_proj, r->frame.inv_view_proj);
    glm_vec4(eye, 1, r->frame.camera_pos);
    glm_vec4_copy((vec4){0.3f, 0.8f, 0.6f, 0.0f}, r->frame.sun_dir);    /* w = 0: no shadows */
    glm_vec3_normalize(r->frame.sun_dir);
    glm_vec4_copy((vec4){3.0f, 2.8f, 2.5f, 0}, r->frame.sun_color);
    glm_vec4_copy((vec4){0.7f, 0.75f, 0.85f, 0}, r->frame.sky_color);
    glm_vec4_copy((vec4){0.3f, 0.25f, 0.2f, 0}, r->frame.ground_color);
    glm_vec4_copy((vec4){0, 0, 0, 0}, r->frame.fog);
    r->frame.misc[1] = 0;
    glNamedBufferSubData(r->ubo, 0, sizeof r->frame, &r->frame);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, size, size);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB);      /* linear shader output -> sRGB texture */

    for (int i = 1; i < ITEM_COUNT; i++) {
        ItemDef *it = &ITEMS[i];
        if (!it->loaded)
            continue;
        glCreateTextures(GL_TEXTURE_2D, 1, &it->icon);
        glTextureStorage2D(it->icon, 1, GL_SRGB8_ALPHA8, size, size);
        glTextureParameteri(it->icon, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(it->icon, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, it->icon, 0);

        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        /* long things lie diagonally, everything else faces the camera */
        mat4 xf = GLM_MAT4_IDENTITY_INIT, fit;
        if (it->kind == KIND_WEAPON || it->kind == KIND_THROWN || i == ITEM_TORCH || i == ITEM_KEY)
            glm_rotate(xf, glm_rad(-40.0f), (vec3){0.3f, 0.2f, 1.0f});
        else
            glm_rotate(xf, glm_rad(25.0f), (vec3){0, 1, 0});
        model_fit(&it->model, 1.0f, fit);
        glm_mat4_mul(xf, fit, xf);
        model_draw(&it->model, NULL, model_prog, r->frame.view_proj, xf, NULL);
    }

    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &depth);
    glClearColor(0, 0, 0, 1);
    r->frame = saved;
}
