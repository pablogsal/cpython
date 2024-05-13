import threading


def calc():
    total = 0
    for i in range(1_000_000):
        total += i
    print(total)


def main(args=None):
    threads = [threading.Thread(target=calc)
               for _ in range(100)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    print("Done")


main()
