#!/bin/sh
docker volume create shared-data
docker build -t concurrent-files .