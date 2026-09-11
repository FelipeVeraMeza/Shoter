"""
Añade el ratón y el teclado de movimiento a IMC_Combate.

La plantilla reparte los mapeos entre IMC_Default e IMC_MouseLook, y no hay
garantía de que la cámara esté en el contexto que carga BP_Jugador. Mapear
aquí las teclas de mirar y moverse hace que el personaje no dependa de cómo
esté organizada la plantilla.

Ejecutar en la consola Python del editor:
    exec(open(r"<proyecto>/Content/Python/fix_camara.py").read())
"""

import unreal

INPUT_DIR = "/Game/Input"
ACTIONS_DIR = "/Game/Input/Actions"
BP_JUGADOR = "/Game/Blueprints/Core/BP_Jugador"


def log(message):
    unreal.log("[camara] {}".format(message))


def make_key(key_name):
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    return key


def add_mapping(context, action, key_name, modifiers=None):
    """Añade un mapeo tecla -> acción, con modificadores opcionales."""
    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("action", action)
    mapping.set_editor_property("key", make_key(key_name))

    if modifiers:
        try:
            mapping.set_editor_property("modifiers", modifiers)
        except Exception as error:
            log("AVISO: modificadores no aplicados a {}: {}".format(key_name, error))

    context.mappings.append(mapping)


def negate(x=False, y=False, z=False):
    """Modificador que invierte un eje."""
    modifier = unreal.InputModifierNegate()
    modifier.set_editor_property("x", x)
    modifier.set_editor_property("y", y)
    modifier.set_editor_property("z", z)
    return modifier


def swizzle_yxz():
    """Intercambia X e Y: convierte un eje 1D en el componente Y de un Vector2D."""
    modifier = unreal.InputModifierSwizzleAxis()
    modifier.set_editor_property(
        "order", unreal.InputAxisSwizzle.YXZ
    )
    return modifier


def run():
    context = unreal.EditorAssetLibrary.load_asset(INPUT_DIR + "/IMC_Combate")
    if not context:
        log("ERROR: falta IMC_Combate. Ejecuta antes build_training_zone.py")
        return

    look = unreal.EditorAssetLibrary.load_asset(ACTIONS_DIR + "/IA_Look")
    move = unreal.EditorAssetLibrary.load_asset(ACTIONS_DIR + "/IA_Move")
    jump = unreal.EditorAssetLibrary.load_asset(ACTIONS_DIR + "/IA_Jump")

    if not look or not move:
        log("ERROR: faltan IA_Look o IA_Move")
        return

    # Conservar los mapeos de combate ya existentes y quitar los que
    # vayamos a volver a añadir, para no duplicar teclas al reejecutar.
    kept = []
    for mapping in context.mappings:
        action = mapping.get_editor_property("action")
        if action in (look, move, jump):
            continue
        kept.append(mapping)
    context.set_editor_property("mappings", kept)

    # Cámara: el ratón entrega un Vector2D listo para IA_Look.
    add_mapping(context, look, "Mouse2D")
    log("cámara -> Mouse2D")

    # Movimiento WASD. IA_Move entrega un Vector2D que ShooterCharacter::Move
    # lee como (X = lateral, Y = adelante).
    #
    # Una tecla produce 1.0 en X. Para adelante/atrás hay que moverlo a Y con
    # el swizzle, y solo entonces negarlo: los modificadores se aplican en
    # orden, así que negar Y después del swizzle es lo correcto.
    add_mapping(context, move, "W", [swizzle_yxz()])
    add_mapping(context, move, "S", [swizzle_yxz(), negate(y=True)])
    add_mapping(context, move, "D")
    add_mapping(context, move, "A", [negate(x=True)])
    log("movimiento -> W A S D")

    if jump:
        add_mapping(context, jump, "SpaceBar")
        log("salto -> Espacio")

    unreal.EditorAssetLibrary.save_loaded_asset(context)
    log("IMC_Combate guardado con {} mapeos".format(len(context.mappings)))

    # Recompilar el personaje para que recoja el contexto actualizado.
    player_bp = unreal.EditorAssetLibrary.load_asset(BP_JUGADOR)
    if player_bp:
        library = getattr(unreal, "BlueprintEditorLibrary", None)
        if library and hasattr(library, "compile_blueprint"):
            try:
                library.compile_blueprint(player_bp)
            except Exception as error:
                log("AVISO: no se pudo recompilar BP_Jugador: {}".format(error))
        unreal.EditorAssetLibrary.save_loaded_asset(player_bp)
        log("BP_Jugador actualizado")

    log("")
    log("Listo. Vuelve a pulsar Play: el ratón debería mover la cámara.")


try:
    run()
except Exception as error:
    import traceback
    unreal.log_error("[camara] FALLÓ: {}".format(error))
    unreal.log_error(traceback.format_exc())
