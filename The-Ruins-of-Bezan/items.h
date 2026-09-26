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

    /* more weapons */
    ITEM_CUTLASS, ITEM_ESTOC, ITEM_KATANA, ITEM_MACHETE, ITEM_HATCHET, ITEM_BEARDED_AXE,
    ITEM_PICKAXE, ITEM_SPADE, ITEM_CROWBAR, ITEM_HARPOON, ITEM_TRIDENT, ITEM_EMBER_BLADE,
    /* armour and clothes (worn, seen on you in third person and on your arms) */
    ITEM_FISHER_HAT, ITEM_IRON_HELM, ITEM_FUR_HAT, ITEM_WIZARD_HAT, ITEM_DIVING_HELM,
    ITEM_KNIGHT_PLATE, ITEM_BARBARIAN_FURS, ITEM_MAGISTER_ROBES,
    ITEM_RED_CAPE, ITEM_FUR_CLOAK, ITEM_STARRY_CAPE, ITEM_RUBBER_BOOTS,
    /* fishing */
    ITEM_ROD_DRIFTWOOD, ITEM_ROD_BRASS, ITEM_ROD_CORAL, ITEM_ROD_LEVIATHAN,
    ITEM_BAIT_WORM, ITEM_BAIT_SHRIMP, ITEM_BAIT_GLOWWORM, ITEM_BAIT_EMBER,
    ITEM_FISH_SARDINE, ITEM_FISH_MACKEREL, ITEM_FISH_SNAPPER, ITEM_FISH_PUFFER, ITEM_FISH_EEL,
    ITEM_FISH_LANTERN, ITEM_FISH_GHOST, ITEM_FISH_GOLDEN,
    ITEM_OLD_BOOT, ITEM_BOTTLE, ITEM_SEAWEED,
    ITEM_SELKIE_SCALE, ITEM_GILL_PEARL, ITEM_DOLPHIN_CHARM, ITEM_ANGLER_LAMP, ITEM_CONCH,
    /* the beach shop's novelties and treats */
    ITEM_GNOME, ITEM_UKULELE, ITEM_METAL_DETECTOR, ITEM_MAGNIFIER, ITEM_ELEPHANT,
    ITEM_BANANA, ITEM_LIME, ITEM_POMEGRANATE, ITEM_CROISSANT,
    /* for the quests */
    ITEM_EMBER_HEART, ITEM_TIDE_BELL, ITEM_FIRST_TIDE_EYE,
    /* new spells */
    ITEM_TOME_GILLS, ITEM_TOME_TIDE, ITEM_TOME_METEOR, ITEM_TOME_SPIKES, ITEM_TOME_SHADOW,
    ITEM_TOME_GALE, ITEM_TOME_MISSILES,
    ITEM_COUNT
} ItemId;

typedef enum {
    KIND_WEAPON, KIND_THROWN, KIND_FOOD, KIND_GADGET, KIND_KEY, KIND_SPELL,
    KIND_ARMOR,     /* worn: click to put on or take off */
    KIND_ROD,       /* a fishing rod: click to cast */
    KIND_BAIT,      /* goes on the hook: the best you carry is used */
    KIND_FISH,      /* eat it, sell it, or feed it to a walrus */
    KIND_TREASURE,  /* quest things, charms and curiosities */
} ItemKind;

typedef enum {
    SPELL_NONE, SPELL_FIREBALL, SPELL_FROST, SPELL_LIGHTNING, SPELL_HEAL, SPELL_WISP, SPELL_BLINK,
    SPELL_GILLS, SPELL_TIDE, SPELL_METEOR, SPELL_SPIKES, SPELL_SHADOW, SPELL_GALE, SPELL_MISSILES,
} Spell;

/* where armour is worn */
typedef enum { EQ_HEAD, EQ_BODY, EQ_BACK, EQ_FEET, EQ_COUNT } EquipSlot;

/* how the hand moves when attacking */
typedef enum { SWING_PUNCH, SWING_STAB, SWING_SLASH, SWING_OVERHEAD, SWING_BASH } SwingStyle;

typedef struct {
    const char *name;
    const char *desc;
    const char *model_path;     /* NULL = built in code (torch, key, rods...) */
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
    int price;                  /* in shells (a white shell is worth 1); 0 = not for sale */
    EquipSlot slot;             /* armour */
    float armor;                /* armour: how much of each blow it takes, 0..1 */
    int tier;                   /* rods: 1..4; baits: how good */
    vec3 turn;                  /* yaw, pitch, roll (radians) to set the model right in the hand
                                   (e.g. an axe's blade facing forward) */

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
