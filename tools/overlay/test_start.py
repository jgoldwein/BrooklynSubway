import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import start


class LauncherTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root/'inputs').mkdir()
        self.patch = mock.patch.object(start, 'ROOT', self.root)
        self.patch.start()

    def tearDown(self):
        self.patch.stop()
        self.temp.cleanup()

    def inputs(self):
        for name in ['Blank_Map.eprj2', 'Blank_Gerbers.zip']:
            (self.root/'inputs'/name).write_bytes(b'fixture')

    def test_missing_pair_stops_before_install_or_picker(self):
        with mock.patch.object(start, 'choose_pdf') as pick, mock.patch.object(start.subprocess, 'run') as install:
            self.assertEqual(start.main(['--bootstrap']), 1)
            pick.assert_not_called()
            install.assert_not_called()

    def test_cancel_never_launches_helper(self):
        self.inputs()
        with mock.patch.object(start, 'choose_pdf', return_value=''), mock.patch.object(start.subprocess, 'call') as call:
            self.assertEqual(start.main(['--choose-pdf']), 0)
            call.assert_not_called()

    def test_chosen_pdf_and_recipe_reach_helper(self):
        self.inputs()
        pdf = self.root/'a map.pdf'; pdf.write_bytes(b'PDF fixture')
        recipe = self.root/'reviewed.json'
        with mock.patch.object(start, 'recipe_for', return_value=recipe), mock.patch.object(start.subprocess, 'call', return_value=0) as call:
            self.assertEqual(start.main(['--pdf', str(pdf)]), 0)
            args = call.call_args.args[0]
            self.assertIn('--open-project', args)
            self.assertIn('--guided-setup', args)
            self.assertEqual(args[args.index('--pdf')+1], str(pdf.resolve()))
            self.assertEqual(args[args.index('--recipe')+1], str(recipe))
            self.assertIn(str(self.root/'builds'), args[-1])

    def test_recipe_selected_by_fingerprint_only(self):
        folder=self.root/'recipes';folder.mkdir()
        pdf=self.root/'map.pdf';pdf.write_bytes(b'known edition')
        recipe=folder/'known.json'
        recipe.write_text(json.dumps({'source':{'sha256':hashlib.sha256(pdf.read_bytes()).hexdigest()}}))
        with mock.patch.object(start,'HERE',self.root):
            self.assertEqual(start.recipe_for(pdf),recipe)
            pdf.write_bytes(b'unknown edition')
            self.assertRaises(ValueError,start.recipe_for,pdf)

    def test_pdf_download_is_local_and_never_overwrites(self):
        with mock.patch.object(start.urllib.request, 'urlopen', return_value=io.BytesIO(b'%PDF-fixture')):
            pdf=start.download_pdf(self.root/'source')
        self.assertEqual(pdf.read_bytes(), b'%PDF-fixture')
        with mock.patch.object(start.urllib.request, 'urlopen', return_value=io.BytesIO(b'%PDF-other')):
            self.assertRaises(FileExistsError,start.download_pdf,self.root/'source')
        self.assertEqual(pdf.read_bytes(),b'%PDF-fixture')

    def test_non_pdf_response_is_not_saved(self):
        with mock.patch.object(start.urllib.request, 'urlopen', return_value=io.BytesIO(b'<html>blocked</html>')):
            self.assertRaises(ValueError,start.download_pdf,self.root/'source')
        self.assertFalse((self.root/'source').exists())

    def test_oversized_response_is_not_saved(self):
        with mock.patch.object(start,'MAX_PDF_BYTES',10), mock.patch.object(start.urllib.request,'urlopen',return_value=io.BytesIO(b'%PDF-'+b'x'*20)):
            self.assertRaises(ValueError,start.download_pdf,self.root/'source')
        self.assertFalse((self.root/'source').exists())

    def test_failed_download_offers_local_picker(self):
        self.inputs()
        with mock.patch.object(start,'download_pdf',side_effect=OSError('offline')), mock.patch.object(start,'choose_pdf',return_value='') as pick, mock.patch.object(start.subprocess,'call') as launch:
            self.assertEqual(start.main([]),0)
            pick.assert_called_once();launch.assert_not_called()

    def test_unknown_downloaded_edition_never_opens_editor(self):
        self.inputs();pdf=self.root/'unknown.pdf';pdf.write_bytes(b'%PDF-unknown')
        with mock.patch.object(start,'download_pdf',return_value=pdf), mock.patch.object(start,'choose_pdf') as pick, mock.patch.object(start.subprocess,'call') as launch:
            self.assertRaises(ValueError,start.main,[])
            pick.assert_not_called();launch.assert_not_called()


if __name__ == '__main__': unittest.main()
