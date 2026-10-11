#!/usr/bin/env python3
#
# WebUI filesystem baseline: builds the LittleFS image of every environment from a git export with fixed
# timestamps, so the output of two web builders (PHP/Python) or two commits can be compared byte for byte.
#
#   python scripts/tools/webui_baseline.py capture [--envs all|<env>...] [-j N]
#   python scripts/tools/webui_baseline.py capture --commits elf/webui_baseline/<name>/baseline.json
#   python scripts/tools/webui_baseline.py diff <baseline dir> <other dir>
#   python scripts/tools/webui_baseline.py compare --baseline <dir> [--overlay <src>=<dst>]...
#
# The source tree is always a `git archive` export of kfc_fw and the repos linked into lib/ (real directories,
# no junctions), never the working copy. Every file and directory of the export gets the same mtime, the web
# builder writes the source mtimes into .lnk/.listings, and the image is created from a copy of data/ with
# the same mtime and a fixed data/.pvt/build, so the image only depends on the commits and the tools.
#

import argparse
import binascii
import concurrent.futures
import datetime
import hashlib
import io
import json
import os
import re
import shlex
import shutil
import stat
import struct
import subprocess
import sys
import tarfile
import tempfile
import threading
import time

PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DEFAULT_OUT_DIR = os.path.join(PROJECT_DIR, 'elf', 'webui_baseline')
DEFAULT_WORK_DIR = os.path.join(tempfile.gettempdir(), 'kfc_webui_baseline')
DEFAULT_MTIME = 1767225600  # 2026-01-01 00:00:00 UTC
PVT_BUILD_PLACEHOLDER = 'KFC baseline'
PROJECT_REPO = 'kfc_fw'

_print_lock = threading.Lock()


def log(msg):
    with _print_lock:
        print(msg, flush=True)


def platformio_core_dir():
    return os.environ.get('PLATFORMIO_CORE_DIR', os.path.join(os.path.expanduser('~'), '.platformio'))


def pio_exe():
    exe = shutil.which('pio')
    if exe:
        return exe
    scripts = 'Scripts' if os.name == 'nt' else 'bin'
    return os.path.join(platformio_core_dir(), 'penv', scripts, 'pio')


def mklittlefs_exe():
    path = os.path.join(platformio_core_dir(), 'packages', 'tool-mklittlefs', 'mklittlefs')
    if os.name == 'nt':
        path += '.exe'
    return path


def run(args, cwd=None, env=None, check=True):
    res = subprocess.run(args, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out = res.stdout.decode('utf-8', errors='replace')
    if check and res.returncode != 0:
        raise RuntimeError('%s failed with exit code %d:\n%s' % (' '.join(args), res.returncode, out[-4000:]))
    return res.returncode, out


def first_line(args):
    try:
        _, out = run(args, check=False)
        return out.strip().splitlines()[0] if out.strip() else ''
    except OSError as e:
        return 'n/a (%s)' % e


# ---------------------------------------------------------------------------------------------------------
# repositories


def git(repo, *args):
    return run(['git', '-C', repo] + list(args))[1].strip()


def find_repos():
    # kfc_fw and every directory in lib/ that is the top level of its own git repo (the junctions)
    repos = {PROJECT_REPO: PROJECT_DIR}
    lib_dir = os.path.join(PROJECT_DIR, 'lib')
    for name in sorted(os.listdir(lib_dir)):
        path = os.path.join(lib_dir, name)
        if not os.path.isdir(path):
            continue
        code, top = run(['git', '-C', path, 'rev-parse', '--show-toplevel'], check=False)
        if code != 0:
            continue
        if os.path.normcase(os.path.realpath(top.strip())) == os.path.normcase(os.path.realpath(path)):
            repos[name] = path
    return repos


def repo_info(name, path, commit=None):
    commit = git(path, 'rev-parse', commit or 'HEAD')
    return {
        'path': os.path.realpath(path),
        'commit': commit,
        'date': git(path, 'show', '-s', '--format=%cI', commit),
        'subject': git(path, 'show', '-s', '--format=%s', commit),
        'dirty': bool(git(path, 'status', '--porcelain', '--untracked-files=no')) if commit == git(path, 'rev-parse', 'HEAD') else None,
    }


def baseline_name(repos):
    kfc = repos[PROJECT_REPO]['commit'][:8]
    lib = repos.get('KFCLibrary', {}).get('commit', '0' * 8)[:8]
    return '%s_%s' % (kfc, lib)


# ---------------------------------------------------------------------------------------------------------
# export


def set_mtime_tree(root, mtime):
    for dirpath, dirnames, filenames in os.walk(root, topdown=False):
        for name in filenames:
            path = os.path.join(dirpath, name)
            if not os.path.islink(path):
                os.utime(path, (mtime, mtime))
        os.utime(dirpath, (mtime, mtime))


def remove_tree(path):
    def onerror(func, p, exc):
        os.chmod(p, stat.S_IWRITE)
        func(p)
    if os.path.lexists(path):
        shutil.rmtree(path, onerror=onerror)


def extract_git_archive(repo, commit, dst):
    os.makedirs(dst, exist_ok=True)
    data = subprocess.run(['git', '-C', repo, 'archive', '--format=tar', commit], stdout=subprocess.PIPE,
                          check=True).stdout
    with tarfile.open(fileobj=io.BytesIO(data), mode='r:') as tar:
        if hasattr(tarfile, 'fully_trusted_filter'):
            tar.extractall(dst, filter='fully_trusted')
        else:
            tar.extractall(dst)


def apply_overlays(export_dir, overlays):
    for src, dst in overlays:
        target = os.path.join(export_dir, dst)
        if os.path.isdir(src):
            remove_tree(target)
            shutil.copytree(src, target, ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
        else:
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copy2(src, target)


def create_export(repos, export_dir, mtime, overlays):
    remove_tree(export_dir)
    extract_git_archive(repos[PROJECT_REPO]['path'], repos[PROJECT_REPO]['commit'], export_dir)
    for name, info in repos.items():
        if name != PROJECT_REPO:
            extract_git_archive(info['path'], info['commit'], os.path.join(export_dir, 'lib', name))
    apply_overlays(export_dir, overlays)
    os.makedirs(os.path.join(export_dir, 'logs'), exist_ok=True)
    set_mtime_tree(export_dir, mtime)
    # pristine data/ that is restored before every environment, nothing leaks from one env into the next
    shutil.copytree(os.path.join(export_dir, 'data'), os.path.join(export_dir, '.pristine_data'))


def reset_export_data(export_dir, mtime):
    data_dir = os.path.join(export_dir, 'data')
    remove_tree(data_dir)
    shutil.copytree(os.path.join(export_dir, '.pristine_data'), data_dir)
    set_mtime_tree(data_dir, mtime)
    # the hash file does not contain the env, a previous env with the same files would skip the build
    for name in ('web_builder.hash', 'web_builder_py.hash'):
        path = os.path.join(export_dir, 'logs', name)
        if os.path.exists(path):
            os.unlink(path)


def list_envs(export_dir):
    _, out = run([pio_exe(), 'project', 'config', '-d', export_dir, '--json-output'])
    start = out.find('[')
    config = json.loads(out[start:])
    return [section[4:] for section, _ in config if section.startswith('env:')]


# ---------------------------------------------------------------------------------------------------------
# per environment


def parse_mklittlefs_args(output):
    for line in output.splitlines():
        if 'mklittlefs' not in line or ' -c ' not in line:
            continue
        try:
            parts = shlex.split(line, posix=False)
        except ValueError:
            continue
        args = {}
        for i, part in enumerate(parts[:-1]):
            if part in ('-s', '-p', '-b'):
                args[part] = int(parts[i + 1].strip('"'))
        if '-s' in args:
            return [str(x) for key in ('-p', '-b', '-s') if key in args for x in (key, args[key])]
    return None


def _lfs_crc(data, crc=0xffffffff):
    # littlefs CRC-32: reflected 0xedb88320, no final xor (binascii applies the inversions)
    return binascii.crc32(data, crc ^ 0xffffffff) ^ 0xffffffff


LFS_TYPE_CRC = 0x500
LFS_TYPE_ATTR_TIME = 0x300 | ord('t')
LFS_TYPE_ATTR_CREATED = 0x300 | ord('c')


def normalize_lfs_root_time(image, block_size, mtime):
    # mklittlefs sets the 't' and 'c' attributes of the root directory (id 0) to time(NULL) after packing,
    # the only value of the image that is not taken from data/. The root directory lives in the superblock
    # pair {0, 1}: rewrite these attributes in every commit to the fixed mtime and recompute the commit CRC.
    with open(image, 'rb') as f:
        img = bytearray(f.read())
    value = struct.pack('<q', mtime)
    patched = 0
    for block in (0, 1):
        base = block * block_size
        off = 4
        ptag = 0xffffffff
        commit_start = 0
        dirty = False
        while off + 4 <= block_size:
            tag = struct.unpack('>I', img[base + off:base + off + 4])[0] ^ ptag
            typ = (tag >> 20) & 0x7ff
            if tag & 0x80000000 and (typ & 0x700) != LFS_TYPE_CRC:
                break  # end of the valid commits
            size = tag & 0x3ff
            dsize = 0 if size == 0x3ff else size
            if (typ & 0x700) == LFS_TYPE_CRC:
                if dirty:
                    crc = _lfs_crc(bytes(img[base + commit_start:base + off + 4]))
                    img[base + off + 4:base + off + 8] = struct.pack('<I', crc)
                    dirty = False
                ptag = tag ^ ((typ & 1) << 31)
                off += 4 + dsize
                commit_start = off
                if off + 4 > block_size or img[base + off:base + off + 4] == b'\xff\xff\xff\xff':
                    break
                continue
            tag_id = (tag >> 10) & 0x3ff
            if typ in (LFS_TYPE_ATTR_TIME, LFS_TYPE_ATTR_CREATED) and tag_id == 0 and dsize == 8:
                if img[base + off + 4:base + off + 12] != value:
                    img[base + off + 4:base + off + 12] = value
                    dirty = True
                patched += 1
            ptag = tag
            off += 4 + dsize
    with open(image, 'wb') as f:
        f.write(img)
    return patched


def file_manifest(root):
    entries = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for name in sorted(filenames):
            path = os.path.join(dirpath, name)
            with open(path, 'rb') as f:
                digest = hashlib.sha256(f.read()).hexdigest()
            rel = os.path.relpath(path, root).replace('\\', '/')
            entries.append((rel, os.path.getsize(path), digest))
    return entries


def write_manifest(path, entries):
    with open(path, 'wt', newline='\n') as f:
        for rel, size, digest in entries:
            f.write('%s\t%d\t%s\n' % (rel, size, digest))


def sha256_file(path):
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def normalize_paths(text, export_dir):
    for variant in {export_dir, export_dir.replace('\\', '/'), os.path.realpath(export_dir)}:
        text = text.replace(variant, '<root>')
    return text.replace('\r\n', '\n')


def php_cli(export_dir, env_name, defines_file, option):
    php = shutil.which('php')
    cli = os.path.join(export_dir, 'lib', 'KFCLibrary', 'KFCWebBuilder', 'bin', 'include', 'cli_tool.php')
    if not php or not os.path.exists(cli):
        return None
    _, out = run([php, cli, os.path.join(export_dir, 'KFCWebBuilder.json'), '--branch', 'spiffs', '--env',
                  'env:' + env_name, '--defines-from', defines_file, option], cwd=export_dir, check=False)
    return normalize_paths(out, export_dir)


def python_builder_cmd(export_dir, env_name, defines_file, *options):
    return [sys.executable, '-m', 'kfc_webbuilder', os.path.join(export_dir, 'KFCWebBuilder.yaml'), '--branch',
            'littlefs', '--env', 'env:' + env_name, '--defines-from', defines_file] + list(options)


def python_builder_env(export_dir):
    env = dict(os.environ)
    env['PYTHONPATH'] = os.path.join(export_dir, 'lib', 'KFCLibrary', 'KFCWebBuilder')
    env['PYTHONIOENCODING'] = 'UTF-8'
    return env


def has_python_builder(export_dir):
    return os.path.isdir(os.path.join(export_dir, 'lib', 'KFCLibrary', 'KFCWebBuilder', 'kfc_webbuilder')) and \
        os.path.isfile(os.path.join(export_dir, 'KFCWebBuilder.yaml'))


def builder_cli(export_dir, env_name, defines_file, option):
    # --files / -DD of the builder under test: the Python builder if the export has it, otherwise PHP
    if has_python_builder(export_dir):
        _, out = run(python_builder_cmd(export_dir, env_name, defines_file, option), cwd=export_dir,
                     env=python_builder_env(export_dir), check=False)
        return normalize_paths(out, export_dir)
    return php_cli(export_dir, env_name, defines_file, option)


def build_env(env_name, export_dir, out_dir, mtime, pio_env, direct=None):
    # direct: {'mklittlefs_args': [...], 'cppdefines': <file>} runs the Python builder without PlatformIO
    env_out = os.path.join(out_dir, env_name)
    remove_tree(env_out)
    os.makedirs(env_out)
    result = {'status': 'failed'}
    start = time.monotonic()
    reset_export_data(export_dir, mtime)
    build_dir = os.path.join(export_dir, '.pio', 'build', env_name)
    defines_file = os.path.join(build_dir, 'cppdefines.txt')

    if direct:
        os.makedirs(build_dir, exist_ok=True)
        shutil.copyfile(direct['cppdefines'], defines_file)
        pvt_dir = os.path.join(export_dir, 'data', '.pvt')
        os.makedirs(pvt_dir, exist_ok=True)
        with open(os.path.join(pvt_dir, 'build'), 'wt', newline='\n') as f:
            f.write(PVT_BUILD_PLACEHOLDER)
        cmd = python_builder_cmd(export_dir, env_name, defines_file, '--force', '--clean-exit-code', '0')
        code, output = run(cmd, cwd=export_dir, env=python_builder_env(export_dir), check=False)
    else:
        code, output = run([pio_exe(), 'run', '-d', export_dir, '-e', env_name, '-t', 'buildfs', '-v'],
                           env=pio_env, check=False)
    with open(os.path.join(env_out, 'buildfs.log'), 'wt', encoding='utf-8', newline='\n') as f:
        f.write(normalize_paths(output, export_dir))
    result['seconds'] = round(time.monotonic() - start, 1)
    if code != 0:
        result['error'] = '%s exit code %d' % ('builder' if direct else 'pio buildfs', code)
        result['log_tail'] = normalize_paths(output, export_dir).splitlines()[-15:]
        return result

    mkfs_args = direct['mklittlefs_args'] if direct else parse_mklittlefs_args(output)
    if not mkfs_args:
        result['error'] = 'mklittlefs command line not found in the buildfs output'
        return result
    result['mklittlefs_args'] = mkfs_args
    if os.path.exists(defines_file):
        shutil.copy2(defines_file, os.path.join(env_out, 'cppdefines.txt'))
        with open(defines_file, 'rt') as f:
            if re.search(r'^INCLUDE_DATA_DIRS=', f.read(), re.M):
                result['warning'] = 'INCLUDE_DATA_DIRS is set, its linked directories are not part of the copy'

    data_copy = os.path.join(env_out, 'data')
    shutil.copytree(os.path.join(export_dir, 'data'), data_copy)
    pvt_build = os.path.join(data_copy, '.pvt', 'build')
    if os.path.exists(pvt_build):
        with open(pvt_build, 'wt', newline='\n') as f:
            f.write(PVT_BUILD_PLACEHOLDER)
    set_mtime_tree(data_copy, mtime)

    image = os.path.join(env_out, 'littlefs.bin')
    code, mkfs_out = run([mklittlefs_exe(), '-c', data_copy] + mkfs_args + [image], check=False)
    if code != 0:
        result['error'] = 'mklittlefs exit code %d: %s' % (code, mkfs_out[-500:])
        return result
    block_size = int(mkfs_args[mkfs_args.index('-b') + 1])
    result['root_time_attrs'] = normalize_lfs_root_time(image, block_size, mtime)
    if not result['root_time_attrs']:
        result['error'] = 'root time attributes not found in %s' % image
        return result
    # the patched image must still mount and list every file
    code, listing = run([mklittlefs_exe(), '-l'] + mkfs_args + [image], check=False)
    if code != 0 or '/webui/.listings' not in listing:
        result['error'] = 'patched image does not mount: %s' % listing[-500:]
        return result

    entries = file_manifest(data_copy)
    write_manifest(os.path.join(env_out, 'manifest.txt'), entries)

    if os.path.exists(defines_file):
        for option, filename in (('-DD', 'defines.txt'), ('--files', 'files.txt')):
            text = builder_cli(export_dir, env_name, defines_file, option)
            if text is not None:
                with open(os.path.join(env_out, filename), 'wt', encoding='utf-8', newline='\n') as f:
                    f.write(text)

    result.update({
        'status': 'ok',
        'image_sha256': sha256_file(image),
        'image_size': os.path.getsize(image),
        'files': len(entries),
        'data_bytes': sum(e[1] for e in entries),
    })
    return result


# ---------------------------------------------------------------------------------------------------------
# tools


def tool_versions():
    uglify = os.path.join(PROJECT_DIR, 'node_modules', 'uglify-js', 'package.json')
    mkfs_pkg = os.path.join(platformio_core_dir(), 'packages', 'tool-mklittlefs', 'package.json')
    versions = {
        'python': sys.version.split()[0],
        'platformio': first_line([pio_exe(), '--version']),
        'php': first_line(['php', '-v']),
        'gzip': first_line(['gzip', '--version']),
        'node': first_line(['node', '-v']),
        'java': first_line(['java', '-version']),
    }
    for key, path in (('uglify-js', uglify), ('mklittlefs', mkfs_pkg)):
        try:
            with open(path, 'rt') as f:
                versions[key] = json.load(f)['version']
        except (OSError, ValueError, KeyError):
            versions[key] = 'n/a'
    return versions


# ---------------------------------------------------------------------------------------------------------
# commands


def resolve_repos(args):
    local = find_repos()
    if args.commits:
        with open(args.commits, 'rt') as f:
            recorded = json.load(f)['repos']
        repos = {}
        for name, info in recorded.items():
            path = local.get(name, info['path'])
            repos[name] = repo_info(name, path, info['commit'])
        return repos
    repos = {name: repo_info(name, path) for name, path in local.items()}
    dirty = [name for name, info in repos.items() if info['dirty']]
    if dirty and not args.allow_dirty:
        raise SystemExit('uncommitted changes in %s: commit them or pass --allow-dirty' % ', '.join(dirty))
    return repos


def parse_overlays(items):
    overlays = []
    for item in items or []:
        if '=' not in item:
            raise SystemExit('--overlay expects <source>=<destination relative to the export>: %s' % item)
        src, dst = item.split('=', 1)
        overlays.append((os.path.abspath(src), dst.replace('\\', '/').strip('/')))
    return overlays


def capture(args, out_dir=None, direct=None):
    # direct: (baseline dir, baseline.json) runs the Python builder with the recorded defines, without pio
    repos = resolve_repos(args)
    name = args.name or baseline_name(repos)
    out_dir = out_dir or os.path.join(args.out, name)
    overlays = parse_overlays(getattr(args, 'overlay', None))
    work_dir = os.path.join(args.work_dir, name)
    jobs = max(1, args.jobs)

    log('Baseline %s -> %s' % (name, out_dir))
    for repo, info in repos.items():
        log('  %-18s %s %s%s' % (repo, info['commit'][:10], info['date'], ' (dirty)' if info['dirty'] else ''))

    exports = []
    for i in range(jobs):
        export_dir = os.path.join(work_dir, 'src_%u' % i)
        log('Exporting %s' % export_dir)
        create_export(repos, export_dir, args.mtime, overlays)
        exports.append(export_dir)

    envs = sorted(direct[1]['envs']) if direct else list_envs(exports[0])
    if args.envs and args.envs != ['all']:
        unknown = [e for e in args.envs if e not in envs]
        if unknown:
            raise SystemExit('unknown environment(s): %s' % ', '.join(unknown))
        envs = args.envs
    os.makedirs(out_dir, exist_ok=True)

    pio_env = dict(os.environ)
    # the lib_deps of the working copy, only read; the image does not depend on them
    pio_env['PLATFORMIO_LIBDEPS_DIR'] = os.path.join(PROJECT_DIR, '.pio', 'libdeps')
    pio_env['PYTHONIOENCODING'] = 'UTF-8'

    results = {}
    free_exports = list(exports)
    lock = threading.Lock()

    def worker(env_name):
        with lock:
            export_dir = free_exports.pop()
        try:
            direct_env = None
            if direct:
                recorded = direct[1]['envs'][env_name]
                if recorded['status'] != 'ok':
                    return env_name, {'status': 'skipped', 'error': 'failed in the baseline'}
                direct_env = {'mklittlefs_args': recorded['mklittlefs_args'],
                              'cppdefines': os.path.join(direct[0], env_name, 'cppdefines.txt')}
            return env_name, build_env(env_name, export_dir, out_dir, args.mtime, pio_env, direct_env)
        except Exception as e:
            return env_name, {'status': 'failed', 'error': str(e)}
        finally:
            with lock:
                free_exports.append(export_dir)

    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        for num, (env_name, result) in enumerate(pool.map(worker, envs), 1):
            results[env_name] = result
            if result['status'] == 'ok':
                log('[%u/%u] %-36s ok %s %u files %.1fs' % (num, len(envs), env_name, result['image_sha256'][:16],
                                                             result['files'], result['seconds']))
            else:
                log('[%u/%u] %-36s FAILED %s' % (num, len(envs), env_name, result.get('error')))

    baseline = {
        'name': name,
        'created': datetime.datetime.now().astimezone().isoformat(timespec='seconds'),
        'mtime': args.mtime,
        'mtime_utc': datetime.datetime.fromtimestamp(args.mtime, datetime.timezone.utc).isoformat(),
        'pvt_build': PVT_BUILD_PLACEHOLDER,
        'builder': 'python' if has_python_builder(exports[0]) else 'php',
        'direct': bool(direct),
        'overlays': [{'source': s, 'destination': d} for s, d in overlays],
        'repos': repos,
        'tools': tool_versions(),
        'envs': results,
    }
    with open(os.path.join(out_dir, 'baseline.json'), 'wt', newline='\n') as f:
        json.dump(baseline, f, indent=2)
        f.write('\n')

    ok = sum(1 for r in results.values() if r['status'] == 'ok')
    failed = sorted(e for e, r in results.items() if r['status'] != 'ok')
    if os.path.normcase(out_dir).startswith(os.path.normcase(os.path.join(os.path.abspath(args.out), ''))):
        with open(os.path.join(args.out, 'index.log'), 'at', newline='\n') as f:
            f.write('%s %s envs=%u ok=%u failed=%u %s\n' % (baseline['created'], name, len(envs), ok, len(failed),
                    ' '.join('%s=%s' % (r, i['commit'][:8]) for r, i in repos.items())))
    if not args.keep_export:
        remove_tree(work_dir)
    log('%u of %u environments ok%s' % (ok, len(envs), (', failed: ' + ' '.join(failed)) if failed else ''))
    return out_dir, baseline


def read_manifest(path):
    entries = {}
    if os.path.exists(path):
        with open(path, 'rt') as f:
            for line in f:
                rel, size, digest = line.rstrip('\n').split('\t')
                entries[rel] = (int(size), digest)
    return entries


def diff_dirs(dir_a, dir_b, envs=None, verbose=False):
    with open(os.path.join(dir_a, 'baseline.json'), 'rt') as f:
        a = json.load(f)
    with open(os.path.join(dir_b, 'baseline.json'), 'rt') as f:
        b = json.load(f)
    names = envs or sorted(a['envs'])
    mismatches = 0
    for env_name in names:
        ra = a['envs'].get(env_name, {'status': 'missing'})
        rb = b['envs'].get(env_name, {'status': 'missing'})
        if ra['status'] != 'ok' or rb['status'] != 'ok':
            if ra['status'] != rb['status']:
                mismatches += 1
                log('%-36s status %s / %s' % (env_name, ra['status'], rb['status']))
            continue
        texts = []
        for filename in ('defines.txt', 'files.txt'):
            fa, fb = os.path.join(dir_a, env_name, filename), os.path.join(dir_b, env_name, filename)
            if os.path.exists(fa) and os.path.exists(fb):
                with open(fa, 'rt', encoding='utf-8') as f1, open(fb, 'rt', encoding='utf-8') as f2:
                    if f1.read() != f2.read():
                        texts.append(filename)
        if texts:
            mismatches += 1
            log('%-36s %s differ(s)' % (env_name, ' and '.join(texts)))
        if ra['image_sha256'] == rb['image_sha256']:
            if verbose and not texts:
                log('%-36s identical' % env_name)
            continue
        if not texts:
            mismatches += 1
        ma = read_manifest(os.path.join(dir_a, env_name, 'manifest.txt'))
        mb = read_manifest(os.path.join(dir_b, env_name, 'manifest.txt'))
        changed = sorted(k for k in set(ma) | set(mb) if ma.get(k) != mb.get(k))
        log('%-36s image differs, %u file(s): %s' % (env_name, len(changed), ', '.join(changed[:8]) +
                                                     (' ...' if len(changed) > 8 else '')))
    log('%u environment(s) compared, %u mismatch(es)' % (len(names), mismatches))
    return mismatches


def cmd_capture(args):
    out_dir, baseline = capture(args)
    failed = [e for e, r in baseline['envs'].items() if r['status'] != 'ok']
    return 1 if failed and not args.ignore_failed else 0


def cmd_diff(args):
    return 1 if diff_dirs(args.a, args.b, args.envs, args.verbose) else 0


def cmd_compare(args):
    with open(os.path.join(args.baseline, 'baseline.json'), 'rt') as f:
        recorded = json.load(f)
    args.commits = os.path.join(args.baseline, 'baseline.json')
    args.mtime = recorded['mtime']
    args.name = args.name or (recorded['name'] + '_compare')
    if not args.envs or args.envs == ['all']:
        args.envs = sorted(recorded['envs'])
    out_dir = os.path.join(args.work_dir, args.name + '_result')
    remove_tree(out_dir)
    out_dir, _ = capture(args, out_dir, (args.baseline, recorded) if args.direct else None)
    log('Result: %s' % out_dir)
    return 1 if diff_dirs(args.baseline, out_dir, args.envs, args.verbose) else 0


def main():
    parser = argparse.ArgumentParser(description='WebUI LittleFS baseline images')
    sub = parser.add_subparsers(dest='command', required=True)

    def common(p):
        p.add_argument('--envs', nargs='+', default=['all'], help='environments (default: all)')
        p.add_argument('-j', '--jobs', type=int, default=1, help='parallel builds (one export each)')
        p.add_argument('--work-dir', default=DEFAULT_WORK_DIR, help='directory for the git exports')
        p.add_argument('--out', default=DEFAULT_OUT_DIR, help='baseline storage')
        p.add_argument('--name', help='baseline name (default: <kfc_fw commit>_<KFCLibrary commit>)')
        p.add_argument('--keep-export', action='store_true', help='keep the exported source tree')
        p.add_argument('--allow-dirty', action='store_true', help='export HEAD although the tree is dirty')
        p.add_argument('--ignore-failed', action='store_true', help='exit code 0 if environments failed')
        p.add_argument('--overlay', action='append', metavar='SRC=DST',
                       help='copy a file/directory over the export (DST relative to the export root)')
        p.add_argument('-v', '--verbose', action='store_true')

    p = sub.add_parser('capture', help='build the images from a git export')
    common(p)
    p.add_argument('--mtime', type=int, default=DEFAULT_MTIME, help='fixed mtime (unix time)')
    p.add_argument('--commits', help='baseline.json whose commits are exported instead of HEAD')
    p.set_defaults(func=cmd_capture)

    p = sub.add_parser('diff', help='compare two baseline directories')
    p.add_argument('a')
    p.add_argument('b')
    p.add_argument('--envs', nargs='+')
    p.add_argument('-v', '--verbose', action='store_true')
    p.set_defaults(func=cmd_diff)

    p = sub.add_parser('compare', help='rebuild the commits of a baseline (plus overlays) and compare')
    common(p)
    p.add_argument('--baseline', required=True, help='baseline directory')
    p.add_argument('--direct', action='store_true',
                   help='run the Python builder with the recorded defines instead of pio buildfs')
    p.set_defaults(func=cmd_compare)

    args = parser.parse_args()
    sys.exit(args.func(args))


if __name__ == '__main__':
    main()
