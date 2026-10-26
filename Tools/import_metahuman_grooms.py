"""Copy the MetaHuman Creator groom library into the project (Wes, Oct 25 2026: "grab all the other hair styles from the metahumans pipeline and
prepare them for use in the character customization screen").

The library is the launcher's "MetaHuman Creator Core Data" (the MetaHumanCharacter plugin's Content/Optional): on this PC only the UE 5.7 install
has it (D:/UnrealEngine/UE_5.7, 5.6 GB), the 5.8 install and DARKTOWER's do not. The project keeps the MetaHuman assembler's own mirror of that
folder (/MetaHumanCharacter/Optional/Grooms/... -> /Game/MetaHumans/Common/Optional/Grooms/..., where Hair_S_UpdoBraids already lives), so each
groom's folder is copied to its mirror path; the packages still NAME their old paths inside, which the [CoreRedirects] PackageRedirects in
Config/DefaultEngine.ini map onto the mirror until Content/Python/resave_metahuman_grooms.py has resaved them.

    python -I Tools/import_metahuman_grooms.py [--src <Engine/Plugins/MetaHuman/MetaHumanCharacter/Content/Optional/Grooms>] [--dry]

The whole import, done Oct 25 2026 for the 70 grooms (re-run it for a newer library, e.g. once the 5.8 install has the Core Data):
  1. this script (copies; never overwrites);
  2. add the TEMPORARY redirects under [CoreRedirects] in Config/DefaultEngine.ini and restart the editor:
       +PackageRedirects=(OldName="/MetaHumanCharacter/Optional/Grooms/...",NewName="/Game/MetaHumans/Common/Optional/Grooms/",MatchWildcard=true)
       +PackageRedirects=(OldName="/MetaHumanCharacter/Materials/MI_Hair",NewName="/Game/MetaHumans/Common/Materials/MI_Hair")
       +PackageRedirects=(OldName="/MetaHumanCharacter/Materials/MI_Hair_Cards",NewName="/Game/MetaHumans/Common/Materials/MI_Hair_Cards")
       +PackageRedirects=(OldName="/MetaHumanCharacter/Materials/MI_Hair_Helmet",NewName="/Game/MetaHumans/Common/Materials/MI_Hair_Helmet")
       +PackageRedirects=(OldName="/MetaHumanCharacter/Materials/MI_Facial_Hair",NewName="/Game/MetaHumans/Common/Materials/MI_Facial_Hair")
     (MI_PeachFuzz, the fuzz group of Hair_S_BaldingStubble, has no project copy -- its master drags two more plugin textures -- and a package
     redirect cannot rename the object, so the resave gives that empty slot MI_Facial_Hair instead);
  3. Content/Python/resave_metahuman_grooms.py (must end "0 dependency(ies) still on /MetaHumanCharacter/"), then remove the redirects
     (Epic: wildcard redirects slow every lookup) and restart;
  4. Content/Python/make_groom_bindings.py (a binding per groom and survivor face).

Copies, per category (Hair, Beards, Mustaches): every GroomAssets/<Category>/<Groom>/ folder (the groom, its cards meshes, helmet LODs and atlas
textures; not <Groom>_baked, the crowd's baked texture, which no groom references) and Thumbnails/<Category>/<Groom>_Thumb (editor-side previews,
not cooked). Plus the two template heads the library grooms were authored on (GroomMesh/SKM_Groom_Head_Legacy01 / 02 and their skeletons): a
binding transfers a groom from its head onto a survivor's face (Content/Python/make_groom_bindings.py). MH_Groom_Head (208 MB, the head of
Hair_S_Coil alone) is left out. Never overwrites a file the project already has. The editor picks the new files up; resave after the redirect.
"""
import argparse
import json
import os
import re
import shutil

CATEGORIES = ("Hair", "Beards", "Mustaches")
HEADS = ("SKM_Groom_Head_Legacy01", "SKM_Groom_Head_Legacy01_Skeleton", "SKM_Groom_Head_Legacy02", "SKM_Groom_Head_Legacy02_Skeleton")
DEFAULT_SRC = "D:/UnrealEngine/UE_5.7/Engine/Plugins/MetaHuman/MetaHumanCharacter/Content/Optional/Grooms"
DEST = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Content", "MetaHumans", "Common", "Optional", "Grooms")
HEADS_JSON = os.path.join(os.path.dirname(os.path.abspath(__file__)), "metahuman_groom_heads.json")   # read by Content/Python/make_groom_bindings.py


def copy_file(src, dst, dry, stats):
    if os.path.exists(dst):
        stats["kept"] += 1
        return
    stats["copied"] += 1
    stats["bytes"] += os.path.getsize(src)
    if not dry:
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=DEFAULT_SRC)
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if not os.path.isdir(os.path.join(a.src, "GroomAssets")):
        raise SystemExit("no MetaHuman groom library at %s (install 'MetaHuman Creator Core Data' in the launcher, or pass --src)" % a.src)
    stats = {"copied": 0, "kept": 0, "bytes": 0}
    for cat in CATEGORIES:
        grooms = sorted(os.listdir(os.path.join(a.src, "GroomAssets", cat)))
        for groom in grooms:
            folder = os.path.join(a.src, "GroomAssets", cat, groom)
            for name in sorted(os.listdir(folder)):
                if name.endswith(".uasset") and not name.endswith("_baked.uasset"):
                    copy_file(os.path.join(folder, name), os.path.join(DEST, "GroomAssets", cat, groom, name), a.dry, stats)
            thumb = os.path.join(a.src, "Thumbnails", cat, groom + "_Thumb.uasset")
            if os.path.exists(thumb):
                copy_file(thumb, os.path.join(DEST, "Thumbnails", cat, groom + "_Thumb.uasset"), a.dry, stats)
        print("MHGROOMS %s: %d groom(s): %s" % (cat, len(grooms), " ".join(grooms)))
    for head in HEADS:
        copy_file(os.path.join(a.src, "GroomMesh", head + ".uasset"), os.path.join(DEST, "GroomMesh", head + ".uasset"), a.dry, stats)
    # Which head each groom was authored on: the library's own binding (Bindings/<Category>/<Groom>_Binding) targets it.
    heads = {}
    for cat in CATEGORIES:
        for groom in sorted(os.listdir(os.path.join(a.src, "GroomAssets", cat))):
            # <Groom>_Binding, or plain <Groom> (Hair_M_TwistedBraids, Hair_S_BrushCut)
            paths = [os.path.join(a.src, "Bindings", cat, groom + suffix + ".uasset") for suffix in ("_Binding", "")]
            path = next((p for p in paths if os.path.exists(p)), None)
            found = sorted(set(re.findall(rb"/Optional/Grooms/GroomMesh/([A-Za-z0-9_]+)", open(path, "rb").read()))) if path else []
            found = [h.decode() for h in found if not h.endswith(b"_Skeleton")]
            heads[groom] = found[0] if len(found) == 1 else None
    if not a.dry:
        with open(HEADS_JSON, "w", encoding="utf-8") as f:
            json.dump(heads, f, indent=1, sort_keys=True)
    print("MHGROOMS authoring heads: %s -> %s" % (", ".join("%s %d" % (h, list(heads.values()).count(h)) for h in sorted(set(heads.values()), key=str)), HEADS_JSON))
    print("MHGROOMS %s %d file(s) (%.2f GB), kept %d the project already had -> %s" % ("would copy" if a.dry else "copied", stats["copied"],
                                                                                     stats["bytes"] / 1e9, stats["kept"], DEST))


if __name__ == "__main__":
    main()
