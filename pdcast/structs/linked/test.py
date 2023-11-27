
from random import randint
from set import *
from timeit import timeit

char = lambda: chr(randint(0, 25) + ord("a"))
t = [char() for _ in range(10**6)]

def test1(s):
    for x in t:
        s.lru_add(x)


def test2(s):
    for x in t:
        s.add(x)


def test3(s):
    for x in t:
        s.add(x)
        s.remove(x)



if __name__ == "__main__":
    f = LinkedSet(max_size=24)
    s = LinkedSet()
    p = set(s)
    print(timeit(lambda: test1(f), number=10) / 10)
    print(timeit(lambda: test2(s), number=10) / 10)
    print(timeit(lambda: test2(p), number=10) / 10)

    # print(timeit(lambda: test3(p1), number=10) / 10)

