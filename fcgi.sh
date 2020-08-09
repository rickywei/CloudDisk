#!/bin/bash

if [ $# -eq 0 ];then
    echo "Options"
    echo "      start"
    echo "      stop"
    exit 0
fi

register(){
    sudo spawn-fcgi -a 127.0.0.1 -p 10000 -f ./bin_cgi/register
}

login(){
    sudo spawn-fcgi -a 127.0.0.1 -p 10001 -f ./bin_cgi/login
}

upload(){
    sudo spawn-fcgi -a 127.0.0.1 -p 10002 -f ./bin_cgi/upload
}

md5(){
    sudo spawn-fcgi -a 127.0.0.1 -p 10003 -f ./bin_cgi/md5
}

myfiles(){
    sudo spawn-fcgi -a 127.0.0.1 -p 10004 -f ./bin_cgi/files
}

delete(){
    sudo spawn-fcgi -a 127.0.0.1 -p 10005 -f ./bin_cgi/filesop
}



stop(){
    sudo kill -9 $(ps aux | grep "./bin_cgi/login" | grep -v grep | awk '{print $2}') > /dev/null 2>&1
    sudo kill -9 $(ps aux | grep "./bin_cgi/register" | grep -v grep | awk '{print $2}') > /dev/null 2>&1
    sudo kill -9 $(ps aux | grep "./bin_cgi/upload" | grep -v grep | awk '{print $2}') > /dev/null 2>&1
    sudo kill -9 $(ps aux | grep "./bin_cgi/md5" | grep -v grep | awk '{print $2}') > /dev/null 2>&1
    sudo kill -9 $(ps aux | grep "./bin_cgi/files" | grep -v grep | awk '{print $2}') > /dev/null 2>&1
    sudo kill -9 $(ps aux | grep "./bin_cgi/filesop" | grep -v grep | awk '{print $2}') > /dev/null 2>&1

}

case $1 in
    start)
        register
        login
        upload
        md5
        myfiles
        delete
        ;;
    stop)
        stop
        ;;
    *)
        ;;
esac