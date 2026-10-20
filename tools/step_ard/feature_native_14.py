"""Disposable Feature worker with current CLS classification and measured bounds."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

REFERENCE=Path(__file__).resolve().parent.parent/'alphacam_reference'
sys.path.insert(0,str(REFERENCE))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dll',type=Path,required=True)
    p.add_argument('--model',type=Path,required=True)
    p.add_argument('--work-dir',type=Path,required=True)
    p.add_argument('--cls-ready',type=Path,required=True)
    from feature_builds import OLD_SHA256, BUILDS
    p.add_argument('--build-sha256',choices=tuple(BUILDS),default=OLD_SHA256)
    p.add_argument('--direct-record',action='store_true')
    p.add_argument('--kernel-errors',action='store_true')
    p.add_argument('--schema-path',action='store_true')
    p.add_argument('--measure-only',action='store_true')
    p.add_argument('--geometry-snapshot',action='store_true')
    p.add_argument('--model-sha256',help='Expected fresh Parasolid digest for the integrated pipeline')
    a=p.parse_args()
    from probe_feature_2025 import worker,inspect_pe
    if not inspect_pe(a.dll,a.build_sha256)['matches_analyzed_build']:raise ValueError('Unexpected Feature.dll build')
    if a.model_sha256:
        before=hashlib.sha256(a.model.read_bytes()).hexdigest()
        if before!=a.model_sha256:
            raise ValueError('Feature input is not the expected fresh Parasolid')
    state=json.loads(a.cls_ready.read_text(encoding='utf-8-sig'))
    if (state.get('schema')!='feature-cls-session-14-v1' or state.get('ready') is not True
        or state.get('argument_bridge_selftest') is not True
        or state.get('access_code')!=348 or state.get('request_count')!=1
        or state.get('validation_before')!=101 or state.get('validation_after')!=101
        or type(state.get('classification_flag')) is not bool
        or state.get('special_key_projection')!=(16 if state['classification_flag'] else 0)
        or state.get('supported_special_key_mask')!=16 or state.get('full_special_key_verified') is not False):
        raise ValueError('No valid current CLS classification')
    environment={'source':'current_CLS_GetAccessCode_348_projection','application':{},
                 'note':'Only bit 16 consumed by this exact Feature Bodies.Save; not full SpecialKey.'}
    result=worker(a.dll.resolve(),'model',[a.dll.parent.parent.parent],a.model.resolve(),
                  a.work_dir.resolve(),environment,
                  batch_profile=dict(state,direct_record=a.direct_record,kernel_errors=a.kernel_errors,
                                     schema_path=a.schema_path,measure_only=a.measure_only,
                                     geometry_snapshot=a.geometry_snapshot),build_sha256=a.build_sha256)
    if a.model_sha256:
        after=hashlib.sha256(a.model.read_bytes()).hexdigest()
        result['input_copy']={'sha256_before':before,'sha256_after':after,
                              'unchanged':before==after,'path':str(a.model.resolve())}
        if after!=a.model_sha256:
            raise ValueError('Feature input changed during conversion')
    print('FEATURE_PROBE_RESULT '+json.dumps(result,ensure_ascii=True),flush=True)
    return 0


if __name__=='__main__':
    raise SystemExit(main())
