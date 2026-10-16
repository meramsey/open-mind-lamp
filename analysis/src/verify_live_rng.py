#!/usr/bin/env python3
"""Bounded, read-only verification of RNG -> target hue -> commanded LED RGB.

Software/dataflow verification under Espressif's RF entropy contract, not an
optical measurement or independent proof of physical unpredictability.
"""
import argparse
import getpass
import json
import math
import os
from pathlib import Path
import sys
import time
import urllib.request

HARDWARE = 'esp32s3-hwrng-rf-no-sleep'
TEST_ONLY = 'deterministic-diagnostic-TEST-ONLY'


def close(a, b):
    return math.isfinite(a) and math.isfinite(b) and abs(a-b) < 1e-7


def require(condition, message):
    if not condition:
        raise ValueError(message)


def check_frame(snapshot):
    s, c, rng = snapshot['settings'], snapshot['color'], snapshot['rng']
    require(s['mode'] == 1 and s['enabled'] and s['saturation'] > 0 and s['brightness'] > 0 and s['sensitivity'] > 0,
            'Choose Random Walk (mode 1), LEDs on, positive brightness/saturation/sensitivity before capture.')
    require(not snapshot['calibration_active'] and not snapshot['experiment_active'],
            'Stop calibration/active experiments before this diagnostic capture; saved trials need not be cleared.')
    require(c['valid'] and c['mode'] == 1 and c['calibration_run'] == 0, 'No valid normal-Walk transmission snapshot')
    require(close(c['walk_hue'], rng['smoothed_hue']) and close(c['logical_hue'], c['walk_hue']),
            'Walk display does not follow the actual rendered walk hue')
    step = c['render_step']
    require(step['dt_ms'] >= 0 and close(step['smoothing_ms'], s['smoothing_ms']), 'Invalid smoothing inputs')
    delta = (step['target']-step['before']+180) % 360 - 180
    alpha = 1 if step['smoothing_ms'] <= 0 else -math.expm1(-step['dt_ms']/step['smoothing_ms'])
    predicted_render = (step['before']+delta*alpha) % 360
    require(close(predicted_render, step['after']) and close(step['after'], c['walk_hue']),
            'LED frame hue does not follow the production smoothing inputs')
    require(close(c['brightness'], s['brightness']) and close(c['saturation'], s['saturation']),
            'Rendered frame settings differ from capture settings; wait for a new frame')
    # Independent HSV/gamma/PWM/gain calculation, not a translation of CSS hue.
    hue = (c['walk_hue'] % 360) / 60
    chroma = s['saturation']; x = chroma * (1 - abs(hue % 2 - 1)); m = 1 - chroma
    sectors = [(chroma, x, 0), (x, chroma, 0), (0, chroma, x),
               (0, x, chroma), (x, 0, chroma), (chroma, 0, x)]
    channels = sectors[int(hue)]
    gains = [s['gain_red'], s['gain_green'], s['gain_blue']]
    rgb = [int(math.floor(min(32, max(0, (a+m)**2.2 * s['brightness'] * gain * 255)) + .5))
           for a, gain in zip(channels, gains)]
    require(c['commanded_rgb'] == rgb, f'LED command does not match rendered hue/gamma/gains: {c["commanded_rgb"]} != {rgb}')
    wire = rgb if s['rgb_order'] else [rgb[1], rgb[0], rgb[2]]
    require(c['wire_bytes'] == wire and c['channel_order'] == ('RGB' if s['rgb_order'] else 'GRB'), 'Wire mapping mismatch')
    require(c['lit_pixels'] == (64 if any(rgb) else 0), 'Normal Walk pixel-count mismatch')
    require(c['lit_mask'] == ('ffffffffffffffff' if any(rgb) else '0000000000000000'), 'Normal Walk mask mismatch')
    require(c['rendered_at_us'] <= snapshot['captured_at_us'], 'Frame is from the future')


def verify(snapshots, allow_test=False):
    require(bool(snapshots), 'Empty trace')
    first = snapshots[0]
    source = first['source']
    require(source == HARDWARE or (allow_test and source == TEST_ONLY), 'Not an accepted hardware trace (test fixtures require explicit offline permission)')
    seq = sample_id = window = target = None
    known = False; count = net = 0; previous_time = -1; previous_frame = -1
    samples = windows = frames = 0; movement_windows = 0; partial_windows = 0
    directions = dict(positive=0, negative=0, zero=0); words = set(); frame_ids = set()
    targets = []
    last_word = None
    for snapshot in snapshots:
        require(snapshot['type'] == 'rng_pipeline_trace' and snapshot['schema'] == 1 and snapshot['experimental_record'] is False,
                'Not a schema-1 read-only diagnostic trace')
        require(snapshot['source'] == source and snapshot['boot_id'] == first['boot_id'], 'Source changed or device rebooted')
        require(snapshot['revision'] == first['revision'] and snapshot['settings'] == first['settings'],
                'Settings/walk reset changed during capture; retry with stable settings')
        rng = snapshot['rng']; sensitivity = snapshot['settings']['sensitivity']
        require(rng['source'] == source and rng['ready'] and not rng['health_failed'], 'Entropy gate/health was not continuously reported ready')
        require(rng['rejected_reads'] == first['rng']['rejected_reads'], 'Entropy gate rejected acquisitions during capture; continuous readiness not verified')
        check_frame(snapshot)
        require(snapshot['color']['frame_id'] >= previous_frame, 'Frame sequence moved backwards')
        previous_frame = snapshot['color']['frame_id']; frame_ids.add(previous_frame); frames += 1
        for e in snapshot['events']:
            require(seq is None or e['seq'] == seq + 1, 'Trace event gap/duplicate: poll faster or shorten busy intervals; incomplete proof rejected')
            require(e['at_us'] >= previous_time and e['at_us'] <= snapshot['captured_at_us'], 'Nonmonotonic event time')
            previous_time = e['at_us']; seq = e['seq']
            if e['kind'] == 'reset':
                require(samples == 0 and windows == 0, 'Walk reset occurred during capture')
                target = e['target_after']; window = 0; known = True; count = net = 0
                targets.append((e['at_us'], target))
                sample_id = e['sample_id']
                last_word = None
            elif e['kind'] == 'sample':
                word = e['word']
                require(type(word) is int and 0 <= word <= 0xffffffff, 'Invalid uint32 sample')
                expected_net = 2*bin(word).count('1')-32
                require(e['net'] == expected_net and e['healthy'] is True and e['applied'] is True, 'Sample contribution/health mismatch')
                require(sample_id is None or e['sample_id'] == sample_id + 1, 'Successful sample sequence gap')
                sample_id = e['sample_id']
                last_word = word
                if window is None: window = e['window_id']
                require(e['window_id'] == window, 'Sample in unexpected accumulation window')
                count += 1; net += expected_net; samples += 1; words.add(word)
            elif e['kind'] == 'commit':
                if window is None: window = e['window_id']
                require(e['window_id'] == window, 'Commit in unexpected accumulation window')
                if known:
                    require(e['samples'] == count and e['net'] == net, 'Committed net/count differs from raw samples')
                    windows += 1
                    key = 'positive' if net > 0 else 'negative' if net < 0 else 'zero'; directions[key] += 1
                    movement_windows += net != 0
                else:
                    require(e['samples'] >= count, 'Partial first window accounting mismatch')
                    partial_windows += 1
                require(target is None or close(target, e['target_before']), 'Target changed without a commit')
                predicted = (e['target_before'] + e['net']*sensitivity) % 360
                require(close(predicted, e['target_after']), 'RNG increments do not predict target hue')
                target = e['target_after']; window += 1; known = True; count = net = 0
                if not targets: targets.append((-1, e['target_before']))
                targets.append((e['at_us'], target))
                require(sample_id is None or sample_id == e['sample_id'], 'Commit sample cursor mismatch')
                sample_id = e['sample_id']
            else:
                raise ValueError('Unknown trace event')
        require(seq is not None and seq == snapshot['latest_seq'], 'Trace response omitted events')
        require(sample_id == rng['successful_samples'], 'Snapshot successful-sample cursor mismatch')
        if last_word is not None:
            require(rng['latest_word'] == last_word and rng['ones'] == bin(last_word).count('1') and
                    rng['zeros'] == 32-rng['ones'] and rng['net'] == 2*rng['ones']-32 and rng['latest_applied'],
                    'Latest-word diagnostics differ from the accepted hardware sample')
        if target is not None: require(close(target, rng['target_hue']), 'Live target changed independently of recorded RNG commits')
        available_targets = [hue for at, hue in targets if at <= snapshot['color']['rendered_at_us']]
        if available_targets:
            require(close(available_targets[-1], snapshot['color']['render_step']['target']),
                    'Render target is not the most recent RNG-derived target at that frame')
        if known:
            require(window == rng['window_id'] and count == rng['window_samples'] and net == rng['window_net'],
                    'Live accumulation differs from replayed samples')
    require(samples >= 4 and windows >= 2 and len(frame_ids) >= 2, 'Need at least four samples, two complete windows and two transmitted frames; capture longer')
    require(movement_windows > 0, 'Only zero-net windows observed; capture longer, without treating neutral randomness as a fault')
    return dict(source=source, experimental_record=False, successful_samples_checked=samples,
                complete_windows_checked=windows, initial_partial_windows_skipped=partial_windows,
                directions=directions, distinct_words=len(words), snapshots_checked=frames,
                transmitted_frames_seen=len(frame_ids),
                result='PASS: raw increments predict targets; normal-Walk LED commands match rendered hues',
                limitation='Software causality under the SDK/RF contract; not optical measurement or independent proof of physical entropy.')


def collect(url, token, seconds=15, interval=.5):
    snapshots = []; cursor = 0; started = time.monotonic()
    while time.monotonic()-started < seconds:
        request = urllib.request.Request(url.rstrip('/') + f'/api/rng/trace?after={cursor}',
                                         headers={'Authorization': f'Bearer {token}'})
        with urllib.request.urlopen(request, timeout=4) as response:
            raw = response.read(65537)
        require(len(raw) <= 65536, 'Oversized trace response')
        snapshot = json.loads(raw)
        require(snapshot.get('source') == HARDWARE, 'Live endpoint is not the production hardware source')
        check_frame(snapshot)  # fail early on mode/active-trial mismatches; never alter the lamp
        snapshots.append(snapshot)
        if snapshot['events']: cursor = snapshot['events'][-1]['seq']
        time.sleep(min(interval, max(0, seconds-(time.monotonic()-started))))
    return snapshots


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://192.168.4.1')
    parser.add_argument('--seconds', type=float, default=15)
    parser.add_argument('--interval', type=float, default=.5)
    parser.add_argument('--output', type=Path, help='Create a new host diagnostic JSON file; never overwrite')
    parser.add_argument('--offline', type=Path)
    parser.add_argument('--allow-test-fixture', action='store_true', help='Offline deterministic fixtures only, never a hardware validation claim')
    args = parser.parse_args()
    if not 5 <= args.seconds <= 120 or not .1 <= args.interval <= 2:
        parser.error('Bounded capture requires 5–120 seconds and 0.1–2 seconds polling interval')
    if args.allow_test_fixture and not args.offline:
        parser.error('--allow-test-fixture is only valid with --offline')
    try:
        if args.output and args.output.exists():
            raise ValueError('Output already exists; choose a fresh diagnostic filename')
        if args.offline:
            document = json.loads(args.offline.read_text())
        else:
            token = os.environ.get('OML_TOKEN') or getpass.getpass('Admin token: ')
            snapshots = collect(args.url, token, args.seconds, args.interval)
            document = dict(type='open_mind_lamp_live_validation', schema=1, experimental_record=False, snapshots=snapshots)
            if args.output:
                fd = os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
                with os.fdopen(fd, 'w') as output: json.dump(document, output, indent=2, allow_nan=False)
        require(document['type'] == 'open_mind_lamp_live_validation' and document['schema'] == 1 and document['experimental_record'] is False, 'Not a diagnostic capture')
        print(json.dumps(verify(document['snapshots'], args.allow_test_fixture), indent=2, allow_nan=False))
    except (ValueError, KeyError, TypeError, OSError) as error:
        print(f'Validation failed: {error}', file=sys.stderr); return 1
    return 0


if __name__ == '__main__': sys.exit(main())
