#!/bin/bash

echo "stop fastdfs..."
./fdfs.sh stop

echo "stop fastCGI..."
./fcgi.sh stop

echo "stop nginx..."
./nginx.sh stop

echo "stop redis..."
./redis.sh stop
