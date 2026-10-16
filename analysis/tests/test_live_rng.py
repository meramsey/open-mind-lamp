import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]


def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'analysis/src' / f'{name}.py')
    result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result


live = module('verify_live_rng')
analysis = module('analyze')


class LivePipelineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Emitted by real Walk/color_frame/Matrix::show/API code, native SDK/RMT doubles.
        cls.document = json.loads(subprocess.check_output([str(ROOT / '.tools/host-tests/pipeline_fixture')], text=True))

    def snapshots(self): return copy.deepcopy(self.document['snapshots'])

    def test_known_production_pipeline_replays_all_directions(self):
        result = live.verify(self.snapshots(), allow_test=True)
        self.assertEqual(result['successful_samples_checked'], 8)
        self.assertEqual(result['complete_windows_checked'], 4)
        self.assertEqual(result['directions'], dict(positive=1, negative=1, zero=2))

    def test_deterministic_trace_is_not_hardware_or_an_experiment(self):
        self.assertFalse(self.document['experimental_record'])
        with self.assertRaises(ValueError): live.verify(self.snapshots())
        # The independent experiment parser must reject this diagnostic format.
        import tempfile
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'not-a-trial.ndjson'
            path.write_text(json.dumps(self.document) + '\n')
            with self.assertRaises(ValueError): analysis.audit(path)

    def test_word_increment_and_target_tampering_detected(self):
        for field, value in [('word', 0), ('net', -32), ('applied', False), ('healthy', False)]:
            snapshots = self.snapshots(); snapshots[0]['events'][1][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError): live.verify(snapshots, True)
        snapshots = self.snapshots(); snapshots[1]['events'][-1]['target_after'] += 1
        with self.assertRaises(ValueError): live.verify(snapshots, True)

    def test_led_and_smoothing_tampering_detected(self):
        for field, value in [('commanded_rgb', [0, 0, 32]), ('wire_bytes', [32, 0, 0]), ('walk_hue', 50)]:
            snapshots = self.snapshots(); snapshots[0]['color'][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError): live.verify(snapshots, True)
        snapshots = self.snapshots(); snapshots[0]['color']['render_step']['target'] += 3
        with self.assertRaises(ValueError): live.verify(snapshots, True)

    def test_gaps_reboot_readiness_and_configuration_changes_rejected(self):
        snapshots = self.snapshots(); del snapshots[1]['events'][0]
        with self.assertRaises(ValueError): live.verify(snapshots, True)
        for field in ('boot_id', 'revision'):
            snapshots = self.snapshots(); snapshots[1][field] += 1
            with self.subTest(field=field), self.assertRaises(ValueError): live.verify(snapshots, True)
        snapshots = self.snapshots(); snapshots[1]['rng']['ready'] = False
        with self.assertRaises(ValueError): live.verify(snapshots, True)
        snapshots = self.snapshots(); snapshots[1]['rng']['rejected_reads'] += 1
        with self.assertRaises(ValueError): live.verify(snapshots, True)

    def test_initial_partial_window_explicitly_skipped(self):
        snapshots = self.snapshots(); snapshots[0]['events'] = snapshots[0]['events'][2:]
        snapshots[0]['oldest_seq'] = 3
        result = live.verify(snapshots, True)
        self.assertEqual(result['initial_partial_windows_skipped'], 1)
        self.assertEqual(result['complete_windows_checked'], 3)

    def test_override_modes_are_not_misreported_as_random_walk(self):
        for mode in (0, 2, 3, 4, 5, 6):
            snapshots = self.snapshots(); snapshots[0]['settings']['mode'] = mode
            with self.subTest(mode=mode), self.assertRaisesRegex(ValueError, 'Random Walk'):
                live.verify(snapshots, True)
        snapshots = self.snapshots(); snapshots[0]['settings']['enabled'] = False
        with self.assertRaises(ValueError): live.verify(snapshots, True)
        snapshots = self.snapshots(); snapshots[0]['calibration_active'] = True
        with self.assertRaises(ValueError): live.verify(snapshots, True)


if __name__ == '__main__': unittest.main()
