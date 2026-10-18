"""Reviewed layouts for two exact supplied x64 Feature.dll builds.

No runtime offset guessing. Selecting a profile never accepts a different hash.
The 2025.4 typelib is byte-identical to the 2025.3 typelib.
"""
OLD_SHA256 = '4bd2cbb8606a8a5c9dc5c44b9fb0de1e34ecfb01438b6a1b13898873f6878fec'
CURRENT_SHA256 = '5bbfbdbd41c32dde36dfc81db3a7a022b9e3688622f7005130cc207bc6638438'
BASE_TEXT_COUNTS = (25, 6, 8, 31, 36, 34, 162, 19, 83, 10, 7, 29, 5,
                    7, 17, 19, 9, 6, 7, 2, 11, 10, 25, 2, 6, 7)

BUILDS = {
    OLD_SHA256: {
        'version': '2025.3.0.200',
        'exports': dict(GetUIVersion=0x38A10, GetSolidFeatures=0x389C0,
                        InitAlphacamAddIn=0x38B50, ObjectDoWrite2=0x40050,
                        ObjectQueryWrite2=0x41370, ObjectNumberOf=0x40CE0),
        'state': dict(application=0x186658, context_manager=0x1865B0, depth=0x180360,
                      backend=0x180140, progress=0x186640, progress_parent=0x186648),
        'text_pointer': 0x186548,
        'text_counts': BASE_TEXT_COUNTS,
        'mfc_iat': dict(construct=0x13D068, assign=0x13D8B0, get=0x13D060, destroy=0x13D070),
        'crt_iat': dict(_wfopen_s=0x13CE28, fwrite=0x13CE38, fflush=0x13CE20, fclose=0x13CE68),
    },
    CURRENT_SHA256: {
        'version': '2025.4.1.200',
        'exports': dict(GetUIVersion=0x38A40, GetSolidFeatures=0x389F0,
                        InitAlphacamAddIn=0x38B80, ObjectDoWrite2=0x40080,
                        ObjectQueryWrite2=0x413A0, ObjectNumberOf=0x40D10),
        'state': dict(application=0x18A660, context_manager=0x18A5B8, depth=0x184360,
                      backend=0x184140, progress=0x18A648, progress_parent=0x18A650),
        'text_pointer': 0x18A588,
        'text_counts': BASE_TEXT_COUNTS[:3] + (33,) + BASE_TEXT_COUNTS[4:],
        'mfc_iat': dict(construct=0x140068, assign=0x1408B8, get=0x140060, destroy=0x140070),
        'crt_iat': dict(_wfopen_s=0x13FE30, fwrite=0x13FE40, fflush=0x13FE28, fclose=0x13FE70),
    },
}


def profile(sha256):
    if sha256 not in BUILDS:
        raise ValueError('No reviewed Feature.dll layout for this SHA256; a verified x64 DLL build is required')
    # Consumers get private copies; tests or another adapter cannot change offsets.
    import copy
    return copy.deepcopy(BUILDS[sha256])
