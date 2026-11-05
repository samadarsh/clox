# Python version of bench/lox/fib.lox: recursive calls.
import time


def fib(n):
    if n < 2:
        return n
    return fib(n - 2) + fib(n - 1)


start = time.process_time()
print("true" if fib(35) == 9227465 else "false")
print(time.process_time() - start)
