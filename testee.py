from time import sleep, time

SLEEP_DURATION = 1

if __name__ == "__main__":
    input()
    start = time()
    sleep(SLEEP_DURATION)
    print(time() - start)
