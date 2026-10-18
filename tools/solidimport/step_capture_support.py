"""Test 13: read-only COM/server checks and strict Parasolid capture evidence."""
import ctypes as C
import hashlib
from pathlib import Path
import uuid
import re

from read_solidlink_environment import referenced_file
from probe_solidimport_text_catalog import confirmed_catalog_bridge

SERVER_SHA256='71d48d786a76aa16031be28c9ebbe4c9b95555643fd79f475b5a6e76587ce83c'
SERVER_CLSID='{0F3A6583-13A8-4304-8544-88602B3A56F5}'
PROGID='SolidLink.Document.2025.4'


def file_digest(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for data in iter(lambda:stream.read(1024*1024),b''):h.update(data)
    return h.hexdigest()


def check_registration(registry,server,record):
    server=Path(server).resolve()
    record['progid']=PROGID
    record['progid_clsid']=registry.value(PROGID+r'\CLSID')
    if uuid.UUID((record['progid_clsid'] or '').strip())!=uuid.UUID(SERVER_CLSID):
        raise ValueError('Nieoczekiwana rejestracja SolidLink 2025.4')
    key='CLSID\\'+SERVER_CLSID
    record['local_server_command']=registry.value(key+r'\LocalServer32')
    record['inproc_server_command']=registry.value(key+r'\InprocServer32')
    record['errors']=list(registry.errors)
    path=referenced_file(record['local_server_command'])
    if record['errors'] or record['inproc_server_command'] or not path or Path(path).resolve()!=server:
        raise ValueError('Rejestracja COM nie wskazuje oczekiwanego SolidLink.exe')
    record['server_path']=str(server);record['sha256']=file_digest(server)
    if record['sha256']!=SERVER_SHA256:raise ValueError('Nieoczekiwana wersja SolidLink.exe')
    record['verified']=True
    return server


def process_image(pid):
    """Query the actual newly observed server image without attaching a debugger."""
    k=C.WinDLL('kernel32',use_last_error=True)
    k.OpenProcess.argtypes=[C.c_uint32,C.c_int32,C.c_uint32];k.OpenProcess.restype=C.c_void_p
    k.QueryFullProcessImageNameW.argtypes=[C.c_void_p,C.c_uint32,C.c_wchar_p,C.POINTER(C.c_uint32)]
    k.QueryFullProcessImageNameW.restype=C.c_int32
    k.CloseHandle.argtypes=[C.c_void_p];k.CloseHandle.restype=C.c_int32
    handle=k.OpenProcess(0x1000,0,pid)
    if not handle:raise C.WinError(C.get_last_error())
    try:
        count=C.c_uint32(32768);path=C.create_unicode_buffer(count.value)
        if not k.QueryFullProcessImageNameW(handle,0,path,C.byref(count)):
            raise C.WinError(C.get_last_error())
        return Path(path.value).resolve()
    finally:k.CloseHandle(handle)


def confirmed_capture(report):
    if not confirmed_catalog_bridge(report,worker_schema='solidimport-step-capture-worker-13-v1',step_capture=True):
        return False
    w=report['worker'];capture=w.get('capture',{});output=report.get('captured_file',{})
    argument=w.get('step_argument',{})
    if (not all(report.get(k) is True for k in ['source_unchanged','input_copy_unchanged','server_unchanged'])
        or report.get('solidlink_processes_before')!=[] or not report.get('registration',{}).get('verified')
        or report.get('registration',{}).get('sha256')!=SERVER_SHA256 or report.get('server_monitor_error')
        or not report.get('observed_servers')):
        return False
    if not all(isinstance(s,dict) and s.get('sha256')==SERVER_SHA256 and type(s.get('pid')) is int
               and s['pid']>0 and s.get('path_matches_registration') is True for s in report['observed_servers']):
        return False
    return (all(w.get(k) is True for k in ['step_conversion_attempted','step_mfc_thread_ready',
                'no_parasolid_before_step','capture_boundary_instruction_checked','parasolid_capture_written',
                'worker_input_copy_unchanged'])
        and w.get('step_returned') is False and w.get('active_drawing_supplied') is False
        and w.get('geometry_verified') is False
        and w.get('step_thread_id')==w.get('capture_thread_id')==w.get('mfc_thread_id')
        and w.get('capture_boundary')=='IAlphaCamApp.ActiveDrawing'
        and type(w.get('recognized_candidate_count')) is int and w['recognized_candidate_count']==1
        and argument.get('argument_type')=='const char*' and argument.get('native_unicode_path_matches') is True
        and argument.get('native_input_sha256')==report.get('input_sha256')
        and capture.get('recognized_parasolid_header') is True and capture.get('source_copy_match') is True
        and capture.get('filename') in ('captured.x_t','captured.x_b')
        and output.get('exists') is True and output.get('recognized_parasolid') is True
        and type(output.get('size_bytes')) is int and output['size_bytes']>0
        and type(capture.get('size_bytes')) is int and output.get('size_bytes')==capture.get('size_bytes')
        and output.get('format') in ('text','binary')
        and output.get('sha256')==capture.get('sha256')
        and isinstance(output.get('sha256'),str) and re.fullmatch(r'[0-9a-f]{64}',output['sha256']) is not None
        and isinstance(report.get('input_sha256'),str) and re.fullmatch(r'[0-9a-f]{64}',report['input_sha256']) is not None)
