#!/usr/bin/env python3
# [AI:GPT-6 | 2026-09-28 14:52:23 UTC]
"""Linux-only isolated process regression; never opens production Chain data."""
import concurrent.futures
import json
import pathlib
import socket
import struct
import subprocess
import sys
import tempfile
import time

HEADER = struct.Struct('>4sHHHHQI')


def exact(peer, size):
    data = b''
    while len(data) < size:
        chunk = peer.recv(size - len(data))
        assert chunk, 'Truncated STNC response'
        data += chunk
    return data


def info(peer, request_id, fragmented=False):
    frame = HEADER.pack(b'STNC', 2, 1, 1, 0, request_id, 0)
    if fragmented:
        peer.sendall(frame[:9])
        time.sleep(.01)
        peer.sendall(frame[9:])
    else:
        peer.sendall(frame)
    assert HEADER.unpack(exact(peer, 24)) == (b'STNC', 2, 2, 1, 0, request_id, 184)
    return exact(peer, 184)


def connect(port):
    return socket.create_connection(('127.0.0.1', port), timeout=1)


def unused_port():
    with socket.socket() as peer:
        peer.bind(('127.0.0.1', 0))
        return peer.getsockname()[1]


def stop(process):
    process.terminate()
    try:
        process.wait(timeout=12)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        raise AssertionError('Node shutdown timed out')
    assert process.returncode == 0, 'Unclean node shutdown'


def run(binary, folder, once=False):
    data = folder / ('once.stns' if once else 'stalled.stns')
    config = folder / 'config.json'
    config.write_text(json.dumps({'internal_miner': {'enabled': False}}))
    rpc_port, p2p_port = unused_port(), unused_port()
    while p2p_port == rpc_port:
        p2p_port = unused_port()
    command = [str(binary), '--dev', '--data', str(data), '--config', str(config),
               '--rpc-port', str(rpc_port), '--p2p-port', str(p2p_port)]
    with socket.socket() as stalled, (folder / ('once.log' if once else 'stalled.log')).open('w+') as log:
        stalled.bind(('127.0.0.1', 0))
        stalled.listen()
        stalled.settimeout(15)
        if once:
            command.append('--once')
        else:
            command += ['--peer', '127.0.0.1:' + str(stalled.getsockname()[1])]
        process = subprocess.Popen(command, stdout=log, stderr=log)
        outbound = None
        try:
            if not once:
                outbound, _ = stalled.accept()
                outbound.settimeout(2)
                # Observe a real outbound STNP request, then withhold its reply.
                # The old runtime holds dispatch_lock while waiting here.
                assert exact(outbound, 4) == b'STNP'
            deadline = time.monotonic() + 5
            while True:
                if process.poll() is not None:
                    log.seek(0)
                    raise AssertionError(log.read())
                try:
                    client = connect(rpc_port)
                    break
                except ConnectionRefusedError:
                    if time.monotonic() >= deadline:
                        raise
                    time.sleep(.02)
            with client:
                began = time.monotonic()
                original = info(client, 123, fragmented=True)
                assert time.monotonic() - began < 1, 'INFO waited behind outbound I/O'
                assert info(client, 124) == original, 'Long-lived session changed state'
                assert struct.unpack('>Q', original[64:72])[0] == 0
                assert struct.unpack('>II', original[176:184]) == (1, 1)
                if not once:
                    def request(i):
                        with connect(rpc_port) as other:
                            return info(other, i)
                    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
                        assert all(payload == original for payload in pool.map(request, range(200, 216)))
                    # Malformed INFO must retain the standard INVALID response.
                    client.sendall(HEADER.pack(b'STNC', 2, 1, 1, 0, 125, 1) + b'X')
                    assert HEADER.unpack(exact(client, 24)) == (b'STNC', 2, 2, 0, 1, 0, 0)
                    assert info(client, 126) == original
            if once:
                process.wait(timeout=12)
                assert process.returncode == 0
        finally:
            if outbound is not None:
                outbound.close()
            if process.poll() is None:
                stop(process)


if __name__ == '__main__':
    binary = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else 'build/stn-chain').resolve()
    with tempfile.TemporaryDirectory(prefix='stn-info-') as directory:
        run(binary, pathlib.Path(directory))
        run(binary, pathlib.Path(directory), once=True)
    print('PASS: INFO during stalled outbound P2P, concurrent sessions, framing, --once')
# [End AI:GPT-6]
