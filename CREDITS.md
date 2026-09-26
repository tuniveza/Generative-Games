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
tombs, spell tomes, wall torches, key, and all the architecture are generated in code.

## Voices

The palace characters' lines were written for this game and spoken by
[Piper](https://github.com/rhasspy/piper) text-to-speech (run once, offline, by
tools/make_voices.sh; the results are in assets/voices):
- Magister Elowen: **en_GB-cori-high**, trained on public-domain LibriVox recordings
- Everyone else: **en_GB-vctk-medium**, trained on the **CSTR VCTK Corpus** by the Centre for
  Speech Technology Research, University of Edinburgh,
  [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)

## Poly Haven ([polyhaven.com](https://polyhaven.com)), all CC0 1.0

- Textures: mossy_stone_wall, medieval_blocks_02, castle_brick_07, mossy_cobblestone,
  monastery_stone_floor, old_planks_02, leafy_grass, rock_face, forest_ground_04,
  marble_mosaic_tiles, marble_01, white_sandstone_blocks_02, velour_velvet, herringbone_parquet,
  old_wood_floor, red_sandstone_wall, sandstone_blocks_08, mossy_sandstone
- Models: treasure_chest, large_castle_door, wooden_lantern_01, stone_fire_pit,
  ornate_medieval_dagger, ornate_medieval_mace, ornate_war_hammer, wooden_axe, sledgehammer_01,
  kite_shield, stick_grenade, rubber_duck_toy, pocket_watch, brass_goblets, seadogs_compass,
  old_gas_mask, vintage_binocular, boombox, round_spectacles, sweet_potato, CheeseBox_01,
  gothic_statue, boulder_01, antique_ceramic_vase_01, wooden_crate_01, Barrel_01, street_rat,
  fern_02, shrub_02, shrub_03, shrub_sorrel_01, flower_empodium, tree_stump_01,
  painted_wooden_bench, wooden_bookshelf_worn, book_encyclopedia_set_01, wooden_candlestick,
  vintage_oil_lamp, Chandelier_02, potted_plant_02, GothicCabinet_01, GothicCommode_01,
  ornate_mirror_01, horse_statue_01, WoodenTable_02, ArmChair_01

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
