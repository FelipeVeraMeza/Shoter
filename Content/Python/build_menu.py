"""
Crea los Blueprints de widget del menú y el nivel que lo aloja.

Los widgets se generan vacíos, derivados de UShooterMenuWidget: la lógica ya
está en C++ y solo hay que colocar botones y enlazarlos a sus funciones
(OpenScreen, StartMatch, QuitGame...).

Ejecutar en la consola Python del editor:
    exec(open(r"<proyecto>/Content/Python/build_menu.py").read())
"""

import unreal

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LEVEL_SUB = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTOR_SUB = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

UI_DIR = "/Game/UI"
CORE_DIR = "/Game/Blueprints/Core"
MENU_LEVEL = "/Game/Maps/Lvl_Menu"

# Pantalla -> nombre del widget. Coinciden con EShooterMenuScreen.
SCREENS = (
    ("Main", "WBP_MenuPrincipal"),
    ("Play", "WBP_Jugar"),
    ("Arsenal", "WBP_Arsenal"),
    ("Progression", "WBP_Progresion"),
    ("Options", "WBP_Opciones"),
    ("Pause", "WBP_Pausa"),
)


def log(message):
    unreal.log("[menu] {}".format(message))


def ensure_dirs(*paths):
    for path in paths:
        if not unreal.EditorAssetLibrary.does_directory_exist(path):
            unreal.EditorAssetLibrary.make_directory(path)


def find_factory(*names):
    for name in names:
        factory_class = getattr(unreal, name, None)
        if factory_class:
            try:
                return factory_class()
            except Exception:
                continue
    return None


def create_widget_blueprint(name):
    """Widget derivado de UShooterMenuWidget, para que herede la navegación."""
    path = "{}/{}".format(UI_DIR, name)
    existing = unreal.EditorAssetLibrary.load_asset(path)
    if existing:
        log("ya existe: {}".format(name))
        return existing

    factory = find_factory("WidgetBlueprintFactory")
    if not factory:
        log("ERROR: no hay WidgetBlueprintFactory en esta versión")
        return None

    try:
        factory.set_editor_property("parent_class", unreal.ShooterMenuWidget)
    except Exception as error:
        log("AVISO: no se pudo fijar la clase base de {}: {}".format(name, error))

    widget = ASSET_TOOLS.create_asset(name, UI_DIR, None, factory)
    if widget:
        unreal.EditorAssetLibrary.save_loaded_asset(widget)
        log("creado: {}".format(name))
    return widget


def build_menu_level():
    """Nivel mínimo que aloja el menú: una cámara y luz, sin geometría."""
    if unreal.EditorAssetLibrary.does_asset_exist(MENU_LEVEL):
        log("el nivel de menú ya existe")
        return

    LEVEL_SUB.new_level(MENU_LEVEL)

    for class_name, location, rotation, label in (
        ("DirectionalLight", (0, 0, 500), (-45, -30, 0), "Luz"),
        ("SkyLight", (0, 0, 300), (0, 0, 0), "Luz_Ambiente"),
        ("SkyAtmosphere", (0, 0, 200), (0, 0, 0), "Atmosfera"),
        ("PlayerStart", (0, 0, 100), (0, 0, 0), "Inicio"),
    ):
        actor_class = getattr(unreal, class_name, None)
        if not actor_class:
            continue
        try:
            actor = ACTOR_SUB.spawn_actor_from_class(
                actor_class, unreal.Vector(*location), unreal.Rotator(*rotation)
            )
            if actor:
                actor.set_actor_label(label)
        except Exception as error:
            log("AVISO: {} no se pudo crear: {}".format(label, error))

    LEVEL_SUB.save_current_level()
    log("nivel de menú creado: {}".format(MENU_LEVEL))


def run():
    ensure_dirs(UI_DIR, "/Game/Maps")

    log("--- widgets ---")
    created = {}
    for screen, widget_name in SCREENS:
        widget = create_widget_blueprint(widget_name)
        if widget:
            created[screen] = widget

    log("--- nivel ---")
    build_menu_level()

    log("")
    log("Widgets creados en {}.".format(UI_DIR))
    log("Faltan dos pasos manuales:")
    log("  1. Abrir cada widget y colocar los botones, enlazándolos a")
    log("     OpenScreen / StartMatch / QuitGame / GoBack (ya heredadas).")
    log("  2. Project Settings -> Game -> Shooter Menu Subsystem:")
    log("     asignar cada widget a su pantalla en Screen Widgets.")


try:
    run()
except Exception as error:
    import traceback
    unreal.log_error("[menu] FALLÓ: {}".format(error))
    unreal.log_error(traceback.format_exc())
