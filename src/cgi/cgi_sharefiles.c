#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#include "cJSON.h"
#include "cfg.h"
#include "cgi_util.h"
#include "fcgi_config.h"
#include "fcgi_stdio.h"
#include "make_log.h"
#include "mysql_op.h"
#include "redis_keys.h"
#include "redis_op.h"

#define SHAREFILES_LOG_MODULE "cgi"
#define SHAREFILES_LOG_PROC "sharefiles"

static char mysql_user[128] = {0};
static char mysql_pwd[128] = {0};
static char mysql_db[128] = {0};

static char redis_ip[30] = {0};
static char redis_port[10] = {0};

void read_cfg() {
    get_cfg_value(CFG_PATH, "mysql", "user", mysql_user);
    get_cfg_value(CFG_PATH, "mysql", "password", mysql_pwd);
    get_cfg_value(CFG_PATH, "mysql", "database", mysql_db);
    LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
        "mysql:[user=%s,pwd=%s,database=%s]", mysql_user, mysql_pwd, mysql_db);

    get_cfg_value(CFG_PATH, "redis", "ip", redis_ip);
    get_cfg_value(CFG_PATH, "redis", "port", redis_port);
    LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "redis:[ip=%s,port=%s]\n",
        redis_ip, redis_port);
}

void get_share_files_count() {
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL *conn = NULL;
    long line = 0;

    conn = msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL) {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "msql_conn err\n");
        goto END;
    }

    mysql_query(conn, "set names utf8");

    sprintf(sql_cmd, "select count from user_file_count where user=\"%s\"",
            "xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    char tmp[512] = {0};
    int ret2 = process_result_one(conn, sql_cmd, tmp);
    if (ret2 != 0) {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s 操作失败\n",
            sql_cmd);
        goto END;
    }

    line = atol(tmp);

END:
    if (conn != NULL) {
        mysql_close(conn);
    }

    LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "line = %ld\n", line);
    printf("%ld", line);
}

int get_fileslist_json_info(char *buf, int *p_start, int *p_count) {
    int ret = 0;

    /*json数据如下
    {
        "start": 0
        "count": 10
    }
    */

    cJSON *root = cJSON_Parse(buf);
    if (NULL == root) {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "cJSON_Parse err\n");
        ret = -1;
        goto END;
    }

    cJSON *child2 = cJSON_GetObjectItem(root, "start");
    if (NULL == child2) {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
            "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    *p_start = child2->valueint;

    cJSON *child3 = cJSON_GetObjectItem(root, "count");
    if (NULL == child3) {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
            "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    *p_count = child3->valueint;

END:
    if (root != NULL) {
        cJSON_Delete(root);
        root = NULL;
    }

    return ret;
}

int get_share_filelist(int start, int count) {
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL *conn = NULL;
    cJSON *root = NULL;
    cJSON *array = NULL;
    char *out = NULL;
    char *out2 = NULL;
    MYSQL_RES *res_set = NULL;

    conn = msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL) {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "msql_conn err\n");
        ret = -1;
        goto END;
    }

    mysql_query(conn, "set names utf8");

    sprintf(sql_cmd,
            "select share_file_list.*, file_info.url, file_info.size, "
            "file_info.type from file_info, share_file_list where "
            "file_info.md5 = share_file_list.md5 limit %d, %d",
            start, count);

    LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s 在操作\n", sql_cmd);

    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s 操作失败: %s\n",
            sql_cmd, mysql_error(conn));
        ret = -1;
        goto END;
    }

    res_set = mysql_store_result(conn);
    if (res_set == NULL) {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
            "smysql_store_result error!\n");
        ret = -1;
        goto END;
    }

    ulong line = 0;

    line = mysql_num_rows(res_set);
    if (line == 0) {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
            "mysql_num_rows(res_set) failed\n");
        ret = -1;
        goto END;
    }

    MYSQL_ROW row;

    root = cJSON_CreateObject();
    array = cJSON_CreateArray();

    while ((row = mysql_fetch_row(res_set)) != NULL) {
        cJSON *item = cJSON_CreateObject();

        /*
        {
        "user": "yoyo",
        "md5": "e8ea6031b779ac26c319ddf949ad9d8d",
        "time": "2017-02-26 21:35:25",
        "filename": "test.mp4",
        "share_status": 1,
        "pv": 0,
        "url":
        "http:
        "size": 27473666,
         "type": "mp4"
        }

        */

        if (row[0] != NULL) {
            cJSON_AddStringToObject(item, "user", row[0]);
        }

        if (row[1] != NULL) {
            cJSON_AddStringToObject(item, "md5", row[1]);
        }

        if (row[2] != NULL) {
            cJSON_AddStringToObject(item, "time", row[2]);
        }

        if (row[3] != NULL) {
            cJSON_AddStringToObject(item, "filename", row[3]);
        }

        cJSON_AddNumberToObject(item, "share_status", 1);

        if (row[4] != NULL) {
            cJSON_AddNumberToObject(item, "pv", atol(row[4]));
        }

        if (row[5] != NULL) {
            cJSON_AddStringToObject(item, "url", row[5]);
        }

        if (row[6] != NULL) {
            cJSON_AddNumberToObject(item, "size", atol(row[6]));
        }

        if (row[7] != NULL) {
            cJSON_AddStringToObject(item, "type", row[7]);
        }

        cJSON_AddItemToArray(array, item);
    }

    cJSON_AddItemToObject(root, "files", array);

    out = cJSON_Print(root);

    LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s\n", out);

END:
    if (ret == 0) {
        printf("%s", out);
    } else {
        out2 = NULL;
        out2 = return_status("015");
    }
    if (out2 != NULL) {
        printf(out2);
        free(out2);
    }

    if (res_set != NULL) {
        mysql_free_result(res_set);
    }

    if (conn != NULL) {
        mysql_close(conn);
    }

    if (root != NULL) {
        cJSON_Delete(root);
    }

    if (out != NULL) {
        free(out);
    }

    return ret;
}

int get_ranking_filelist(int start, int count) {
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL *conn = NULL;
    cJSON *root = NULL;
    RVALUES value = NULL;
    cJSON *array = NULL;
    char *out = NULL;
    char *out2 = NULL;
    char tmp[512] = {0};
    int ret2 = 0;
    MYSQL_RES *res_set = NULL;
    redisContext *redis_conn = NULL;

    redis_conn = rop_connectdb_nopwd(redis_ip, redis_port);
    if (redis_conn == NULL) {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
            "redis connected error");
        ret = -1;
        goto END;
    }

    conn = msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL) {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "msql_conn err\n");
        ret = -1;
        goto END;
    }

    mysql_query(conn, "set names utf8");

    sprintf(sql_cmd, "select count from user_file_count where user=\"%s\"",
            "xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    ret2 = process_result_one(conn, sql_cmd, tmp);
    if (ret2 != 0) {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s 操作失败\n",
            sql_cmd);
        ret = -1;
        goto END;
    }

    int sql_num = atoi(tmp);

    int redis_num = rop_zset_zcard(redis_conn, FILE_PUBLIC_ZSET);
    if (redis_num == -1) {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
            "rop_zset_zcard 操作失败\n");
        ret = -1;
        goto END;
    }

    LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
        "sql_num = %d, redis_num = %d\n", sql_num, redis_num);

    if (redis_num != sql_num) {
        rop_del_key(redis_conn, FILE_PUBLIC_ZSET);
        rop_del_key(redis_conn, FILE_NAME_HASH);

        strcpy(
            sql_cmd,
            "select md5, filename, pv from share_file_list order by pv desc");

        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s 在操作\n", sql_cmd);

        if (mysql_query(conn, sql_cmd) != 0) {
            LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s 操作失败: %s\n",
                sql_cmd, mysql_error(conn));
            ret = -1;
            goto END;
        }

        res_set = mysql_store_result(conn);
        if (res_set == NULL) {
            LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
                "smysql_store_result error!\n");
            ret = -1;
            goto END;
        }

        ulong line = 0;

        line = mysql_num_rows(res_set);
        if (line == 0) {
            LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
                "mysql_num_rows(res_set) failed\n");
            ret = -1;
            goto END;
        }

        MYSQL_ROW row;

        while ((row = mysql_fetch_row(res_set)) != NULL) {
            if (row[0] == NULL || row[1] == NULL || row[2] == NULL) {
                LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
                    "mysql_fetch_row(res_set)) failed\n");
                ret = -1;
                goto END;
            }

            char fileid[1024] = {0};
            sprintf(fileid, "%s%s", row[0], row[1]);

            rop_zset_add(redis_conn, FILE_PUBLIC_ZSET, atoi(row[2]), fileid);

            rop_hash_set(redis_conn, FILE_NAME_HASH, fileid, row[1]);
        }
    }

    value = (RVALUES)calloc(count, VALUES_ID_SIZE);
    if (value == NULL) {
        ret = -1;
        goto END;
    }

    int n = 0;
    int end = start + count - 1;

    ret =
        rop_zset_zrevrange(redis_conn, FILE_PUBLIC_ZSET, start, end, value, &n);
    if (ret != 0) {
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
            "rop_zset_zrevrange 操作失败\n");
        goto END;
    }

    root = cJSON_CreateObject();
    array = cJSON_CreateArray();

    for (int i = 0; i < n; ++i) {
        cJSON *item = cJSON_CreateObject();

        /*
        {
            "filename": "test.mp4",
            "pv": 0
        }
        */

        char filename[1024] = {0};
        ret = rop_hash_get(redis_conn, FILE_NAME_HASH, value[i], filename);
        if (ret != 0) {
            LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
                "rop_hash_get 操作失败\n");
            ret = -1;
            goto END;
        }
        cJSON_AddStringToObject(item, "filename", filename);

        int score = rop_zset_get_score(redis_conn, FILE_PUBLIC_ZSET, value[i]);
        if (score == -1) {
            LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
                "rop_zset_get_score 操作失败\n");
            ret = -1;
            goto END;
        }
        cJSON_AddNumberToObject(item, "pv", score);

        cJSON_AddItemToArray(array, item);
    }

    cJSON_AddItemToObject(root, "files", array);

    out = cJSON_Print(root);

    LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "%s\n", out);

END:
    if (ret == 0) {
        printf("%s", out);
    } else {
        out2 = NULL;
        out2 = return_status("015");
    }
    if (out2 != NULL) {
        printf(out2);
        free(out2);
    }

    if (res_set != NULL) {
        mysql_free_result(res_set);
    }

    if (redis_conn != NULL) {
        rop_disconnect(redis_conn);
    }

    if (conn != NULL) {
        mysql_close(conn);
    }

    if (value != NULL) {
        free(value);
    }

    if (root != NULL) {
        cJSON_Delete(root);
    }

    if (out != NULL) {
        free(out);
    }

    return ret;
}

int main() {
    char cmd[20];

    read_cfg();

    while (FCGI_Accept() >= 0) {
        char *query = getenv("QUERY_STRING");

        query_parse_key_value(query, "cmd", cmd, NULL);
        LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "cmd = %s\n", cmd);

        printf("Content-type: text/html\r\n\r\n");

        if (strcmp(cmd, "count") == 0) {
            get_share_files_count();
        } else {
            char *contentLength = getenv("CONTENT_LENGTH");
            int len;

            if (contentLength == NULL) {
                len = 0;
            } else {
                len = atoi(contentLength);
            }

            if (len <= 0) {
                printf("No data from standard input.<p>\n");
                LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
                    "len = 0, No data from standard input\n");
            } else {
                char buf[4 * 1024] = {0};
                int ret = 0;
                ret = fread(buf, 1, len, stdin);
                if (ret == 0) {
                    LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
                        "fread(buf, 1, len, stdin) err\n");
                    continue;
                }

                LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC, "buf = %s\n",
                    buf);

                int start;
                int count;
                get_fileslist_json_info(buf, &start, &count);
                LOG(SHAREFILES_LOG_MODULE, SHAREFILES_LOG_PROC,
                    "start = %d, count = %d\n", start, count);
                if (strcmp(cmd, "normal") == 0) {
                    get_share_filelist(start, count);
                } else if (strcmp(cmd, "pvdesc") == 0) {
                    get_ranking_filelist(start, count);
                }
            }
        }
    }

    return 0;
}
