# -*- mode: python ; coding: utf-8 -*-
#
# PyInstaller spec for TweakLoader.
#
# Build with:
#   pip install pyinstaller
#   pyinstaller build.spec
#
# The resulting single .exe is in dist/TweakLoader.exe.
# Run it directly — no Python installation required.

block_cipher = None

a = Analysis(
    ["tweakloader.py"],
    pathex=["."],
    binaries=[
        # keystone's native DLL — PyInstaller finds the Python wrapper but
        # misses the actual shared library without this explicit entry.
        ("C:/path/to/keystone.dll", "keystone"),
    ],
    datas=[
        # Bundle reloc.py as a plain data file so it's importable at runtime.
        ("reloc.py", "."),
    ],
    hiddenimports=[
        # bleak's Windows BLE backend is loaded dynamically.
        "bleak.backends.winrt",
        "bleak.backends.winrt.client",
        "bleak.backends.winrt.scanner",
        "bleak.backends.winrt.utils",
        "bleak.backends.service",
        "bleak.backends.characteristic",
        "bleak.backends.descriptor",
        # winrt / winsdk packages used by bleak ≥ 0.20.
        "winrt.windows.devices.bluetooth",
        "winrt.windows.devices.bluetooth.advertisement",
        "winrt.windows.devices.bluetooth.genericattributeprofile",
        "winrt.windows.devices.radios",
        "winrt.windows.foundation",
        "winrt.windows.foundation.collections",
        "winrt.windows.storage.streams",
        # keystone and capstone have native extension modules — include them.
        "keystone",
        "capstone",
        "capstone.arm",
        # elftools sub-packages.
        "elftools.elf.elffile",
        "elftools.elf.sections",
        "elftools.elf.relocation",
        "elftools.common.elfstructs",
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[
        # Trim things we definitely don't use.
        "matplotlib",
        "numpy",
        "scipy",
        "PIL",
        "cv2",
        "IPython",
        "jupyter",
    ],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name="TweakLoader",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    # CLI tool — keep the console attached so stdout/stderr and the
    # interactive device picker work.
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=None,
)
