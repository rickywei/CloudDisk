#!/bin/bash

tracker_start(){
    ps aux | grep fdfs_trackerd | grep -v grep > /dev/null
    if [ $? -eq 0 ];then
        echo "fdfs_trackerd has been running..."
    else
        sudo fdfs_trackerd /etc/fdfs/tracker.conf
        if [ $? -ne 0 ];then
            echo "tracker start failed..."
        else
            echo "tracker start successful"
        fi
    fi
}

storage_start(){
    ps aux | grep fdfs_storaged | grep -v grep > /dev/null
    if [ $? -eq 0 ];then
        echo "fdfs_storaged has been runnig..."
    else
        sudo fdfs_storaged /etc/fdfs/storage.conf
        if [ $? -ne 0 ];then
            echo "storage start failed..."
        else
            echo "storage start successful..."
        fi
    fi
}

if [ $# -eq 0 ];then
    echo "Options"
    echo "      start_storage"
    echo "      start_tracker"
    echo "      start"
    echo "      stop"
    exit 0
fi

case $1 in
    start_storage)
        storage_start
        ;;
    start_tracker)
        tracker_start
        ;;
    start)
        tracker_start
        storage_start
        ;;
    stop)
        sudo fdfs_storaged /etc/fdfs/storage.conf stop
        sudo fdfs_trackerd /etc/fdfs/tracker.conf stop
        ;;
    *)
        ;;
esac