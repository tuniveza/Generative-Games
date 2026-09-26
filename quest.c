#include "game.h"
#include "meshgen.h"
#include "shapes.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

/* The coast's quests and the things you use along the way.
 *
 * The Ember and the Tide (the main chain):
 *    0  hear of Brinewick                     -> talk to Elder Maren
 *    1  relight the three shrine lanterns     (Pell says how)
 *    2  ask Harbourmaster Quill about the bell
 *    3  find his three tide charts            -> a driftwood rod
 *    4  catch a fish for Old Brine            -> he tells of the Conch of the Deep
 *    5  bring Old Ember's heart to Bram       (the crater's fumes: the gas mask) -> ember lures
 *    6  hook the Conch of the Deep            (Leviathan rod, ember lure, deep water, night)
 *    7  blow it by the bell buoy far out at sea -> a whirlpool down to the Drowned Court
 *    8  wake the three seals, face the Drowned King, take the Tide Bell
 *    9  hang the bell in Brinewick's tower    -> a festival
 *   10  done
 * Side quests: the twelve relics of the First Tide (and the Unmoored's gift), feeding the
 * walruses, Old Brine's golden snapper, and odd jobs from the notice board. */

/* ---------- words ---------- */

static const char *LORE[16][2] = {
    { "The Coming of the Tide", "Before there was land, there was the Tide. It came in once, and never went out, and on it rode a people with shells for ships. Where they stepped ashore, the coast began." },
    { "The Shell Road", "The First Tide's people paved their roads with shells, so that walking home at night you could hear where you were going. Brinewick's lanes still sing a little underfoot." },
    { "The Bronze Beasts", "The whale, the shark and the ray were cast from the bells of ships that never came home. On storm nights they are said to turn and face the sea." },
    { "Old Ember", "The mountain to the north-east wakes every half hour and burns the coast to ash. Then the Covenant sings it all back: every creature, every flower, every shell. Nobody remembers a time before the Covenant." },
    { "The Tide Bell", "Brinewick's bell rang the tides in and out for three hundred years, until the Drowned Court took it under the sea. Since then the tides have wandered, like dogs without a whistle." },
    { "The Harbour Wall", "The breakwater was built in a single night by fishermen who were very, very tired of losing boats. The lighthouse was built the next morning, by the same fishermen, who were even more tired." },
    { "The Blossom Tree", "Brinewick's great tree was planted by the village's first child. It flowers whenever someone in the village is in love, which, in Brinewick, is always." },
    { "The Lantern Shrine", "Three lanterns on the cliff show the tide the way home. When they go dark, the tide forgets, and the fish swim off to sulk." },
    { "The Covenant", "Carved by the first survivors of the fire: WHAT OLD EMBER TAKES, THE COVENANT RETURNS. WHAT THE COVENANT RETURNS, OLD EMBER WILL TAKE. KEEP YOUR SHELLS IN YOUR POCKETS." },
    { "Warning", "The air of the crater is poison. Those who climb without a mask come down coughing, or do not come down at all. Bring a mask. Bring two." },
    { "The Queen Under the Sea", "The Court did not drown. Its queen, weary of the land's quarrels, walked into the sea with her whole household, and the sea made room. Her bell was the promise that she would listen." },
    { "The Bell Gallery", "The queen's song, as carved on the gallery wall: first the second bell, then the lowest, then the highest, and last of all the third. Ring it so, and the gallery will remember her." },
    { "The Tide Engine", "When the first tide rises and the second falls, the third must follow the first. Only then will the engine's heart turn." },
    { "The Pearl Garden", "Stand on the shell of beginnings and the pearls will show their order. Walk it back to them, stone by stone, and the garden will open its hand." },
    { "The Drowned King", "The queen's last guard never stopped guarding. He keeps the bell still, long after he forgot why. Be gentle with him, if you can. You can't." },
    { "The Walruses", "The walruses of the west rocks whistle to one another across the water. Fishermen swear they whistle shanties. Fishermen swear a lot of things." },
};

static const char *RELICS[12][2] = {
    { "The Tide-Singer's Whistle", "A whistle of pale bone. Blown on the first shore, it called the fish in from the deep for the first feast." },
    { "The Sand Clock", "A little hourglass of black sand that runs upward. The First Tide's people kept time by the sea, not the sun." },
    { "A Shell-Road Tile", "A mosaic tile from the oldest road on the coast. Press your ear to it and you can almost hear footsteps." },
    { "The Watcher's Eye", "A polished lens that once topped a tower of driftwood. The first lighthouse, long before the one that stands now." },
    { "The Keeper's Lamp-Wick", "A wick that has never burned down. The lighthouse still lights itself; maybe this is why." },
    { "An Anchor Charm", "Carved from coral and worn smooth by hands. Sailors of the First Tide carried one so the sea would bring them home." },
    { "The Cove Stone", "A flat stone with a hole worn through it by waves. Look through it and the cove looks the way it did a thousand years ago." },
    { "A Toast-Cup", "A tiny cup, shaped like a scallop. Every Brinewick wedding still ends with a toast to the tide from one like it." },
    { "The Cinder Seal", "A clay seal fired black by the very first eruption. On it, a mountain and a wave, holding hands." },
    { "A Net Needle", "The needle of the first net-mender. The knots it taught are the same ones Hollis ties today." },
    { "The Queen's Comb", "A comb of mother-of-pearl, left on the shore the night the Court walked into the sea." },
    { "The Last Tablet", "Words in a tongue nobody speaks: WHO GATHERS THE TWELVE WILL MEET THE ONE WHO DOES NOT MOOR." },
};

static const char *MAIN_STAGES[11] = {
    "Rumour has it there's a fishing village, Brinewick, to the west of the ruins. Go and see.",
    "Relight Brinewick's three shrine lanterns on the cliff (ask Pell the lamplighter how).",
    "Ask Harbourmaster Quill, at the marina east of the beach, what became of the Tide Bell.",
    "Find Quill's three lost tide charts: on the walrus rocks, at the lighthouse, at the end of pier two.",
    "Catch a fish, any fish, and take it to Old Brine at the end of the long pier.",
    "Fetch Old Ember's heart from the volcano's crater (wear the gas mask!) and take it to Bram the smith.",
    "Hook the Conch of the Deep: a Leviathan rod, an ember lure, deep water, after dark.",
    "Blow the Conch of the Deep beside the bell buoy, far out to sea south of the beach.",
    "In the Drowned Court: wake the three seals (bells, levers, pearls), face the Drowned King, take the Tide Bell.",
    "Hang the Tide Bell in Brinewick's bell tower.",
    "Done! Brinewick's bell rings the tides home again.",
};

/* ---------- models for things ---------- */

static Model lever_model, plate_model, gate_model;

static void build_parchment(Model *m)
{
    MeshBuilder mb[2];
    Material mats[2];
    mb_init(&mb[0]);
    mb_init(&mb[1]);
    material_color(&mats[0], 0.85f, 0.78f, 0.6f, 0.9f, 0.0f);
    material_color(&mats[1], 0.6f, 0.1f, 0.08f, 0.7f, 0.0f);
    mat4 xf;
    glm_translate_make(xf, (vec3){-0.16f, 0.03f, 0});
    glm_rotate_z(xf, -GLM_PI_2f, xf);
    mb_cylinder(&mb[0], xf, 0.03f, 0.03f, 0.32f, 12, 1.0f);
    glm_translate_make(xf, (vec3){0.0f, 0.03f, 0});
    glm_rotate_z(xf, -GLM_PI_2f, xf);
    glm_translate(xf, (vec3){0, -0.012f, 0});
    mb_cylinder(&mb[1], xf, 0.033f, 0.033f, 0.024f, 12, 1.0f);
    model_from_builders(m, mb, mats, 2);
    mb_free(&mb[0]);
    mb_free(&mb[1]);
}

static void build_puzzle_models(void)
{
    MeshBuilder mb[3];
    Material mats[3];
    for (int i = 0; i < 3; i++)
        mb_init(&mb[i]);
    material_color(&mats[0], 0.35f, 0.3f, 0.28f, 0.6f, 0.0f);
    material_color(&mats[1], 0.8f, 0.6f, 0.25f, 0.25f, 1.0f);
    material_color(&mats[2], 0.1f, 0.3f, 0.3f, 0.3f, 0.0f);
    glm_vec3_copy((vec3){0.3f, 1.6f, 1.8f}, mats[2].emissive);
    mat4 xf;
    /* a lever: a brass handle on a pivot (drawn swung up or down), knob glowing */
    glm_translate_make(xf, (vec3){0, 0.45f, 0});
    mb_box(&mb[1], xf, (vec3){0.04f, 0.45f, 0.04f}, 1.0f);
    glm_translate_make(xf, (vec3){0, 0.92f, 0});
    glm_scale(xf, (vec3){0.08f, 0.08f, 0.08f});
    mb_capsule(&mb[2], xf, 1.0f, 0.0f, 10);
    model_from_builders(&lever_model, mb, mats, 3);
    for (int i = 0; i < 3; i++) {
        mb_free(&mb[i]);
        mb_init(&mb[i]);
    }
    /* a pressure plate: a shell-shaped stone disc */
    glm_mat4_identity(xf);
    mb_cylinder(&mb[0], xf, 0.75f, 0.7f, 0.08f, 20, 1.0f);
    glm_translate_make(xf, (vec3){0, 0.08f, 0});
    mb_cylinder(&mb[2], xf, 0.5f, 0.5f, 0.01f, 20, 1.0f);
    model_from_builders(&plate_model, mb, mats, 3);
    for (int i = 0; i < 3; i++) {
        mb_free(&mb[i]);
        mb_init(&mb[i]);
    }
    /* the throne room's gate: a coral slab carved with three round seals */
    Material coral;
    material_from_dir(&coral, "assets/textures/coral_fort_wall_01");
    mats[0] = coral;
    glm_translate_make(xf, (vec3){0, 2.5f, 0});
    mb_box(&mb[0], xf, (vec3){3.0f, 2.5f, 0.35f}, 2.0f);
    for (int k = 0; k < 3; k++) {
        glm_translate_make(xf, (vec3){(k - 1) * 1.6f, 3.2f, 0.36f});
        glm_rotate_x(xf, GLM_PI_2f, xf);
        mb_cylinder(&mb[1], xf, 0.5f, 0.5f, 0.05f, 20, 1.0f);
    }
    model_from_builders(&gate_model, mb, mats, 2);
    for (int i = 0; i < 3; i++)
        mb_free(&mb[i]);
}

void things_load(Game *g)
{
    static const float colors[SHELL_KINDS][3] = {
        { 0.9f, 0.88f, 0.82f }, { 0.95f, 0.55f, 0.65f }, { 0.35f, 0.6f, 0.95f }, { 0.95f, 0.72f, 0.2f } };
    for (int i = 0; i < SHELL_KINDS; i++)
        shape_scallop(&g->shell_models[i], (float *)colors[i], i == SHELL_GOLD ? (vec3){0.4f, 0.28f, 0.02f} : NULL);
    shape_charm(&g->relic_model, (vec3){0.75f, 0.72f, 0.62f}, (vec3){0.3f, 1.4f, 1.5f});
    build_parchment(&g->chart_model);
    shape_bell(&g->bell_model, 1.0f, (vec3){0.55f, 0.62f, 0.5f});
    shape_crystal(&g->crystal_model, 0.5f, (vec3){0.9f, 0.3f, 0.05f}, (vec3){7.0f, 2.0f, 0.2f});
    shape_bobber(&g->bobber_model);
    build_puzzle_models();
}

void things_free(Game *g)
{
    for (int i = 0; i < SHELL_KINDS; i++)
        model_free(&g->shell_models[i]);
    model_free(&g->relic_model);
    model_free(&g->chart_model);
    model_free(&g->bell_model);
    model_free(&g->crystal_model);
    model_free(&g->bobber_model);
    model_free(&lever_model);
    model_free(&plate_model);
    model_free(&gate_model);
}

Thing *thing_add(Game *g, ThingKind kind, int index, vec3 pos, float yaw)
{
    if (g->thing_count >= MAX_THINGS)
        return NULL;
    Thing *t = &g->things[g->thing_count++];
    *t = (Thing){ .used = true, .kind = kind, .index = index };
    t->xf = TRANSFORM_AT(pos, yaw);
    return t;
}

/* ---------- small helpers ---------- */

static void quest_note(Game *g, const char *fmt, const char *text)
{
    message(g, COL_GOLD, fmt, text);
    audio_play(SFX_QUEST, 1.0f);
}

static void advance(Game *g, int stage)
{
    g->quest.stage[Q_MAIN] = stage;
    if (stage <= 10) {
        char note[240];
        snprintf(note, sizeof note, "Journal: %s", MAIN_STAGES[stage]);
        message(g, COL_GOLD, "%s", note);
        audio_play(stage == 10 ? SFX_QUEST_DONE : SFX_QUEST, 1.0f);
    }
}

/* shells worth `value`, given as the fewest shells */
static void give_value(Game *g, int value)
{
    for (int i = SHELL_KINDS - 1; i >= 0; i--) {
        g->shells[i] += value / SHELL_VALUE[i];
        value %= SHELL_VALUE[i];
    }
    audio_play(SFX_SHELLS, 1.0f);
}

static int relics_found(const Game *g)
{
    int n = 0;
    for (int i = 0; i < 12; i++)
        n += g->quest.relics[i];
    return n;
}

static bool has_any_fish(const Game *g)
{
    for (int i = 0; i < INV_SLOTS; i++)
        if (g->slots[i].id != ITEM_NONE && ITEMS[g->slots[i].id].kind == KIND_FISH)
            return true;
    return false;
}

static ItemId first_fish(const Game *g)
{
    for (int i = 0; i < INV_SLOTS; i++)
        if (g->slots[i].id != ITEM_NONE && ITEMS[g->slots[i].id].kind == KIND_FISH)
            return g->slots[i].id;
    return ITEM_NONE;
}

static bool left_flame(const Game *g)
{
    return g->left == LEFT_TORCH || g->left == LEFT_LANTERN || g->slots[g->selected].id == ITEM_TOME_FIREBALL;
}

/* the Court's puzzles */
static const int CHIME_ORDER[4] = { 1, 0, 3, 2 };
static const int LEVER_GOAL[3] = { 1, 0, 1 };
static const int PEARL_ORDER[5] = { 3, 1, 4, 0, 2 };
static const float CHIME_PITCH[4] = { 0.75f, 1.0f, 1.26f, 1.5f };

static Thing *find_thing(Game *g, ThingKind kind, int index)
{
    for (int i = 0; i < g->thing_count; i++)
        if (g->things[i].used && g->things[i].kind == kind && g->things[i].index == index)
            return &g->things[i];
    return NULL;
}

static void solve(Game *g, int puzzle)
{
    if (g->quest.puzzles[puzzle])
        return;
    g->quest.puzzles[puzzle] = true;
    static const char *names[3] = { "the bell gallery", "the tide engine", "the pearl garden" };
    message(g, COL_MAGIC, "Somewhere deep in the Court, a seal begins to glow: %s is answered.", names[puzzle]);
    audio_play(SFX_QUEST, 1.0f);
    if (g->quest.puzzles[0] && g->quest.puzzles[1] && g->quest.puzzles[2] && !g->quest.court_open) {
        g->quest.court_open = true;
        if (court_gate_box >= 0)
            g->level.boxes[court_gate_box].off = true;
        audio_play(SFX_GATE, 1.0f);
        message(g, COL_GOLD, "All three seals blaze. With a sound like the tide turning, the throne room's gate sinks into the floor.");
    }
}

/* ---------- using things ---------- */

const char *thing_prompt(Game *g, Thing *t)
{
    QuestState *q = &g->quest;
    static char buf[96];
    switch (t->kind) {
    case THING_SHELL: return t->done ? NULL : (snprintf(buf, sizeof buf, "Pick up the %s shell", SHELL_NAME[t->index]), buf);
    case THING_RELIC: return t->done ? NULL : "Take the relic";
    case THING_CHART: return t->done ? NULL : "Take the tide chart";
    case THING_LORE: return "Read the tablet";
    case THING_PEDESTAL:
        return q->donated[t->index] ? (snprintf(buf, sizeof buf, "Read: %s", RELICS[t->index][0]), buf) : NULL;
    case THING_SHRINE_LANTERN:
        if (t->done) return NULL;
        return left_flame(g) ? "Light the shrine lantern" : "A dark lantern (hold a flame to light it)";
    case THING_NOTICEBOARD: return "Read the notice board";
    case THING_WHIRLPOOL: return has_item(g, ITEM_CONCH) ? "Blow the Conch of the Deep" : NULL;
    case THING_EMBER_HEART: return t->done ? NULL : "Take Old Ember's heart";
    case THING_BELL_TOWER: return !q->bell_home && has_item(g, ITEM_TIDE_BELL) ? "Hang the Tide Bell" : q->bell_home ? "Ring the Tide Bell" : NULL;
    case THING_TREASURE: return NULL;       /* dug up with the spade */
    case THING_CHIME: return q->puzzles[0] ? NULL : "Ring the bell";
    case THING_LEVER: return q->puzzles[1] ? NULL : "Pull the lever";
    case THING_PLATE: return NULL;          /* stepped on */
    case THING_SEAL: return NULL;
    case THING_EXIT_CURRENT: return "Ride the rising current back to the surface";
    case THING_ANVIL: return NULL;
    default: return NULL;
    }
}

void reading_show(Game *g, const char *title, const char *text)
{
    snprintf(g->reading_title, sizeof g->reading_title, "%s", title);
    snprintf(g->reading, sizeof g->reading, "%s", text);
    g->reading_t = 14.0f;
}

static void start_whirlpool(Game *g)
{
    QuestState *q = &g->quest;
    if (q->whirl_arrive > 0.0f)
        return;
    q->whirl_arrive = 7.0f;
    audio_play(SFX_CONCH, 1.0f);
    audio_play(SFX_WHIRLPOOL, 1.0f);
    message(g, COL_MAGIC, "The conch's note rolls out under the waves. The sea answers: it begins to turn...");
    if (q->stage[Q_MAIN] <= 7)
        q->stage[Q_MAIN] = 7;
}

void thing_use(Game *g, Thing *t)
{
    QuestState *q = &g->quest;
    switch (t->kind) {
    case THING_SHELL:
        if (t->done)
            return;
        t->done = true;
        add_shells(g, t->index, 1);
        audio_play(SFX_SHELLS, 0.8f);
        message(g, COL_FUN, "A %s shell. (worth %d)", SHELL_NAME[t->index], SHELL_VALUE[t->index]);
        if (q->job == 3)
            q->job_count++;
        break;
    case THING_RELIC:
        if (t->done)
            return;
        t->done = true;
        q->relics[t->index] = true;
        audio_play(SFX_QUEST, 1.0f);
        reading_show(g, RELICS[t->index][0], RELICS[t->index][1]);
        message(g, COL_GOLD, "Relic of the First Tide found: %s (%d / 12). Juniper at the Hall of Tides would treasure it.",
                RELICS[t->index][0], relics_found(g));
        if (q->stage[Q_RELICS] == 0)
            q->stage[Q_RELICS] = 1;
        break;
    case THING_CHART: {
        if (t->done)
            return;
        t->done = true;
        q->charts[t->index] = true;
        int have = q->charts[0] + q->charts[1] + q->charts[2];
        audio_play(SFX_PAGE, 1.0f);
        message(g, COL_GOLD, "A tide chart, soggy but readable. (%d / 3)", have);
        break;
    }
    case THING_LORE:
        if (t->index >= 0 && t->index < 16) {
            reading_show(g, LORE[t->index][0], LORE[t->index][1]);
            q->lore_read[t->index] = true;
            audio_play(SFX_PAGE, 0.8f);
        }
        break;
    case THING_PEDESTAL:
        if (q->donated[t->index])
            reading_show(g, RELICS[t->index][0], RELICS[t->index][1]);
        break;
    case THING_SHRINE_LANTERN:
        if (t->done || !left_flame(g)) {
            if (!t->done)
                message(g, COL_TEXT, "Cold and dark. You'll need a torch or a lantern in your off hand (T).");
            return;
        }
        t->done = true;
        q->lanterns[t->index] = true;
        audio_play_at(SFX_TORCH_ON, t->pos, 1.0f);
        {
            LightSource *l = spawn_light(g, LIGHT_LANTERN, (vec3){t->pos[0], t->pos[1] - 0.3f, t->pos[2]}, (vec3){0, 1, 0}, 0, true);
            if (l)
                l->empty = true;
        }
        fx_magic(&g->ps, t->pos, (vec3){4.0f, 2.5f, 0.8f}, 0.4f, 40);
        if (q->lanterns[0] && q->lanterns[1] && q->lanterns[2])
            message(g, COL_GOLD, "All three shrine lanterns burn. Far below, the tide seems to settle. Tell Elder Maren.");
        else
            message(g, COL_TEXT, "The shrine lantern catches with a soft whoomph.");
        break;
    case THING_NOTICEBOARD: {
        static const char *jobs[4][2] = {
            { "WANTED: fish for the Salted Eel. Bring Tamsin three fish. -- T.", "Deliver 3 fish to Tamsin at the Salted Eel" },
            { "CRABS! Three giant ones on the beach again. Somebody please. -- the lifeguard", "Clear 3 giant crabs off the beach" },
            { "The walruses look hungry. Feed one a fish. -- a concerned citizen", "Feed a walrus a fish" },
            { "Tidy the beach: pick up ten shells, keep them, tell nobody. -- Coralie", "Pick up 10 shells from the sand" },
        };
        static const int goals[4] = { 3, 3, 1, 10 }, pays[4] = { 14, 16, 8, 6 };
        if (q->stage[Q_JOB] == 1 && q->job_count >= q->job_goal) {
            give_value(g, pays[q->job]);
            message(g, COL_GOLD, "Job done! The notice board has a little pouch pinned to it for you: %d shells' worth.", pays[q->job]);
            audio_play(SFX_QUEST_DONE, 1.0f);
            q->stage[Q_JOB] = 0;
            q->job = (q->job + 1) % 4;
            return;
        }
        if (q->stage[Q_JOB] == 1) {
            char text[200];
            snprintf(text, sizeof text, "%s\n\nSo far: %d of %d.", jobs[q->job][0], q->job_count < q->job_goal ? q->job_count : q->job_goal, q->job_goal);
            reading_show(g, "The notice board", text);
            return;
        }
        q->stage[Q_JOB] = 1;
        q->job_count = 0;
        q->job_goal = goals[q->job];
        reading_show(g, "The notice board", jobs[q->job][0]);
        quest_note(g, "New job: %s", jobs[q->job][1]);
        break;
    }
    case THING_WHIRLPOOL:
        if (has_item(g, ITEM_CONCH))
            start_whirlpool(g);
        break;
    case THING_EMBER_HEART:
        if (t->done)
            return;
        t->done = true;
        game_give(g, ITEM_EMBER_HEART, 1);
        audio_play(SFX_QUEST, 1.0f);
        hurt_player(g, t->pos, 4.0f);
        message(g, COL_GOLD, "You prise Old Ember's heart from its altar. It's hot enough to sting, even through your glove. Take it to Bram.");
        break;
    case THING_BELL_TOWER:
        if (!q->bell_home && has_item(g, ITEM_TIDE_BELL)) {
            take_items(g, ITEM_TIDE_BELL, 1);
            q->bell_home = true;
            q->bell_ring_t = 0.01f;
            q->festival_t = 180.0f;
            audio_play(SFX_BELL, 1.0f);
            advance(g, 10);
            message(g, COL_GOLD, "The Tide Bell swings in its tower and rings out over the water. Brinewick comes running.");
            game_give(g, ITEM_TOME_TIDE, 1);
            give_value(g, 100);
            message(g, COL_MAGIC, "Elder Maren presses a tome into your hands: the Tome of Tides. And a purse of shells.");
            for (int i = 0; i < g->npc_count; i++)
                if (g->npcs[i].kind >= NPC_ELDER && g->npcs[i].kind != NPC_ANCIENT)
                    g->npcs[i].cheering = true;
        } else if (q->bell_home) {
            q->bell_ring_t = 0.01f;
            audio_play(SFX_BELL, 1.0f);
        }
        break;
    case THING_TREASURE: {
        if (t->done)
            return;
        t->done = true;
        audio_play(SFX_DIG, 1.0f);
        fx_dust(&g->ps, t->pos, 10);
        static const ItemId finds[6] = { ITEM_TOME_SPIKES, ITEM_ELEPHANT, ITEM_BOTTLE, ITEM_POMEGRANATE, ITEM_GILL_PEARL, ITEM_BOTTLE };
        ItemId found = finds[t->index % 6];
        if (found == ITEM_TOME_SPIKES && has_item(g, ITEM_TOME_SPIKES))
            found = ITEM_BOTTLE;
        game_give(g, found, 1);
        give_value(g, 20 + t->index * 5);
        message(g, COL_GOLD, "You dig up a little box, crusted with salt: %s, and a handful of shells!", ITEMS[found].name);
        break;
    }
    case THING_CHIME: {
        if (q->puzzles[0])
            return;
        t->anim = 1.0f;
        audio_play_at_pitch(SFX_CHIME, t->pos, 1.0f, CHIME_PITCH[t->index]);
        if (CHIME_ORDER[q->chime_step] == t->index) {
            if (++q->chime_step == 4)
                solve(g, 0);
        } else {
            q->chime_step = CHIME_ORDER[0] == t->index ? 1 : 0;
            if (q->chime_step == 0)
                message(g, COL_TEXT, "The note hangs wrong in the water-heavy air. The gallery waits for the right song.");
        }
        break;
    }
    case THING_LEVER:
        if (q->puzzles[1])
            return;
        q->levers[t->index] = !q->levers[t->index];
        audio_play_at(SFX_LEVER, t->pos, 1.0f);
        if (q->levers[0] == LEVER_GOAL[0] && q->levers[1] == LEVER_GOAL[1] && q->levers[2] == LEVER_GOAL[2]) {
            audio_play(SFX_GATE, 0.8f);
            solve(g, 1);
        }
        break;
    case THING_EXIT_CURRENT:
        audio_play(SFX_BUBBLES, 1.0f);
        respawn_player_at(g, (vec3){WHIRL_X + 4.0f, SEA_Y - 1.2f, WHIRL_Z + 6.0f}, g->cam.fp_yaw);
        g->hp = g->max_hp;
        g->court_t = 1.5f;
        message(g, COL_MAGIC, "The current lifts you, spinning, up and up, and spits you out into the daylight.");
        break;
    default:
        break;
    }
}

/* ---------- the conch, and people the quests care about ---------- */

static void conch(Game *g)
{
    Thing *w = find_thing(g, THING_WHIRLPOOL, 0);
    if (w && glm_vec3_distance((vec3){g->pos[0], w->pos[1], g->pos[2]}, w->pos) < 30.0f) {
        start_whirlpool(g);
        return;
    }
    audio_play(SFX_CONCH, 0.9f);
    message(g, COL_TEXT, "The conch's deep note rolls out and fades. Nothing answers here; the bell buoy is far out to sea, south of the beach.");
}

void quest_talk(Game *g, Npc *n)
{
    if (!n) {
        conch(g);
        return;
    }
    QuestState *q = &g->quest;
    int st = q->stage[Q_MAIN];
    switch (n->kind) {
    case NPC_ELDER:
        if (st == 0) {
            npc_say(g, n, "start");
            advance(g, 1);
        } else if (st == 1) {
            if (q->lanterns[0] && q->lanterns[1] && q->lanterns[2]) {
                npc_say(g, n, "lit");
                give_value(g, 15);
                advance(g, 2);
            } else
                npc_say(g, n, "lanterns");
        } else if (st >= 10) {
            npc_say(g, n, q->festival_t > 0.0f ? "bell" : "after");
        } else
            npc_talk(g, n);
        return;
    case NPC_LAMPLIGHTER:
        if (st == 1)
            npc_say(g, n, "how");
        else
            npc_talk(g, n);
        return;
    case NPC_HARBOURMASTER:
        if (st == 2) {
            npc_say(g, n, "charts");
            advance(g, 3);
        } else if (st == 3) {
            if (q->charts[0] && q->charts[1] && q->charts[2]) {
                npc_say(g, n, "done");
                game_give(g, ITEM_ROD_DRIFTWOOD, 1);
                game_give(g, ITEM_BAIT_WORM, 8);
                advance(g, 4);
            } else
                npc_say(g, n, "waiting");
        } else
            npc_talk(g, n);
        return;
    case NPC_FISHER:
        if (q->stage[Q_BIG_ONE] < 2 && has_item(g, ITEM_FISH_GOLDEN)) {
            npc_say(g, n, "bigdone");
            take_items(g, ITEM_FISH_GOLDEN, 1);
            game_give(g, ITEM_ROD_CORAL, 1);
            q->stage[Q_BIG_ONE] = 2;
            audio_play(SFX_QUEST_DONE, 1.0f);
        } else if (st == 4) {
            if (has_any_fish(g)) {
                npc_say(g, n, "court");
                advance(g, 5);
            } else
                npc_say(g, n, "fish");
        } else if (st == 5 || st == 6) {
            if (q->stage[Q_BIG_ONE] == 0) {
                npc_say(g, n, "big");
                q->stage[Q_BIG_ONE] = 1;
            } else
                npc_say(g, n, "conch");
        } else
            npc_talk(g, n);
        return;
    case NPC_SMITH:
        if (st == 5 && has_item(g, ITEM_EMBER_HEART)) {
            npc_say(g, n, "heart");
            take_items(g, ITEM_EMBER_HEART, 1);
            game_give(g, ITEM_BAIT_EMBER, 3);
            advance(g, 6);
            return;
        }
        if (st == 5 && !n->talk_t)
            npc_say(g, n, "need");
        else
            npc_talk(g, n);
        shop_open(g, n);
        return;
    case NPC_TAVERN:
        if (q->stage[Q_JOB] == 1 && q->job == 0 && has_any_fish(g)) {
            ItemId f = first_fish(g);
            take_items(g, f, 1);
            q->job_count++;
            message(g, COL_TEXT, "Tamsin takes your %s with a grin. (%d / %d for the notice board)", ITEMS[f].name,
                    q->job_count < q->job_goal ? q->job_count : q->job_goal, q->job_goal);
            audio_play(SFX_PICKUP, 0.7f);
            return;
        }
        npc_talk(g, n);
        shop_open(g, n);
        return;
    case NPC_SHOPKEEPER:
        if (st == 6 && !has_item(g, ITEM_ROD_LEVIATHAN))
            npc_say(g, n, "rod");
        else
            npc_talk(g, n);
        shop_open(g, n);
        return;
    case NPC_CURATOR: {
        int given = 0;
        for (int i = 0; i < 12; i++)
            if (q->relics[i] && !q->donated[i]) {
                q->donated[i] = true;
                given++;
            }
        int all = 0;
        for (int i = 0; i < 12; i++)
            all += q->donated[i];
        if (given && all == 12) {
            npc_say(g, n, "all");
            q->stage[Q_RELICS] = 2;
            audio_play(SFX_QUEST_DONE, 1.0f);
        } else if (given) {
            npc_say(g, n, "relic");
            give_value(g, given * 10);
            message(g, COL_GOLD, "Juniper sets %d relic%s on %s pedestal%s and pays you a finder's fee. (%d / 12 on show)",
                    given, given > 1 ? "s" : "", given > 1 ? "their" : "its", given > 1 ? "s" : "", all);
        } else
            npc_talk(g, n);
        return;
    }
    case NPC_ANCIENT:
        if (q->stage[Q_RELICS] >= 2 && !q->eye_given) {
            npc_say(g, n, "gift");
            game_give(g, ITEM_FIRST_TIDE_EYE, 1);
            q->eye_given = true;
            q->stage[Q_RELICS] = 3;
            audio_play(SFX_QUEST_DONE, 1.0f);
        } else if (q->eye_given)
            npc_say(g, n, "after");
        else
            npc_talk(g, n);
        return;
    default:
        npc_talk(g, n);
        return;
    }
}

void quest_fish_caught(Game *g, ItemId fish)
{
    QuestState *q = &g->quest;
    q->fish_caught++;
    if (fish == ITEM_CONCH) {
        q->conch_caught = true;
        if (q->stage[Q_MAIN] <= 6)
            advance(g, 7);
    }
}

void quest_creature_killed(Game *g, Creature *c)
{
    QuestState *q = &g->quest;
    if (c->type == CR_CRAB && q->stage[Q_JOB] == 1 && q->job == 1)
        q->job_count++;
    if (c->type == CR_KING && !q->king_dead) {
        q->king_dead = true;
        vec3 dais = { PALACE_X, PALACE_FLOOR + 0.8f, PALACE_Z + 36.0f };
        spawn_pickup(g, ITEM_TIDE_BELL, 1, dais);
        spawn_pickup(g, ITEM_TRIDENT, 1, (vec3){dais[0] + 2.0f, dais[1] - 0.6f, dais[2] - 1.5f});
        message(g, COL_GOLD, "The Drowned King sinks down, and for a moment looks almost grateful. On the dais, the Tide Bell hums.");
        if (q->stage[Q_MAIN] <= 8)
            advance(g, 9);
        audio_play(SFX_QUEST_DONE, 1.0f);
    }
}

/* ---------- each frame ---------- */

void things_update(Game *g, float dt)
{
    QuestState *q = &g->quest;
    ItemId held = g->slots[g->selected].id;
    float nearest_treasure = 1e9f;
    for (int i = 0; i < g->thing_count; i++) {
        Thing *t = &g->things[i];
        if (!t->used)
            continue;
        t->anim = fmaxf(0.0f, t->anim - dt * 0.8f);
        float d = glm_vec3_distance(t->pos, g->pos);
        switch (t->kind) {
        case THING_SHELL:
            if (!t->done && d < 30.0f && frand() < dt * (t->index == SHELL_GOLD ? 3.0f : 0.5f))
                fx_sparkle(&g->ps, (vec3){t->pos[0], t->pos[1] + 0.05f, t->pos[2]});
            break;
        case THING_RELIC:
        case THING_CHART:
            if (!t->done && d < 40.0f && frand() < dt * 4.0f)
                fx_magic(&g->ps, t->pos, (vec3){0.4f, 1.8f, 2.0f}, 0.25f, 1);
            break;
        case THING_SHRINE_LANTERN:
            if (t->done && d < 60.0f)
                fx_flame(&g->ps, t->pos, 0.45f, dt);
            break;
        case THING_EMBER_HEART:
            if (!t->done && d < 80.0f && frand() < dt * 10.0f)
                fx_flame(&g->ps, (vec3){t->pos[0], t->pos[1] + 0.3f, t->pos[2]}, 0.3f, 0.05f);
            break;
        case THING_TREASURE:
            if (!t->done && d < nearest_treasure)
                nearest_treasure = d;
            break;
        case THING_EXIT_CURRENT:
            if (d < 40.0f && frand() < dt * 30.0f) {
                Particle p = {0};
                glm_vec3_copy((vec3){t->pos[0] + (frand() - 0.5f) * 1.6f, t->pos[1], t->pos[2] + (frand() - 0.5f) * 1.6f}, p.pos);
                glm_vec3_copy((vec3){0, 2.5f + frand() * 2.0f, 0}, p.vel);
                glm_vec4_copy((vec4){0.7f, 0.95f, 1.0f, 0.7f}, p.color0);
                glm_vec4_copy((vec4){0.7f, 0.95f, 1.0f, 0.0f}, p.color1);
                p.size0 = 0.05f;
                p.size1 = 0.12f;
                p.max_life = p.life = 2.0f;
                particles_emit(&g->ps, &p);
            }
            break;
        case THING_PLATE:
            /* stepping on the garden's plates */
            if (!q->puzzles[2] && d < 0.9f && fabsf(g->pos[1] - t->pos[1]) < 0.6f) {
                if (t->index == 5) {
                    if (q->show_t <= 0.0f && q->plate_step <= 0) {
                        q->show_t = 0.01f;
                        q->show_i = 0;
                        q->plate_step = 0;
                    }
                } else if (q->show_t <= 0.0f && t->glow <= 0.0f && q->show_i >= 5) {
                    t->glow = 0.6f;
                    audio_play_at_pitch(SFX_CHIME, t->pos, 0.8f, 0.8f + t->index * 0.15f);
                    if (PEARL_ORDER[q->plate_step] == t->index) {
                        if (++q->plate_step == 5)
                            solve(g, 2);
                    } else {
                        q->plate_step = 0;
                        q->show_i = 0;
                        message(g, COL_TEXT, "The pearls dim. Step on the shell of beginnings to see their order again.");
                    }
                }
            }
            t->glow = fmaxf(0.0f, t->glow - dt);
            break;
        default:
            break;
        }
    }
    /* the pearl garden shows its sequence, one plate at a time */
    if (q->show_t > 0.0f && !q->puzzles[2]) {
        q->show_t += dt;
        if (q->show_t > 0.9f) {
            q->show_t = 0.01f;
            if (q->show_i < 5) {
                Thing *p = find_thing(g, THING_PLATE, PEARL_ORDER[q->show_i]);
                if (p) {
                    p->glow = 0.7f;
                    audio_play_at_pitch(SFX_CHIME, p->pos, 0.9f, 0.8f + p->index * 0.15f);
                }
                q->show_i++;
            } else {
                q->show_t = 0.0f;
                message(g, COL_TEXT, "The pearls go dark. Now walk their order.");
            }
        }
    }
    /* the metal detector beeps faster near buried treasure */
    if (held == ITEM_METAL_DETECTOR && nearest_treasure < 25.0f) {
        g->detector_t -= dt;
        if (g->detector_t <= 0.0f) {
            audio_play_pitch(SFX_DETECTOR, 0.7f, 1.0f + (1.0f - nearest_treasure / 25.0f) * 0.8f);
            g->detector_t = 0.12f + nearest_treasure * 0.06f;
            if (nearest_treasure < 1.6f)
                message(g, COL_FUN, "BEEEEEP. Something's buried right here. Dig with a spade!");
        }
    }
}

void quest_update(Game *g, float dt)
{
    QuestState *q = &g->quest;
    g->reading_t = fmaxf(0.0f, g->reading_t - dt);
    g->court_t = fmaxf(0.0f, g->court_t - dt);
    if (q->festival_t > 0.0f && (q->festival_t -= dt) <= 0.0f)
        for (int i = 0; i < g->npc_count; i++)
            g->npcs[i].cheering = false;
    if (q->bell_ring_t > 0.0f)
        q->bell_ring_t = q->bell_ring_t > 4.0f ? 0.0f : q->bell_ring_t + dt;

    /* the whirlpool: it spins up, drags you in, and takes you down */
    if (q->whirl_arrive > 0.0f) {
        q->whirl_arrive -= dt;
        float strength = glm_clamp((7.0f - q->whirl_arrive) / 3.0f, 0.0f, 1.0f);
        glm_vec4_copy((vec4){WHIRL_X, WHIRL_Z, 22.0f, strength}, g->ocean.whirl);
        vec3 d = { WHIRL_X - g->pos[0], 0, WHIRL_Z - g->pos[2] };
        float r = glm_vec3_norm(d);
        if (r > 0.5f && r < 40.0f) {
            vec3 around = { -d[2] / r, 0, d[0] / r };
            glm_vec3_muladds(d, dt * strength * 0.6f / fmaxf(r, 1.0f) * 6.0f, g->pos);
            glm_vec3_muladds(around, dt * strength * 5.0f, g->pos);
            g->cam.fp_yaw += dt * strength * 0.8f;
        }
        if (q->whirl_arrive <= 0.0f) {
            if (g->boat_in >= 0) {
                g->boats[g->boat_in].occupied = false;
                g->boat_in = -1;
            }
            respawn_player_at(g, (vec3){PALACE_X, PALACE_FLOOR, COURT_ENTRY_Z}, 0.0f);
            g->cam.fp_yaw = GLM_PIf;     /* facing into the Court */
            g->hp = g->max_hp;
            g->court_t = 2.0f;
            glm_vec4_zero(g->ocean.whirl);
            if (q->stage[Q_MAIN] <= 8)
                advance(g, 8);
            message(g, COL_MAGIC, "Down and down, spinning, into the dark... and out, gasping, onto cold stone. The air here tastes of salt and pearls.");
            audio_play(SFX_GASP, 1.0f);
        }
    }
    /* the jobs that finish by themselves */
    if (q->stage[Q_JOB] == 1 && q->job == 2 && q->walrus_fed > 0 && q->job_count == 0)
        q->job_count = 1;
}

void quest_regenerate(Game *g)
{
    /* the Covenant brings the shells back to the sand */
    for (int i = 0; i < g->thing_count; i++)
        if (g->things[i].kind == THING_SHELL)
            g->things[i].done = false;
}

/* ---------- drawing things ---------- */

void things_draw(Game *g, GLuint prog, mat4 vp, bool depth)
{
    QuestState *q = &g->quest;
    DrawParams dp = { .depth_only = depth };
    for (int i = 0; i < g->thing_count; i++) {
        Thing *t = &g->things[i];
        if (!t->used || glm_vec3_distance(t->pos, g->eye) > (depth ? 40.0f : 90.0f))
            continue;
        mat4 xf;
        Transform tr = t->xf;
        const Model *m = NULL;
        DrawParams tp = dp;
        switch (t->kind) {
        case THING_SHELL:
            if (t->done || depth)
                continue;
            m = &g->shell_models[t->index];
            glm_vec3_fill(tr.scale, 1.6f);
            break;
        case THING_RELIC:
            if (t->done)
                continue;
            m = &g->relic_model;
            tr.pos[1] += 0.12f + sinf(g->time * 1.6f + i) * 0.03f;
            tr.yaw += g->time * 0.8f;
            glm_vec3_fill(tr.scale, 2.2f);
            break;
        case THING_CHART:
            if (t->done)
                continue;
            m = &g->chart_model;
            break;
        case THING_PEDESTAL:
            if (!q->donated[t->index])
                continue;
            m = &g->relic_model;
            tr.pos[1] += 0.2f;
            tr.yaw += g->time * 0.4f;
            glm_vec3_fill(tr.scale, 4.0f);
            break;
        case THING_EMBER_HEART:
            if (t->done)
                continue;
            m = &g->crystal_model;
            tr.yaw += g->time;
            break;
        case THING_BELL_TOWER:
            if (!q->bell_home || t->index != 0)
                continue;
            m = &g->bell_model;
            tr.roll = q->bell_ring_t > 0.0f ? sinf(q->bell_ring_t * 5.0f) * 0.5f * (1.0f - q->bell_ring_t / 4.0f) : 0.0f;
            break;
        case THING_CHIME: {
            m = &g->bell_model;
            glm_vec3_fill(tr.scale, 0.55f);
            tr.roll = sinf(g->time * 9.0f) * 0.35f * t->anim;
            static const float tints[4][3] = { { 0.1f, 0.0f, 0.12f }, { 0.0f, 0.08f, 0.12f }, { 0.0f, 0.12f, 0.04f }, { 0.12f, 0.06f, 0.0f } };
            if (!depth)
                glm_vec4_copy((vec4){tints[t->index][0] * (1 + t->anim * 8), tints[t->index][1] * (1 + t->anim * 8), tints[t->index][2] * (1 + t->anim * 8), 0}, tp.tint);
            break;
        }
        case THING_LEVER:
            m = &lever_model;
            tr.pitch = q->levers[t->index] ? -0.7f : 0.7f;
            break;
        case THING_PLATE:
            m = &plate_model;
            if (!depth)
                glm_vec4_copy((vec4){0.0f, t->glow * 3.0f + (q->puzzles[2] ? 0.3f : 0.0f), t->glow * 3.5f + (t->index == 5 ? 0.3f : 0.0f), 0}, tp.tint);
            break;
        case THING_SEAL:
            if (t->index == 3) {
                /* the gate: sinks away once the seals are woken */
                m = &gate_model;
                if (q->court_open)
                    tr.pos[1] -= fminf(5.2f, 5.2f);
                if (!depth) {
                    int lit = q->puzzles[0] + q->puzzles[1] + q->puzzles[2];
                    glm_vec4_copy((vec4){0.0f, lit * 0.25f, lit * 0.3f, 0}, tp.tint);
                }
                if (q->court_open)
                    continue;
            } else
                continue;
            break;
        default:
            continue;
        }
        transform_matrix(&tr, xf);
        model_draw(m, NULL, prog, vp, xf, &tp);
    }
}

/* ---------- the journal ---------- */

static const float COL_PANEL_Q[4] = { 0.05f, 0.04f, 0.03f, 0.9f };

/* text wrapped to `width` pixels; returns the height used */
static float wrapped(int font, float x, float y, float width, const float col[4], const char *text)
{
    char line[256];
    int len = 0;
    float lh = ui_line_height(font), yy = y;
    const char *p = text;
    while (*p) {
        const char *word = p;
        while (*p && *p != ' ' && *p != '\n')
            p++;
        int wl = (int)(p - word);
        char test[256];
        snprintf(test, sizeof test, "%.*s%s%.*s", len, line, len ? " " : "", wl, word);
        if (len && ui_text_width(font, test) > width) {
            line[len] = 0;
            ui_text(font, x, yy, col, line);
            yy += lh;
            len = snprintf(line, sizeof line, "%.*s", wl, word);
        } else {
            len = snprintf(line, sizeof line, "%s", test);
        }
        if (*p == '\n') {
            line[len] = 0;
            ui_text(font, x, yy, col, line);
            yy += lh;
            len = 0;
            p++;
        } else if (*p == ' ')
            p++;
    }
    if (len) {
        line[len] = 0;
        ui_text(font, x, yy, col, line);
        yy += lh;
    }
    return yy - y;
}

void journal_draw(Game *g)
{
    QuestState *q = &g->quest;
    float w = 900, h = 620;
    float x = (g->width - w) * 0.5f, y = (g->height - h) * 0.5f;
    ui_rect(x, y, w, h, COL_PANEL_Q);
    ui_frame(x, y, w, h, 2, COL_EDGE);
    ui_text(FONT_LARGE, x + 30, y + 12, COL_GOLD, "Journal");
    float dim[4] = { 0.95f, 0.9f, 0.8f, 0.6f };
    ui_text(FONT_SMALL, x + w - 30 - ui_text_width(FONT_SMALL, "J or Esc to close"), y + 26, dim, "J or Esc to close");

    float ty = y + 80, cw = w - 60;
    ui_text(FONT_MEDIUM, x + 30, ty, COL_GOLD, "The Ember and the Tide");
    ty += 36;
    ty += wrapped(FONT_SMALL, x + 50, ty, cw - 20, COL_TEXT, MAIN_STAGES[q->stage[Q_MAIN]]) + 14;

    char buf[256];
    int found = relics_found(g), shown = 0;
    for (int i = 0; i < 12; i++)
        shown += q->donated[i];
    ui_text(FONT_MEDIUM, x + 30, ty, COL_GOLD, "Relics of the First Tide");
    ty += 36;
    if (q->stage[Q_RELICS] == 3)
        snprintf(buf, sizeof buf, "All twelve gathered, and the Unmoored's gift is yours.");
    else if (q->stage[Q_RELICS] == 2)
        snprintf(buf, sizeof buf, "All twelve are on show. Seek the Unmoored at the lighthouse, on a clear night.");
    else
        snprintf(buf, sizeof buf, "%d of 12 found, %d on show in the Hall of Tides (the promenade's west end).", found, shown);
    ty += wrapped(FONT_SMALL, x + 50, ty, cw - 20, COL_TEXT, buf) + 14;

    ui_text(FONT_MEDIUM, x + 30, ty, COL_GOLD, "Around the coast");
    ty += 36;
    snprintf(buf, sizeof buf, "The walruses: %d fed%s.   Old Brine's big one: %s.   Fish caught: %d.   Tablets read: %d / 16.",
             q->walrus_fed, q->stage[Q_WALRUS] >= 2 ? " (they sang for you)" : " of 5",
             q->stage[Q_BIG_ONE] == 2 ? "landed" : q->stage[Q_BIG_ONE] == 1 ? "a golden snapper" : "?",
             q->fish_caught, (int)(q->lore_read[0] + q->lore_read[1] + q->lore_read[2] + q->lore_read[3] + q->lore_read[4] +
                                   q->lore_read[5] + q->lore_read[6] + q->lore_read[7] + q->lore_read[8] + q->lore_read[9] +
                                   q->lore_read[10] + q->lore_read[11] + q->lore_read[12] + q->lore_read[13] +
                                   q->lore_read[14] + q->lore_read[15]));
    ty += wrapped(FONT_SMALL, x + 50, ty, cw - 20, COL_TEXT, buf) + 6;
    if (q->stage[Q_JOB] == 1) {
        static const char *jobs[4] = { "Deliver fish to Tamsin", "Clear giant crabs off the beach", "Feed a walrus", "Pick up shells from the sand" };
        snprintf(buf, sizeof buf, "Notice board job: %s (%d / %d).", jobs[q->job], q->job_count < q->job_goal ? q->job_count : q->job_goal, q->job_goal);
        ty += wrapped(FONT_SMALL, x + 50, ty, cw - 20, COL_TEXT, buf) + 6;
    }
    buf[0] = 0;
    if (g->selkie) strcat(buf, "Selkie swimming.  ");
    if (g->gill_pearl) strcat(buf, "Deep lungs.  ");
    if (g->dolphin) strcat(buf, "Dolphin's ease.  ");
    if (g->angler) strcat(buf, "The angler's light.  ");
    if (g->first_tide) strcat(buf, "The Eye of the First Tide.  ");
    if (buf[0]) {
        char gifts[300];
        snprintf(gifts, sizeof gifts, "Gifts of the sea: %s", buf);
        ty += wrapped(FONT_SMALL, x + 50, ty, cw - 20, COL_MAGIC, gifts) + 6;
    }
    int left = (int)(ERUPTION_PERIOD - g->volcano.t);
    if (g->volcano.phase == VOLC_CALM || g->volcano.phase == VOLC_STIRRING)
        snprintf(buf, sizeof buf, "Old Ember will erupt in about %d:%02d. Shelter underground, or under the sea.", left / 60, left % 60);
    else
        snprintf(buf, sizeof buf, "Old Ember is erupting! The Covenant will bring everything back.");
    ty += 8;
    wrapped(FONT_SMALL, x + 50, ty, cw - 20, COL_RED, buf);
}

/* ---------- reading ---------- */

void reading_draw(Game *g)
{
    float w = 760, h = 260;
    float x = (g->width - w) * 0.5f, y = g->height * 0.18f;
    float a = fminf(1.0f, g->reading_t);
    float bg[4] = { 0.1f, 0.08f, 0.05f, 0.88f * a };
    ui_rect(x, y, w, h, bg);
    ui_frame(x, y, w, h, 2, COL_EDGE);
    ui_text(FONT_MEDIUM, x + 28, y + 16, COL_GOLD, g->reading_title);
    wrapped(FONT_SMALL, x + 28, y + 62, w - 56, COL_TEXT, g->reading);
    float dim[4] = { 0.95f, 0.9f, 0.8f, 0.5f };
    ui_text(FONT_SMALL, x + w - 28 - ui_text_width(FONT_SMALL, "E to close"), y + h - 30, dim, "E to close");
}

/* ---------- shops ---------- */

static int stock_for(int kind, ItemId *out)
{
    static const ItemId clam[] = { ITEM_ROD_BRASS, ITEM_ROD_CORAL, ITEM_ROD_LEVIATHAN, ITEM_BAIT_WORM, ITEM_BAIT_SHRIMP,
                                   ITEM_BAIT_GLOWWORM, ITEM_FISHER_HAT, ITEM_RUBBER_BOOTS, ITEM_GNOME, ITEM_UKULELE,
                                   ITEM_METAL_DETECTOR, ITEM_MAGNIFIER, ITEM_ELEPHANT, ITEM_SPADE, ITEM_BANANA, ITEM_LIME,
                                   ITEM_POMEGRANATE, ITEM_CROISSANT, ITEM_TOME_GILLS };
    static const ItemId forge[] = { ITEM_CUTLASS, ITEM_ESTOC, ITEM_KATANA, ITEM_MACHETE, ITEM_HATCHET, ITEM_BEARDED_AXE,
                                    ITEM_PICKAXE, ITEM_CROWBAR, ITEM_HARPOON, ITEM_IRON_HELM, ITEM_FUR_HAT, ITEM_KNIGHT_PLATE,
                                    ITEM_BARBARIAN_FURS, ITEM_RED_CAPE, ITEM_FUR_CLOAK, ITEM_DIVING_HELM };
    static const ItemId eel[] = { ITEM_CROISSANT, ITEM_POMEGRANATE, ITEM_BANANA, ITEM_LIME, ITEM_WIZARD_HAT,
                                  ITEM_MAGISTER_ROBES, ITEM_STARRY_CAPE, ITEM_TOME_SHADOW };
    const ItemId *src = kind == NPC_SMITH ? forge : kind == NPC_TAVERN ? eel : clam;
    int n = kind == NPC_SMITH ? (int)(sizeof forge / sizeof forge[0]) : kind == NPC_TAVERN ? (int)(sizeof eel / sizeof eel[0])
                                                                        : (int)(sizeof clam / sizeof clam[0]);
    memcpy(out, src, n * sizeof *out);
    return n;
}

static int sell_price(ItemId id)
{
    const ItemDef *it = &ITEMS[id];
    if (it->kind == KIND_FISH || id == ITEM_OLD_BOOT)
        return it->price;
    if (id == ITEM_EMBER_HEART || id == ITEM_TIDE_BELL || id == ITEM_CONCH || it->kind == KIND_SPELL || it->kind == KIND_ROD)
        return 0;
    return it->price / 2;
}

#define SHOP_W 980.0f
#define SHOP_H 640.0f
#define ROW 50.0f

static void shop_rect(Game *g, float *x, float *y)
{
    *x = (g->width - SHOP_W) * 0.5f;
    *y = (g->height - SHOP_H) * 0.5f;
}

void shop_open(Game *g, Npc *n)
{
    g->shop_npc = (int)(n - g->npcs);
    g->shop_scroll = 0;
    g->inv_open = false;
    audio_play(SFX_SHOP_BELL, 0.8f);
}

/* which row the mouse is over: buy rows are 0.., sell rows 100.. */
static int shop_hover(Game *g, ItemId *stock, int n, int *sell_slot)
{
    (void)stock;
    float x, y;
    shop_rect(g, &x, &y);
    float mx = g->mouse_x, my = g->mouse_y;
    for (int i = 0; i < n; i++) {
        float ry = y + 90 + i * ROW * 0.72f;
        if (mx > x + 20 && mx < x + SHOP_W * 0.55f && my > ry && my < ry + ROW * 0.7f)
            return i;
    }
    int row = 0;
    for (int s = 0; s < INV_SLOTS; s++) {
        ItemId id = g->slots[s].id;
        if (id == ITEM_NONE || sell_price(id) <= 0)
            continue;
        float ry = y + 90 + row * ROW * 0.72f;
        if (mx > x + SHOP_W * 0.58f && mx < x + SHOP_W - 20 && my > ry && my < ry + ROW * 0.7f) {
            *sell_slot = s;
            return 100 + row;
        }
        if (++row >= 14)
            break;
    }
    return -1;
}

bool shop_event(Game *g, const SDL_Event *e)
{
    if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat &&
        (e->key.key == SDLK_ESCAPE || e->key.key == SDLK_E || e->key.key == SDLK_TAB)) {
        g->shop_npc = -1;
        audio_play(SFX_CLICK, 1.0f);
        return true;
    }
    if (e->type == SDL_EVENT_MOUSE_MOTION)
        return false;       /* let the game track the pointer */
    if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == SDL_BUTTON_LEFT) {
        ItemId stock[32];
        int n = stock_for(g->npcs[g->shop_npc].kind, stock);
        int sell_slot = -1;
        int h = shop_hover(g, stock, n, &sell_slot);
        if (h >= 0 && h < 100) {
            ItemId id = stock[h];
            int price = ITEMS[id].price * (ITEMS[id].kind == KIND_BAIT ? 5 : 1);
            if (!pay_shells(g, price)) {
                message(g, COL_RED, "You can't afford the %s. (%d shells' worth; you have %d)", ITEMS[id].name, price, shell_total(g));
                audio_play(SFX_NO_MANA, 1.0f);
            } else {
                game_give(g, id, ITEMS[id].kind == KIND_BAIT ? 5 : 1);
                audio_play(SFX_BUY, 1.0f);
                message(g, COL_GOLD, "Bought: %s%s", ITEMS[id].name, ITEMS[id].kind == KIND_BAIT ? " x5" : "");
            }
        } else if (h >= 100 && sell_slot >= 0) {
            ItemId id = g->slots[sell_slot].id;
            int price = sell_price(id);
            if (--g->slots[sell_slot].count <= 0)
                g->slots[sell_slot] = (Slot){0};
            give_value(g, price);
            message(g, COL_TEXT, "Sold: %s for %d.", ITEMS[id].name, price);
        }
        return true;
    }
    return e->type == SDL_EVENT_MOUSE_BUTTON_UP || e->type == SDL_EVENT_MOUSE_WHEEL;
}

void shop_draw(Game *g)
{
    Npc *n = &g->npcs[g->shop_npc];
    float x, y;
    shop_rect(g, &x, &y);
    ui_rect(x, y, SHOP_W, SHOP_H, COL_PANEL_Q);
    ui_frame(x, y, SHOP_W, SHOP_H, 2, COL_EDGE);
    const char *title = n->kind == NPC_SMITH ? "Bram's Forge" : n->kind == NPC_TAVERN ? "The Salted Eel" : "The Curious Clam";
    ui_text(FONT_LARGE, x + 24, y + 10, COL_GOLD, title);
    char purse[64];
    snprintf(purse, sizeof purse, "Your shells are worth %d", shell_total(g));
    ui_text(FONT_SMALL, x + SHOP_W - 24 - ui_text_width(FONT_SMALL, purse), y + 22, COL_FUN, purse);
    float dim[4] = { 0.95f, 0.9f, 0.8f, 0.55f };
    ui_text(FONT_SMALL, x + 24, y + SHOP_H - 34, dim, "Click to buy or sell.  E or Esc to leave.");
    ui_text(FONT_SMALL, x + 24, y + 62, COL_EDGE, "For sale");
    ui_text(FONT_SMALL, x + SHOP_W * 0.58f, y + 62, COL_EDGE, "Sell what you carry");

    ItemId stock[32];
    int count = stock_for(n->kind, stock);
    int sell_slot = -1;
    int hover = shop_hover(g, stock, count, &sell_slot);
    for (int i = 0; i < count; i++) {
        ItemId id = stock[i];
        float ry = y + 90 + i * ROW * 0.72f;
        if (hover == i) {
            float hl[4] = { 0.3f, 0.22f, 0.1f, 0.6f };
            ui_rect(x + 20, ry, SHOP_W * 0.55f - 20, ROW * 0.7f, hl);
        }
        if (ITEMS[id].icon)
            ui_image(ITEMS[id].icon, x + 24, ry, ROW * 0.7f, ROW * 0.7f, NULL, true);
        int price = ITEMS[id].price * (ITEMS[id].kind == KIND_BAIT ? 5 : 1);
        char name[80];
        snprintf(name, sizeof name, "%s%s", ITEMS[id].name, ITEMS[id].kind == KIND_BAIT ? " (5)" : "");
        ui_text(FONT_SMALL, x + 70, ry + 4, shell_total(g) >= price ? COL_TEXT : COL_RED, name);
        char p[16];
        snprintf(p, sizeof p, "%d", price);
        ui_text(FONT_SMALL, x + SHOP_W * 0.55f - 10 - ui_text_width(FONT_SMALL, p), ry + 4, COL_GOLD, p);
    }
    int row = 0;
    for (int s = 0; s < INV_SLOTS; s++) {
        ItemId id = g->slots[s].id;
        if (id == ITEM_NONE || sell_price(id) <= 0)
            continue;
        float ry = y + 90 + row * ROW * 0.72f;
        if (hover == 100 + row) {
            float hl[4] = { 0.3f, 0.22f, 0.1f, 0.6f };
            ui_rect(x + SHOP_W * 0.58f, ry, SHOP_W * 0.42f - 20, ROW * 0.7f, hl);
        }
        if (ITEMS[id].icon)
            ui_image(ITEMS[id].icon, x + SHOP_W * 0.58f + 4, ry, ROW * 0.7f, ROW * 0.7f, NULL, true);
        char name[80];
        snprintf(name, sizeof name, "%s x%d", ITEMS[id].name, g->slots[s].count);
        ui_text(FONT_SMALL, x + SHOP_W * 0.58f + 50, ry + 4, COL_TEXT, name);
        char p[16];
        snprintf(p, sizeof p, "+%d", sell_price(id));
        ui_text(FONT_SMALL, x + SHOP_W - 30 - ui_text_width(FONT_SMALL, p), ry + 4, COL_FUN, p);
        if (++row >= 14)
            break;
    }
    /* a description of what's under the pointer */
    ItemId show = hover >= 0 && hover < 100 ? stock[hover] : hover >= 100 && sell_slot >= 0 ? g->slots[sell_slot].id : ITEM_NONE;
    if (show != ITEM_NONE)
        wrapped(FONT_SMALL, x + 24, y + SHOP_H - 80, SHOP_W - 48, COL_FUN, ITEMS[show].desc);
}
