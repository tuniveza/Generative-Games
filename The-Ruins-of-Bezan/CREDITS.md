# Credits

Two assets require attribution under their licenses; keep these credits with the game:
the **Fox** animation and the **VCTK voice data**.

## Characters and creatures

**Fox** (the ruin foxes), from the Khronos glTF Sample Assets.
- Model by **PixelMannen**, [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)
- Rigging & animation by **@tomkranis**, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
- glTF conversion by **@AsoboStudio and @scurest**, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)

**KayKit Character Packs: Skeletons and Adventurers** by **Kay Lousberg**
([kaylousberg.com](https://www.kaylousberg.com)), CC0 1.0. The skeletons, the goblins (the
hooded rogue, recolored in code), and the palace's characters.

**Avocado** (the Sacred Avocado), Khronos glTF Sample Assets, CC0 1.0.

The giant rats are Poly Haven's street_rat, scaled up and rigged in code. The slimes, hands,
tombs, spell tomes, wall torches, key, and all the architecture are generated in code, as are
the coast's walruses, crabs, gulls, turtles, fish, dolphins, jellyfish, sharks, eels, the
Drowned and their King, the boats, rods, bait, bells, the village, marina and Drowned Court.

## Voices

The palace and coast characters' lines were written for this game and spoken by
[Piper](https://github.com/rhasspy/piper) text-to-speech (run once, offline, by
tools/make_voices.sh; the results are in assets/voices):
- Magister Elowen and several Brinewick villagers: **en_GB-cori-high**, trained on public-domain LibriVox recordings
- Everyone else: **en_GB-vctk-medium**, trained on the **CSTR VCTK Corpus** by the Centre for
  Speech Technology Research, University of Edinburgh,
  [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)

## Poly Haven ([polyhaven.com](https://polyhaven.com)), all CC0 1.0

- Textures: mossy_stone_wall, medieval_blocks_02, castle_brick_07, mossy_cobblestone,
  monastery_stone_floor, old_planks_02, leafy_grass, rock_face, forest_ground_04,
  marble_mosaic_tiles, marble_01, white_sandstone_blocks_02, velour_velvet, herringbone_parquet,
  old_wood_floor, red_sandstone_wall, sandstone_blocks_08, mossy_sandstone,
  coast_sand_01, damp_beach_sand, blue_plaster_weathered, red_plaster_weathered, yellow_plaster,
  white_plaster_rough_01, thatch_roof_angled, clay_roof_tiles_02, shell_floor_01, weathered_planks,
  wood_floor_deck, seaworn_stone_tiles, seaworn_sandstone_brick, coral_stone_wall, coral_fort_wall_01,
  coral_ground_02, burned_ground_01, dark_rock, palm_bark, low_tide_rocks
- Models: treasure_chest, large_castle_door, wooden_lantern_01, stone_fire_pit,
  ornate_medieval_dagger, ornate_medieval_mace, ornate_war_hammer, wooden_axe, sledgehammer_01,
  kite_shield, stick_grenade, rubber_duck_toy, pocket_watch, brass_goblets, seadogs_compass,
  old_gas_mask, vintage_binocular, boombox, round_spectacles, sweet_potato, CheeseBox_01,
  gothic_statue, boulder_01, antique_ceramic_vase_01, wooden_crate_01, Barrel_01, street_rat,
  fern_02, shrub_02, shrub_03, shrub_sorrel_01, flower_empodium, tree_stump_01,
  painted_wooden_bench, wooden_bookshelf_worn, book_encyclopedia_set_01, wooden_candlestick,
  vintage_oil_lamp, Chandelier_02, potted_plant_02, GothicCabinet_01, GothicCommode_01,
  ornate_mirror_01, horse_statue_01, WoodenTable_02, ArmChair_01,
  dutch_ship_medium, dutch_ship_large_02, lifebuoy, ocean_buoy, lateral_sea_marker, cannon_01,
  bronze_whale_statue, bronze_shark_statue, bronze_ray_statue, marble_bust_01, lion_head,
  wooden_barrels_01, wooden_bucket_02, lambis_shell, CashRegister_01, fishermans_hat, rubber_boots,
  garden_gnome, Ukulele_01, magnifying_glass_01, metal_detector, bananas, food_lime_01,
  food_pomegranate_01, croissant, carved_wooden_elephant, wicker_basket_01, wooden_handle_saber,
  antique_estoc, antique_katana_01, machete, hatchet, wooden_axe_02, picke_dirty_01, rusted_spade_01,
  crowbar_01, brass_diya_lantern, Lantern_01, lantern_chandelier_01, wine_barrel_01,
  wooden_picnic_table, painted_wooden_chair_01, painted_wooden_table, painted_wooden_shelves,
  spinning_wheel_01, planter_box_01, tea_set_01, chess_set, moon_rock_01, moon_rock_03,
  ceramic_vase_02, potted_plant_04, rock_moss_set_01, weed_plant_02, wooden_bucket_01

Each asset's page is https://polyhaven.com/a/<name>; the exact download URLs are in
assets/SOURCES.txt (written by tools/fetch_assets.py).

## Music and sound

All music and sound effects are synthesized in code (audio.c).

## Fonts (SIL Open Font License 1.1)

- **Cinzel** by Natanael Gama (assets/fonts/OFL-Cinzel.txt)
- **Spectral** by Production Type (assets/fonts/OFL-Spectral.txt)

## Libraries

- [SDL3](https://libsdl.org), zlib license
- [glad](https://github.com/Dav1dde/glad) generated loader, (WTFPL or CC0) and Apache-2.0
- [cglm](https://github.com/recp/cglm) by Recep Aslantas, MIT (deps/cglm/LICENSE)
- [cgltf](https://github.com/jkuhlmann/cgltf) by Johannes Kuhlmann, MIT (deps/cgltf/LICENSE)
- [stb_image, stb_truetype, stb_image_write](https://github.com/nothings/stb) by Sean Barrett,
  public domain or MIT (deps/stb/LICENSE)
