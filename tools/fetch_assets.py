#!/usr/bin/env python3
"""Downloads the Poly Haven (CC0) assets the coast, village, volcano and undersea
palace use, in the layout the game loads them from:

    assets/textures/<id>/{albedo,normal,arm}.jpg
    assets/models/<id>/<id>.gltf (+ .bin and textures/)

Already-downloaded files are skipped. Every URL fetched is appended to
assets/SOURCES.txt so the README can list where everything came from.

Usage: tools/fetch_assets.py            (run from the project root)
"""
import json
import os
import sys
import urllib.request

API = "https://api.polyhaven.com"

# texture sets: id -> resolution
TEXTURES = {
    "coast_sand_01": "2k",          # the beach and the sea floor
    "damp_beach_sand": "1k",        # the wet strip where the waves run up
    "blue_plaster_weathered": "1k",  # Brinewick's chalky painted walls
    "red_plaster_weathered": "1k",
    "yellow_plaster": "1k",
    "white_plaster_rough_01": "1k",
    "thatch_roof_angled": "1k",
    "clay_roof_tiles_02": "1k",
    "shell_floor_01": "1k",         # shell-mosaic village lanes
    "weathered_planks": "1k",       # docks and piers
    "wood_floor_deck": "1k",        # the promenade boardwalk
    "seaworn_stone_tiles": "1k",    # the promenade
    "seaworn_sandstone_brick": "1k",  # sea walls, the lighthouse
    "coral_stone_wall": "1k",       # the undersea palace
    "coral_fort_wall_01": "1k",
    "coral_ground_02": "1k",
    "burned_ground_01": "1k",       # the volcano's slopes
    "dark_rock": "1k",
    "palm_bark": "1k",              # palm trunks
    "low_tide_rocks": "1k",         # rocks along the shore
}

MODELS = [
    # the marina and promenade
    "dutch_ship_medium", "dutch_ship_large_02", "lifebuoy", "ocean_buoy", "lateral_sea_marker",
    "cannon_01", "bronze_whale_statue", "bronze_shark_statue", "bronze_ray_statue",
    "marble_bust_01", "lion_head", "wooden_barrels_01", "wooden_bucket_02",
    # the beach and its shop
    "lambis_shell", "CashRegister_01", "fishermans_hat", "rubber_boots", "garden_gnome",
    "Ukulele_01", "magnifying_glass_01", "metal_detector", "bananas", "food_lime_01",
    "food_pomegranate_01", "croissant", "carved_wooden_elephant", "wicker_basket_01",
    # weapons and tools
    "wooden_handle_saber", "antique_estoc", "antique_katana_01", "machete", "hatchet",
    "wooden_axe_02", "picke_dirty_01", "rusted_spade_01", "crowbar_01",
    # the village
    "brass_diya_lantern", "Lantern_01", "lantern_chandelier_01",
    "wine_barrel_01", "wooden_picnic_table", "painted_wooden_chair_01", "painted_wooden_table",
    "painted_wooden_shelves", "spinning_wheel_01", "planter_box_01", "tea_set_01", "chess_set",
    # the volcano
    "moon_rock_01", "moon_rock_03",
]

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SOURCES = os.path.join(ROOT, "assets", "SOURCES.txt")


HEADERS = {"User-Agent": "bsg-fetch-assets/1.0 (+https://polyhaven.com)"}


def get_json(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS), timeout=60) as r:
        return json.load(r)


def fetch(url, dest):
    with open(SOURCES, "a") as log:
        log.write(url + "\n")
    if os.path.exists(dest):
        return
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    tmp = dest + ".part"
    with urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS), timeout=120) as r, \
         open(tmp, "wb") as f:
        f.write(r.read())
    os.replace(tmp, dest)
    print(dest)


def texture(tid, res):
    files = get_json(f"{API}/files/{tid}")
    out = os.path.join(ROOT, "assets", "textures", tid)
    for key, name in (("Diffuse", "albedo"), ("nor_gl", "normal"), ("arm", "arm")):
        fetch(files[key][res]["jpg"]["url"], os.path.join(out, name + ".jpg"))


def model(mid):
    files = get_json(f"{API}/files/{mid}")
    g = files["gltf"]["1k"]["gltf"]
    out = os.path.join(ROOT, "assets", "models", mid)
    fetch(g["url"], os.path.join(out, mid + ".gltf"))
    for rel, info in g["include"].items():
        fetch(info["url"], os.path.join(out, rel))


def main():
    os.makedirs(os.path.dirname(SOURCES), exist_ok=True)
    only = set(sys.argv[1:])
    for tid, res in TEXTURES.items():
        if not only or tid in only:
            texture(tid, res)
    for mid in MODELS:
        if not only or mid in only:
            model(mid)
    # keep the source list tidy: one line per URL
    with open(SOURCES) as f:
        lines = sorted(set(l.strip() for l in f if l.strip()))
    with open(SOURCES, "w") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
