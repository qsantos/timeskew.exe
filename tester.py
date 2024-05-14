import filecmp
from math import isclose
from os import environ, remove
from socket import create_connection
from subprocess import Popen, PIPE
from sys import executable
from time import sleep, time

# Do it that way to minimize overhead when running testee
from testee import SLEEP_DURATION

COMMAND = ["./timeskew.exe", executable, "testee.py"]
TOLERANCE = 0.1
LOGFILE = "test.log"
EXPECTED = "expected.log"
PORT = 40000
print(f'{SLEEP_DURATION=}')
print(f'{TOLERANCE=}')

try:
    remove(LOGFILE)
except FileNotFoundError:
    pass
environ["TIMESKEW_LOGFILE"] = LOGFILE


def run_testee_with_env(**env: str) -> Popen:
    return Popen(COMMAND, stdin=PIPE, stdout=PIPE, env={**env, **environ})


def test_relative_time(p: Popen, time_ratio: float) -> None:
    sleep(.1)
    p.stdin.write(b'\n')
    p.stdin.flush()
    start = time()
    assert p.wait() == 0
    real_elapsed = time() - start
    fake_elapsed = float(p.stdout.read().decode())
    print(f'{real_elapsed=}')
    print(f'{fake_elapsed=}')
    # Check whether the duration is consistent in real and skewed time
    assert isclose(real_elapsed, SLEEP_DURATION / time_ratio, rel_tol=TOLERANCE)
    assert isclose(fake_elapsed, SLEEP_DURATION, rel_tol=TOLERANCE)


print('Test with no time acceleration')
p = run_testee_with_env()
test_relative_time(p, 1)

print('Test time acceleration with environment-variables')
p = run_testee_with_env(TIMESKEW="10 1")
test_relative_time(p, 10)

print('Test time slow-down with environment-variables')
p = run_testee_with_env(TIMESKEW="1 10")
test_relative_time(p, .1)

print('Test time acceleration with port')
p = run_testee_with_env(TIMESKEW_PORT=str(PORT))
sleep(.1)
with create_connection(('127.0.0.1', PORT)) as sock:
    sock.sendall(b"10 1\n")
test_relative_time(p, 10)

print('Test time slow-down with port')
p = run_testee_with_env(TIMESKEW_PORT=str(PORT))
sleep(.1)
with create_connection(('127.0.0.1', PORT)) as sock:
    sock.sendall(b"1 10\n")
test_relative_time(p, .1)

if not filecmp.cmp(LOGFILE, EXPECTED):
    print('The logfile contains:')
    print('=' * 50)
    with open(LOGFILE) as f:
        print(f.read())
    print('=' * 50)
