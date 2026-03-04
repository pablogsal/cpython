"""Tests for sys.remote_exec (PEP 768 backport to 3.12)"""

import os
import socket
import subprocess
import sys
import textwrap
import time
import unittest
from io import StringIO
from test import support
from test.support import os_helper, SHORT_TIMEOUT
from test.support.socket_helper import find_unused_port
from unittest import mock

# Skip on non-Linux platforms
if sys.platform != 'linux':
    raise unittest.SkipTest("sys.remote_exec is only supported on Linux")

# Check if remote_exec is available
if not hasattr(sys, 'remote_exec'):
    raise unittest.SkipTest("sys.remote_exec is not available")


@support.cpython_only
class TestRemoteExec(unittest.TestCase):
    def tearDown(self):
        support.reap_children()

    def _run_remote_exec_test(self, script_code, python_args=None, env=None,
                              prologue='',
                              script_path=os_helper.TESTFN + '_remote.py'):
        # Create the script that will be remotely executed
        self.addCleanup(os_helper.unlink, script_path)

        with open(script_path, 'w') as f:
            f.write(script_code)

        # Create and run the target process
        target = os_helper.TESTFN + '_target.py'
        self.addCleanup(os_helper.unlink, target)

        port = find_unused_port()

        with open(target, 'w') as f:
            f.write(f'''
import sys
import time
import socket

# Connect to the test process
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', {port}))

{prologue}

# Signal that the process is ready
sock.sendall(b"ready")

print("Target process running...")

# Wait for remote script to be executed
# (the execution will happen as the following
# code is processed as soon as the recv call
# unblocks)
sock.recv(1024)

# Do a bunch of work to give the remote script time to run
# The eval loop checks eval_breaker every ~100 bytecode ops
import time
time.sleep(0.1)
x = 0
for i in range(10000):
    x += i
time.sleep(0.1)

# Write confirmation back
sock.sendall(b"executed")
sock.close()
''')

        # Start the target process and capture its output
        cmd = [sys.executable]
        if python_args:
            cmd.extend(python_args)
        cmd.append(target)

        # Create a socket server to communicate with the target process
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.bind(('localhost', port))
        server_socket.settimeout(SHORT_TIMEOUT)
        server_socket.listen(1)

        with subprocess.Popen(cmd,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE,
                              env=env,
                              ) as proc:
            client_socket = None
            try:
                # Accept connection from target process
                client_socket, _ = server_socket.accept()
                server_socket.close()

                response = client_socket.recv(1024)
                self.assertEqual(response, b"ready")

                # Try remote exec on the target process
                sys.remote_exec(proc.pid, script_path)

                # Signal script to continue
                client_socket.sendall(b"continue")

                # Wait for execution confirmation
                response = client_socket.recv(1024)
                self.assertEqual(response, b"executed")

                # Return output for test verification
                stdout, stderr = proc.communicate(timeout=10.0)
                return proc.returncode, stdout, stderr
            except PermissionError:
                self.skipTest("Insufficient permissions to execute code in remote process")
            finally:
                if client_socket is not None:
                    client_socket.close()
                proc.kill()
                proc.terminate()
                proc.wait(timeout=SHORT_TIMEOUT)

    def test_remote_exec(self):
        """Test basic remote exec functionality"""
        script = 'print("Remote script executed successfully!")'
        returncode, stdout, stderr = self._run_remote_exec_test(script)
        self.assertIn(b"Remote script executed successfully!", stdout)
        self.assertEqual(stderr, b"")

    def test_remote_exec_bytes(self):
        script = 'print("Remote script executed successfully!")'
        script_path = os.fsencode(os_helper.TESTFN) + b'_bytes_remote.py'
        returncode, stdout, stderr = self._run_remote_exec_test(script,
                                                    script_path=script_path)
        self.assertIn(b"Remote script executed successfully!", stdout)
        self.assertEqual(stderr, b"")

    def test_remote_exec_with_self_process(self):
        """Test remote exec with the target process being the same as the test process"""
        code = 'import sys;print("Remote script executed successfully!", file=sys.stderr)'
        file = os_helper.TESTFN + '_remote_self.py'
        with open(file, 'w') as f:
            f.write(code)
        self.addCleanup(os_helper.unlink, file)
        with mock.patch('sys.stderr', new_callable=StringIO) as mock_stderr:
            sys.remote_exec(os.getpid(), os.path.abspath(file))
            # Give the eval loop a chance to process the pending call.
            # The remote debugger is checked in _Py_HandlePending, which
            # runs when the eval_breaker is set. We need to execute some
            # bytecode to trigger that check.
            for _ in range(100):
                time.sleep(0.01)
                if mock_stderr.getvalue():
                    break
            self.assertEqual(mock_stderr.getvalue(), "Remote script executed successfully!\n")

    def test_remote_exec_raises_audit_event(self):
        """Test remote exec raises an audit event"""
        prologue = '''\
import sys
def audit_hook(event, arg):
    print(f"Audit event: {event}, arg: {arg}".encode("ascii", errors="replace"))
sys.addaudithook(audit_hook)
'''
        script = '''
print("Remote script executed successfully!")
'''
        returncode, stdout, stderr = self._run_remote_exec_test(script, prologue=prologue)
        self.assertEqual(returncode, 0)
        self.assertIn(b"Remote script executed successfully!", stdout)
        self.assertIn(b"Audit event: cpython.remote_debugger_script, arg: ", stdout)
        self.assertEqual(stderr, b"")

    def test_remote_exec_with_exception(self):
        """Test remote exec with an exception raised in the target process

        The exception should be raised in the main thread of the target process
        but not crash the target process.
        """
        script = '''
raise Exception("Remote script exception")
'''
        returncode, stdout, stderr = self._run_remote_exec_test(script)
        self.assertEqual(returncode, 0)
        self.assertIn(b"Remote script exception", stderr)
        self.assertEqual(stdout.strip(), b"Target process running...")

    def test_new_namespace_for_each_remote_exec(self):
        """Test that each remote_exec call gets its own namespace."""
        script = textwrap.dedent(
            """
            assert globals() is not __import__("__main__").__dict__
            print("Remote script executed successfully!")
            """
        )
        returncode, stdout, stderr = self._run_remote_exec_test(script)
        self.assertEqual(returncode, 0)
        self.assertEqual(stderr, b"")
        self.assertIn(b"Remote script executed successfully", stdout)

    def test_remote_exec_invalid_pid(self):
        """Test remote exec with invalid process ID"""
        with self.assertRaises(OSError):
            sys.remote_exec(99999, "print('should not run')")

    def test_remote_exec_invalid_script(self):
        """Test remote exec with invalid script type"""
        with self.assertRaises(TypeError):
            sys.remote_exec(0, None)
        with self.assertRaises(TypeError):
            sys.remote_exec(0, 123)

    def test_remote_exec_syntax_error(self):
        """Test remote exec with syntax error in script"""
        script = '''
this is invalid python code
'''
        returncode, stdout, stderr = self._run_remote_exec_test(script)
        self.assertEqual(returncode, 0)
        self.assertIn(b"SyntaxError", stderr)
        self.assertEqual(stdout.strip(), b"Target process running...")

    def test_remote_exec_invalid_script_path(self):
        """Test remote exec with invalid script path"""
        with self.assertRaises(OSError):
            sys.remote_exec(os.getpid(), "invalid_script_path")


if __name__ == '__main__':
    unittest.main()
