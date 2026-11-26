import json
import os
import socket
import uuid

import redis
from flask import Flask, jsonify, request

app = Flask(__name__)
r = redis.Redis(host=os.environ.get("REDIS_HOST", "redis"), decode_responses=True)
ME = socket.gethostname()


@app.get("/whoami")
def whoami():
    return jsonify(api=ME)


@app.post("/jobs")
def create_job():
    n = int(request.args.get("n", "1000000"))
    job_id = str(uuid.uuid4())
    r.lpush("jobs", json.dumps({"id": job_id, "n": n}))
    return jsonify(id=job_id, accepted_by=ME)


@app.get("/jobs/<job_id>")
def get_job(job_id):
    res = r.hgetall("result:" + job_id)
    if not res:
        return jsonify(status="pending", api=ME)
    res["api"] = ME
    return jsonify(res)


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)