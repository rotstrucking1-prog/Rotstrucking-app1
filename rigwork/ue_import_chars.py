import unreal, os, subprocess

REPO = r"C:\Users\Bradh\Downloads\Rotstrucking-app1"
RIGDIR = os.path.join(REPO, "rigwork")
DEST = "/Game/Characters/AoC"

# --- fetch latest assets from branch ---
def git_pull():
    try:
        out = subprocess.run(["git", "fetch", "origin", "aoc-source-v14"], cwd=REPO,
                             capture_output=True, text=True, timeout=120)
        print("GIT_FETCH:", out.returncode, out.stderr[-200:])
        out = subprocess.run(["git", "checkout", "origin/aoc-source-v14", "--", "rigwork"],
                             cwd=REPO, capture_output=True, text=True, timeout=60)
        print("GIT_CHECKOUT_RIGWORK:", out.returncode, out.stderr[-200:])
    except Exception as e:
        print("GIT_ERR:", e)

git_pull()
print("RIGDIR_CONTENTS:", os.listdir(RIGDIR) if os.path.isdir(RIGDIR) else "MISSING")

log = unreal.log
warn = unreal.log_warning

def find_skeleton():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    # search all skeletons, prefer PeasantMan
    assets = ar.get_assets_by_class(unreal.TopLevelAssetPath("/Script/Engine", "Skeleton"), True)
    cand = []
    for a in assets:
        nm = str(a.asset_name)
        pn = str(a.package_name)
        cand.append(pn)
        if "Peasant" in nm or "Peasant" in pn:
            return pn, [str(x) for x in cand]
    return (cand[0] if cand else None), [str(x) for x in cand]

def do_import(fbx, dest, skel):
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = dest
    task.automated = True
    task.save = True
    task.replace_existing = True
    opt = unreal.FbxImportUI()
    opt.set_editor_property("import_mesh", True)
    opt.set_editor_property("import_as_skeletal", True)
    opt.set_editor_property("import_animations", False)
    opt.set_editor_property("import_materials", False)
    opt.set_editor_property("import_textures", False)
    opt.set_editor_property("create_physics_asset", True)
    opt.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    if skel:
        opt.set_editor_property("skeleton", unreal.load_asset(skel))
    sk_data = opt.get_editor_property("skeletal_mesh_import_data")
    sk_data.set_editor_property("import_content_type", unreal.FBXImportContentType.FBXICT_ALL)
    sk_data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    sk_data.set_editor_property("convert_scene", True)
    sk_data.set_editor_property("use_t0_as_ref_pose", False)
    task.options = opt
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = task.get_editor_property("imported_object_paths")
    return [str(p) for p in paths]

def import_tex(png, dest, name, srgb):
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    p = task.get_editor_property("imported_object_paths")
    if p:
        t = unreal.load_asset(str(p[0]))
        if t:
            t.set_editor_property("srgb", srgb)
            if not srgb:
                t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            unreal.EditorAssetLibrary.save_asset(str(p[0]))
        return str(p[0])
    return None

print("=== AOC CHARACTER IMPORT START ===")
skel, allskel = find_skeleton()
print("SKELETONS_FOUND:", allskel)
print("USING_SKELETON:", skel)

results = {}
for tag in ["warrior", "oracle"]:
    fbx = os.path.join(RIGDIR, tag + "_rigged.fbx")
    if not os.path.exists(fbx):
        print("MISSING_FBX:", fbx); continue
    dest = DEST + "/" + tag.capitalize()
    imp = do_import(fbx, dest, skel)
    print("IMPORTED_%s:" % tag, imp)
    results[tag] = imp
    # bounds check
    for op in imp:
        a = unreal.load_asset(op)
        if isinstance(a, unreal.SkeletalMesh):
            b = a.get_bounds()
            be = b.box_extent
            print("BOUNDS_%s:" % tag, "extent=", be.x, be.y, be.z)

print("=== IMPORT DONE ===")
