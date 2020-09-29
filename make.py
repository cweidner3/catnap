#!/usr/bin/env python3

import argparse
import enum
import glob
import logging
import os
import re
import subprocess
import sys
from pathlib import Path


class Color(enum.Enum):
    RED = '\033[1;31m'
    GREEN = '\033[1;32m'
    YELLOW = '\033[1;33m'
    NONE = '\033[0m'

def _colorize(val: str, color: Color) -> str:
    retval = f'{color.value}{val}{Color.NONE.value}'
    return retval

def _find_path(executable: str) -> Path:
    logging.debug('Looking for %s in PATH', executable)
    for _path in os.environ['PATH'].split(os.pathsep):
        exe = Path(_path, executable)
        logging.debug('Checking for exe: %s', exe)
        if exe.exists():
            logging.debug('Found It!!')
            return exe
    raise FileNotFoundError(f'Unable to find {executable} in PATH')

def _clean(this_dir: Path, build_dir: Path, args):
    for _root, _dirs, _files in os.walk(build_dir, topdown=False):
        for _file in _files:
            logging.debug('Removing file: %s', Path(_root, _file))
            os.remove(Path(_root, _file))
        for _dir in _dirs:
            logging.debug('Removing dir: %s', Path(_root, _dir))
            os.rmdir(Path(_root, _dir))
    return 0

def _setup(this_dir: Path, build_dir: Path, args):
    # Use conan get obtain dependencies
    cmd = [
        'conan', 'install', str(this_dir),
    ]
    logging.info('Running -> %s', ' '.join(cmd))
    proc = subprocess.run(cmd, cwd=build_dir, env=os.environ, check=False)
    if proc.returncode:
        return proc.returncode
    # Copy any finds needed for find_package() to work
    find_files = glob.glob(f'{Path(this_dir, "cmake")}/Find*.cmake')
    for _file in find_files:
        filename = os.path.basename(_file)
        with open(_file, 'r') as infile:
            with open(Path(build_dir, filename), 'w') as outfile:
                outfile.write(infile.read())
    # Use cmake to setup makefiles
    cmd = [
        'cmake',
        '-DCMAKE_EXPORT_COMPILE_COMMANDS=yes',
        '-DCMAKE_TOOLCHAIN_FILE=conan_paths.cmake',
    ]
    # Force the use of clang over the system compiler
    if args.clang:
        try:
            logging.info('Finding clang executable')
            ccomp = _find_path('clang')
            logging.info('Finding clang++ executable')
            cxxcomp = _find_path('clang++')
            cmd.append(f'-DCMAKE_C_COMPILER={ccomp}')
            cmd.append(f'-DCMAKE_CXX_COMPILER={cxxcomp}')
        except FileNotFoundError as _e:
            logging.error(_e)
            return 1
    b_type = 'Release' if args.release else 'Debug'
    cmd.append(f'-DCMAKE_BUILD_TYPE={b_type}')
    if args.coverage:
        cmd.append('-DENABLE_CODE_COVERAGE=ON')
    cmd.append(str(this_dir))
    logging.info('Running -> %s', ' '.join(cmd))
    proc = subprocess.run(cmd, cwd=build_dir, env=os.environ, check=False)
    return proc.returncode

def _build(this_dir: Path, build_dir: Path, args):
    if not Path(build_dir, 'CMakeCache.txt').exists():
        setattr(args, 'release', False)
        setattr(args, 'coverage', False)
        _setup(this_dir, build_dir, args)
    cmd = [
        'cmake',
        '--build', str(build_dir),
    ]
    if hasattr(args, 'verbose') and args.verbose:
        cmd.append('--verbose')
    logging.info('Running -> %s', ' '.join(cmd))
    proc = subprocess.run(cmd, cwd=build_dir, env=os.environ, check=False)
    if proc.returncode != 0 or (hasattr(args, 'notest') and args.notest):
        return proc.returncode
    cmd = ['ctest', '-V']
    logging.info('Running -> %s', ' '.join(cmd))
    proc = subprocess.run(cmd, cwd=build_dir, env=os.environ, check=False)
    return proc.returncode

def _run(this_dir: Path, build_dir: Path, args):
    executable = 'kitten'
    prog = Path(build_dir, executable)
    if not prog.exists():
        logging.error('Uanble to find %s', executable)
        return 1
    cmd = [str(prog)]
    logging.info('Running -> %s', ' '.join(cmd))
    proc = subprocess.run(cmd, cwd=build_dir, env=os.environ, check=False)
    return proc.returncode

def _docs(this_dir: Path, build_dir: Path, args):
    def _color_warn_and_err(this_line: str) -> str:
        res = err_pat.search(line)
        if res:
            return err_pat.sub(_colorize(res.group(1), Color.RED), line)
        res = warn_pat.search(line)
        if res:
            return warn_pat.sub(_colorize(res.group(1), Color.YELLOW), line)
        return line
    proc = subprocess.Popen('doxygen'.split(), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    warn_pat = re.compile(r'(warning:)', re.IGNORECASE)
    err_pat = re.compile(r'(error:)', re.IGNORECASE)
    while True:
        line = proc.stdout.readline()
        if not line:
            break
        line = line.rstrip().decode()
        print(_color_warn_and_err(line))
    return proc.returncode

def _main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--debug', action='store_true')
    parser.add_argument('--build-dir', type=str)
    subparser = parser.add_subparsers()
    # Setup
    parser_setup = subparser.add_parser('setup')
    parser_setup.add_argument('--release', action='store_true')
    parser_setup.add_argument('--coverage', action='store_true')
    parser_setup.add_argument('--clang', action='store_true')
    parser_setup.set_defaults(func=_setup)
    # Clean
    parser_clean = subparser.add_parser('clean')
    parser_clean.set_defaults(func=_clean)
    # Build
    parser_build = subparser.add_parser('build')
    parser_build.add_argument('--notest', action='store_true')
    parser_build.add_argument('--verbose', '-v', action='store_true')
    parser_build.set_defaults(func=_build)
    # Run
    parser_run = subparser.add_parser('run')
    parser_run.set_defaults(func=_run)
    # Docs
    parser_docs = subparser.add_parser('docs')
    parser_docs.set_defaults(func=_docs)
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.debug else logging.INFO,
        format='%(levelname)s: %(message)s'
        )

    this_dir = Path(__file__).parent.resolve()
    build_dir = Path(args.build_dir).resolve() if args.build_dir else Path(this_dir, 'build')
    os.makedirs(build_dir, exist_ok=True)

    if hasattr(args, 'func'):
        return args.func(this_dir, build_dir, args)
    return _build(this_dir, build_dir, args)

if __name__ == '__main__':
    RET = (_main())
    if RET:
        logging.error('%s', _colorize(f'Return code: {RET}', Color.RED))
        sys.exit(RET)
