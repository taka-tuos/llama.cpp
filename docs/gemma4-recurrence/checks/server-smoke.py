#!/usr/bin/env python3
"""Technical HTTP checks; no RP quality evaluation. Uses an existing local GGUF."""
import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import socket
import subprocess
import time
import urllib.error
import urllib.request

parser = argparse.ArgumentParser()
parser.add_argument('--server', required=True)
parser.add_argument('--model', required=True)
parser.add_argument('--output', required=True)
parser.add_argument('--gpu-layers', default='0')
args = parser.parse_args()
out = Path(args.output)
out.mkdir(parents=True, exist_ok=True)
with socket.socket() as sock:
    sock.bind(('127.0.0.1', 0))
    port = sock.getsockname()[1]
base = f'http://127.0.0.1:{port}'
env = os.environ.copy()
command = [str(Path(args.server).resolve()), '--model', str(Path(args.model).resolve()),
           '--ctx-size', '512', '--batch-size', '64', '--ubatch-size', '64',
           '--threads', '8', '--threads-batch', '8', '--parallel', '1',
           '--no-context-shift', '--host', '127.0.0.1', '--port', str(port),
           '--gpu-layers', args.gpu_layers, '--log-verbosity', '5']
(out / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
results = []

def request(data, path='/completion'):
    req = urllib.request.Request(base + path, json.dumps(data).encode(),
                                 {'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=180) as response:
        if data.get('stream'):
            chunks = [json.loads(line[6:]) for line in response.read().decode().splitlines()
                      if line.startswith('data: ') and line[6:] != '[DONE]']
            return next(chunk for chunk in reversed(chunks) if chunk.get('stop'))
        return json.load(response)

prompt = {'prompt': 'Hello', 'n_predict': 4, 'temperature': 0,
          'seed': 42, 'ignore_eos': True, 'return_tokens': True}
linear = {'n': 1, 'm': 4, 'alpha': 0, 'beta': 0.5,
          'mode': 'prelude-input', 'scope': 'decode'}
def check(label, cfg=None, stream=False):
    data = dict(prompt, stream=stream)
    if cfg is not None:
        data['gemma4_recur'] = cfg
    response = request(data)
    applied = response['generation_settings']['gemma4_recur']
    expected = dict(n=0, m=1, alpha=0, beta=1, mode='output', scope='decode')
    if cfg is not None:
        expected.update(cfg)
    assert applied == expected, (label, applied, expected)
    results.append({'label': label, 'applied': applied, 'tokens': response.get('tokens'),
                    'cached_prompt': response.get('timings', {}).get('prompt_n')})
    (out / (label + '.json')).write_text(json.dumps(response, ensure_ascii=False, indent=2)+'\n')
    return response

with (out / 'server.log').open('w') as log:
    proc = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT)
    try:
        for _ in range(180):
            if proc.poll() is not None:
                raise RuntimeError(f'server exited {proc.returncode}')
            try:
                with urllib.request.urlopen(base + '/health', timeout=1) as response:
                    if response.status == 200:
                        break
            except (urllib.error.URLError, TimeoutError):
                time.sleep(1)
        else:
            raise TimeoutError('server startup')
        baseline = check('baseline')
        check('linear-n1', linear)
        restored = check('restored-default')
        assert baseline['tokens'] == restored['tokens'], 'N0 token regression'
        beta0 = check('beta-zero', dict(linear, beta=0))
        assert baseline['tokens'] == beta0['tokens'], 'beta0 token regression'
        check('linear-n2-alpha', dict(linear, n=2, alpha=0.25, m=2, beta=0.75))
        check('linear-n32', dict(linear, n=32, m=48))
        check('streaming', linear, True)
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            futures = [pool.submit(check, 'queued-n1', linear),
                       pool.submit(check, 'queued-n2', dict(linear, n=2, m=2))]
            for future in futures:
                future.result()
        for label, cfg in [('bad-m', {'m': 0}), ('bad-scope', {'scope': 'both'}),
                           ('bad-type', {'n': 1.5}), ('bad-combination', {'n': 8}),
                           ('bad-key', {'unknown': 1})]:
            try:
                request(dict(prompt, gemma4_recur=cfg))
            except urllib.error.HTTPError as error:
                assert error.code == 400, (label, error.code)
                results.append({'label': label, 'status': error.code})
            else:
                raise AssertionError(label + ' was accepted')
        check('after-invalid')
        chat = request({'messages': [{'role': 'user', 'content': 'Hello'}],
                        'max_tokens': 4, 'temperature': 0, 'verbose': True,
                        'gemma4_recur': linear}, '/v1/chat/completions')
        assert chat['__verbose']['generation_settings']['gemma4_recur'] == linear
        results.append({'label': 'openai-chat-custom-json', 'applied': linear})
        log.flush()
        text = (out / 'server.log').read_text()
        assert 'input role = decode' in text and 'input role = prompt' in text
        assert 'completed passes=32' in text
        if args.gpu_layers != '0':
            assert 'offloaded 43/43 layers' in text, 'GPU offload not confirmed'
        (out / 'summary.json').write_text(json.dumps({'status': 'PASS', 'backend':
            'GPU (see server log)' if args.gpu_layers != '0' else 'CPU',
            'checks': results}, ensure_ascii=False, indent=2)+'\n')
        print(f'PASS: {len(results)} HTTP checks')
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=15)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
