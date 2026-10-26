"""Run with UE 5.8's PythonScript commandlet after building RoamAloneEditor.

Creates project-owned assets once; reruns preserve an existing map and its actors.
Uses the installed PythonScriptPlugin and EditorScriptingUtilities via -EnablePlugins.
"""
import unreal

ROOT = '/Game/VanPrototype'
MAP = ROOT + '/Maps/Lvl_VanPrototype'
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def make_asset(name, folder, cls, factory):
    path = folder + '/' + name
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    assets.make_directory(folder)
    result = tools.create_asset(name, folder, cls, factory)
    assert result, path
    return result


def material(name, color):
    path = ROOT + '/Materials/' + name
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    result = make_asset(name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    node = unreal.MaterialEditingLibrary.create_material_expression(result, unreal.MaterialExpressionConstant3Vector, -250, 0)
    node.set_editor_property('constant', unreal.LinearColor(*color))
    unreal.MaterialEditingLibrary.connect_material_property(node, '', unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(result, unreal.MaterialExpressionConstant, -250, 150)
    roughness.set_editor_property('r', 0.85)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(result)
    assert assets.save_loaded_asset(result)
    return result


action = make_asset('IA_Interact', ROOT + '/Input', unreal.InputAction, unreal.InputAction_Factory())
action.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
context = make_asset('IMC_VanInteraction', ROOT + '/Input', unreal.InputMappingContext, unreal.InputMappingContext_Factory())
context.unmap_all()
for name in ('E', 'Gamepad_FaceButton_Left'):
    key = unreal.Key()
    key.set_editor_property('key_name', name)
    context.map_key(action, key)
assert assets.save_loaded_asset(action)
assert assets.save_loaded_asset(context)

character_bp = unreal.load_asset('/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter')
character_cdo = unreal.get_default_object(character_bp.generated_class())
character_cdo.set_editor_property('interact_action', action)
# Keep Epic's animation Blueprint and skeleton; Quinn is the requested player placeholder.
quinn = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple')
assert quinn
character_mesh = character_cdo.get_editor_property('mesh')
character_bp.modify()
character_mesh.modify()
character_mesh.set_editor_property('skeletal_mesh_asset', quinn)
character_mesh.set_editor_property('override_materials', [])
assert unreal.BlueprintEditorLibrary.compile_blueprint(character_bp)
assert assets.save_loaded_asset(character_bp)

controller_bp = unreal.load_asset('/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController')
controller_cdo = unreal.get_default_object(controller_bp.generated_class())
contexts = list(controller_cdo.get_editor_property('default_mapping_contexts'))
if context not in contexts:
    contexts.append(context)
    controller_cdo.set_editor_property('default_mapping_contexts', contexts)
assert unreal.BlueprintEditorLibrary.compile_blueprint(controller_bp)
assert assets.save_loaded_asset(controller_bp)

van_class = unreal.load_class(None, '/Script/RoamAlone.GreyboxVan')
assert van_class
van_bp = unreal.load_asset(ROOT + '/Blueprints/BP_GreyboxVan')
assert van_bp, 'The authored BP_GreyboxVan model must exist; this script does not generate van geometry.'

shell = material('M_BlockoutShell', (0.42, 0.46, 0.5, 1))
door = material('M_BlockoutDoor', (0.72, 0.38, 0.08, 1))
ground = material('M_BlockoutGround', (0.13, 0.16, 0.18, 1))
van_defaults = unreal.get_default_object(van_bp.generated_class())
van_bp.modify()
van_defaults.modify()
for property_name, component_name in (
    ('door_root_reference', 'SlidingDoorRoot'),
    ('door_panel_reference', 'SlidingDoorPanel'),
    ('inside_switch_reference', 'InsideDoorSwitch')):
    reference = unreal.ComponentReference()
    reference.set_editor_property('component_property', component_name)
    van_defaults.set_editor_property(property_name, reference)
# Door travel and the interior volume are authored in BP_GreyboxVan; preserve editor tuning.
assert unreal.BlueprintEditorLibrary.compile_blueprint(van_bp)
assert assets.save_loaded_asset(van_bp)

if not assets.does_asset_exist(MAP):
    assets.make_directory(ROOT + '/Maps')
    assert levels.new_level(MAP)

    floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -10))
    floor.set_actor_label('Prototype Ground')
    mesh = floor.static_mesh_component
    mesh.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    floor.set_actor_scale3d(unreal.Vector(24, 24, 0.2))
    mesh.set_material(0, ground)

    van = actors.spawn_actor_from_class(van_bp.generated_class(), unreal.Vector(0, 0, 0))
    van.set_actor_label('Greybox Camper Van')

    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-15, -400, 100), unreal.Rotator(pitch=0, yaw=90, roll=0))
    start.set_actor_label('Van Test Player Start')
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 650), unreal.Rotator(pitch=-40, yaw=-35, roll=0))
    sun.light_component.set_editor_property('intensity', 5.0)
    sun.light_component.set_editor_property('atmosphere_sun_light', True)
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    skylight = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 400))
    skylight.light_component.set_editor_property('intensity', 1.0)
    skylight.light_component.set_editor_property('real_time_capture', True)
    lamp = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(0, 0, 235))
    lamp.light_component.set_editor_property('intensity', 1.0)
    lamp.light_component.set_editor_property('attenuation_radius', 450.0)
    lamp.set_actor_label('Temporary Interior Light')
    settings = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_world_settings()
    settings.set_editor_property('default_game_mode', unreal.load_class(None, '/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C'))
    assert levels.save_current_level()
else:
    assert levels.load_level(MAP)

unreal.log('VAN_PROTOTYPE_SETUP_COMPLETE: ' + MAP)
