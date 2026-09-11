"""
Asigna la malla y los datos que faltan al arma.

DA_Rifle se creó con los valores por defecto del DataAsset: sin malla, el
arma existe y dispara pero no se ve. Este script le pone la malla del rifle
de la plantilla y un nombre visible para el HUD.

Ejecutar en la consola Python del editor:
    exec(open(r"<proyecto>/Content/Python/fix_arma.py").read())
"""

import unreal

DA_RIFLE = "/Game/Weapons/DA_Rifle"
BP_RIFLE = "/Game/Blueprints/Weapons/BP_Rifle"
BP_JUGADOR = "/Game/Blueprints/Core/BP_Jugador"

# Mallas candidatas, de preferida a alternativa.
RIFLE_MESHES = (
    "/Game/Weapons/Rifle/Meshes/SKM_Rifle",
    "/Game/Weapons/Rifle/Meshes/SK_Rifle",
)


def log(message):
    unreal.log("[arma] {}".format(message))


def blueprint_class(blueprint):
    if not blueprint:
        return None
    try:
        generated = blueprint.generated_class()
        if generated:
            return generated
    except Exception:
        pass
    try:
        return unreal.load_object(None, blueprint.get_path_name() + "_C")
    except Exception:
        return None


def compile_blueprint(blueprint, label):
    if not blueprint:
        return
    library = getattr(unreal, "BlueprintEditorLibrary", None)
    if library and hasattr(library, "compile_blueprint"):
        try:
            library.compile_blueprint(blueprint)
        except Exception as error:
            log("AVISO: no se pudo recompilar {}: {}".format(label, error))
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)


def run():
    data = unreal.EditorAssetLibrary.load_asset(DA_RIFLE)
    if not data:
        log("ERROR: falta DA_Rifle. Ejecuta antes build_training_zone.py")
        return

    # --- Malla ---
    mesh = None
    for path in RIFLE_MESHES:
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if mesh:
            log("malla encontrada: {}".format(path))
            break

    if mesh:
        try:
            data.set_editor_property("weapon_mesh", mesh)
            log("DA_Rifle -> malla asignada")
        except Exception as error:
            log("AVISO: no se pudo asignar la malla: {}".format(error))
    else:
        log("AVISO: no se encontró ninguna malla de rifle")

    # --- Nombre visible, que el HUD muestra junto a la munición ---
    try:
        data.set_editor_property("display_name", unreal.Text("Rifle de asalto"))
        log("DA_Rifle -> nombre visible")
    except Exception as error:
        log("AVISO: no se pudo asignar el nombre: {}".format(error))

    unreal.EditorAssetLibrary.save_loaded_asset(data)

    # --- Verificación ---
    log("--- comprobación ---")
    for prop in ("display_name", "damage", "rounds_per_minute", "magazine_size"):
        try:
            log("  {:20} {}".format(prop, data.get_editor_property(prop)))
        except Exception:
            log("  {:20} (no legible)".format(prop))

    assigned = data.get_editor_property("weapon_mesh")
    log("  {:20} {}".format("weapon_mesh",
        assigned.get_name() if assigned else "NINGUNA"))

    # --- Recompilar lo que depende del arma ---
    rifle_bp = unreal.EditorAssetLibrary.load_asset(BP_RIFLE)
    if rifle_bp:
        cdo = unreal.get_default_object(blueprint_class(rifle_bp))
        if cdo:
            cdo.set_editor_property("weapon_data", data)
        compile_blueprint(rifle_bp, "BP_Rifle")
        log("BP_Rifle recompilado")

    compile_blueprint(unreal.EditorAssetLibrary.load_asset(BP_JUGADOR), "BP_Jugador")

    log("")
    log("Listo. Abre Lvl_Entrenamiento y pulsa Play.")


try:
    run()
except Exception as error:
    import traceback
    unreal.log_error("[arma] FALLÓ: {}".format(error))
    unreal.log_error(traceback.format_exc())
