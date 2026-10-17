"""Shared release metadata preparation; never an installed inference runtime."""
import hashlib
import json
from pathlib import Path
import shutil
from urllib.parse import urlsplit


def digest(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(65536), b''):
            result.update(block)
    return result.hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, sort_keys=True, separators=(',', ':')) + '\n')
    path.chmod(0o400)


def prepare(*, output, release_url, paths, expected, engine, model, recipe,
            provider, license_notice, metadata, policy, upstream=None, minimum_os='14.0', roles=None):
    """Validate separately pinned inputs and write metadata in an exclusive root.

    Engine/model bytes stay in their original files. Only metadata is copied.
    A non-upstream model is listed for release upload directly from its source.
    """
    url = urlsplit(release_url)
    if url.scheme != 'https' or not url.hostname or url.username or url.password or url.query or url.fragment or release_url.endswith('/'):
        raise ValueError('release URL must be an HTTPS asset prefix without credentials, query, fragment or trailing slash')
    inputs = dict(paths)
    file_roles = {name: 'engine' if name == engine else 'model' if name == model else 'metadata' for name in inputs}
    file_roles.update(roles or {})
    if set(inputs) != set(expected) or (engine is not None and engine not in inputs) or model not in inputs:
        raise ValueError('every source requires its own expected SHA-256')
    if set(file_roles) != set(inputs) or (engine is not None and file_roles[engine] != 'engine') or file_roles[model] != 'model' or any(
            role not in ('engine', 'library', 'model', 'data', 'metadata') for role in file_roles.values()):
        raise ValueError('invalid package roles')
    for name, path in inputs.items():
        if Path(name).is_absolute() or '..' in Path(name).parts or len(name) > 255:
            raise ValueError('invalid package source name')
        if not path.is_absolute() or path.is_symlink() or not path.is_file():
            raise ValueError('inputs must be absolute regular files')
        if digest(path) != expected[name]:
            raise ValueError('qualified input checksum mismatch: ' + name)
    output.mkdir(mode=0o700)
    try:
        base = output.resolve()
        payload = base / 'payload'
        payload.mkdir(mode=0o700)
        paths = dict(inputs)
        for name, source in inputs.items():
            if file_roles[name] == 'metadata':
                (payload / name).parent.mkdir(mode=0o700, parents=True, exist_ok=True)
                shutil.copyfile(source, payload / name)
                (payload / name).chmod(0o400)
                paths[name] = payload / name
        for name, value in metadata.items():
            if name in paths or Path(name).name != name or name.startswith('.'):
                raise ValueError('invalid metadata name')
            path = payload / name
            if isinstance(value, str):
                path.write_text(value)
                path.chmod(0o400)
            else:
                write(path, value)
            paths[name] = path
            file_roles[name] = 'metadata'
        files = [dict(name=name, role=file_roles[name],
                      bytes=path.stat().st_size, sha256=digest(path)) for name, path in sorted(paths.items())]
        index = base / 'index.json'
        write(index, dict(format='samosa.inference.package.v1', platform='macos-arm64', minimum_os=minimum_os, recipe=recipe, files=files))
        pin = digest(index)
        entry = dict(id=provider, version='1', url=release_url + '/' + pin, license=license_notice,
                     platform='macos-arm64', minimum_os=minimum_os, runtime='system-runtime-v1', recipe=recipe,
                     sha256=pin, size=index.stat().st_size)
        write(base / 'catalog.json', dict(catalog_version=1, revision=provider + '-v1', entries=[entry]))
        upstream = upstream or {}
        write(base / 'sources.json', dict(format='samosa.inference.package-sources.v1', index_sha256=pin,
              files=[dict(url=upstream.get(file['name'], release_url + '/' + file['sha256'])) for file in files]))
        write(base / 'host-policy.json', policy)
        write(base / 'local-sources.json', {name: str(path.resolve()) for name, path in paths.items()})
        write(base / 'release-assets.json', dict(index_sha256=pin, assets=[dict(name=file['sha256'], source=str(paths[file['name']].resolve()))
              for file in files if file['name'] not in upstream] + [dict(name=pin, source=str(index))]))
        for name, checksum in expected.items():
            if digest(inputs[name]) != checksum:
                raise RuntimeError('source changed: ' + name)
        return dict(root=str(payload), index=str(index), pin=pin, source_map=str(base / 'local-sources.json'),
                    files=len(files), payload_bytes=sum(file['bytes'] for file in files))
    except BaseException:
        shutil.rmtree(output)
        raise
