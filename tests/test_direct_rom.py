from pathlib import Path
import shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class DirectRomTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_external_roms_keep_save_location(self):
        with tempfile.TemporaryDirectory() as folder:
            exe=Path(folder)/'direct-rom'
            subprocess.run(['c++','-std=c++17','-pthread','-I'+str(ROOT/'frontend'),str(ROOT/'tests/test_direct_rom.cpp'),str(ROOT/'frontend/launcher_cjk.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe),str(Path(folder)/'library')],check=True,timeout=30)
    @unittest.skipUnless(shutil.which('javac') and shutil.which('java'),'Java tools required')
    def test_document_import_safety(self):
        with tempfile.TemporaryDirectory() as folder:
            subprocess.run(['javac','-d',folder,str(ROOT/'tools/android/allgames/java/io/github/mamesj/pokemonds/RomImport.java'),str(ROOT/'tests/android/RomImportTest.java')],check=True)
            subprocess.run(['java','-cp',folder,'io.github.mamesj.pokemonds.RomImportTest',str(Path(folder)/'imports')],check=True,timeout=30)

    @unittest.skipUnless(shutil.which('javac') and shutil.which('java'),'Java tools required')
    def test_document_tree_import(self):
        with tempfile.TemporaryDirectory() as folder:
            java=ROOT/'tools/android/allgames/java/io/github/mamesj/pokemonds'
            subprocess.run(['javac','-d',folder,str(java/'RomImport.java'),str(java/'RomTreeImport.java'),str(ROOT/'tests/android/RomTreeImportTest.java')],check=True)
            subprocess.run(['java','-cp',folder,'io.github.mamesj.pokemonds.RomTreeImportTest',str(Path(folder)/'tree')],check=True,timeout=30)
