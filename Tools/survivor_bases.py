"""The survivor bases as Config/DefaultGame.ini lists them (UMOSurvivorBaseSettings +Bases, MOSurvivorBase.h), for the checks: they follow the
config, so a new base (or one dropped, Oct 27: "drop lillian for now and add the four variants i made") touches no check.

    import survivor_bases as sb
    sb.bases()          # [{'id': 'Femme', 'name': 'Femme (original)', 'body': 'SKM_MH_F_1_BodyMesh', 'face': ..., 'hair': 'Hair_S_UpdoBraids', 'fat': 18.0, 'muscle': 30.0, ...}, ...]
    sb.new_bases()      # every base but the femme (the Blueprint's own body)
"""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INI = os.path.join(ROOT, "Config", "DefaultGame.ini")


def _asset(path):
    """'/Game/X/Y.Y' -> 'Y' ('' when empty or NoGroom)."""
    return path.rsplit(".", 1)[-1].rsplit("/", 1)[-1] if path and path.startswith("/") else ""


def bases():
    rows = []
    for line in open(INI, encoding="utf-8"):
        if not line.startswith("+Bases=("):
            continue
        get = lambda key: (re.search(r'\b%s="([^"]*)"' % key, line) or [None, ""])[1]
        num = lambda key, default: float((re.search(r'\b%s=([\d.]+)' % key, line) or [None, default])[1])
        name = re.search(r'DisplayName=INVTEXT\("([^"]*)"\)', line)
        rows.append({"id": get("Id"), "name": name.group(1) if name else get("Id"), "body": _asset(get("Body")), "face": _asset(get("Face")),
                     "hair": _asset(get("Hair")), "eyebrows": _asset(get("Eyebrows")), "eyelashes": _asset(get("Eyelashes")),
                     "beard": _asset(get("Beard")), "mustache": _asset(get("Mustache")),
                     "sex": (re.search(r"\bSex=(\w+)", line) or [None, "Unspecified"])[1],
                     "fat": num("BodyFatPercent", 18.0), "muscle": num("MuscleMassKg", 30.0)})
    return rows


def new_bases():
    return [b for b in bases() if b["id"] != "Femme"]


def groom_label(asset_name):
    """The creator's name for a groom (UMOAppearanceSubsystem::MakeGroomDisplayName, test MOFramework.Appearance.GroomStyleNames):
    Hair_S_CoilBuzzCut -> 'Coil buzz cut (short)'."""
    def words(camel):
        out = ""
        for i, c in enumerate(camel):
            if i > 0 and (c.isupper() or c.isdigit() != camel[i - 1].isdigit()):
                out += " "
            out += c.lower()
        return out

    parts = asset_name.split("_")
    length = {"S": "short", "M": "medium", "L": "long"}.get(parts[1]) if len(parts) == 3 else None
    if not length:
        plain = words(asset_name.replace("_", ""))
        return plain[:1].upper() + plain[1:]
    style = words(parts[2])
    if parts[0] != "Hair":
        style += " " + parts[0].lower()
    return "%s%s (%s)" % (style[:1].upper(), style[1:], length)
