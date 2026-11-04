"""Pack a build folder into its release archive, for GitHub (make dist).

usage: python tools/dist.py <build dir> <out dir> <windows|linux> <version> [--bonus <file>] [--keyboard-mouse-mod <Auto mod dir>]

Windows: <out>/MHGURecomp-Enhancement-win_x86-64.zip; Linux: <out>/ArchLinux-MHGURecomp.<version>.tar.gz (the programs
keep their executable permission). Inside, Windows-MHGURecomp/ or ArchLinux-MHGURecomp/ holds what a player copies: the game, its code
library, the updater, the DLLs (Windows, including SR and both NR runtimes), pipelines.list, licenses/, bonus/system (--bonus: the donor save of the free downloads) and
the empty data/, mods/ and cheats/ folders, with a note in data/ of what goes there. Nothing else of the build folder:
not its game files, mods, settings, caches or log. The updater (updater/updater.c) installs the same archive, leaving
data/, other mods, cheats/ and bonus/ alone; it may update the bundled Auto prompt resources. Prints the archive's size and SHA-256: GitHub shows that digest for the upload.
"""

import hashlib
import io
import json
import os
import sys
import tarfile
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

DLSS_RUNTIMES = (
    "graphics/nvidia/ngx/mhgu-nvngx.dll",
    "graphics/nvidia/ngx/nr/official/nvngx_dlssnr.dll",
    "graphics/nvidia/ngx/nr/sm89/nvngx_dlssnr.dll",
)
GRAPHICS_RUNTIME_GROUPS = (
    DLSS_RUNTIMES,
    ("graphics/intel/xess/libxess.dll",),
    (
        "graphics/amd/fsr/amd_fidelityfx_loader_vk.dll",
        "graphics/amd/fsr/amd_fidelityfx_upscaler_vk.dll",
        "graphics/amd/fsr/amd_fidelityfx_framegeneration_vk.dll",
    ),
    (
        "graphics/nvidia/streamline/NvLowLatencyVk.dll",
        "graphics/nvidia/streamline/nvngx_dlss.dll",
        "graphics/nvidia/streamline/nvngx_dlssg.dll",
        "graphics/nvidia/streamline/sl.common.dll",
        "graphics/nvidia/streamline/sl.dlss.dll",
        "graphics/nvidia/streamline/sl.dlss_g.dll",
        "graphics/nvidia/streamline/sl.interposer.dll",
        "graphics/nvidia/streamline/sl.pcl.dll",
        "graphics/nvidia/streamline/sl.reflex.dll",
    ),
)
LEGACY_GRAPHICS_DLLS = {
    "mhgu-nvngx.dll",
    "nvngx_dlss.dll",
    "nvngx_dlssnr.dll",
    "amd_fidelityfx_vk.dll",
    *(Path(p).name.lower() for group in GRAPHICS_RUNTIME_GROUPS for p in group),
}

MENU_FONT = "NotoSansCJKsc-Regular.otf"

DATA_NOTE = (
    "Put your own dump of Monster Hunter Generations Ultimate 1.4.0 here: the exefs and romfs folders\n"
    "(data/exefs/main, data/romfs/...).\n"
)


def files(build, platform):
    """(path in the archive, file on disk, executable) of everything shipped from the build folder."""
    if platform == "windows":
        programs = ["mhgurecomp.exe", "mhgugame.dll", "mhgurecomp-updater.exe"]
        extra = sorted(
            n
            for n in os.listdir(build)
            if n.lower().endswith(".dll")
            and n not in programs
            and n.lower() not in LEGACY_GRAPHICS_DLLS
        )
    else:
        programs = ["mhgurecomp", "mhgugame.so", "mhgurecomp-updater"]
        extra = []
    out = []
    runtimes = (
        [name for group in GRAPHICS_RUNTIME_GROUPS for name in group]
        if platform == "windows"
        else []
    )
    for n in programs + extra + runtimes:
        p = os.path.join(build, n)
        if not os.path.isfile(p):
            sys.exit(f"dist: {p} is missing (make first)")
        out.append((n, p, n in programs))
    pl = os.path.join(
        build, "pipelines.list"
    )  # (the recorded pipelines a first run builds: make's pipelist)
    if not os.path.isfile(pl):
        sys.exit(f"dist: {pl} is missing (make first)")
    out.append(("pipelines.list", pl, False))
    lic = os.path.join(build, "licenses")
    if not os.path.isdir(lic):
        sys.exit(f"dist: {lic} is missing (make first)")
    for d, _, names in sorted(os.walk(lic)):
        for n in sorted(names):
            p = os.path.join(d, n)
            out.append(
                ("licenses/" + os.path.relpath(p, lic).replace(os.sep, "/"), p, False)
            )
    # The UI needs Chinese glyphs even with no Forge mod installed. Keep this explicit so unrelated
    # developer fonts never enter a release, and carry both the license and upstream attribution.
    for archived, name in (
        ("fonts/" + MENU_FONT, MENU_FONT),
        ("licenses/NotoSansCJK-OFL.txt", "OFL.txt"),
        ("licenses/NotoSansCJK-NOTICE.txt", "NOTICE.txt"),
    ):
        path = ROOT / "third_party/noto" / name
        if not path.is_file():
            sys.exit(f"dist: bundled UI font asset {path} is missing")
        out.append((archived, str(path), False))
    for rel in (
        "README.md",
        "docs/README.zh-CN.md",
        "docs/README.zh-TW.md",
        "docs/DEVELOPMENT.md",
        "third_party/ngx/README.md",
        "third_party/ngx/LICENSE.txt",
        "third_party/xess/LICENSE.txt",
        "third_party/amd-fsr/LICENSE.md",
        "third_party/streamline/LICENSE.txt",
        "third_party/streamline/3rd-party-licenses.md",
        "third_party/streamline/nvngx_dlss.license.txt",
        "third_party/streamline/reflex.license.txt",
        "LICENSE",
        "THIRD_PARTY_NOTICES",
        "VERSION",
        "UPSTREAM_VERSION",
    ):
        out.append((rel, str(ROOT / rel), False))
    return out


def check_version(build, platform, version):
    """The updater must be this version's: it compares its own with GitHub's latest release."""
    p = os.path.join(
        build, "mhgurecomp-updater" + (".exe" if platform == "windows" else "")
    )
    with open(p, "rb") as f:
        if b"MHGURecomp-updater/" + version.encode() + b"\0" not in f.read():
            sys.exit(
                f"dist: {p} was not built as version {version} (make dist VERSION={version} rebuilds it)"
            )


def prompt_files(mod):
    """Ship only the explicitly selected Auto pack, never the build's other mods."""
    root = Path(mod).resolve() / "romfs"
    if (root / "keyboard_mouse_input.flag").read_text().strip() != "Auto":
        raise ValueError("Release prompts must use the Auto layout")
    with (root / "keyboard_mouse_prompts.bin").open("rb") as stream:
        if stream.read(8) != b"MHGUPC01":
            raise ValueError("Invalid automatic prompt pack")
    entries = []
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise ValueError(f"Prompt pack contains a symbolic link: {path}")
        if path.is_file():
            rel = path.relative_to(root).as_posix()
            if rel not in (
                "keyboard_mouse_input.flag",
                "keyboard_mouse_prompts.bin",
            ) and not rel.startswith("nativeNX/"):
                raise ValueError(f"Unexpected prompt file: {rel}")
            entries.append(
                (f"mods/Keyboard_Mouse_Input_Auto/romfs/{rel}", str(path), False)
            )
    if not any("/nativeNX/" in rel for rel, _, _ in entries):
        raise ValueError("Prompt pack has no game resource overlays")
    return entries


def main():
    args = sys.argv[1:]
    bonus = None
    prompt_mod = None
    if "--keyboard-mouse-mod" in args:
        i = args.index("--keyboard-mouse-mod")
        prompt_mod = args[i + 1]
        del args[i : i + 2]
    if "--bonus" in args:
        i = args.index("--bonus")
        bonus = args[i + 1]
        del args[i : i + 2]
    if len(args) != 4 or args[2] not in ("windows", "linux"):
        sys.exit(__doc__)
    build, out_dir, platform, version = args
    upstream = (ROOT / "UPSTREAM_VERSION").read_text(encoding="utf-8").strip()
    metadata = (
        json.dumps(
            {
                "product": "MHGURecomp Enhanced",
                "version": version,
                "upstream_version": upstream,
                "release_title": f"{version}-{upstream}",
            },
            ensure_ascii=False,
            indent=2,
        )
        + "\n"
    )
    check_version(build, platform, version)
    top = ("Windows" if platform == "windows" else "ArchLinux") + "-MHGURecomp"
    name = (
        "MHGURecomp-Enhancement-win_x86-64.zip"
        if platform == "windows"
        else f"{top}.{version}.tar.gz"
    )
    entries = files(build, platform)
    if prompt_mod:
        entries.extend(prompt_files(prompt_mod))
    if bonus:
        if not os.path.isfile(bonus):
            sys.exit(f"dist: {bonus} is missing")
        entries.append(("bonus/system", bonus, False))
    else:
        print("dist: no bonus/system (BONUS=<file>): bonus/ left empty")
    dirs = ["", "data", "mods", "cheats", "bonus"]
    for rel, _, _ in entries:
        parts = rel.split("/")[:-1]
        for k in range(1, len(parts) + 1):
            d = "/".join(parts[:k])
            if d not in dirs:
                dirs.append(d)
    note = "data/putexefsromfsfoldershere.txt"

    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, name)
    part = path + ".part"
    now = time.time()
    if platform == "windows":
        with zipfile.ZipFile(part, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for d in dirs:
                z.writestr(
                    zipfile.ZipInfo(
                        (top + "/" + d).rstrip("/") + "/", time.localtime(now)[:6]
                    ),
                    b"",
                )
            z.writestr(
                zipfile.ZipInfo(f"{top}/{note}", time.localtime(now)[:6]),
                DATA_NOTE,
                zipfile.ZIP_DEFLATED,
            )
            z.writestr(f"{top}/release-info.json", metadata, zipfile.ZIP_DEFLATED)
            for rel, p, _ in entries:
                z.write(p, f"{top}/{rel}")
    else:

        def info(rel, size, mode, mtime, kind=tarfile.REGTYPE):
            t = tarfile.TarInfo((top + "/" + rel).rstrip("/"))
            t.size, t.mode, t.mtime, t.type = size, mode, int(mtime), kind
            t.uid = t.gid = 0
            t.uname = t.gname = ""
            return t

        with tarfile.open(
            part, "w:gz", compresslevel=9, format=tarfile.GNU_FORMAT
        ) as t:
            for d in dirs:
                t.addfile(info(d, 0, 0o755, now, tarfile.DIRTYPE))
            t.addfile(
                info(note, len(DATA_NOTE), 0o644, now), io.BytesIO(DATA_NOTE.encode())
            )
            meta_bytes = metadata.encode("utf-8")
            t.addfile(
                info("release-info.json", len(meta_bytes), 0o644, now),
                io.BytesIO(meta_bytes),
            )
            for rel, p, program in entries:
                with open(p, "rb") as f:
                    t.addfile(
                        info(
                            rel,
                            os.path.getsize(p),
                            0o755 if program else 0o644,
                            os.path.getmtime(p),
                        ),
                        f,
                    )
    os.replace(part, path)

    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    print(f"dist: {path}")
    print(f"      {os.path.getsize(path)} bytes, sha256:{h.hexdigest()}")
    Path(path + ".sha256").write_text(f"{h.hexdigest()}  {name}\n", encoding="ascii")
    print(f"      release title {version}-{upstream}; tag {version}")


if __name__ == "__main__":
    main()
