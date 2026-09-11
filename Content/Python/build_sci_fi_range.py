"""
Construye la zona de entrenamiento con estética futurista.

Reemplaza el nivel anterior por un hangar cerrado con paneles murales,
tiras luminosas, drones flotantes y el gestor de reinicio. Usa solo
geometría y materiales de la plantilla: cuando haya assets propios, basta
con sustituir las mallas en los Blueprints.

Ejecutar en la consola Python del editor:
    exec(open(r"<proyecto>/Content/Python/build_sci_fi_range.py").read())
"""

import unreal

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LEVEL_SUB = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTOR_SUB = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

LEVEL_PATH = "/Game/Maps/Lvl_Entrenamiento"
CORE_DIR = "/Game/Blueprints/Core"
DRONE_BP = "BP_Dron"

CUBE = "/Engine/BasicShapes/Cube.Cube"

# Materiales de la plantilla que dan el aspecto de hangar.
MATERIALS = {
    "suelo": "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray",
    "muro": "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark",
    "brillo": "/Game/LevelPrototyping/Interactable/JumpPad/Assets/Materials/MI_GlowNT",
}


def log(message):
    unreal.log("[hangar] {}".format(message))


def load_material(key):
    path = MATERIALS.get(key)
    return unreal.EditorAssetLibrary.load_asset(path) if path else None


def spawn_box(location, scale, label, material=None, rotation=(0, 0, 0)):
    """Bloque a partir del cubo del motor, con material opcional."""
    try:
        actor = ACTOR_SUB.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator(*rotation)
        )
        if not actor:
            return None

        component = actor.static_mesh_component
        mesh = unreal.EditorAssetLibrary.load_asset(CUBE)
        if mesh and component:
            component.set_static_mesh(mesh)
            # Movible: con geometría estática el motor exigiría construir la
            # iluminación antes de que el nivel se vea bien.
            component.set_mobility(unreal.ComponentMobility.MOVABLE)

            if material:
                component.set_material(0, material)

        actor.set_actor_scale3d(unreal.Vector(*scale))
        actor.set_actor_label(label)
        return actor
    except Exception as error:
        log("AVISO: '{}' no se pudo crear: {}".format(label, error))
        return None


def spawn_light(class_name, location, rotation, label, setup=None):
    actor_class = getattr(unreal, class_name, None)
    if not actor_class:
        log("AVISO: unreal.{} no existe".format(class_name))
        return None

    try:
        actor = ACTOR_SUB.spawn_actor_from_class(
            actor_class, unreal.Vector(*location), unreal.Rotator(*rotation)
        )
        if actor:
            actor.set_actor_label(label)
            if setup:
                setup(actor)
        return actor
    except Exception as error:
        log("AVISO: luz '{}' falló: {}".format(label, error))
        return None


def create_drone_blueprint():
    """Blueprint del dron, para poder ajustarlo sin tocar C++."""
    path = "{}/{}".format(CORE_DIR, DRONE_BP)
    existing = unreal.EditorAssetLibrary.load_asset(path)
    if existing:
        return existing

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.TrainingDrone)

    blueprint = ASSET_TOOLS.create_asset(DRONE_BP, CORE_DIR, unreal.Blueprint, factory)
    if blueprint:
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
        log("BP_Dron creado")
    return blueprint


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


def build_hangar():
    """Recinto cerrado: suelo, muros y paneles con tiras luminosas."""
    floor_material = load_material("suelo")
    wall_material = load_material("muro")
    glow_material = load_material("brillo")

    # Suelo de 70 x 70 m.
    spawn_box((0, 0, -50), (70, 70, 1), "Suelo", floor_material)

    # Muro perimetral.
    wall_height = 8
    for x, y, sx, sy, name in (
        (0, 3500, 70, 1, "Muro_Norte"),
        (0, -3500, 70, 1, "Muro_Sur"),
        (3500, 0, 1, 70, "Muro_Este"),
        (-3500, 0, 1, 70, "Muro_Oeste"),
    ):
        spawn_box((x, y, wall_height * 50 - 50), (sx, sy, wall_height), name, wall_material)

    # Paneles murales con una tira luminosa, como en la referencia.
    panel_count = 0
    for side, (base_x, base_y, rot) in enumerate((
        (0, 3400, 0), (0, -3400, 0), (3400, 0, 90), (-3400, 0, 90)
    )):
        for offset in (-2000, -700, 700, 2000):
            panel_count += 1

            if rot == 0:
                location = (base_x + offset, base_y, 200)
                panel_scale = (5, 0.4, 5)
                strip_location = (base_x + offset, base_y - 30, 200)
                strip_scale = (4.4, 0.1, 0.2)
            else:
                location = (base_x, base_y + offset, 200)
                panel_scale = (0.4, 5, 5)
                strip_location = (base_x - 30, base_y + offset, 200)
                strip_scale = (0.1, 4.4, 0.2)

            spawn_box(location, panel_scale,
                      "Panel_{}".format(panel_count), wall_material)
            spawn_box(strip_location, strip_scale,
                      "Luz_Panel_{}".format(panel_count), glow_material)

    log("hangar: {} paneles murales".format(panel_count))

    # Bloques de cobertura repartidos por la sala.
    for index, (x, y, sx, sy, sz) in enumerate((
        (900, -1400, 3, 1, 4),
        (900, 1400, 3, 1, 4),
        (1900, 0, 1, 5, 3),
        (-1200, -900, 2, 2, 5),
        (-1200, 900, 2, 2, 5),
    ), start=1):
        spawn_box((x, y, sz * 50 - 50), (sx, sy, sz),
                  "Cobertura_{}".format(index), wall_material)


def build_lighting():
    """Luz principal fría, rebote suave y cielo, para un tono de hangar."""

    def setup_sun(actor):
        light = actor.directional_light_component
        light.set_intensity(4.0)
        light.set_light_color(unreal.Color(r=200, g=220, b=255, a=255))

    def setup_sky(actor):
        light = actor.sky_light_component
        light.set_intensity(1.4)

    spawn_light("DirectionalLight", (0, 0, 2000), (-50, -35, 0), "Luz_Solar", setup_sun)
    spawn_light("SkyLight", (0, 0, 600), (0, 0, 0), "Luz_Ambiente", setup_sky)
    spawn_light("SkyAtmosphere", (0, 0, 300), (0, 0, 0), "Atmosfera")
    spawn_light("ExponentialHeightFog", (0, 0, 0), (0, 0, 0), "Niebla")

    # Post-proceso: contraste y algo de brillo en las tiras luminosas.
    volume_class = getattr(unreal, "PostProcessVolume", None)
    if volume_class:
        try:
            volume = ACTOR_SUB.spawn_actor_from_class(
                volume_class, unreal.Vector(0, 0, 500), unreal.Rotator(0, 0, 0))
            if volume:
                volume.set_actor_label("PostProceso")
                volume.set_editor_property("unbound", True)

                settings = volume.get_editor_property("settings")
                settings.set_editor_property("override_bloom_intensity", True)
                settings.set_editor_property("bloom_intensity", 1.2)
                settings.set_editor_property("override_auto_exposure_min_brightness", True)
                settings.set_editor_property("auto_exposure_min_brightness", 0.9)
                settings.set_editor_property("override_auto_exposure_max_brightness", True)
                settings.set_editor_property("auto_exposure_max_brightness", 1.3)
                volume.set_editor_property("settings", settings)

                log("post-proceso configurado")
        except Exception as error:
            log("AVISO: post-proceso sin configurar: {}".format(error))


def build_drones(drone_class):
    """Tres filas de drones a distinta distancia y altura."""
    if not drone_class:
        log("AVISO: sin clase de dron, la galería queda vacía")
        return

    count = 0
    rows = (
        # (distancia, altura, vida, flotado, desplazamiento lateral)
        (1400, 180, 100.0, 25.0, 0.0),
        (2200, 260, 100.0, 35.0, 200.0),
        (3000, 340, 150.0, 45.0, 350.0),
    )

    for distance, height, health, hover, strafe in rows:
        for offset in (-1100, -370, 370, 1100):
            try:
                drone = ACTOR_SUB.spawn_actor_from_class(
                    drone_class,
                    unreal.Vector(distance, offset, height),
                    unreal.Rotator(0, 180, 0))

                if not drone:
                    continue

                count += 1
                drone.set_actor_label("Dron_{}m_{}".format(distance // 100, count))
                drone.set_editor_property("max_health", health)
                drone.set_editor_property("hover_amplitude", hover)
                drone.set_editor_property("strafe_amplitude", strafe)
            except Exception as error:
                log("AVISO: dron a {} falló: {}".format(distance, error))

    log("drones colocados: {}".format(count))


def run():
    LEVEL_SUB.new_level(LEVEL_PATH)
    log("nivel recreado: {}".format(LEVEL_PATH))

    build_hangar()
    build_lighting()

    drone_bp = create_drone_blueprint()
    build_drones(blueprint_class(drone_bp))

    # Gestor de la zona: repone drones y munición (RF6).
    manager_class = getattr(unreal, "TrainingRangeManager", None)
    if manager_class:
        try:
            manager = ACTOR_SUB.spawn_actor_from_class(
                manager_class, unreal.Vector(0, 0, 100), unreal.Rotator(0, 0, 0))
            if manager:
                manager.set_actor_label("GestorEntrenamiento")
                log("gestor de entrenamiento colocado")
        except Exception as error:
            log("AVISO: gestor sin colocar: {}".format(error))

    # Punto de aparición mirando hacia la galería.
    start = ACTOR_SUB.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(-1500, 0, 150), unreal.Rotator(0, 0, 0))
    if start:
        start.set_actor_label("Aparicion")

    # GameMode del nivel.
    mode_bp = unreal.EditorAssetLibrary.load_asset(CORE_DIR + "/BP_ModoJuego")
    if mode_bp:
        try:
            world = unreal.get_editor_subsystem(
                unreal.UnrealEditorSubsystem).get_editor_world()
            settings = world.get_world_settings() if world else None
            generated = blueprint_class(mode_bp)
            if settings and generated:
                settings.set_editor_property("default_game_mode", generated)
                log("GameMode del nivel asignado")
        except Exception as error:
            log("AVISO: GameMode sin asignar: {}".format(error))

    LEVEL_SUB.save_current_level()
    log("")
    log("Hangar listo. Pulsa Play.")
    log("Para sustituir el arte: abre BP_Dron y cambia su malla y efectos.")


try:
    run()
except Exception as error:
    import traceback
    unreal.log_error("[hangar] FALLÓ: {}".format(error))
    unreal.log_error(traceback.format_exc())
