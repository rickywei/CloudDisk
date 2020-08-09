#!/bin/bash

if [ $# -eq 0 ];then
    echo "Options"
    echo "      start"
    echo "      stop"
    echo "      reload"
fi

case $1 in
    start)
        sudo /usr/local/nginx/sbin/nginx
        if [ $? -eq 0 ];then
            echo "nginx start successful..."
        else
            echo "nginx start failed..."
        fi
        ;;
    stop)
        sudo /usr/local/nginx/sbin/nginx -s quit
        if [ $? -eq 0 ];then
            echo "nginx stop successful..."
        else
            echo "nginx stop failed..."
        fi
        ;;
    reload)
        sudo /usr/local/nginx/sbin/nginx -s reload
        if [ $? -eq 0 ];then
            echo "nginx reload successful..."
        else
            echo "nginx relaod failed..."
        fi
        ;;
    *)
    ;;
esac