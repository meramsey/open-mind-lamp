#!/usr/bin/env python3
"""Audit Open Mind Lamp NDJSON trials and compute independently reproducible statistics.

Only the Python standard library is required. P-values assume independent fair bits;
they do not measure the probability of a hypothesis or establish causation.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import sys


def binomial_lower(n, k):
    """P[X <= k], X ~ Binomial(n, .5), stable log PMF + recurrence sum."""
    if k < 0:
        return 0.0
    if k >= n:
        return 1.0
    if k >= n / 2:
        return max(0.0, 1.0 - binomial_lower(n, n - k - 1))
    log_p = math.lgamma(n + 1) - math.lgamma(k + 1) - math.lgamma(n - k + 1) - n * math.log(2)
    term = total = 1.0
    for j in range(k, 0, -1):
        term *= j / (n - j + 1)
        total += term
        if term < total * 1e-16:
            break
    return min(1.0, math.exp(log_p) * total)


def statistics(n, ones, direction='high'):
    if n <= 0 or not 0 <= ones <= n or direction not in ('high', 'low'):
        raise ValueError('Invalid bit counts or direction')
    p = ones / n
    z = (ones - n / 2) / math.sqrt(n / 4)
    z95 = 1.959963984540054
    d = 1 + z95 * z95 / n
    center = (p + z95 * z95 / (2 * n)) / d
    half = z95 * math.sqrt(p * (1 - p) / n + z95 * z95 / (4 * n * n)) / d
    return dict(bits=n, ones=ones, fraction=p, z=z,
                p_two_sided=min(1.0, 2 * binomial_lower(n, min(ones, n - ones))),
                p_predeclared_one_sided=binomial_lower(n, n - ones if direction == 'high' else ones),
                wilson95=[center - half, center + half])


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f'Duplicate JSON key: {key}')
        result[key] = value
    return result


def integer(value, low=0, high=2**63 - 1):
    return type(value) is int and low <= value <= high


def audit(path, recover_truncated=False):
    raw = Path(path).read_bytes()
    if len(raw) > 2_000_000:
        raise ValueError('Export exceeds bounded device session size')
    lines = raw.splitlines()
    records = []
    truncated = False
    for i, line in enumerate(lines):
        try:
            record = json.loads(line, object_pairs_hook=unique_object,
                                parse_constant=lambda x: (_ for _ in ()).throw(ValueError(f'Nonfinite {x}')))
        except (ValueError, UnicodeDecodeError):
            if recover_truncated and i == len(lines) - 1 and not raw.endswith(b'\n'):
                truncated = True
                continue
            raise ValueError(f'Invalid NDJSON at line {i + 1}') from None
        if not isinstance(record, dict):
            raise ValueError(f'Expected JSON object at line {i + 1}')
        records.append(record)
    if not records or records[0].get('type') != 'header' or records[0].get('schema') != 1:
        raise ValueError('Missing schema-1 header')
    header = records[0]
    if header.get('algorithm') != 'net-bits-v1-lsb-first':
        raise ValueError('Unknown random-walk algorithm')
    if header.get('condition') not in ('baseline', 'control', 'intention') or header.get('direction') not in ('high', 'low'):
        raise ValueError('Missing experimental condition or direction')
    if not integer(header.get('planned_samples'), 32, 4096):
        raise ValueError('Invalid planned sample count')
    samples, footer = [], None
    previous_time = -1
    for record in records[1:]:
        if footer is not None:
            raise ValueError('Records found after footer')
        if record.get('type') == 'footer':
            footer = record
            continue
        if record.get('type') != 'sample':
            raise ValueError('Unexpected record type')
        if not integer(record.get('i')) or record['i'] != len(samples):
            raise ValueError('Sample sequence gap/duplicate')
        if not integer(record.get('word'), 0, 2**32 - 1):
            raise ValueError('Invalid raw RNG word')
        if not integer(record.get('t_us')) or record['t_us'] <= previous_time:
            raise ValueError('Nonmonotonic sample time')
        if not integer(record.get('late_us')) or type(record.get('healthy')) is not bool:
            raise ValueError('Missing scheduler/health measurement')
        previous_time = record['t_us']
        samples.append(record)
    if len(samples) > header['planned_samples']:
        raise ValueError('More samples than predeclared')
    ones = sum(bin(s['word']).count('1') for s in samples)
    if footer and (not integer(footer.get('count')) or not integer(footer.get('ones')) or
                   footer['count'] != len(samples) or footer['ones'] != ones):
        raise ValueError('Footer does not match raw sample accounting')
    reason = footer.get('reason') if footer else 'interrupted'
    if reason == 'completed' and len(samples) != header['planned_samples']:
        raise ValueError('Completed trial has wrong sample count')
    eligible = (reason == 'completed' and all(s['healthy'] for s in samples) and
                header.get('source') == 'esp32s3-hwrng-rf-no-sleep' and not truncated)
    gaps = [samples[i]['t_us'] - samples[i-1]['t_us'] for i in range(1, len(samples))]
    report = dict(file=str(path), sha256=hashlib.sha256(raw).hexdigest(), id=header.get('id'),
                  condition=header['condition'], direction=header['direction'], source=header.get('source'),
                  protocol=header.get('protocol'), reason=reason, count=len(samples),
                  fixed_n_eligible=eligible, truncated_last_line=truncated,
                  max_interval_us=max(gaps, default=0), max_late_us=max((s['late_us'] for s in samples), default=0),
                  statistics=statistics(32 * len(samples), ones, header['direction']) if samples else None,
                  interpretation='Descriptive only unless fixed-N and independent fair-bit assumptions and preregistered protocol hold; no causal inference.')
    return report, samples


def compare(reports):
    """Exploratory pooled two-proportion comparison; explicitly not session-level inference."""
    groups = {}
    for condition in ('control', 'intention'):
        selected = [r for r in reports if r['condition'] == condition and r['fixed_n_eligible']]
        groups[condition] = dict(sessions=len(selected), bits=sum(r['statistics']['bits'] for r in selected),
                                 ones=sum(r['statistics']['ones'] for r in selected))
    c, i = groups['control'], groups['intention']
    if not c['bits'] or not i['bits']:
        return dict(groups=groups, note='Need completed, eligible control and intention trials.')
    pooled = (c['ones'] + i['ones']) / (c['bits'] + i['bits'])
    difference = i['ones'] / i['bits'] - c['ones'] / c['bits']
    se = math.sqrt(pooled * (1-pooled) * (1/c['bits'] + 1/i['bits']))
    z = difference / se if se else 0.0
    return dict(groups=groups, intention_minus_control=difference, exploratory_z=z,
                exploratory_p_two_sided=math.erfc(abs(z) / math.sqrt(2)),
                note='Pooled-bit normal approximation assumes independent bits and comparable conditions. Preregister session-level randomized comparisons for confirmatory work.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('files', nargs='+', type=Path)
    parser.add_argument('--csv', type=Path, help='Export raw samples from one file as CSV')
    parser.add_argument('--raw', type=Path, help='Export uint32 words in little-endian order from one file')
    parser.add_argument('--compare', action='store_true')
    parser.add_argument('--recover-truncated', action='store_true', help='Explicitly salvage incomplete final line; always marked ineligible')
    args = parser.parse_args()
    if (args.csv or args.raw) and len(args.files) != 1:
        parser.error('CSV/raw export requires one input')
    try:
        reports = []
        for path in args.files:
            report, samples = audit(path, args.recover_truncated)
            reports.append(report)
        if len({r['sha256'] for r in reports}) != len(reports) or len({r['id'] for r in reports}) != len(reports):
            raise ValueError('Duplicate session input detected')
        if args.csv:
            with args.csv.open('x', newline='') as handle:
                writer = csv.DictWriter(handle, ['i', 't_us', 'word', 'late_us', 'healthy'])
                writer.writeheader()
                for s in samples:
                    writer.writerow({key: s[key] for key in writer.fieldnames})
        if args.raw:
            with args.raw.open('xb') as handle:
                for s in samples:
                    handle.write(s['word'].to_bytes(4, 'little'))
        print(json.dumps(dict(trials=reports, **({'comparison': compare(reports)} if args.compare else {})), indent=2, allow_nan=False))
    except (ValueError, KeyError, OSError) as error:
        print(f'Audit failed: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
