from time import sleep, time

SLEEP_DURATION = 10

if __name__ == "__main__":
    start = time()
    sleep(SLEEP_DURATION)
    print(time() - start)
