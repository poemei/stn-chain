#!/usr/bin/env python3
"""Run on Linux; all writes are confined to a temporary test directory."""
import pathlib
import socket
import struct
import subprocess
import sys
import tempfile
import time


def exact(peer, size):
    result = b''
    while len(result) < size:
        part = peer.recv(size - len(result))
        assert part, 'Truncated INFO'
        result += part
    return result


def start(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise AssertionError(process.communicate()[1].decode())
            try:
                with socket.create_connection(('127.0.0.1', int(command[-1])), timeout=1) as peer:
                    peer.sendall(struct.pack('>4sHHHHQI', b'STNC', 2, 1, 1, 0, 1, 0))
                    assert struct.unpack('>4sHHHHQI', exact(peer, 24)) == (b'STNC', 2, 2, 1, 0, 1, 184)
                    return process, exact(peer, 184)
            except OSError:
                time.sleep(.05)
        raise AssertionError('Startup timed out')
    except BaseException:
        process.kill()
        process.communicate()
        raise


def stop(process):
    process.terminate()
    try:
        process.communicate(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.communicate()
        raise AssertionError('SIGTERM shutdown timed out')
    assert process.returncode == 0, 'Unclean shutdown'


binary = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else 'build/stn-chain').resolve()
with tempfile.TemporaryDirectory(prefix='stn-startup-') as folder:
    data = pathlib.Path(folder) / 'chain.stns'
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    command = [str(binary), '--data', str(data), '--rpc-port', str(port)]
    process, first = start(command)
    try:
        assert first[64:72] == bytes(8), 'Fresh height is not zero'
        original = data.read_bytes()
        # A different RPC port ensures the failure proves data exclusion.
        duplicate = subprocess.run(command[:-1] + ['0'], capture_output=True, timeout=5)
        assert duplicate.returncode != 0, 'Duplicate node accepted'
        assert data.read_bytes() == original
    finally:
        stop(process)
    process, second = start(command)
    try:
        assert first == second, 'Restart changed state'
        assert data.read_bytes() == original, 'Restart rewrote history'
    finally:
        stop(process)
    for damaged in (b'', b'broken', original[:-1] + bytes([original[-1] ^ 1])):
        data.write_bytes(damaged)
        result = subprocess.run(command, capture_output=True, timeout=5)
        assert result.returncode != 0, 'Damaged history accepted'
        assert data.read_bytes() == damaged, 'Damaged history overwritten'
print('PASS: bootstrap, INFO, duplicate exclusion, restart, SIGTERM, corruption preservation')
