import io
import tempfile
import unittest
import zipfile
from pathlib import Path
import art_free
import overlay


class ArtFreeTests(unittest.TestCase):
    def test_only_print_and_color_removed(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            source = root / 'source.zip'
            members = {'board.GTL': b'copper', 'board.DRL': b'holes',
                       'board.GKO': b'outline', 'board.GTS': b'mask',
                       'board.GTO': b'top artwork', 'board.GBO': b'bottom labels',
                       'board.GDL': b'document drawing', 'board.FCTS': b'color',
                       'board.FCBS': b'back color', 'board.FCBO': b'color outline',
                       'board.FCBM': b'color marks'}
            with zipfile.ZipFile(source, 'w') as z:
                for n, data in members.items(): z.writestr(n, data)
            report = art_free.prepare(source, root/'result.zip', root/'report.json')
            actual = overlay.archive(root/'result.zip')
            self.assertEqual(len(report['removedColorMembers']), 4)
            self.assertEqual(len(report['clearedPrintedMembers']), 3)
            for n, data in members.items():
                ext = Path(n).suffix
                if ext in overlay.COLOR: self.assertNotIn(n, actual)
                elif ext in art_free.PRINT_LAYERS: self.assertEqual(actual[n], art_free.EMPTY_GERBER)
                else: self.assertEqual(actual[n], data)
            self.assertRaises(ValueError, art_free.prepare, source, root/'result.zip', root/'report.json')


if __name__ == '__main__': unittest.main()
