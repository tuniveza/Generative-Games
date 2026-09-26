#ifndef ITEMS_H
#define ITEMS_H

#include "model.h"
#include "renderer.h"

typedef enum {
    ITEM_NONE,
    ITEM_TORCH,
    ITEM_DAGGER, ITEM_AXE, ITEM_MACE, ITEM_WARHAMMER, ITEM_SLEDGEHAMMER, ITEM_SHIELD,
    ITEM_GRENADE,
    ITEM_SWEET_POTATO, ITEM_CHEESE, ITEM_GOBLET, ITEM_AVOCADO,
    ITEM_RUBBER_DUCK, ITEM_POCKET_WATCH, ITEM_BOOMBOX, ITEM_COMPASS, ITEM_BINOCULARS,
    ITEM_GAS_MASK, ITEM_SPECTACLES,
    ITEM_KEY,
    ITEM_LANTERN, ITEM_MANA_POTION,
    ITEM_TOME_FIREBALL, ITEM_TOME_FROST, ITEM_TOME_LIGHTNING, ITEM_TOME_HEAL, ITEM_TOME_WISP, ITEM_TOME_BLINK,
    ITEM_COUNT
} ItemId;

typedef enum { KIND_WEAPON, KIND_THROWN, KIND_FOOD, KIND_GADGET, KIND_KEY, KIND_SPELL } ItemKind;

typedef enum {
    SPELL_NONE, SPELL_FIREBALL, SPELL_FROST, SPELL_LIGHTNING, SPELL_HEAL, SPELL_WISP, SPELL_BLINK,
} Spell;

/* how the hand moves when attacking */
typedef enum { SWING_PUNCH, SWING_STAB, SWING_SLASH, SWING_OVERHEAD, SWING_BASH } SwingStyle;

typedef struct {
    const char *name;
    const char *desc;
    const char *model_path;     /* NULL = built in code (torch, key) */
    ItemKind kind;
    SwingStyle swing;
    float damage, range, cooldown, knockback;
    float heal;
    bool stacks;
    float hold_size;            /* meters, longest side, when held */
    float grip;                 /* weapons: where along the handle (0 = bottom) the fist is */
    float mana;                 /* spells: cost; potions: mana restored */
    Spell spell;
    vec3 glow;                  /* spells: the color of the magic */

    /* filled in by items_load */
    Model model;
    bool loaded;
    mat4 hold;                  /* model space -> grip space (fist at origin, handle along +Y) */
    GLuint icon;
} ItemDef;

extern ItemDef ITEMS[ITEM_COUNT];

void items_load(void);
void items_free(void);

/* draws every item's model into a small picture for the inventory */
void items_make_icons(Renderer *r, GLuint model_prog);

/* the torch model is shared with the wall torches */
const Model *items_torch_model(void);

#endif
