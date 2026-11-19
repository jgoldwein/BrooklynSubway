import contextlib
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import launch


class LaunchTests(unittest.TestCase):
    def test_child_error_is_visible_and_saved_with_exit_code(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);here=root/'tools'/'overlay';here.mkdir(parents=True)
            (here/'start.py').write_text("import sys\nprint('setup output',flush=True)\nprint('failure detail',file=sys.stderr,flush=True)\nsys.exit(7)\n")
            console=io.StringIO()
            with mock.patch.object(launch,'HERE',here),mock.patch.object(launch,'ROOT',root),contextlib.redirect_stdout(console):
                self.assertEqual(launch.main(['--bootstrap']),7)
            log=(root/'Overlay-launch.log').read_text()
            for text in ['setup output','failure detail','exit code 7']:
                self.assertIn(text,log);self.assertIn(text,console.getvalue())

    @unittest.skipUnless(sys.platform=='win32','Windows batch launcher')
    def test_batch_shim_returns_to_persistent_end_message(self):
        with tempfile.TemporaryDirectory(prefix='brooklyn launch ') as folder:
            root=Path(folder)
            (root/'py.cmd').write_text('@echo off\nif "%2"=="-c" exit /b 0\nexit /b 7\n')
            env=dict(os.environ,PATH=str(root)+os.pathsep+os.environ['PATH'])
            script=launch.ROOT/'Start_Overlay.cmd'
            result=subprocess.run([os.environ['ComSpec'],'/d','/c',str(script),'--run'],
                env=env,capture_output=True,text=True,timeout=15)
            self.assertEqual(result.returncode,7,result.stdout+result.stderr)
            self.assertIn('This window stays open',result.stdout)
            self.assertIn('exit code 7',result.stdout)


if __name__=='__main__':unittest.main()
