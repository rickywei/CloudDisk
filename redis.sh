#!/bin/sh

NAME=redis
FILE=redis_6379.pid

is_directory(){
    if [ ! -d $1 ]; then
        ehco "creating dir $1 ..."
        mkdir $1
        if [ $? -ne 0 ];then
            ehco "creatring dir $1 failed..."
            exit 1
        fi
    fi
}

is_regfile(){
    if [ ! -f $1 ];then
        echo "file $1 does not exist..."
        return 1
    fi
    return 0
}

if [ $# -eq 0 ];then
    echo "options"
    echo "      start"
    echo "      stop"
    echo "      status"
fi

is_directory $NAME

case $1 in
    start)
        ps aux | grep "redis-server" | grep -v grep > /dev/null
        if [ $? -eq 0];then
            echo "redis has been running..."
        else
            unlink "$NAME/$FILE"
            
            echo "redis starting..."
            redis-server ./conf/redis.conf
            if [ $? -eq 0 ];then
                echo "redis started successfully..."
                # wait for pid file
                sleep 1
                if is_regfile "$NAME/$FILE";then
                    printf "****** Redis server PID: [ %s ] ******\n" $(cat "$NAME/$FILE")
                    printf "****** Redis server PORT: [ %s ] ******\n" $(awk '/^port /{print $2}' "./conf/redis.conf")
                fi
            fi
        fi
        ;;
    stop)
        ps aux | grep "redis-server" | grep -v grep > /dev/null
        if [ $? -ne 0 ];then
            echo "redis has not been running..."
            exit 1
        fi
        echo "redis stoping..."
        if is_regfile "$NAME/$FILE";then
            echo "stop by pid file"
            PID=$(cat "$NAME/$FILE")
        else
            echo "stop by find pid of redis"
            PID=$(ps aux | grep "redis-server" | grep -v grep | awk '{print $2}')
        fi
        echo "redis server pid = $PID"
        kill -9 $PID
        if [ $? -ne 0 ];then
            echo "redis stop failed..."
        else
            echo "redis stop successfully..."
        fi
        ;;
    status)
        ps aux | grep "redis-server" | grep -v grep > /dev/null
        if [ $? -eq 0 ];then
            echo "redis is running..."
        else
            echo "redis is not running..."
        fi
        ;;
    *)
        ;;
esac
