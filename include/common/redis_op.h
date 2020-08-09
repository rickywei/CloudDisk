#ifndef _REDIS_OP_H
#define _REDIS_OP_H

#include <hiredis/hiredis.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "make_log.h"

#define REDIS_LOG_MODULE "database"
#define REDIS_LOG_PROC "redis"

#define REDIS_COMMAND_SIZE 300
#define FIELD_ID_SIZE 100
#define VALUES_ID_SIZE 1024
typedef char (*RCOMMANDS)[REDIS_COMMAND_SIZE];
typedef char (*RFIELDS)[FIELD_ID_SIZE];

typedef char (*RVALUES)[VALUES_ID_SIZE];

redisContext *rop_connectdb_nopwd(char *ip_str, char *port_str);

redisContext *rop_connectdb(char *ip_str, char *port_str, char *pwd);

redisContext *rop_connectdb_unix(char *sock_path, char *pwd);

redisContext *rop_connectdb_timeout(char *ip_str, char *port_str,
                                    struct timeval *timeout);

void rop_disconnect(redisContext *conn);

int rop_selectdatabase(redisContext *conn, unsigned int db_no);

int rop_flush_database(redisContext *conn);

int rop_is_key_exist(redisContext *conn, char *key);

int rop_del_key(redisContext *conn, char *key);

void rop_show_keys(redisContext *conn, char *pattern);

int rop_set_key_lifecycle(redisContext *conn, char *key, time_t delete_time);

int rop_create_or_replace_hash_table(redisContext *conn, char *key,
                                     unsigned int element_num, RFIELDS fields,
                                     RVALUES values);

int rop_hincrement_one_field(redisContext *conn, char *key, char *field,
                             unsigned int num);

int rop_hash_set_append(redisContext *conn, char *key, RFIELDS fields,
                        RVALUES values, int val_num);

int rop_hash_set(redisContext *conn, char *key, char *field, char *value);

int rop_hash_get(redisContext *conn, char *key, char *field, char *value);

int rop_hash_del(redisContext *conn, char *key, char *field);

int rop_list_push_append(redisContext *conn, char *key, RVALUES values,
                         int val_num);

int rop_list_push(redisContext *conn, char *key, char *value);

int rop_get_list_cnt(redisContext *conn, char *key);

int rop_trim_list(redisContext *conn, char *key, int begin, int end);

int rop_range_list(redisContext *conn, char *key, int from_pos, int count,
                   RVALUES values, int *get_num);

int rop_redis_append(redisContext *conn, RCOMMANDS cmds, int cmd_num);

int rop_redis_command(redisContext *conn, char *cmd);

void rop_test_reply_type(redisReply *reply);

int rop_set_string(redisContext *conn, char *key, char *value);

int rop_setex_string(redisContext *conn, char *key, unsigned int seconds,
                     char *value);

int rop_get_string(redisContext *conn, char *key, char *value);

int rop_zset_add(redisContext *conn, char *key, long score, char *member);

int rop_zset_zrem(redisContext *conn, char *key, char *member);

int rop_zset_del_all(redisContext *conn, char *key);

extern int rop_zset_zrevrange(redisContext *conn, char *key, int from_pos,
                              int end_pos, RVALUES values, int *get_num);

int rop_zset_increment(redisContext *conn, char *key, char *member);

int rop_zset_zcard(redisContext *conn, char *key);

int rop_zset_get_score(redisContext *conn, char *key, char *member);

extern int rop_zset_exit(redisContext *conn, char *key, char *member);

int rop_zset_increment_append(redisContext *conn, char *key, RVALUES values,
                              int val_num);

#endif
