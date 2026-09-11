"""
Aplica los valores a los Blueprints y los recompila.

Modificar el objeto por defecto (CDO) no basta: hasta que el Blueprint no se
recompila, el juego sigue usando los valores anteriores. Eso hacía que el
cronómetro corriera y que el nivel usara ShooterCharacter en vez de BP_Jugador.

Ejecutar en la consola Python del editor:
    exec(open(r"<proyecto>/Content/Python/fix_blueprints.py").read())
"""

import unreal

INPUT_DIR = "/Game/Input"
INPUT_ACTIONS_DIR = "/Game/Input/Actions"

BP_RIFLE = "/Game/Blueprints/Weapons/BP_Rifle"
BP_JUGADOR = "/Game/Blueprints/Core/BP_Jugador"
BP_MODO = "/Game/Blueprints/Core/BP_ModoJuego"
DA_RIFLE = "/Game/Weapons/DA_Rifle"

CHARACTER_ACTIONS = {
    "move_action": "IA_Move",
    "look_action": "IA_Look",
    "jump_action": "IA_Jump",
    "fire_action": "IA_Fire",
    "aim_action": "IA_Aim",
    "reload_action": "IA_Reload",
    "sprint_action": "IA_Sprint",
    "crouch_action": "IA_Crouch",
    "switch_weapon_action": "IA_SwitchWeapon",
}


def log(message):
    unreal.log("[fix] {}".format(message))


def blueprint_class(blueprint):
    """Clase generada por un Blueprint, probando las rutas válidas."""
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
    """
    Recompila y guarda. Sin esto los valores del CDO no llegan al juego.
    """
    if not blueprint:
        return

    compiled = False

    # La ruta para recompilar cambia entre versiones: se prueban las conocidas.
    library = getattr(unreal, "BlueprintEditorLibrary", None)
    if library and hasattr(library, "compile_blueprint"):
        try:
            library.compile_blueprint(blueprint)
            compiled = True
        except Exception as error:
            log("AVISO: compile_blueprint falló en {}: {}".format(label, error))

    if not compiled:
        try:
            unreal.KismetEditorUtilities.compile_blueprint(blueprint)
            compiled = True
        except Exception:
            pass

    if not compiled:
        # Sin recompilar, el guardado marca el asset como modificado y el
        # editor lo recompila al cargarlo.
        log("AVISO: {} no se pudo recompilar por script; ábrelo y pulsa "
            "Compilar si algo no responde".format(label))

    try:
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
        log("{} {}guardado".format(label, "recompilado y " if compiled else ""))
    except Exception as error:
        log("AVISO: no se pudo guardar {}: {}".format(label, error))


def run():
    rifle_bp = unreal.EditorAssetLibrary.load_asset(BP_RIFLE)
    player_bp = unreal.EditorAssetLibrary.load_asset(BP_JUGADOR)
    mode_bp = unreal.EditorAssetLibrary.load_asset(BP_MODO)
    weapon_data = unreal.EditorAssetLibrary.load_asset(DA_RIFLE)

    for asset, path in (
        (rifle_bp, BP_RIFLE), (player_bp, BP_JUGADOR), (mode_bp, BP_MODO)
    ):
        if not asset:
            log("ERROR: falta {}. Ejecuta antes build_training_zone.py".format(path))
            return

    # --- BP_Rifle ---
    rifle_cdo = unreal.get_default_object(blueprint_class(rifle_bp))
    if rifle_cdo and weapon_data:
        rifle_cdo.set_editor_property("weapon_data", weapon_data)
        log("BP_Rifle -> DA_Rifle")
    compile_blueprint(rifle_bp, "BP_Rifle")

    # --- BP_Jugador ---
    player_cdo = unreal.get_default_object(blueprint_class(player_bp))
    if player_cdo:
        contexts = []
        for name in ("IMC_Default", "IMC_Combate"):
            ctx = unreal.EditorAssetLibrary.load_asset("{}/{}".format(INPUT_DIR, name))
            if ctx:
                contexts.append(ctx)
        player_cdo.set_editor_property("default_mapping_contexts", contexts)

        assigned = 0
        for prop, action_name in CHARACTER_ACTIONS.items():
            action = unreal.EditorAssetLibrary.load_asset(
                "{}/{}".format(INPUT_ACTIONS_DIR, action_name)
            )
            if not action:
                continue
            try:
                player_cdo.set_editor_property(prop, action)
                assigned += 1
            except Exception as error:
                log("AVISO: {} sin asignar: {}".format(prop, error))
        log("BP_Jugador -> {} contextos, {} acciones".format(len(contexts), assigned))

        try:
            inventory = player_cdo.get_editor_property("inventory")
            rifle_class = blueprint_class(rifle_bp)
            if inventory and rifle_class:
                inventory.set_editor_property("default_weapons", [rifle_class])
                log("BP_Jugador -> arma inicial BP_Rifle")
        except Exception as error:
            log("AVISO: arma inicial sin asignar: {}".format(error))
    compile_blueprint(player_bp, "BP_Jugador")

    # --- BP_ModoJuego ---
    mode_cdo = unreal.get_default_object(blueprint_class(mode_bp))
    if mode_cdo:
        player_class = blueprint_class(player_bp)
        if player_class:
            mode_cdo.set_editor_property("default_pawn_class", player_class)
            log("BP_ModoJuego -> BP_Jugador")

        mode_cdo.set_editor_property("endless_match", True)
        mode_cdo.set_editor_property("team_based", False)
        mode_cdo.set_editor_property("respawn_delay", 2.0)
        log("BP_ModoJuego -> entrenamiento sin límite de tiempo")
    compile_blueprint(mode_bp, "BP_ModoJuego")

    # Verificación: leer de vuelta lo que quedó guardado.
    log("--- comprobación ---")
    mode_cdo = unreal.get_default_object(blueprint_class(mode_bp))
    player_cdo = unreal.get_default_object(blueprint_class(player_bp))
    if mode_cdo:
        log("  endless_match    = {}".format(mode_cdo.get_editor_property("endless_match")))
        pawn = mode_cdo.get_editor_property("default_pawn_class")
        log("  default_pawn     = {}".format(pawn.get_name() if pawn else "NINGUNO"))
    if player_cdo:
        contexts = player_cdo.get_editor_property("default_mapping_contexts")
        move = player_cdo.get_editor_property("move_action")
        log("  mapping_contexts = {}".format(len(contexts) if contexts else 0))
        log("  move_action      = {}".format(move.get_name() if move else "NINGUNA"))

    log("")
    log("Listo. Vuelve a pulsar Play.")


try:
    run()
except Exception as error:
    import traceback
    unreal.log_error("[fix] FALLÓ: {}".format(error))
    unreal.log_error(traceback.format_exc())
