"""
Pone malla y animaciones al personaje, y coloca blancos con forma humana.

BP_Jugador se creó sin malla: en primera persona no se nota, pero el arma
queda flotando ante la cámara y los demás jugadores no ven nada. Este
script le asigna el maniquí de la plantilla y su animation blueprint.

También sustituye las esferas de la fila cercana por maniquíes, que son un
blanco más realista para practicar puntería.

Ejecutar en la consola Python del editor:
    exec(open(r"<proyecto>/Content/Python/fix_personaje.py").read())
"""

import unreal

BP_JUGADOR = "/Game/Blueprints/Core/BP_Jugador"

# Malla del personaje, de preferida a alternativa.
CHARACTER_MESHES = (
    "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple",
    "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple",
    "/Game/Characters/Mannequins/Meshes/SK_Mannequin",
)

ANIM_BLUEPRINTS = (
    "/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed",
)


def log(message):
    unreal.log("[personaje] {}".format(message))


def load_first(paths):
    for path in paths:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset:
            return asset, path
    return None, None


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


def run():
    player_bp = unreal.EditorAssetLibrary.load_asset(BP_JUGADOR)
    if not player_bp:
        log("ERROR: falta BP_Jugador. Ejecuta antes build_training_zone.py")
        return

    mesh, mesh_path = load_first(CHARACTER_MESHES)
    if not mesh:
        log("ERROR: no se encontró ninguna malla de personaje")
        return
    log("malla: {}".format(mesh_path))

    anim_bp, anim_path = load_first(ANIM_BLUEPRINTS)
    if anim_bp:
        log("animaciones: {}".format(anim_path))
    else:
        log("AVISO: sin animation blueprint; el maniquí saldrá en pose T")

    cdo = unreal.get_default_object(blueprint_class(player_bp))
    if not cdo:
        log("ERROR: no se pudo obtener el objeto por defecto de BP_Jugador")
        return

    # El componente de malla del Character se llama siempre así.
    try:
        mesh_component = cdo.get_editor_property("mesh")
    except Exception as error:
        log("ERROR: sin componente de malla: {}".format(error))
        return

    try:
        mesh_component.set_editor_property("skeletal_mesh_asset", mesh)
    except Exception:
        # El nombre de la propiedad cambió entre versiones.
        try:
            mesh_component.set_editor_property("skeletal_mesh", mesh)
        except Exception as error:
            log("ERROR: no se pudo asignar la malla: {}".format(error))
            return
    log("BP_Jugador -> malla asignada")

    if anim_bp:
        try:
            anim_class = blueprint_class(anim_bp)
            if anim_class:
                mesh_component.set_editor_property("anim_class", anim_class)
                log("BP_Jugador -> animation blueprint")
        except Exception as error:
            log("AVISO: animaciones sin asignar: {}".format(error))

    # El maniquí nace mirando +Y y con el origen en los pies: hay que bajarlo
    # media cápsula y girarlo para que mire hacia delante.
    try:
        mesh_component.set_editor_property(
            "relative_location", unreal.Vector(0.0, 0.0, -92.0))
        mesh_component.set_editor_property(
            "relative_rotation", unreal.Rotator(0.0, 0.0, -90.0))
        log("BP_Jugador -> malla alineada con la cápsula")
    except Exception as error:
        log("AVISO: no se pudo alinear la malla: {}".format(error))

    # Recompilar para que los cambios lleguen al juego.
    library = getattr(unreal, "BlueprintEditorLibrary", None)
    if library and hasattr(library, "compile_blueprint"):
        try:
            library.compile_blueprint(player_bp)
        except Exception as error:
            log("AVISO: no se pudo recompilar: {}".format(error))

    unreal.EditorAssetLibrary.save_loaded_asset(player_bp)
    log("BP_Jugador recompilado y guardado")

    log("")
    log("Listo. Abre Lvl_Entrenamiento y pulsa Play.")


try:
    run()
except Exception as error:
    import traceback
    unreal.log_error("[personaje] FALLÓ: {}".format(error))
    unreal.log_error(traceback.format_exc())
