#include "items.h"
#include "meshgen.h"
#include "shapes.h"

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
                   KIND_WEAPON, SWING_SLASH, 24, 2.2f, 0.7f, 3.0f, 0, false, 0.46f, 0.1f,
                   .turn = { -0.25f, 0, 0 } },
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
                      KIND_WEAPON, SWING_BASH, 8, 1.8f, 0.6f, 9.0f, 0, false, 0.75f, 0.5f },

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
    [ITEM_GAS_MASK] = { "Old Gas Mask", "Click to wear. Keeps out volcanic\nfumes; creatures can't smell you.",
                        PH "old_gas_mask/old_gas_mask.gltf",
                        KIND_GADGET, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 1.1f, 0.5f },
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

    /* ---- more weapons (Bram the smith sells most of them) ---- */
    [ITEM_CUTLASS] = { "Cutlass", "A sailor's curved blade. Quick,\nwide slashes; good on a rolling deck.",
        PH "wooden_handle_saber/wooden_handle_saber.gltf", KIND_WEAPON, SWING_SLASH, 26, 2.2f, 0.55f, 2.5f, 0, false, 0.95f, 0.08f,
        .price = 30, .turn = { GLM_PI_2f, 0, 0 } },
    [ITEM_ESTOC] = { "Estoc", "A long, stiff thrusting sword that\nfinds the gaps in any shell.",
        PH "antique_estoc/antique_estoc.gltf", KIND_WEAPON, SWING_STAB, 34, 2.8f, 0.6f, 2.0f, 0, false, 1.15f, 0.1f,
        .price = 45, .turn = { GLM_PI_2f, 0, 0 } },
    [ITEM_KATANA] = { "Katana", "Folded steel from very far away.\nThe finest edge on the coast.",
        PH "antique_katana_01/antique_katana_01.gltf", KIND_WEAPON, SWING_SLASH, 40, 2.5f, 0.6f, 3.0f, 0, false, 1.0f, 0.12f,
        .price = 70 },
    [ITEM_MACHETE] = { "Machete", "For hacking through kelp, vines\nand the occasional crab.",
        PH "machete/machete.gltf", KIND_WEAPON, SWING_SLASH, 18, 1.9f, 0.42f, 2.0f, 0, false, 0.72f, 0.1f,
        .price = 15 },
    [ITEM_HATCHET] = { "Hatchet", "A little axe for kindling. Also\nthrows a surprisingly mean punch.",
        PH "hatchet/hatchet.gltf", KIND_WEAPON, SWING_SLASH, 17, 1.8f, 0.45f, 2.5f, 0, false, 0.42f, 0.15f,
        .price = 12 },
    [ITEM_BEARDED_AXE] = { "Bearded Axe", "Its long lower edge hooks shields\nand shell alike.",
        PH "wooden_axe_02/wooden_axe_02.gltf", KIND_WEAPON, SWING_OVERHEAD, 36, 2.3f, 0.85f, 5.0f, 0, false, 0.72f, 0.1f,
        .price = 35 },
    [ITEM_PICKAXE] = { "Pickaxe", "Breaks rock, and cools a magma imp's\ntemper considerably.",
        PH "picke_dirty_01/picke_dirty_01.gltf", KIND_WEAPON, SWING_OVERHEAD, 30, 2.3f, 0.95f, 4.0f, 0, false, 0.95f, 0.08f,
        .price = 20 },
    [ITEM_SPADE] = { "Rusty Spade", "Dig where the detector beeps.\nClick on sand to dig.",
        PH "rusted_spade_01/rusted_spade_01.gltf", KIND_WEAPON, SWING_OVERHEAD, 12, 2.2f, 0.8f, 3.0f, 0, false, 1.0f, 0.62f,
        .price = 10 },
    [ITEM_CROWBAR] = { "Crowbar", "Pries open crates, and arguments.",
        PH "crowbar_01/crowbar_01.gltf", KIND_WEAPON, SWING_SLASH, 15, 1.9f, 0.5f, 3.0f, 0, false, 0.6f, 0.15f,
        .price = 8 },
    [ITEM_HARPOON] = { "Harpoon", "Right click to throw it, click to\nstab. Sharks respect a harpoon.",
        NULL, KIND_WEAPON, SWING_STAB, 28, 3.0f, 0.7f, 2.0f, 0, false, 1.5f, 0.3f,
        .price = 25 },
    [ITEM_TRIDENT] = { "Trident of the Drowned Court", "Three golden points that remember\nthe sea. Strikes twice as hard in water.",
        NULL, KIND_WEAPON, SWING_STAB, 55, 3.0f, 0.65f, 4.0f, 0, false, 1.5f, 0.3f },
    [ITEM_EMBER_BLADE] = { "Ember Blade", "Forged in Old Ember's heart. It\nsets what it cuts alight.",
        NULL, KIND_WEAPON, SWING_SLASH, 42, 2.4f, 0.6f, 3.0f, 0, false, 1.0f, 0.1f },

    /* ---- armour and clothes ---- */
    [ITEM_FISHER_HAT] = { "Fisherman's Hat", "Keeps the rain off. Fish are said\nto bite more for a proper hat.",
        PH "fishermans_hat/fishermans_hat.gltf", KIND_ARMOR, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.3f, 0.5f,
        .price = 8, .slot = EQ_HEAD, .armor = 0.03f },
    [ITEM_IRON_HELM] = { "Knight's Helm", "Full iron. Blocks a good share of\nany blow, and all of the hail.",
        "assets/kaykit/Knight.glb#Knight_Helmet", KIND_ARMOR, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.3f, 0.5f,
        .price = 30, .slot = EQ_HEAD, .armor = 0.12f },
    [ITEM_FUR_HAT] = { "Bear-head Hat", "A whole bear, from the nose up. Warm,\nheavy, and very dramatic.",
        "assets/kaykit/Barbarian.glb#Barbarian_Hat", KIND_ARMOR, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.34f, 0.5f,
        .price = 18, .slot = EQ_HEAD, .armor = 0.07f },
    [ITEM_WIZARD_HAT] = { "Magister's Hat", "Mana comes back twice as fast\nunder a proper pointed hat.",
        "assets/kaykit/Mage.glb#Mage_Hat", KIND_ARMOR, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.4f, 0.5f,
        .price = 45, .slot = EQ_HEAD, .armor = 0.03f },
    [ITEM_DIVING_HELM] = { "Brass Diving Helm", "Breathe four times as long under\nwater. Heavy on land.",
        NULL, KIND_ARMOR, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.36f, 0.5f,
        .price = 90, .slot = EQ_HEAD, .armor = 0.1f },
    [ITEM_KNIGHT_PLATE] = { "Knight's Plate", "Steel from shoulder to shin. Takes\na third of every blow. You'll sink.",
        "assets/kaykit/Knight.glb#Knight_Body", KIND_ARMOR, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.45f, 0.5f,
        .price = 80, .slot = EQ_BODY, .armor = 0.33f },
    [ITEM_BARBARIAN_FURS] = { "Barbarian Furs", "Leather, fur and confidence.",
        "assets/kaykit/Barbarian.glb#Barbarian_Body", KIND_ARMOR, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.45f, 0.5f,
        .price = 40, .slot = EQ_BODY, .armor = 0.18f },
    [ITEM_MAGISTER_ROBES] = { "Magister's Robes", "Embroidered with small moving stars.\nSpells cost a quarter less.",
        "assets/kaykit/Mage.glb#Mage_Body", KIND_ARMOR, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.45f, 0.5f,
        .price = 60, .slot = EQ_BODY, .armor = 0.07f },
    [ITEM_RED_CAPE] = { "Crimson Cape", "It does nothing at all except look\nsplendid in the wind.",
        "assets/kaykit/Knight.glb#Knight_Cape", KIND_ARMOR, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.5f, 0.5f,
        .price = 20, .slot = EQ_BACK, .armor = 0.02f },
    [ITEM_FUR_CLOAK] = { "Fur Cloak", "Thick enough to shrug off hailstones.",
        "assets/kaykit/Barbarian.glb#Barbarian_Cape", KIND_ARMOR, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.5f, 0.5f,
        .price = 25, .slot = EQ_BACK, .armor = 0.05f },
    [ITEM_STARRY_CAPE] = { "Starry Cape", "A magister's cloak. Your mana\ntrickles back faster.",
        "assets/kaykit/Mage.glb#Mage_Cape", KIND_ARMOR, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.5f, 0.5f,
        .price = 35, .slot = EQ_BACK, .armor = 0.03f },
    [ITEM_RUBBER_BOOTS] = { "Rubber Boots", "Wade through the shallows at full\nspeed. Jellyfish can't sting ankles.",
        PH "rubber_boots/rubber_boots.gltf", KIND_ARMOR, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.4f, 0.5f,
        .price = 12, .slot = EQ_FEET, .armor = 0.02f },

    /* ---- fishing ---- */
    [ITEM_ROD_DRIFTWOOD] = { "Driftwood Rod", "Click to cast into deep water. When\nit bites, click, then hold to reel.",
        NULL, KIND_ROD, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 1.9f, 0.08f, .tier = 1 },
    [ITEM_ROD_BRASS] = { "Brass Rod", "A steadier rod with a wider catch\nand a quicker reel.",
        NULL, KIND_ROD, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 1.9f, 0.08f, .tier = 2, .price = 25 },
    [ITEM_ROD_CORAL] = { "Coral Rod", "Grown, not made. Rare fish can't\nresist it.",
        NULL, KIND_ROD, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 1.9f, 0.08f, .tier = 3, .price = 70 },
    [ITEM_ROD_LEVIATHAN] = { "Leviathan Rod", "Carved from something enormous. It\ncan land whatever lives down there.",
        NULL, KIND_ROD, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 1.9f, 0.08f, .tier = 4, .price = 160 },
    [ITEM_BAIT_WORM] = { "Worms", "Bait. Everyday fish like them fine.",
        NULL, KIND_BAIT, SWING_PUNCH, 0, 0, 0.5f, 0, 0, true, 0.1f, 0.5f, .tier = 1, .price = 1 },
    [ITEM_BAIT_SHRIMP] = { "Shrimp", "Bait. Brings in bigger fish, and faster.",
        NULL, KIND_BAIT, SWING_PUNCH, 0, 0, 0.5f, 0, 0, true, 0.1f, 0.5f, .tier = 2, .price = 3 },
    [ITEM_BAIT_GLOWWORM] = { "Glow-grubs", "Bait that shines. Night fish and\ndeep fish come looking.",
        NULL, KIND_BAIT, SWING_PUNCH, 0, 0, 0.5f, 0, 0, true, 0.1f, 0.5f, .tier = 3, .price = 6 },
    [ITEM_BAIT_EMBER] = { "Ember Lure", "Forged from Old Ember's heart. It\nburns even under the sea.",
        NULL, KIND_BAIT, SWING_PUNCH, 0, 0, 0.5f, 0, 0, true, 0.12f, 0.5f, .tier = 4 },
    [ITEM_FISH_SARDINE] = { "Sardine", "Small and silver. Eat it (+10) or\nsell it. Walruses love them.",
        NULL, KIND_FISH, SWING_PUNCH, 0, 0, 1.0f, 0, 10, true, 0.2f, 0.5f, .price = 2 },
    [ITEM_FISH_MACKEREL] = { "Mackerel", "Striped like the sea on a windy day.\nEat it (+15).",
        NULL, KIND_FISH, SWING_PUNCH, 0, 0, 1.0f, 0, 15, true, 0.3f, 0.5f, .price = 4 },
    [ITEM_FISH_SNAPPER] = { "Red Snapper", "A handsome red fish. Eat it (+25).",
        NULL, KIND_FISH, SWING_PUNCH, 0, 0, 1.0f, 0, 25, true, 0.4f, 0.5f, .price = 8 },
    [ITEM_FISH_PUFFER] = { "Pufferfish", "Do not eat. Or do, and find out.",
        NULL, KIND_FISH, SWING_PUNCH, 0, 0, 1.0f, 0, -20, true, 0.25f, 0.5f, .price = 10 },
    [ITEM_FISH_EEL] = { "Moray Eel", "Long, green, and grumpy. Eat it (+30).",
        NULL, KIND_FISH, SWING_PUNCH, 0, 0, 1.0f, 0, 30, true, 0.7f, 0.5f, .price = 12 },
    [ITEM_FISH_LANTERN] = { "Lanternfish", "It carries its own little lamp.\nOnly bites at night, out deep.",
        NULL, KIND_FISH, SWING_PUNCH, 0, 0, 1.0f, 0, 20, true, 0.3f, 0.5f, .price = 20 },
    [ITEM_FISH_GHOST] = { "Ghost Carp", "You can see right through it. Its\nscales are cold as moonlight.",
        NULL, KIND_FISH, SWING_PUNCH, 0, 0, 1.0f, 0, 35, true, 0.45f, 0.5f, .price = 25 },
    [ITEM_FISH_GOLDEN] = { "Golden Snapper", "The fish every angler on the coast\ndreams about. Old Brine would weep.",
        NULL, KIND_FISH, SWING_PUNCH, 0, 0, 1.0f, 0, 60, true, 0.45f, 0.5f, .price = 60 },
    [ITEM_OLD_BOOT] = { "Soggy Boots", "Somebody's. Still soggy.",
        PH "rubber_boots/rubber_boots.gltf", KIND_TREASURE, SWING_PUNCH, 0, 0, 0.5f, 0, 0, true, 0.3f, 0.5f, .price = 1 },
    [ITEM_BOTTLE] = { "Message in a Bottle", "Click to read the note inside.",
        NULL, KIND_TREASURE, SWING_PUNCH, 0, 0, 0.5f, 0, 0, true, 0.25f, 0.5f },
    [ITEM_SEAWEED] = { "Seaweed", "Salty and chewy. Eat it (+5).",
        NULL, KIND_FOOD, SWING_PUNCH, 0, 0, 1.0f, 0, 5, true, 0.2f, 0.5f },
    [ITEM_SELKIE_SCALE] = { "Selkie Scale", "Click to press it to your skin: you\nswim half again as fast, for good.",
        NULL, KIND_TREASURE, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.08f, 0.5f },
    [ITEM_GILL_PEARL] = { "Gill Pearl", "Click to swallow it: you can hold\nyour breath twice as long, for good.",
        NULL, KIND_TREASURE, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.06f, 0.5f },
    [ITEM_DOLPHIN_CHARM] = { "Dolphin Charm", "Click to wear it: swimming barely\ntires you, and you leap from waves.",
        NULL, KIND_TREASURE, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.08f, 0.5f },
    [ITEM_ANGLER_LAMP] = { "Angler's Lamp", "Click to take its light: the deep\nglows around you when you dive.",
        NULL, KIND_TREASURE, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.08f, 0.5f },
    [ITEM_CONCH] = { "Conch of the Deep", "Blow it by the bell buoy far out at\nsea, and the Drowned Court will open.",
        PH "lambis_shell/lambis_shell.gltf", KIND_TREASURE, SWING_PUNCH, 0, 0, 2.0f, 0, 0, false, 0.25f, 0.5f },

    /* ---- the Curious Clam's novelties and treats ---- */
    [ITEM_GNOME] = { "Garden Gnome", "Click to set him down. Every creature\nnearby stops to stare at him.",
        PH "garden_gnome/garden_gnome.gltf", KIND_GADGET, SWING_PUNCH, 0, 0, 20.0f, 0, 0, false, 0.4f, 0.5f, .price = 15 },
    [ITEM_UKULELE] = { "Ukulele", "Click to play. Walruses sing along,\nand people can't help dancing.",
        PH "Ukulele_01/Ukulele_01.gltf", KIND_GADGET, SWING_PUNCH, 0, 0, 6.0f, 0, 0, false, 0.5f, 0.5f, .price = 20 },
    [ITEM_METAL_DETECTOR] = { "Metal Detector", "Hold it out on the sand: it beeps\nfaster near buried treasure.",
        PH "metal_detector/metal_detector.gltf", KIND_GADGET, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.9f, 0.5f, .price = 30 },
    [ITEM_MAGNIFIER] = { "Magnifying Glass", "Hold it up to see what's faint or\nhidden, even the very old.",
        PH "magnifying_glass_01/magnifying_glass_01.gltf", KIND_GADGET, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.25f, 0.5f, .price = 18 },
    [ITEM_ELEPHANT] = { "Carved Elephant", "A lucky charm. Carry it and rare\nfish bite a little more often.",
        PH "carved_wooden_elephant/carved_wooden_elephant.gltf", KIND_TREASURE, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.14f, 0.5f, .price = 25 },
    [ITEM_BANANA] = { "Bananas", "A whole bunch. Heals 15.",
        PH "bananas/bananas.gltf", KIND_FOOD, SWING_PUNCH, 0, 0, 1.0f, 0, 15, true, 0.3f, 0.5f, .price = 2 },
    [ITEM_LIME] = { "Lime", "Sour enough to wake the dead. Heals 8\nand cures fumes and stings.",
        PH "food_lime_01/food_lime_01.gltf", KIND_FOOD, SWING_PUNCH, 0, 0, 1.0f, 0, 8, true, 0.08f, 0.5f, .price = 1 },
    [ITEM_POMEGRANATE] = { "Pomegranate", "A thousand little jewels. Heals 25.",
        PH "food_pomegranate_01/food_pomegranate_01.gltf", KIND_FOOD, SWING_PUNCH, 0, 0, 1.0f, 0, 25, true, 0.12f, 0.5f, .price = 3 },
    [ITEM_CROISSANT] = { "Croissant", "Flaky, buttery, entirely out of place.\nHeals 30.",
        PH "croissant/croissant.gltf", KIND_FOOD, SWING_PUNCH, 0, 0, 1.0f, 0, 30, true, 0.2f, 0.5f, .price = 4 },

    /* ---- for the quests ---- */
    [ITEM_EMBER_HEART] = { "Ember Heart", "A coal that has burned since the\nmountain was young. Bram could forge it.",
        NULL, KIND_TREASURE, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.18f, 0.5f },
    [ITEM_TIDE_BELL] = { "The Tide Bell", "Brinewick's lost bell. It hums with\nevery wave. Take it home.",
        NULL, KIND_TREASURE, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.4f, 0.5f },
    [ITEM_FIRST_TIDE_EYE] = { "Eye of the First Tide", "You breathe water like air, and the\nsea's creatures will not harm you.",
        NULL, KIND_TREASURE, SWING_PUNCH, 0, 0, 0.5f, 0, 0, false, 0.1f, 0.5f },

    /* ---- new spells ---- */
    [ITEM_TOME_GILLS] = { "Tome of Gills", "Breathe water and swim without\ntiring for a minute. 20 mana.",
        NULL, KIND_SPELL, SWING_PUNCH, 0, 0, 3.0f, 0, 0, false, 0.16f, 0.5f,
        .mana = 20, .spell = SPELL_GILLS, .glow = { 0.6f, 3.5f, 4.5f }, .price = 40 },
    [ITEM_TOME_TIDE] = { "Tome of Tides", "A wave that bowls over everything\nin front of you and douses fire. 25 mana.",
        NULL, KIND_SPELL, SWING_PUNCH, 30, 9.0f, 2.5f, 14.0f, 0, false, 0.16f, 0.5f,
        .mana = 25, .spell = SPELL_TIDE, .glow = { 0.8f, 3.0f, 5.5f } },
    [ITEM_TOME_METEOR] = { "Tome of Meteors", "Calls a burning stone down from the\nsky where you look. 45 mana.",
        NULL, KIND_SPELL, SWING_PUNCH, 80, 6.0f, 8.0f, 16.0f, 0, false, 0.16f, 0.5f,
        .mana = 45, .spell = SPELL_METEOR, .glow = { 7.0f, 2.0f, 0.3f } },
    [ITEM_TOME_SPIKES] = { "Tome of Thorns", "Stone spikes burst from the ground\nin a line ahead of you. 30 mana.",
        NULL, KIND_SPELL, SWING_PUNCH, 35, 14.0f, 2.0f, 6.0f, 0, false, 0.16f, 0.5f,
        .mana = 30, .spell = SPELL_SPIKES, .glow = { 3.0f, 2.2f, 1.0f } },
    [ITEM_TOME_SHADOW] = { "Tome of Shadows", "Fade from sight for ten seconds.\nNothing can find you. 30 mana.",
        NULL, KIND_SPELL, SWING_PUNCH, 0, 0, 20.0f, 0, 0, false, 0.16f, 0.5f,
        .mana = 30, .spell = SPELL_SHADOW, .glow = { 1.2f, 0.6f, 2.5f }, .price = 55 },
    [ITEM_TOME_GALE] = { "Tome of Gales", "A whirlwind that lifts your foes and\nflings them away. 35 mana.",
        NULL, KIND_SPELL, SWING_PUNCH, 15, 12.0f, 6.0f, 0, 0, false, 0.16f, 0.5f,
        .mana = 35, .spell = SPELL_GALE, .glow = { 2.5f, 4.0f, 3.0f } },
    [ITEM_TOME_MISSILES] = { "Tome of Seeking Stars", "Three stars that hunt down the\nnearest foes. 20 mana.",
        NULL, KIND_SPELL, SWING_PUNCH, 16, 25.0f, 1.2f, 2.0f, 0, false, 0.16f, 0.5f,
        .mana = 20, .spell = SPELL_MISSILES, .glow = { 4.0f, 3.0f, 6.5f } },
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
    /* some models need turning to sit right (an axe's blade facing forward) */
    if (it->turn[0] != 0.0f) glm_rotate_y(it->hold, it->turn[0], it->hold);
    if (it->turn[1] != 0.0f) glm_rotate_x(it->hold, it->turn[1], it->hold);
    if (it->turn[2] != 0.0f) glm_rotate_z(it->hold, it->turn[2], it->hold);
    bool handled = it->kind == KIND_WEAPON || it->kind == KIND_THROWN || it->kind == KIND_ROD || it == &ITEMS[ITEM_TORCH];
    if (handled)
        glm_translate(it->hold, (vec3){-c[0], -(m->min[1] + ext[1] * it->grip), -c[2]});
    else
        glm_translate(it->hold, (vec3){-c[0], -c[1], -c[2]});
}

/* "file.glb#NodeA,NodeB": a character model with only those pieces showing (armour) */
static bool load_pieces(Model *m, const char *spec)
{
    char path[160];
    const char *hash = strchr(spec, '#');
    snprintf(path, sizeof path, "%.*s", (int)(hash - spec), spec);
    if (!model_load(m, path))
        return false;
    for (int i = 0; i < m->node_count; i++) {
        if (m->nodes[i].mesh < 0)
            continue;
        bool keep = false;
        const char *p = hash + 1;
        while (*p) {
            const char *end = strchr(p, ',');
            size_t n = end ? (size_t)(end - p) : strlen(p);
            if (strlen(m->nodes[i].name) == n && strncmp(m->nodes[i].name, p, n) == 0)
                keep = true;
            p += n + (end ? 1 : 0);
        }
        if (!keep)
            model_hide_node(m, m->nodes[i].name);
    }
    return true;
}

/* the things made in code */
static bool build_item(ItemId id, Model *m)
{
    switch (id) {
    case ITEM_HARPOON: shape_harpoon(m); return true;
    case ITEM_TRIDENT: shape_trident(m); return true;
    case ITEM_EMBER_BLADE: shape_ember_blade(m); return true;
    case ITEM_DIVING_HELM: shape_diving_helm(m); return true;
    case ITEM_ROD_DRIFTWOOD: case ITEM_ROD_BRASS: case ITEM_ROD_CORAL: case ITEM_ROD_LEVIATHAN:
        shape_rod(m, ITEMS[id].tier);
        return true;
    case ITEM_BAIT_WORM: shape_worm(m, (vec3){0.75f, 0.35f, 0.35f}, NULL); return true;
    case ITEM_BAIT_SHRIMP: shape_shrimp(m); return true;
    case ITEM_BAIT_GLOWWORM: shape_worm(m, (vec3){0.5f, 0.9f, 0.3f}, (vec3){0.8f, 2.5f, 0.4f}); return true;
    case ITEM_BAIT_EMBER: shape_lure(m); return true;
    case ITEM_FISH_SARDINE: shape_fish(m, (vec3){0.35f, 0.45f, 0.55f}, (vec3){0.85f, 0.88f, 0.9f}, 0.2f, 0.22f, NULL); return true;
    case ITEM_FISH_MACKEREL: shape_fish(m, (vec3){0.1f, 0.35f, 0.3f}, (vec3){0.8f, 0.85f, 0.8f}, 0.3f, 0.22f, NULL); return true;
    case ITEM_FISH_SNAPPER: shape_fish(m, (vec3){0.8f, 0.18f, 0.12f}, (vec3){0.95f, 0.6f, 0.5f}, 0.4f, 0.36f, NULL); return true;
    case ITEM_FISH_PUFFER: shape_fish(m, (vec3){0.7f, 0.6f, 0.25f}, (vec3){0.9f, 0.88f, 0.75f}, 0.25f, 0.8f, NULL); return true;
    case ITEM_FISH_EEL: shape_fish(m, (vec3){0.18f, 0.28f, 0.08f}, (vec3){0.5f, 0.55f, 0.2f}, 0.7f, 0.1f, NULL); return true;
    case ITEM_FISH_LANTERN: shape_fish(m, (vec3){0.05f, 0.06f, 0.12f}, (vec3){0.2f, 0.2f, 0.3f}, 0.3f, 0.4f, (vec3){0.3f, 1.2f, 1.4f}); return true;
    case ITEM_FISH_GHOST: shape_fish(m, (vec3){0.75f, 0.85f, 0.95f}, (vec3){0.9f, 0.95f, 1.0f}, 0.45f, 0.34f, (vec3){0.25f, 0.4f, 0.6f}); return true;
    case ITEM_FISH_GOLDEN: shape_fish(m, (vec3){0.95f, 0.68f, 0.15f}, (vec3){1.0f, 0.9f, 0.5f}, 0.45f, 0.36f, (vec3){0.5f, 0.3f, 0.02f}); return true;
    case ITEM_BOTTLE: shape_bottle(m); return true;
    case ITEM_SEAWEED: shape_seaweed(m); return true;
    case ITEM_SELKIE_SCALE: shape_charm(m, (vec3){0.55f, 0.7f, 0.85f}, (vec3){0.2f, 0.4f, 0.6f}); return true;
    case ITEM_GILL_PEARL: shape_pearl(m, 0.025f, (vec3){0.9f, 0.9f, 0.95f}, (vec3){0.3f, 0.6f, 0.7f}); return true;
    case ITEM_DOLPHIN_CHARM: shape_charm(m, (vec3){0.2f, 0.6f, 0.65f}, (vec3){0.1f, 0.5f, 0.5f}); return true;
    case ITEM_ANGLER_LAMP: shape_pearl(m, 0.03f, (vec3){0.9f, 1.0f, 0.7f}, (vec3){2.5f, 3.0f, 1.2f}); return true;
    case ITEM_EMBER_HEART: shape_crystal(m, 0.18f, (vec3){0.9f, 0.3f, 0.05f}, (vec3){6.0f, 1.6f, 0.2f}); return true;
    case ITEM_TIDE_BELL: shape_bell(m, 0.4f, (vec3){0.55f, 0.62f, 0.5f}); return true;
    case ITEM_FIRST_TIDE_EYE: shape_pearl(m, 0.05f, (vec3){0.4f, 0.9f, 0.95f}, (vec3){0.6f, 2.6f, 3.0f}); return true;
    default: return false;
    }
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
        } else if (build_item((ItemId)i, &it->model)) {
            /* made in code */
        } else if (it->model_path && strchr(it->model_path, '#')) {
            if (!load_pieces(&it->model, it->model_path)) {
                fprintf(stderr, "item %s: missing model\n", it->name);
                continue;
            }
        } else if (!it->model_path || !model_load(&it->model, it->model_path)) {
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
        if (it->kind == KIND_WEAPON || it->kind == KIND_THROWN || it->kind == KIND_ROD || i == ITEM_TORCH || i == ITEM_KEY)
            glm_rotate(xf, glm_rad(-40.0f), (vec3){0.3f, 0.2f, 1.0f});
        else if (it->kind == KIND_ARMOR && it->model_path && strchr(it->model_path, '#'))
            glm_rotate(xf, glm_rad(35.0f), (vec3){0, 1, 0});    /* armour faces you a little turned */
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
