"""Coverage failures use synthetic metadata only; no ROMs or saves."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('audit_coverage', ROOT / 'tools/regions/audit_coverage.py')
audit_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit_module)


class CoverageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ['frontend', 'tools/soulsilver', 'tools/regions', 'tools/versions', 'funcs']:
            (self.root / name).mkdir(parents=True)
        (self.root / 'frontend/gamecode.h').write_text('{ "IRBF", "IRBO" }')
        (self.root / 'frontend/launcher.h').write_text(
            '{ "IRBF", "French", "fr.sav" }, { "IRBO", "English", "en.sav" }')
        (self.root / 'tools/regions/IRBF.txt').write_text('address metadata\n')
        for code in ['IRBF', 'IRBO']:
            (self.root / f'funcs/{code}.txt').write_text('address metadata\n')
        (self.root / 'portable.cpp').write_text('{"IPGE", "Pokemon SoulSilver", shifts, 0}')

    def run_audit(self):
        return audit_module.audit(self.root, [self.root / 'funcs'], [self.root / 'portable.cpp'])

    def test_complete(self):
        self.assertTrue(self.run_audit()['ok'])

    def test_missing_package_table(self):
        (self.root / 'funcs/IRBF.txt').unlink()
        self.assertIn('missing or empty IRBF.txt', ' '.join(self.run_audit()['errors']))

    def test_stale_direct_table(self):
        (self.root / 'funcs/IRBF.txt').write_text('old metadata')
        self.assertIn('stale IRBF.txt', ' '.join(self.run_audit()['errors']))

    def test_missing_source(self):
        (self.root / 'tools/regions/IRBF.txt').unlink()
        self.assertIn('missing regional source metadata', ' '.join(self.run_audit()['errors']))

    def test_save_collision(self):
        path = self.root / 'frontend/launcher.h'
        path.write_text(path.read_text().replace('fr.sav', 'en.sav'))
        self.assertIn('launcher: shared save filename', self.run_audit()['errors'])

    def test_missing_portable_profile(self):
        (self.root / 'portable.cpp').write_text('')
        self.assertIn('missing portable profile IPGE', ' '.join(self.run_audit()['errors']))

    def test_unknown_language_not_inferred(self):
        self.assertNotIn('IRBD', self.run_audit()['function_tables'])


if __name__ == '__main__':
    unittest.main()
