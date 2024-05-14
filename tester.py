from math import isclose
from os import environ
from subprocess import Popen, PIPE
from sys import executable
from time import time

# Do it that way to minimize overhead when running testee
from testee import SLEEP_DURATION

COMMAND = ["./timeskew.exe", executable, "testee.py"]
TOLERANCE = 0.05
print(f'{SLEEP_DURATION=}')
print(f'{TOLERANCE=}')

print('Measure the actual time to run the testee.py to account for overhead')
p = Popen(COMMAND, stdout=PIPE)
start = time()
assert p.wait() == 0
real_elapsed = time() - start
fake_elapsed = float(p.stdout.read().decode())
print(f'{real_elapsed=}')
print(f'{fake_elapsed=}')
# It should still be pretty close to 1
assert isclose(real_elapsed, SLEEP_DURATION, rel_tol=TOLERANCE)
assert isclose(fake_elapsed, SLEEP_DURATION, rel_tol=TOLERANCE)
# Count overhead as the difference between the two
overhead = real_elapsed - fake_elapsed
print(f'{overhead=}')
assert overhead >= 0.010

print('Test time acceleration with environment-variables')
p = Popen(COMMAND, stdout=PIPE, env={"TIMESKEW": "10 1", **environ})
start = time()
assert p.wait() == 0
real_elapsed = time() - start
fake_elapsed = float(p.stdout.read().decode())
print(f'{real_elapsed=}')
print(f'{fake_elapsed=}')
assert isclose(real_elapsed - overhead, SLEEP_DURATION / 10, rel_tol=TOLERANCE)
assert isclose(fake_elapsed, SLEEP_DURATION, rel_tol=TOLERANCE)

print('Test time slow-down with environment-variables')
p = Popen(COMMAND, stdout=PIPE, env={"TIMESKEW": "1 10", **environ})
start = time()
assert p.wait() == 0
real_elapsed = time() - start
fake_elapsed = float(p.stdout.read().decode())
print(f'{real_elapsed=}')
print(f'{fake_elapsed=}')
assert isclose(real_elapsed - overhead, SLEEP_DURATION * 10, rel_tol=TOLERANCE)
assert isclose(fake_elapsed, SLEEP_DURATION, rel_tol=TOLERANCE)
