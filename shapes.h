#ifndef SHAPES_H
#define SHAPES_H

#include "model.h"

/* Small models built in code, shared by items, fishing, the quests and the sea.
 * Each is made at a sensible real size, standing on or centered at the origin. */

/* a fish about `length` long along +Z (head at +Z), `fat` = body height / length */
void shape_fish(Model *m, vec3 back, vec3 belly, float length, float fat, vec3 glow);
/* a fan-ribbed scallop shell ~8 cm across, lying flat */
void shape_scallop(Model *m, vec3 color, vec3 glow);
/* a bronze bell hanging from its crown at the origin, `height` tall */
void shape_bell(Model *m, float height, vec3 color);
/* a glowing crystal (two cones point to point), standing on the origin */
void shape_crystal(Model *m, float height, vec3 color, vec3 glow);
/* a fishing rod along +Y: handle, reel, guides; tier 1..4 changes its look */
void shape_rod(Model *m, int tier);
/* the red and white float on the line */
void shape_bobber(Model *m);
/* a glass bottle with a rolled message in it */
void shape_bottle(Model *m);
/* a pearl (or an eye) `radius` big */
void shape_pearl(Model *m, float radius, vec3 color, vec3 glow);
/* a wooden-shafted harpoon and a three-pronged trident, along +Y */
void shape_harpoon(Model *m);
void shape_trident(Model *m);
/* a sword whose blade glows like embers, along +Y */
void shape_ember_blade(Model *m);
/* a brass diving helmet with round portholes, sized for a head */
void shape_diving_helm(Model *m);
/* baits: a worm, a curled shrimp, a glowing grub, a spoon lure that glows like a coal */
void shape_worm(Model *m, vec3 color, vec3 glow);
void shape_shrimp(Model *m);
void shape_lure(Model *m);
/* a limp green frond */
void shape_seaweed(Model *m);
/* a rowing boat, bow toward +Z, its keel at y = 0, ~3.6 m long */
void shape_boat(Model *m);
/* a small carved charm (selkie scale, dolphin) */
void shape_charm(Model *m, vec3 color, vec3 glow);

#endif
