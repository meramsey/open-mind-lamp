"""Actual Chromium + Firefox desktop/mobile-viewport integration, mocked device HTTP.

This verifies browser behavior, not the ESP32 HTTP implementation or Android OS Wi-Fi.
"""
import functools
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import threading
import unittest
from playwright.sync_api import sync_playwright, expect

ROOT = Path(__file__).resolve().parents[1]
TOKEN = 'a' * 64
SETTINGS = dict(brightness=.08, saturation=.9, sensitivity=.2, smoothing_ms=1500,
                starting_hue=180, sample_hz=20, accumulation_ms=1000, frame_hz=30,
                pixel=0, mode=1, enabled=True, rgb_order=False)


class BrowserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(SimpleHTTPRequestHandler, directory=str(ROOT)))
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True); cls.thread.start()
        cls.url = f'http://127.0.0.1:{cls.server.server_port}'

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown(); cls.server.server_close(); cls.thread.join()

    def test_offline_browser_workflows(self):
        with sync_playwright() as p:
            for engine in (p.chromium, p.firefox):
                for width in (390, 1280):
                    with self.subTest(browser=engine.name, width=width):
                        browser = engine.launch()
                        page = browser.new_page(viewport=dict(width=width, height=1000), accept_downloads=True)
                        errors, external, writes = [], [], []
                        page.on('pageerror', lambda error: errors.append(str(error)))
                        page.on('request', lambda req: external.append(req.url) if not req.url.startswith(self.url) else None)
                        settings = dict(SETTINGS)
                        session = dict(active=False, available=False, storage_ready=True, io_failed=False,
                                       id='', reason='none', count=0, planned=0)
                        state = dict(version='0.1.0', source='esp32s3-hwrng-rf-no-sleep', uptime_s=10,
                                     free_heap=100000, min_heap=99000, hue=180, target_hue=182,
                                     rng_words=200, rng_ones=3200, missed_slots=0, rng_failed=False, led_ok=True,
                                     network=dict(ap_ssid='OpenMindLamp-test', ap_ip='192.168.4.1',
                                                  station_ip='', hostname='openmindlamp-test', connected=False,
                                                  rf_entropy_ready=True), session=session)

                        def route_api(route):
                            request = route.request
                            if request.headers.get('authorization') != f'Bearer {TOKEN}':
                                return route.fulfill(status=401, json=dict(error='Invalid token'))
                            path = request.url.split('/api/')[1]
                            if request.method == 'GET':
                                if path == 'status': return route.fulfill(json=state)
                                if path == 'settings': return route.fulfill(json=settings)
                                if path == 'export': return route.fulfill(body='{"type":"header","schema":1}\n', content_type='application/x-ndjson')
                            data = request.post_data_json; writes.append((path, data))
                            if path == 'settings': settings.update(data)
                            if path == 'session/start': session.update(active=True, available=True, id='test-session', reason='running', planned=data['samples'])
                            if path == 'session/stop': session.update(active=False, reason='user_stopped')
                            if path == 'session/clear': session.update(available=False, reason='none')
                            route.fulfill(json=dict(ok=True))

                        page.route('**/api/**', route_api)
                        page.goto(self.url)
                        page.locator('#token').fill(TOKEN)
                        page.locator('#auth button').click()
                        expect(page.locator('#login')).to_be_hidden()
                        expect(page.locator('#hue')).to_have_text('180.0')
                        self.assertLessEqual(page.evaluate('document.documentElement.scrollWidth'), width)
                        page.locator('#theme').click()
                        expect(page.locator('body')).to_have_class('light')
                        page.locator('[name=sensitivity]').fill('0.4')
                        page.locator('#controls button').click()
                        expect(page.locator('#notice')).to_contain_text('Settings saved')
                        self.assertEqual(settings['sensitivity'], .4)
                        self.assertIs(settings['enabled'], True)
                        page.locator('[name=protocol]').fill('Predeclared protocol 1')
                        page.locator('#trial button').click()
                        expect(page.locator('#controls button')).to_be_disabled()
                        expect(page.locator('#session')).to_contain_text('running')
                        page.locator('#stop').click()
                        expect(page.locator('#export')).to_be_enabled()
                        with page.expect_download() as download:
                            page.locator('#export').click()
                        self.assertIn('test-session', download.value.suggested_filename)
                        page.on('dialog', lambda dialog: dialog.accept())
                        page.locator('#clear').click()
                        expect(page.locator('#trial button')).to_be_enabled()
                        page.locator('#logout').click()
                        expect(page.locator('#login')).to_be_visible()
                        self.assertEqual(errors, [])
                        self.assertEqual(external, [])
                        self.assertTrue(any(path == 'session/start' and isinstance(data['samples'], int) for path, data in writes))
                        browser.close()


if __name__ == '__main__': unittest.main()
