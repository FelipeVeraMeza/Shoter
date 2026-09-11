"""
Construye la zona de entrenamiento completa, de cero.

Crea los assets que el módulo C++ necesita (input, arma, blueprints), los
conecta entre sí y genera un nivel jugable con suelo, luz, cobertura,
puntos de aparición y una galería de blancos.

Ejecutar desde la consola de Python del editor:
    exec(open(r"<proyecto>/Content/Python/build_training_zone.py").read())

Es idempotente: reejecutarlo reutiliza lo que ya exista.
"""

import unreal

def require(attribute_path):
    """
    Resuelve unreal.<algo> avisando con claridad si no existe.

    Los nombres de la API de Python cambian entre versiones del motor, y el
    fallo por defecto (AttributeError a media ejecución) no dice qué hacer.
    """
    current = unreal
    for part in attribute_path.split("."):
        if not hasattr(current, part):
            raise RuntimeError(
                "Esta versión de Unreal no expone 'unreal.{}'. "
                "Pásale este mensaje a quien mantenga el script.".format(attribute_path)
            )
        current = getattr(current, part)
    return current


ASSET_TOOLS = require("AssetToolsHelpers").get_asset_tools()
LEVEL_SUB = unreal.get_editor_subsystem(require("LevelEditorSubsystem"))
ACTOR_SUB = unreal.get_editor_subsystem(require("EditorActorSubsystem"))

LEVEL_PATH = "/Game/Maps/Lvl_Entrenamiento"

INPUT_ACTIONS_DIR = "/Game/Input/Actions"
INPUT_DIR = "/Game/Input"
WEAPONS_DIR = "/Game/Blueprints/Weapons"
CORE_DIR = "/Game/Blueprints/Core"
DATA_DIR = "/Game/Weapons"

CUBE = "/Engine/BasicShapes/Cube.Cube"

# Acción de combate -> nombre de la tecla.
#
# unreal.InputCoreKeys no existe en 5.8: las teclas se construyen pasando su
# nombre a unreal.Key, que es como las guarda el motor internamente.
COMBAT_ACTIONS = {
    "IA_Fire": "LeftMouseButton",
    "IA_Aim": "RightMouseButton",
    "IA_Reload": "R",
    "IA_Sprint": "LeftShift",
    "IA_Crouch": "LeftControl",
    "IA_SwitchWeapon": "Q",
}


def make_key(key_name):
    """
    Construye un FKey a partir de su nombre.

    La forma de crearlo varía entre versiones del motor, así que se prueban
    las conocidas en orden en vez de asumir una.
    """
    # 5.8: el constructor no acepta argumentos; se fija la propiedad.
    try:
        key = unreal.Key()
        key.set_editor_property("key_name", key_name)
        return key
    except Exception:
        pass

    # Otras versiones aceptan el nombre en el constructor.
    try:
        return unreal.Key(key_name)
    except Exception:
        pass

    # Versiones antiguas exponían las teclas como constantes.
    for container in ("InputCoreKeys", "Keys"):
        holder = getattr(unreal, container, None)
        constant = getattr(holder, key_name.upper(), None) if holder else None
        if constant is not None:
            return constant

    raise RuntimeError(
        "No se pudo crear la tecla '{}' con ninguna forma conocida.".format(key_name)
    )

# Nombre de la propiedad en ShooterCharacter -> asset de acción.
CHARACTER_ACTION_PROPERTIES = {
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
    unreal.log("[entrenamiento] {}".format(message))


def dump_api_report():
    """
    Deja en el registro qué expone esta versión del motor.

    Los nombres de la API de Python cambian entre versiones, y sin un stub
    generado no hay forma de comprobarlo desde fuera del editor. Esto permite
    diagnosticar un fallo leyendo el log en vez de adivinar.
    """
    log("--- API disponible en este motor ---")
    log("version: {}".format(unreal.SystemLibrary.get_engine_version()))

    for name in (
        "Key", "InputCoreKeys", "EnhancedActionKeyMapping", "InputAction",
        "InputMappingContext", "InputActionFactory", "InputMappingContextFactory",
        "LevelEditorSubsystem", "EditorActorSubsystem", "UnrealEditorSubsystem",
        "EditorLevelLibrary", "TargetActor", "ShooterCharacter", "ShooterGameMode",
        "WeaponBase", "WeaponDataAsset",
    ):
        log("  {:32} {}".format(name, "sí" if hasattr(unreal, name) else "NO"))


def ensure_dirs(*paths):
    for path in paths:
        if not unreal.EditorAssetLibrary.does_directory_exist(path):
            unreal.EditorAssetLibrary.make_directory(path)


def find_factory(*candidate_names):
    """
    Devuelve la primera factory que exista con estos nombres.

    Los nombres varían entre versiones (InputActionFactory no existe en 5.8),
    así que se prueban alternativas en vez de asumir una.
    """
    for name in candidate_names:
        factory_class = getattr(unreal, name, None)
        if factory_class:
            try:
                return factory_class()
            except Exception:
                continue
    return None


def load_or_create(path, name, asset_class, factory):
    full_path = "{}/{}".format(path, name)
    existing = unreal.EditorAssetLibrary.load_asset(full_path)
    if existing:
        return existing

    if factory:
        return ASSET_TOOLS.create_asset(name, path, asset_class, factory)

    # Sin factory conocida, crear el asset directamente por su clase.
    return ASSET_TOOLS.create_asset(name, path, asset_class, None)


# --------------------------------------------------------------------------
# Assets
# --------------------------------------------------------------------------

def create_input_actions():
    """Crea las acciones de combate y devuelve todas las que el personaje usa."""
    actions = {}

    for name in COMBAT_ACTIONS:
        action = load_or_create(
            INPUT_ACTIONS_DIR, name, unreal.InputAction,
            find_factory("InputAction_Factory", "InputActionFactory")
        )
        if action:
            action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
            unreal.EditorAssetLibrary.save_loaded_asset(action)
            actions[name] = action

    # Movimiento y cámara ya vienen con la plantilla.
    for name in ("IA_Move", "IA_Look", "IA_Jump"):
        existing = unreal.EditorAssetLibrary.load_asset(
            "{}/{}".format(INPUT_ACTIONS_DIR, name)
        )
        if existing:
            actions[name] = existing
        else:
            log("AVISO: falta {} en {}".format(name, INPUT_ACTIONS_DIR))

    log("acciones de entrada listas: {}".format(len(actions)))
    return actions


def create_mapping_context(actions):
    context = load_or_create(
        INPUT_DIR, "IMC_Combate",
        unreal.InputMappingContext,
        find_factory("InputMappingContext_Factory", "InputMappingContextFactory")
    )
    if not context:
        return None

    # Remapear de cero evita teclas duplicadas al reejecutar.
    context.set_editor_property("mappings", [])

    for name, key_name in COMBAT_ACTIONS.items():
        action = actions.get(name)
        if not action:
            continue
        try:
            mapping = unreal.EnhancedActionKeyMapping()
            mapping.set_editor_property("action", action)
            mapping.set_editor_property("key", make_key(key_name))
            context.mappings.append(mapping)
        except Exception as error:
            # Una tecla suelta se puede asignar a mano; no vale la pena
            # abortar la construcción del nivel por esto.
            log("no se pudo mapear {} -> {}: {}".format(name, key_name, error))

    unreal.EditorAssetLibrary.save_loaded_asset(context)
    log("IMC_Combate: {} teclas mapeadas".format(len(context.mappings)))
    return context


def create_weapon_data():
    factory = find_factory("DataAssetFactory")
    factory.set_editor_property("data_asset_class", unreal.WeaponDataAsset)

    data = load_or_create(DATA_DIR, "DA_Rifle", unreal.WeaponDataAsset, factory)
    if data:
        unreal.EditorAssetLibrary.save_loaded_asset(data)
        log("DA_Rifle listo")
    return data


def create_blueprint(path, name, parent_class):
    factory = find_factory("BlueprintFactory")
    factory.set_editor_property("parent_class", parent_class)
    return load_or_create(path, name, unreal.Blueprint, factory)


def blueprint_class(blueprint):
    """
    Devuelve la clase que genera un Blueprint.

    En 5.8 'generated_class' ya no es una propiedad del editor: hay que
    cargar la clase por su ruta o pedirla al propio objeto.
    """
    if not blueprint:
        return None

    # Método directo del objeto Blueprint.
    try:
        generated = blueprint.generated_class()
        if generated:
            return generated
    except Exception:
        pass

    # Versiones antiguas lo exponían como propiedad del editor.
    try:
        generated = blueprint.get_editor_property("generated_class")
        if generated:
            return generated
    except Exception:
        pass

    # Último recurso: cargar la clase por su ruta, que siempre es <ruta>_C.
    try:
        path = blueprint.get_path_name()
        if path.endswith(".{}".format(blueprint.get_name())):
            return unreal.load_object(None, path + "_C")
    except Exception as error:
        log("no se pudo resolver la clase de {}: {}".format(blueprint, error))

    return None


def get_cdo(blueprint):
    """Objeto por defecto del Blueprint: donde viven los valores editables."""
    generated = blueprint_class(blueprint)
    return unreal.get_default_object(generated) if generated else None


def build_blueprints(actions, context, weapon_data):
    weapon_bp = create_blueprint(WEAPONS_DIR, "BP_Rifle", unreal.WeaponBase)
    player_bp = create_blueprint(CORE_DIR, "BP_Jugador", unreal.ShooterCharacter)
    mode_bp = create_blueprint(CORE_DIR, "BP_ModoJuego", unreal.ShooterGameMode)

    # BP_Rifle -> datos de balance
    try:
        weapon_cdo = get_cdo(weapon_bp)
        if weapon_cdo and weapon_data:
            weapon_cdo.set_editor_property("weapon_data", weapon_data)
            log("BP_Rifle -> DA_Rifle")
    except Exception as error:
        log("AVISO: BP_Rifle sin datos: {}".format(error))

    # BP_Jugador -> input y arma inicial
    player_cdo = get_cdo(player_bp)
    if player_cdo:
        contexts = []
        default_ctx = unreal.EditorAssetLibrary.load_asset(INPUT_DIR + "/IMC_Default")
        if default_ctx:
            contexts.append(default_ctx)
        if context:
            contexts.append(context)
        player_cdo.set_editor_property("default_mapping_contexts", contexts)

        assigned = 0
        for prop, action_name in CHARACTER_ACTION_PROPERTIES.items():
            action = actions.get(action_name)
            if not action:
                continue
            try:
                player_cdo.set_editor_property(prop, action)
                assigned += 1
            except Exception as error:
                log("no se pudo asignar {}: {}".format(prop, error))
        log("BP_Jugador -> {} contextos, {} acciones".format(len(contexts), assigned))

        # El inventario es un componente: desde el CDO no siempre es accesible.
        try:
            inventory = player_cdo.get_editor_property("inventory")
            generated = blueprint_class(weapon_bp)
            if inventory and generated:
                inventory.set_editor_property("default_weapons", [generated])
                log("BP_Jugador -> arma inicial BP_Rifle")
            else:
                log("AVISO: asigna BP_Rifle a mano en Inventory/Default Weapons")
        except Exception as error:
            log("AVISO: arma inicial sin asignar ({}): hazlo a mano en "
                "BP_Jugador > Inventory > Default Weapons".format(error))

    # BP_ModoJuego -> pawn del jugador y reglas de entrenamiento
    mode_cdo = get_cdo(mode_bp)
    if mode_cdo:
        try:
            generated = blueprint_class(player_bp)
            if generated:
                mode_cdo.set_editor_property("default_pawn_class", generated)
                log("BP_ModoJuego -> BP_Jugador")
            else:
                log("AVISO: asigna BP_Jugador a mano en Default Pawn Class")
        except Exception as error:
            log("AVISO: pawn sin asignar: {}".format(error))

        # Sin cronómetro ni equipos: la práctica no se corta sola.
        try:
            mode_cdo.set_editor_property("endless_match", True)
            mode_cdo.set_editor_property("team_based", False)
            mode_cdo.set_editor_property("respawn_delay", 2.0)
            log("BP_ModoJuego -> modo entrenamiento (sin límite de tiempo)")
        except Exception as error:
            log("AVISO: reglas de entrenamiento sin aplicar: {}".format(error))

    for blueprint in (weapon_bp, player_bp, mode_bp):
        if blueprint:
            unreal.EditorAssetLibrary.save_loaded_asset(blueprint)

    return weapon_bp, player_bp, mode_bp


# --------------------------------------------------------------------------
# Nivel
# --------------------------------------------------------------------------

def spawn_box(location, scale, label):
    """Bloque estático a partir del cubo del motor."""
    try:
        actor = ACTOR_SUB.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator(0, 0, 0)
        )
        if not actor:
            return None

        mesh = unreal.EditorAssetLibrary.load_asset(CUBE)
        component = actor.static_mesh_component
        if mesh and component:
            component.set_static_mesh(mesh)
            # Movible: con geometría estática el motor exigiría construir la
            # iluminación antes de que el nivel se vea correctamente.
            component.set_mobility(unreal.ComponentMobility.MOVABLE)

        actor.set_actor_scale3d(unreal.Vector(*scale))
        actor.set_actor_label(label)
        return actor
    except Exception as error:
        log("AVISO: no se pudo crear '{}': {}".format(label, error))
        return None


def build_level(mode_bp, target_class):
    LEVEL_SUB.new_level(LEVEL_PATH)
    log("nivel creado: {}".format(LEVEL_PATH))

    # Suelo: 60 x 60 m.
    spawn_box((0, 0, -50), (60, 60, 1), "Suelo")

    # Muro perimetral, para no caerse del mapa.
    wall_height = 6
    for x, y, sx, sy, name in (
        (0, 3000, 60, 1, "Muro_Norte"),
        (0, -3000, 60, 1, "Muro_Sur"),
        (3000, 0, 1, 60, "Muro_Este"),
        (-3000, 0, 1, 60, "Muro_Oeste"),
    ):
        spawn_box((x, y, wall_height * 50 - 50), (sx, sy, wall_height), name)

    # Cobertura para practicar asomarse y cubrirse.
    for index, (x, y) in enumerate(((800, -600), (800, 600), (1600, 0)), start=1):
        spawn_box((x, y, 100), (1, 4, 3), "Cobertura_{}".format(index))

    # Iluminación y cielo. Sin luz el nivel se ve negro, así que si algo aquí
    # falla conviene saberlo en vez de acabar con una pantalla oscura.
    for class_name, location, rotation, label in (
        ("DirectionalLight", (0, 0, 1200), (-46, -35, 0), "Luz_Solar"),
        ("SkyLight", (0, 0, 300), (0, 0, 0), "Luz_Ambiente"),
        ("SkyAtmosphere", (0, 0, 300), (0, 0, 0), "Atmosfera"),
    ):
        actor_class = getattr(unreal, class_name, None)
        if not actor_class:
            log("AVISO: unreal.{} no existe, se omite".format(class_name))
            continue
        try:
            actor = ACTOR_SUB.spawn_actor_from_class(
                actor_class, unreal.Vector(*location), unreal.Rotator(*rotation)
            )
            if actor:
                actor.set_actor_label(label)
        except Exception as error:
            log("AVISO: no se pudo crear {}: {}".format(label, error))

    # Puntos de aparición separados: el modo elige el más lejano al enemigo.
    # Sin ninguno, el jugador aparece en el origen y el nivel es injugable.
    spawns = 0
    for index, (x, y) in enumerate(
        ((-2200, -2200), (-2200, 2200), (2200, -2200), (2200, 2200)), start=1
    ):
        try:
            start = ACTOR_SUB.spawn_actor_from_class(
                unreal.PlayerStart, unreal.Vector(x, y, 120), unreal.Rotator(0, 0, 0)
            )
            if start:
                start.set_actor_label("Aparicion_{}".format(index))
                spawns += 1
        except Exception as error:
            log("AVISO: aparición {} falló: {}".format(index, error))
    log("puntos de aparición: {}".format(spawns))

    # Galería de tiro. Tres filas a distinta distancia dejan ver la caída de
    # daño; la fila lejana se mueve para practicar puntería sobre blanco móvil.
    if target_class:
        count = 0
        rows = (
            # (distancia, altura, vida, amplitud del vaivén, velocidad)
            (1200, 90, 100.0, 0.0, 1.0),
            (2000, 140, 100.0, 250.0, 0.9),
            (2800, 200, 150.0, 450.0, 1.4),
        )

        for distance, height, health, swing, speed in rows:
            for offset in (-900, -300, 300, 900):
                try:
                    target = ACTOR_SUB.spawn_actor_from_class(
                        target_class,
                        unreal.Vector(distance, offset, height),
                        unreal.Rotator(0, 180, 0),
                    )
                    if not target:
                        continue

                    count += 1
                    target.set_actor_label(
                        "Blanco_{}m_{}".format(distance // 100, count)
                    )

                    target.set_editor_property("max_health", health)
                    target.set_editor_property("respawn_delay", 3.0)
                    target.set_editor_property("swing_amplitude", swing)
                    target.set_editor_property("swing_speed", speed)
                except Exception as error:
                    log("AVISO: blanco a {} falló: {}".format(distance, error))

        log("blancos colocados: {} (la fila lejana se mueve)".format(count))
    else:
        log("AVISO: sin clase de blanco, la galería queda vacía")

    # GameMode del nivel, para que no dependa de la configuración global.
    # EditorLevelLibrary está deprecado; el subsistema es la ruta actual, con
    # respaldo por si la versión del motor todavía no lo expone.
    if mode_bp:
        generated = blueprint_class(mode_bp)
        world = None
        try:
            world = unreal.get_editor_subsystem(
                unreal.UnrealEditorSubsystem
            ).get_editor_world()
        except Exception:
            try:
                world = unreal.EditorLevelLibrary.get_editor_world()
            except Exception as error:
                log("no se pudo obtener el mundo: {}".format(error))

        settings = world.get_world_settings() if world else None
        if settings and generated:
            settings.set_editor_property("default_game_mode", generated)
            log("GameMode del nivel -> BP_ModoJuego")

    LEVEL_SUB.save_current_level()
    log("nivel guardado")


# --------------------------------------------------------------------------

def run():
    dump_api_report()

    ensure_dirs(INPUT_ACTIONS_DIR, INPUT_DIR, WEAPONS_DIR, CORE_DIR, DATA_DIR, "/Game/Maps")

    log("--- assets ---")
    actions = create_input_actions()
    context = create_mapping_context(actions)
    weapon_data = create_weapon_data()

    # Un fallo aquí no debe impedir que se construya el nivel: es preferible
    # tener el campo de tiro y asignar un Blueprint a mano que quedarse sin nada.
    log("--- blueprints ---")
    mode_bp = None
    try:
        _, _, mode_bp = build_blueprints(actions, context, weapon_data)
    except Exception as error:
        log("AVISO: los Blueprints fallaron: {}".format(error))
        log("       el nivel se construye igual; habrá que crearlos a mano")

    log("--- nivel ---")
    build_level(mode_bp, unreal.TargetActor)

    log("")
    if mode_bp:
        log("Zona de entrenamiento lista. Pulsa Play.")
    else:
        log("Nivel creado, pero sin Blueprints: revisa los avisos de arriba.")
    log("Para probar en red: Net Mode = Play As Client, 2 jugadores.")


try:
    run()
except Exception as error:
    # Un fallo a media construcción deja el proyecto en un estado confuso:
    # mejor decir qué falló que morir con un traceback suelto.
    import traceback

    unreal.log_error("[entrenamiento] FALLÓ: {}".format(error))
    unreal.log_error(traceback.format_exc())
    unreal.log_error(
        "[entrenamiento] Copia este error y pásalo para diagnosticarlo."
    )
