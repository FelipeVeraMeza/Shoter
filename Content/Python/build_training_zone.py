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

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LEVEL_SUB = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTOR_SUB = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

LEVEL_PATH = "/Game/Maps/Lvl_Entrenamiento"

INPUT_ACTIONS_DIR = "/Game/Input/Actions"
INPUT_DIR = "/Game/Input"
WEAPONS_DIR = "/Game/Blueprints/Weapons"
CORE_DIR = "/Game/Blueprints/Core"
DATA_DIR = "/Game/Weapons"

CUBE = "/Engine/BasicShapes/Cube.Cube"

# Acción de combate -> tecla por defecto.
COMBAT_ACTIONS = {
    "IA_Fire": unreal.InputCoreKeys.LEFT_MOUSE_BUTTON,
    "IA_Aim": unreal.InputCoreKeys.RIGHT_MOUSE_BUTTON,
    "IA_Reload": unreal.InputCoreKeys.R,
    "IA_Sprint": unreal.InputCoreKeys.LEFT_SHIFT,
    "IA_Crouch": unreal.InputCoreKeys.LEFT_CONTROL,
    "IA_SwitchWeapon": unreal.InputCoreKeys.Q,
}

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


def ensure_dirs(*paths):
    for path in paths:
        if not unreal.EditorAssetLibrary.does_directory_exist(path):
            unreal.EditorAssetLibrary.make_directory(path)


def load_or_create(path, name, asset_class, factory):
    full_path = "{}/{}".format(path, name)
    existing = unreal.EditorAssetLibrary.load_asset(full_path)
    if existing:
        return existing
    return ASSET_TOOLS.create_asset(name, path, asset_class, factory)


# --------------------------------------------------------------------------
# Assets
# --------------------------------------------------------------------------

def create_input_actions():
    """Crea las acciones de combate y devuelve todas las que el personaje usa."""
    actions = {}

    for name in COMBAT_ACTIONS:
        action = load_or_create(
            INPUT_ACTIONS_DIR, name, unreal.InputAction, unreal.InputActionFactory()
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
        unreal.InputMappingContext, unreal.InputMappingContextFactory()
    )
    if not context:
        return None

    # Remapear de cero evita teclas duplicadas al reejecutar.
    context.set_editor_property("mappings", [])

    for name, key in COMBAT_ACTIONS.items():
        action = actions.get(name)
        if not action:
            continue
        mapping = unreal.EnhancedActionKeyMapping()
        mapping.set_editor_property("action", action)
        mapping.set_editor_property("key", key)
        context.mappings.append(mapping)

    unreal.EditorAssetLibrary.save_loaded_asset(context)
    log("IMC_Combate: {} teclas mapeadas".format(len(context.mappings)))
    return context


def create_weapon_data():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.WeaponDataAsset)

    data = load_or_create(DATA_DIR, "DA_Rifle", unreal.WeaponDataAsset, factory)
    if data:
        unreal.EditorAssetLibrary.save_loaded_asset(data)
        log("DA_Rifle listo")
    return data


def create_blueprint(path, name, parent_class):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return load_or_create(path, name, unreal.Blueprint, factory)


def get_cdo(blueprint):
    """Objeto por defecto del Blueprint: donde viven los valores editables."""
    if not blueprint:
        return None
    generated = blueprint.get_editor_property("generated_class")
    return unreal.get_default_object(generated) if generated else None


def build_blueprints(actions, context, weapon_data):
    weapon_bp = create_blueprint(WEAPONS_DIR, "BP_Rifle", unreal.WeaponBase)
    player_bp = create_blueprint(CORE_DIR, "BP_Jugador", unreal.ShooterCharacter)
    mode_bp = create_blueprint(CORE_DIR, "BP_ModoJuego", unreal.ShooterGameMode)

    # BP_Rifle -> datos de balance
    weapon_cdo = get_cdo(weapon_bp)
    if weapon_cdo and weapon_data:
        weapon_cdo.set_editor_property("weapon_data", weapon_data)
        log("BP_Rifle -> DA_Rifle")

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

        inventory = player_cdo.get_editor_property("inventory")
        if inventory and weapon_bp:
            generated = weapon_bp.get_editor_property("generated_class")
            if generated:
                inventory.set_editor_property("default_weapons", [generated])
                log("BP_Jugador -> arma inicial BP_Rifle")

    # BP_ModoJuego -> pawn del jugador y reglas de entrenamiento
    mode_cdo = get_cdo(mode_bp)
    if mode_cdo:
        if player_bp:
            generated = player_bp.get_editor_property("generated_class")
            if generated:
                mode_cdo.set_editor_property("default_pawn_class", generated)
                log("BP_ModoJuego -> BP_Jugador")

        # Sin cronómetro ni equipos: la práctica no se corta sola.
        mode_cdo.set_editor_property("endless_match", True)
        mode_cdo.set_editor_property("team_based", False)
        mode_cdo.set_editor_property("respawn_delay", 2.0)
        log("BP_ModoJuego -> modo entrenamiento (sin límite de tiempo)")

    for blueprint in (weapon_bp, player_bp, mode_bp):
        if blueprint:
            unreal.EditorAssetLibrary.save_loaded_asset(blueprint)

    return weapon_bp, player_bp, mode_bp


# --------------------------------------------------------------------------
# Nivel
# --------------------------------------------------------------------------

def spawn_box(location, scale, label):
    """Bloque estático a partir del cubo del motor."""
    actor = ACTOR_SUB.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator(0, 0, 0)
    )
    if not actor:
        return None

    mesh = unreal.EditorAssetLibrary.load_asset(CUBE)
    component = actor.static_mesh_component
    if mesh and component:
        component.set_static_mesh(mesh)
    component.set_mobility(unreal.ComponentMobility.STATIC)

    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_label(label)
    return actor


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

    # Iluminación y cielo.
    light = ACTOR_SUB.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 1200), unreal.Rotator(-46, -35, 0)
    )
    if light:
        light.set_actor_label("Luz_Solar")

    for actor_class, label in (
        (unreal.SkyLight, "Luz_Ambiente"),
        (unreal.SkyAtmosphere, "Atmosfera"),
    ):
        actor = ACTOR_SUB.spawn_actor_from_class(
            actor_class, unreal.Vector(0, 0, 300), unreal.Rotator(0, 0, 0)
        )
        if actor:
            actor.set_actor_label(label)

    # Puntos de aparición separados: el modo elige el más lejano al enemigo.
    for index, (x, y) in enumerate(
        ((-2200, -2200), (-2200, 2200), (2200, -2200), (2200, 2200)), start=1
    ):
        start = ACTOR_SUB.spawn_actor_from_class(
            unreal.PlayerStart, unreal.Vector(x, y, 120), unreal.Rotator(0, 0, 0)
        )
        if start:
            start.set_actor_label("Aparicion_{}".format(index))

    # Galería de tiro: tres filas a distinta distancia, para ver la caída de daño.
    if target_class:
        count = 0
        for distance in (1200, 2000, 2800):
            for offset in (-900, -300, 300, 900):
                target = ACTOR_SUB.spawn_actor_from_class(
                    target_class,
                    unreal.Vector(distance, offset, 90),
                    unreal.Rotator(0, 180, 0),
                )
                if target:
                    count += 1
                    target.set_actor_label("Blanco_{}m_{}".format(distance // 100, count))
        log("blancos colocados: {}".format(count))

    # GameMode del nivel, para que no dependa de la configuración global.
    # EditorLevelLibrary está deprecado; el subsistema es la ruta actual, con
    # respaldo por si la versión del motor todavía no lo expone.
    if mode_bp:
        generated = mode_bp.get_editor_property("generated_class")
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
    ensure_dirs(INPUT_ACTIONS_DIR, INPUT_DIR, WEAPONS_DIR, CORE_DIR, DATA_DIR, "/Game/Maps")

    log("--- assets ---")
    actions = create_input_actions()
    context = create_mapping_context(actions)
    weapon_data = create_weapon_data()

    log("--- blueprints ---")
    _, _, mode_bp = build_blueprints(actions, context, weapon_data)

    log("--- nivel ---")
    build_level(mode_bp, unreal.TargetActor)

    log("")
    log("Zona de entrenamiento lista. Pulsa Play.")
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
