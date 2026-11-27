import json
import os
import signal
import socket
import threading
import time

import redis

HOST = os.environ.get("REDIS_HOST", "redis")
ME = socket.gethostname()
PROCESSING = "processing:" + ME
ALIVE = "alive:" + ME

stop = False


def on_signal(signum, frame):
    global stop
    stop = True
    print("signal", signum, "received, finishing current task", flush=True)


signal.signal(signal.SIGTERM, on_signal)
signal.signal(signal.SIGINT, on_signal)


def connect():
    return redis.Redis(host=HOST, decode_responses=True,
                       socket_connect_timeout=5, health_check_interval=10)


def heartbeat():
    hb = connect()
    while True:
        try:
            hb.set(ALIVE, 1, ex=15)
        except redis.exceptions.RedisError:
            pass
        time.sleep(5)


def reap(rc):
    for key in rc.scan_iter("processing:*"):
        name = key.split(":", 1)[1]
        if name == ME or rc.exists("alive:" + name):
            continue
        while rc.lmove(key, "jobs", "LEFT", "RIGHT") is not None:
            print("requeued task of dead worker", name, flush=True)


def count_primes(n):
    sieve = bytearray([1]) * (n + 1)
    sieve[0:2] = b"\x00\x00"
    for i in range(2, int(n ** 0.5) + 1):
        if sieve[i]:
            sieve[i * i::i] = bytearray(len(sieve[i * i::i]))
    return sum(sieve)


r = connect()
while True:
    try:
        r.set(ALIVE, 1, ex=15)
        break
    except redis.exceptions.RedisError:
        time.sleep(1)
        r = connect()
threading.Thread(target=heartbeat, daemon=True).start()
print("worker started:", ME, flush=True)

last_reap = 0
while not stop:
    try:
        if time.time() - last_reap > 10:
            reap(r)
            last_reap = time.time()
        raw = r.blmove("jobs", PROCESSING, 5, "RIGHT", "LEFT")
        if raw is None:
            continue
        job = json.loads(raw)
        started = time.time()
        count = count_primes(job["n"])
        r.hset("result:" + job["id"], mapping={
            "status": "done",
            "n": job["n"],
            "primes": count,
            "worker": ME,
            "seconds": round(time.time() - started, 3),
        })
        r.lrem(PROCESSING, 1, raw)
        print("done", job["id"], "n =", job["n"], flush=True)
    except redis.exceptions.RedisError as e:
        print("redis error, retry:", e, flush=True)
        time.sleep(1)
        r = connect()

try:
    r.delete(ALIVE)
except redis.exceptions.RedisError:
    pass
print("worker stopped:", ME, flush=True)