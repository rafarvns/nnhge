# ctest helper (spec 012 T5.6): the asset name each source file becomes in the packed folder and its index.
# Tiled maps become .nnhmap; Tiled's own files are not packed; Aseprite exports (`.ase.json`, spec 016 D31) become
# .nnhsprite and Aseprite's own files are not packed; everything else keeps its name.
include(${ASSETS_SCRIPT})
foreach(case "levels/level1.tmj|levels/level1.nnhmap"
             "levels/a.b.tmj|levels/a.b.nnhmap"
             "tiles/world.tsj|"
             "game.tiled-project|"
             "game.tiled-session|"
             "tiles/world.png|tiles/world.png"
             "levels/notes.tmj.txt|levels/notes.tmj.txt"
             "data/x.json|data/x.json"
             "art/hero.ase.json|art/hero.nnhsprite"
             "art/a.b.ase.json|art/a.b.nnhsprite"
             "art/hero.ase|"
             "art/hero.aseprite|"
             "art/hero.json|art/hero.json"
             "art/hero.ase.json.txt|art/hero.ase.json.txt"
             "models/ship.gltf|models/ship.nnhmesh"
             "models/ship.glb|models/ship.nnhmesh"
             "models/ship.bin|models/ship.bin")
    string(REPLACE "|" ";" case "${case}")
    list(GET case 0 name)
    list(LENGTH case n)
    set(expected "")
    if(n GREATER 1)
        list(GET case 1 expected)
    endif()
    nnh_asset_output_name("${name}" got)
    if(NOT got STREQUAL expected)
        message(FATAL_ERROR "${name}: expected \"${expected}\", got \"${got}\"")
    endif()
endforeach()
