"""Scoped configuration of the existing Feature schema-directory resolver.

The reviewed initializer copies IFrame.PathOfThisAddin to CString RVA 18a3b8,
with a trailing backslash. Original I/O callbacks append Schemas and a filename.
Use native MFC assignment, preserve the previous value, and restore before unload.
No vendor code/import/callback changes and no schema-file writes by this adapter.
"""
import ctypes as C
import hashlib
from pathlib import Path

from feature_builds import CURRENT_SHA256, profile
from feature_texts import MFCStrings

PATH_RVA = 0x18A3B8


def inventory(directory):
    directory = Path(directory)
    paths = sorted(p for p in directory.iterdir() if p.is_file())
    if not paths or len(paths) > 4096:
        raise ValueError('Missing or unexpectedly large Feature/Schemas catalog')
    result, total = [], 0
    for path in paths:
        size = path.stat().st_size
        total += size
        if size > 16 * 1024 * 1024 or total > 512 * 1024 * 1024:
            raise ValueError('Unexpected schema catalog size')
        result.append({'name': path.name, 'size_bytes': size,
                       'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    return result


class NativeSchemaPath:
    def __init__(self, module_base, feature_directory, verified_sha256, functions=None):
        if verified_sha256 != CURRENT_SHA256 or C.sizeof(C.c_void_p) != 8:
            raise ValueError('Schema path adapter requires the reviewed Feature 2025.4 x64 build')
        self.directory = Path(feature_directory).resolve()
        self.schemas = self.directory / 'Schemas'
        self.before = inventory(self.schemas)
        self.address = int(module_base) + PATH_RVA
        self.functions = functions or MFCStrings(int(module_base), profile(verified_sha256)['mfc_iat'])
        self.previous = self.functions.read(self.address)
        # A pre-existing value means a different initialization state, which
        # this narrow adapter must not silently overwrite.
        if self.previous:
            raise ValueError('Feature add-in path already configured; unexpected initializer state')
        self.value = str(self.directory).rstrip('\\/') + '\\'
        self.installed = False
        self.closed = False

    def install(self):
        if self.installed or self.closed:
            raise RuntimeError('Schema path adapter cannot be reused')
        # Mark ownership before assignment so a failed readback is still restored.
        self.installed = True
        self.functions.assign(self.address, self.value)
        if self.functions.read(self.address) != self.value:
            raise RuntimeError('Native Feature add-in path readback mismatch')
        return {'ok': True, 'path_rva': hex(PATH_RVA), 'path': self.value,
                'schema_directory': str(self.schemas), 'native_readback_matches': True,
                'previous_path_empty': True, 'files_before': self.before,
                'original_schema_callbacks_retained': True,
                'full_addin_initializer_called': False}

    def close(self):
        if self.closed:
            return self.result
        current_matches = True
        if self.installed:
            current_matches = self.functions.read(self.address) == self.value
            self.functions.assign(self.address, self.previous)
            restored = self.functions.read(self.address) == self.previous
        else:
            restored = True
        self.closed = True
        after = inventory(self.schemas)
        self.result = {'configured_value_unchanged': current_matches,
                       'previous_path_restored': restored,
                       'files_after': after, 'files_unchanged': self.before == after}
        self.result['ok'] = current_matches and restored and self.before == after
        return self.result
