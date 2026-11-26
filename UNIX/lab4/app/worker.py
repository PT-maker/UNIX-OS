import json
import os
import socket
import time

import redis

HOST = os.environ.get("REDIS_HOST", "redis")
ME = socket.gethostname()


def connect():
    return redis.Redis(host=HOST, decode_responses=True,
                       socket_connect_timeout=5, health_check_interval=10)


def count_primes(n):
    sieve = bytearray([1]) * (n + 1)
    sieve[0:2] = b"\x00\x00"
    for i in range(2, int(n ** 0.5) + 1):
        if sieve[i]:
            sieve[i * i::i] = bytearray(len(sieve[i * i::i]))
    return sum(sieve)


r = connect()
print("worker started:", ME, flush=True)
while True:
    try:
        item = r.brpop("jobs", timeout=5)
        if item is None:
            continue
        job = json.loads(item[1])
        started = time.time()
        count = count_primes(job["n"])
        r.hset("result:" + job["id"], mapping={
            "status": "done",
            "n": job["n"],
            "primes": count,
            "worker": ME,
            "seconds": round(time.time() - started, 3),
        })
        print("done", job["id"], "n =", job["n"], flush=True)
    except redis.exceptions.RedisError as e:
        print("redis error, retry:", e, flush=True)
        time.sleep(1)
        r = connect()