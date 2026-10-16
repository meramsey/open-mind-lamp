import importlib.util
import json
import math
from pathlib import Path
import tempfile
import unittest

MODULE = Path(__file__).resolve().parents[1] / 'src/analyze.py'
spec = importlib.util.spec_from_file_location('analyze', MODULE)
analyze = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analyze)


def fixture(reason='completed'):
    header = dict(type='header', schema=1, id='test', condition='control', direction='high',
                  algorithm='net-bits-v1-lsb-first', source='esp32s3-hwrng-rf-no-sleep', planned_samples=32)
    samples = [dict(type='sample', i=i, t_us=i * 50000, word=0xAAAAAAAA, late_us=0, healthy=True) for i in range(32)]
    return [header, *samples, dict(type='footer', count=32, ones=512, reason=reason)]


class AnalysisTests(unittest.TestCase):
    def audit(self, records):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'trial.ndjson'
            path.write_text('\n'.join(json.dumps(x) for x in records) + '\n')
            return analyze.audit(path)

    def test_binomial_matches_exact_enumeration(self):
        for n in range(1, 45):
            for k in range(n + 1):
                exact = sum(math.comb(n, i) for i in range(k + 1)) / 2**n
                self.assertAlmostEqual(analyze.binomial_lower(n, k), exact, places=12)

    def test_extreme_tail_and_large_center(self):
        self.assertAlmostEqual(analyze.statistics(32, 32)['p_two_sided'], 2 / 2**32)
        self.assertAlmostEqual(analyze.statistics(131072, 65536)['p_two_sided'], 1)
        self.assertLess(analyze.statistics(1024, 600)['p_predeclared_one_sided'], .00001)
        self.assertGreater(analyze.statistics(1024, 600, 'low')['p_predeclared_one_sided'], .99)

    def test_round_trip_counts_hash_and_eligibility(self):
        report, samples = self.audit(fixture())
        self.assertTrue(report['fixed_n_eligible'])
        self.assertEqual(report['statistics']['z'], 0)
        self.assertEqual(len(report['sha256']), 64)
        self.assertEqual(len(samples), 32)

    def test_gaps_tampering_and_footer_order_rejected(self):
        for field, value in [('word', 2**32), ('word', True), ('i', 4), ('t_us', -1), ('healthy', 1)]:
            records = fixture(); records[1][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                self.audit(records)
        records = fixture(); records[-1]['ones'] += 1
        with self.assertRaises(ValueError): self.audit(records)
        records = fixture(); records.append(records[1])
        with self.assertRaises(ValueError): self.audit(records)

    def test_interrupted_early_and_test_source_not_eligible(self):
        self.assertFalse(self.audit(fixture()[:-1])[0]['fixed_n_eligible'])
        self.assertFalse(self.audit(fixture('user_stopped'))[0]['fixed_n_eligible'])
        records = fixture(); records[0]['source'] = 'deterministic-test'
        self.assertFalse(self.audit(records)[0]['fixed_n_eligible'])
        records = fixture(); records[10]['healthy'] = False
        self.assertFalse(self.audit(records)[0]['fixed_n_eligible'])

    def test_truncation_requires_explicit_salvage(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'broken.ndjson'
            path.write_text('\n'.join(json.dumps(x) for x in fixture()[:-1]) + '\n{"type":')
            with self.assertRaises(ValueError): analyze.audit(path)
            report, samples = analyze.audit(path, True)
            self.assertTrue(report['truncated_last_line'])
            self.assertFalse(report['fixed_n_eligible'])
            self.assertEqual(len(samples), 32)

    def test_duplicate_keys_rejected(self):
        with self.assertRaises(ValueError):
            json.loads('{"word":0,"word":1}', object_pairs_hook=analyze.unique_object)

    def test_control_comparison(self):
        c = self.audit(fixture())[0]
        i = dict(c, condition='intention', id='intention')
        result = analyze.compare([c, i])
        self.assertEqual(result['exploratory_z'], 0)
        self.assertEqual(result['exploratory_p_two_sided'], 1)


if __name__ == '__main__': unittest.main()
