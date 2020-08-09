#!/bin/bash

echo "start fastdfs..."
./fdfs.sh stop
./fdfs.sh start

echo "start fastCGI..."
./fcgi.sh stop
./fcgi.sh start

echo "start nginx..."
./nginx.sh stop
./nginx.sh start

echo "start redis..."
./redis.sh stop
./redis.sh start