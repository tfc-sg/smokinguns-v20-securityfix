import importlib.util
from pathlib import Path
import struct
import unittest
import tempfile
import zipfile
spec=importlib.util.spec_from_file_location('powerup_models',Path(__file__).parents[1]/'tools/prepare_powerup_models.py')
mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)

def fixture():
    data=bytearray(392)
    struct.pack_into('<4si64s9i',data,0,b'IDP3',15,b'model',0,1,1,1,0,108,164,276,392)
    struct.pack_into('<10f',data,108,*range(1,11))
    struct.pack_into('<3f',data,164+64,1,2,3)
    struct.pack_into('<9f',data,164+76,1,0,0,0,1,0,0,0,1)
    struct.pack_into('<4s64s10i',data,276,b'IDP3',b'surface',0,1,0,1,0,108,108,108,108,116)
    struct.pack_into('<3hH',data,384,100,-200,300,65500)
    return bytes(data)

class ModelTests(unittest.TestCase):
    def test_scale_geometry_and_keep_normals(self):
        data=mod.scale_md3(fixture(),3)
        self.assertEqual(struct.unpack_from('<3hH',data,384),(300,-600,900,65500))
        self.assertEqual(struct.unpack_from('<3f',data,164+64),(3,6,9))
        self.assertEqual(struct.unpack_from('<9f',data,164+76),(1,0,0,0,1,0,0,0,1))
        self.assertEqual(struct.unpack_from('<10f',data,108),tuple(x*3 for x in range(1,11)))
    def test_prepare_keeps_source_and_is_repeatable(self):
        with tempfile.TemporaryDirectory() as tmp:
            game=Path(tmp)/'game';(game/'powerups').mkdir(parents=True)
            model=game/'models/misc/flagpole.md3';model.parent.mkdir(parents=True)
            model.write_bytes(fixture())
            (game/'powerups/dm_test.txt').write_text('{ classname "misc_externalmodel" model "models/misc/flagpole" modelscale "3" origin "0 0 0" }')
            out=Path(tmp)/'generated.pk3';mod.prepare(game,out);first=out.read_bytes()
            mod.prepare(game,out);self.assertEqual(out.read_bytes(),first)
            self.assertEqual(model.read_bytes(),fixture())
            with zipfile.ZipFile(out) as z:
                self.assertEqual(struct.unpack_from('<3h',z.read('models/misc/flagpole_pu3000.md3'),384),(300,-600,900))
    def test_missing_asset_does_not_write_archive(self):
        with tempfile.TemporaryDirectory() as tmp:
            game=Path(tmp);(game/'powerups').mkdir()
            (game/'powerups/dm_test.txt').write_text('{ model "models/misc/missing" modelscale "3" }')
            out=game/'generated.pk3'
            with self.assertRaises(FileNotFoundError):mod.prepare(game,out)
            self.assertFalse(out.exists())
    def test_coordinate_overflow(self):
        with self.assertRaises(ValueError):mod.scale_md3(fixture(),200)
    def test_invalid_input(self):
        with self.assertRaises(ValueError):mod.scale_md3(b'invalid',3)
    def test_bad_surface_offset(self):
        data=bytearray(fixture());struct.pack_into('<i',data,276+100,500)
        with self.assertRaises(ValueError):mod.scale_md3(data,3)

if __name__=='__main__':unittest.main()
