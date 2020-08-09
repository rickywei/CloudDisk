#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "cJSON.h"
#include "cfg.h"
#include "cgi_util.h"
#include "fcgi_config.h"
#include "fcgi_stdio.h"
#include "make_log.h"
#include "mysql_op.h"
#include "redis_keys.h"
#include "redis_op.h"

#define DEALFILE_LOG_MODULE "cgi"
#define DEALFILE_LOG_PROC "dealfile"

static char mysql_user[128] = {0};
static char mysql_pwd[128] = {0};
static char mysql_db[128] = {0};

static char redis_ip[30] = {0};
static char redis_port[10] = {0};

void read_cfg() {
    get_cfg_value(CFG_PATH, "mysql", "user", mysql_user);
    get_cfg_value(CFG_PATH, "mysql", "password", mysql_pwd);
    get_cfg_value(CFG_PATH, "mysql", "database", mysql_db);
    LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC,
        "mysql:[user=%s,pwd=%s,database=%s]", mysql_user, mysql_pwd, mysql_db);

    get_cfg_value(CFG_PATH, "redis", "ip", redis_ip);
    get_cfg_value(CFG_PATH, "redis", "port", redis_port);
    LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "redis:[ip=%s,port=%s]\n",
        redis_ip, redis_port);
}

int get_json_info(char *buf, char *user, char *token, char *md5,
                  char *filename) {
    int ret = 0;

    cJSON *root = cJSON_Parse(buf);
    if (NULL == root) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "cJSON_Parse err\n");
        ret = -1;
        goto END;
    }

    cJSON *child1 = cJSON_GetObjectItem(root, "user");
    if (NULL == child1) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC,
            "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    strcpy(user, child1->valuestring);

    cJSON *child2 = cJSON_GetObjectItem(root, "md5");
    if (NULL == child2) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC,
            "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    strcpy(md5, child2->valuestring);

    cJSON *child3 = cJSON_GetObjectItem(root, "filename");
    if (NULL == child3) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC,
            "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    strcpy(filename, child3->valuestring);

    cJSON *child4 = cJSON_GetObjectItem(root, "token");
    if (NULL == child4) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC,
            "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    strcpy(token, child4->valuestring);

END:
    if (root != NULL) {
        cJSON_Delete(root);
        root = NULL;
    }

    return ret;
}

int share_file(char *user, char *md5, char *filename) {
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL *conn = NULL;
    redisContext *redis_conn = NULL;
    char *out = NULL;
    char tmp[512] = {0};
    char fileid[1024] = {0};
    int ret2 = 0;

    redis_conn = rop_connectdb_nopwd(redis_ip, redis_port);
    if (redis_conn == NULL) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "redis connected error");
        ret = -1;
        goto END;
    }

    conn = msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "msql_conn err\n");
        ret = -1;
        goto END;
    }

    mysql_query(conn, "set names utf8");

    sprintf(fileid, "%s%s", md5, filename);

    ret2 = rop_zset_exit(redis_conn, FILE_PUBLIC_ZSET, fileid);
    if (ret2 == 1) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "[%s] has been shared\n",
            fileid);
        ret = -2;
        goto END;
    } else if (ret2 == 0) {
        sprintf(sql_cmd,
                "select * from share_file_list where md5 = '%s' and filename = "
                "'%s'",
                md5, filename);
        ret2 = process_result_one(conn, sql_cmd, NULL);
        if (ret2 == 2) {
            rop_zset_add(redis_conn, FILE_PUBLIC_ZSET, 0, fileid);

            LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC,
                "[%s] has been shared\n", fileid);
            ret = -2;
            goto END;
        }
    } else {
        ret = -1;
        goto END;
    }

    sprintf(sql_cmd,
            "update user_file_list set shared_status = 1 where user = '%s' and "
            "md5 = '%s' and filename = '%s'",
            user, md5, filename);

    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed: %s\n",
            sql_cmd, mysql_error(conn));
        ret = -1;
        goto END;
    }

    time_t now;
    ;
    char create_time[TIME_STRING_LEN];
    now = time(NULL);
    strftime(create_time, TIME_STRING_LEN - 1, "%Y-%m-%d %H:%M:%S",
             localtime(&now));

    sprintf(sql_cmd,
            "insert into share_file_list (user, md5, createtime, filename, pv) "
            "values ('%s', '%s', '%s', '%s', %d)",
            user, md5, create_time, filename, 0);
    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed: %s\n",
            sql_cmd, mysql_error(conn));
        ret = -1;
        goto END;
    }

    sprintf(sql_cmd, "select count from user_file_count where user = '%s'",
            "xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    int count = 0;
    ret2 = process_result_one(conn, sql_cmd, tmp);
    if (ret2 == 1) {
        sprintf(sql_cmd,
                "insert into user_file_count (user, count) values('%s', %d)",
                "xxx_share_xxx_file_xxx_list_xxx_count_xxx", 1);
    } else if (ret2 == 0) {
        count = atoi(tmp);
        sprintf(sql_cmd,
                "update user_file_count set count = %d where user = '%s'",
                count + 1, "xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    }

    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed: %s\n",
            sql_cmd, mysql_error(conn));
        ret = -1;
        goto END;
    }

    rop_zset_add(redis_conn, FILE_PUBLIC_ZSET, 0, fileid);

    rop_hash_set(redis_conn, FILE_NAME_HASH, fileid, filename);

END:
    /*
   share files:
        success{"code":"010"}
        failed{"code":"011"}
        has been shared{"code", "012"}
    */
    out = NULL;
    if (ret == 0) {
        out = return_status("010");
    } else if (ret == -1) {
        out = return_status("011");
    } else if (ret == -2) {
        out = return_status("012");
    }

    if (out != NULL) {
        printf(out);
        free(out);
    }

    if (redis_conn != NULL) {
        rop_disconnect(redis_conn);
    }

    if (conn != NULL) {
        mysql_close(conn);
    }

    return ret;
}

int remove_file_from_storage(char *fileid) {
    int ret = 0;

    char fdfs_cli_conf_path[256] = {0};
    get_cfg_value(CFG_PATH, "dfs_path", "client", fdfs_cli_conf_path);

    char cmd[1024 * 2] = {0};
    sprintf(cmd, "fdfs_delete_file %s %s", fdfs_cli_conf_path, fileid);

    ret = system(cmd);
    LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC,
        "remove_file_from_storage ret = %d\n", ret);

    return ret;
}

int del_file(char *user, char *md5, char *filename) {
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL *conn = NULL;
    redisContext *redis_conn = NULL;
    char *out = NULL;
    char tmp[512] = {0};
    char fileid[1024] = {0};
    int ret2 = 0;
    int count = 0;
    int share = 0;
    int flag = 0;

    redis_conn = rop_connectdb_nopwd(redis_ip, redis_port);
    if (redis_conn == NULL) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "redis connected error");
        ret = -1;
        goto END;
    }

    conn = msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "msql_conn err\n");
        ret = -1;
        goto END;
    }

    mysql_query(conn, "set names utf8");

    sprintf(fileid, "%s%s", md5, filename);

    ret2 = rop_zset_exit(redis_conn, FILE_PUBLIC_ZSET, fileid);
    if (ret2 == 1) {
        share = 1;
        flag = 1;
    } else if (ret2 == 0) {
        sprintf(sql_cmd,
                "select shared_status from user_file_list where user = '%s' "
                "and md5 = '%s' and filename = '%s'",
                user, md5, filename);

        ret2 = process_result_one(conn, sql_cmd, tmp);
        if (ret2 == 0) {
            share = atoi(tmp);
        } else if (ret2 == -1) {
            LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed\n",
                sql_cmd);
            ret = -1;
            goto END;
        }
    } else {
        ret = -1;
        goto END;
    }

    if (share == 1) {
        sprintf(sql_cmd,
                "delete from share_file_list where user = '%s' and md5 = '%s' "
                "and filename = '%s'",
                user, md5, filename);

        if (mysql_query(conn, sql_cmd) != 0) {
            LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed: %s\n",
                sql_cmd, mysql_error(conn));
            ret = -1;
            goto END;
        }

        sprintf(sql_cmd, "select count from user_file_count where user = '%s'",
                "xxx_share_xxx_file_xxx_list_xxx_count_xxx");

        ret2 = process_result_one(conn, sql_cmd, tmp);
        if (ret2 != 0) {
            LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed\n",
                sql_cmd);
            ret = -1;
            goto END;
        }

        count = atoi(tmp);

        sprintf(sql_cmd,
                "update user_file_count set count = %d where user = '%s'",
                count - 1, "xxx_share_xxx_file_xxx_list_xxx_count_xxx");
        if (mysql_query(conn, sql_cmd) != 0) {
            LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed: %s\n",
                sql_cmd, mysql_error(conn));
            ret = -1;
            goto END;
        }

        if (1 == flag) {
            rop_zset_zrem(redis_conn, FILE_PUBLIC_ZSET, fileid);

            rop_hash_del(redis_conn, FILE_NAME_HASH, fileid);
        }
    }

    sprintf(sql_cmd, "select count from user_file_count where user = '%s'",
            user);
    ret2 = process_result_one(conn, sql_cmd, tmp);
    if (ret2 != 0) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed\n",
            sql_cmd);
        ret = -1;
        goto END;
    }

    count = atoi(tmp);

    if (count == 1) {
        sprintf(sql_cmd, "delete from user_file_count where user = '%s'", user);
    } else {
        sprintf(sql_cmd,
                "update user_file_count set count = %d where user = '%s'",
                count - 1, user);
    }

    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed: %s\n",
            sql_cmd, mysql_error(conn));
        ret = -1;
        goto END;
    }

    sprintf(sql_cmd,
            "delete from user_file_list where user = '%s' and md5 = '%s' and "
            "filename = '%s'",
            user, md5, filename);

    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed: %s\n",
            sql_cmd, mysql_error(conn));
        ret = -1;
        goto END;
    }

    sprintf(sql_cmd, "select count from file_info where md5 = '%s'", md5);
    ret2 = process_result_one(conn, sql_cmd, tmp);
    if (ret2 == 0) {
        count = atoi(tmp);
    } else {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed\n",
            sql_cmd);
        ret = -1;
        goto END;
    }

    count--;
    sprintf(sql_cmd, "update file_info set count=%d where md5 = '%s'", count,
            md5);
    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed: %s\n",
            sql_cmd, mysql_error(conn));
        ret = -1;
        goto END;
    }

    if (count == 0) {
        sprintf(sql_cmd, "select file_id from file_info where md5 = '%s'", md5);
        ret2 = process_result_one(conn, sql_cmd, tmp);
        if (ret2 != 0) {
            LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed\n",
                sql_cmd);
            ret = -1;
            goto END;
        }

        sprintf(sql_cmd, "delete from file_info where md5 = '%s'", md5);
        if (mysql_query(conn, sql_cmd) != 0) {
            LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed: %s\n",
                sql_cmd, mysql_error(conn));
            ret = -1;
            goto END;
        }

        ret2 = remove_file_from_storage(tmp);
        if (ret2 != 0) {
            LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC,
                "remove_file_from_storage err\n");
            ret = -1;
            goto END;
        }
    }

END:
    /*
    delete file
        success{"code":"013"}
        failed{"code":"014"}
    */
    out = NULL;
    if (ret == 0) {
        out = return_status("013");
    } else {
        out = return_status("014");
    }

    if (out != NULL) {
        printf(out);
        free(out);
    }

    if (redis_conn != NULL) {
        rop_disconnect(redis_conn);
    }

    if (conn != NULL) {
        mysql_close(conn);
    }

    return ret;
}

int pv_file(char *user, char *md5, char *filename) {
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL *conn = NULL;
    char *out = NULL;
    char tmp[512] = {0};
    int ret2 = 0;

    conn = msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "msql_conn err\n");
        ret = -1;
        goto END;
    }

    mysql_query(conn, "set names utf8");

    sprintf(sql_cmd,
            "select pv from user_file_list where user = '%s' and md5 = '%s' "
            "and filename = '%s'",
            user, md5, filename);

    ret2 = process_result_one(conn, sql_cmd, tmp);
    int pv = 0;
    if (ret2 == 0) {
        pv = atoi(tmp);
    } else {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed\n",
            sql_cmd);
        ret = -1;
        goto END;
    }

    sprintf(sql_cmd,
            "update user_file_list set pv = %d where user = '%s' and md5 = "
            "'%s' and filename = '%s'",
            pv + 1, user, md5, filename);

    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "%s mysql failed: %s\n",
            sql_cmd, mysql_error(conn));
        ret = -1;
        goto END;
    }

END:
    /*
    pv
        success{"code":"016"}
        failed{"code":"017"}
    */
    out = NULL;
    if (ret == 0) {
        out = return_status("016");
    } else {
        out = return_status("017");
    }

    if (out != NULL) {
        printf(out);
        free(out);
    }

    if (conn != NULL) {
        mysql_close(conn);
    }

    return ret;
}

int main() {
    // char cmd[20];
    char user[USER_NAME_LEN];
    char token[TOKEN_LEN];
    char md5[MD5_LEN];
    char filename[FILE_NAME_LEN];

    read_cfg();

    while (FCGI_Accept() >= 0) {
        // char *query = getenv("QUERY_STRING");

        // query_parse_key_value(query, "cmd", cmd, NULL);
        // LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "cmd = %s\n", cmd);

        char *contentLength = getenv("CONTENT_LENGTH");
        int len;

        printf("Content-type: text/html\r\n\r\n");

        if (contentLength == NULL) {
            len = 0;
        } else {
            len = atoi(contentLength);
        }

        if (len <= 0) {
            printf("No data from standard input.<p>\n");
            LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC,
                "len = 0, No data from standard input\n");
        } else {
            char buf[4 * 1024] = {0};
            int ret = 0;
            ret = fread(buf, 1, len, stdin);
            if (ret == 0) {
                LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC,
                    "fread(buf, 1, len, stdin) err\n");
                continue;
            }

            LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC, "buf = %s\n", buf);

            get_json_info(buf, user, token, md5, filename);
            LOG(DEALFILE_LOG_MODULE, DEALFILE_LOG_PROC,
                "user = %s, token = %s, md5 = %s, filename = %s\n", user, token,
                md5, filename);

            ret = verify_token(user, token);
            if (ret != 0) {
                char *out = return_status("111");
                if (out != NULL) {
                    printf(out);
                    free(out);
                }
                continue;
            }

            del_file(user, md5, filename);
            // if (strcmp(cmd, "share") == 0) {
            //     share_file(user, md5, filename);
            // } else if (strcmp(cmd, "del") == 0) {
            //     del_file(user, md5, filename);
            // } else if (strcmp(cmd, "pv") == 0) {
            //     pv_file(user, md5, filename);
            // }
        }
    }

    return 0;
}
