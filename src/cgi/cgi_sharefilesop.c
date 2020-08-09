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

#define DEALSHAREFILE_LOG_MODULE "cgi"
#define DEALSHAREFILE_LOG_PROC "dealsharefile"

static char mysql_user[128] = {0};
static char mysql_pwd[128] = {0};
static char mysql_db[128] = {0};

static char redis_ip[30] = {0};
static char redis_port[10] = {0};

void read_cfg() {
    get_cfg_value(CFG_PATH, "mysql", "user", mysql_user);
    get_cfg_value(CFG_PATH, "mysql", "password", mysql_pwd);
    get_cfg_value(CFG_PATH, "mysql", "database", mysql_db);
    LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
        "mysql:[user=%s,pwd=%s,database=%s]", mysql_user, mysql_pwd, mysql_db);

    get_cfg_value(CFG_PATH, "redis", "ip", redis_ip);
    get_cfg_value(CFG_PATH, "redis", "port", redis_port);
    LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
        "redis:[ip=%s,port=%s]\n", redis_ip, redis_port);
}

int get_json_info(char *buf, char *user, char *md5, char *filename) {
    int ret = 0;

    /*
    {
    "user": "yoyo",
    "md5": "xxx",
    "filename": "xxx"
    }
    */

    cJSON *root = cJSON_Parse(buf);
    if (NULL == root) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
            "cJSON_Parse err\n");
        ret = -1;
        goto END;
    }

    cJSON *child1 = cJSON_GetObjectItem(root, "user");
    if (NULL == child1) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
            "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    strcpy(user, child1->valuestring);

    cJSON *child2 = cJSON_GetObjectItem(root, "md5");
    if (NULL == child2) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
            "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    strcpy(md5, child2->valuestring);

    cJSON *child3 = cJSON_GetObjectItem(root, "filename");
    if (NULL == child3) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
            "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    strcpy(filename, child3->valuestring);

END:
    if (root != NULL) {
        cJSON_Delete(root);
        root = NULL;
    }

    return ret;
}

int pv_file(char *md5, char *filename) {
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL *conn = NULL;
    char *out = NULL;
    char tmp[512] = {0};
    int ret2 = 0;
    redisContext *redis_conn = NULL;
    char fileid[1024] = {0};

    redis_conn = rop_connectdb_nopwd(redis_ip, redis_port);
    if (redis_conn == NULL) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
            "redis connected error");
        ret = -1;
        goto END;
    }

    sprintf(fileid, "%s%s", md5, filename);

    conn = msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
            "msql_conn err\n");
        ret = -1;
        goto END;
    }

    mysql_query(conn, "set names utf8");

    sprintf(
        sql_cmd,
        "select pv from share_file_list where md5 = '%s' and filename = '%s'",
        md5, filename);

    ret2 = process_result_one(conn, sql_cmd, tmp);
    int pv = 0;
    if (ret2 == 0) {
        pv = atoi(tmp);
    } else {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n",
            sql_cmd);
        ret = -1;
        goto END;
    }

    sprintf(sql_cmd,
            "update share_file_list set pv = %d where md5 = '%s' and filename "
            "= '%s'",
            pv + 1, md5, filename);

    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n",
            sql_cmd);
        ret = -1;
        goto END;
    }

    ret2 = rop_zset_exit(redis_conn, FILE_PUBLIC_ZSET, fileid);
    if (ret2 == 1) {
        ret = rop_zset_increment(redis_conn, FILE_PUBLIC_ZSET, fileid);
        if (ret != 0) {
            LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
                "rop_zset_increment 操作失败\n");
        }
    } else if (ret2 == 0) {
        rop_zset_add(redis_conn, FILE_PUBLIC_ZSET, pv + 1, fileid);

        rop_hash_set(redis_conn, FILE_NAME_HASH, fileid, filename);

    } else {
        ret = -1;
        goto END;
    }

END:
    /*
    pv(download):
        success: {"code":"016"}
        failed: {"code":"017"}
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

    if (redis_conn != NULL) {
        rop_disconnect(redis_conn);
    }

    if (conn != NULL) {
        mysql_close(conn);
    }

    return ret;
}

int cancel_share_file(char *user, char *md5, char *filename) {
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL *conn = NULL;
    redisContext *redis_conn = NULL;
    char *out = NULL;
    char fileid[1024] = {0};

    redis_conn = rop_connectdb_nopwd(redis_ip, redis_port);
    if (redis_conn == NULL) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
            "redis connected error");
        ret = -1;
        goto END;
    }

    conn = msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
            "msql_conn err\n");
        ret = -1;
        goto END;
    }

    mysql_query(conn, "set names utf8");

    sprintf(fileid, "%s%s", md5, filename);

    sprintf(sql_cmd,
            "update user_file_list set shared_status = 0 where user = '%s' and "
            "md5 = '%s' and filename = '%s'",
            user, md5, filename);

    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n",
            sql_cmd);
        ret = -1;
        goto END;
    }

    sprintf(sql_cmd, "select count from user_file_count where user = '%s'",
            "xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    int count = 0;
    char tmp[512] = {0};
    int ret2 = process_result_one(conn, sql_cmd, tmp);
    if (ret2 != 0) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n",
            sql_cmd);
        ret = -1;
        goto END;
    }

    count = atoi(tmp);
    if (count == 1) {
        sprintf(sql_cmd, "delete from user_file_count where user = '%s'",
                "xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    } else {
        sprintf(sql_cmd,
                "update user_file_count set count = %d where user = '%s'",
                count - 1, "xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    }

    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n",
            sql_cmd);
        ret = -1;
        goto END;
    }

    sprintf(sql_cmd,
            "delete from share_file_list where user = '%s' and md5 = '%s' and "
            "filename = '%s'",
            user, md5, filename);
    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n",
            sql_cmd);
        ret = -1;
        goto END;
    }

    ret = rop_zset_zrem(redis_conn, FILE_PUBLIC_ZSET, fileid);
    if (ret != 0) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
            "rop_zset_zrem 操作失败\n");
        goto END;
    }

    ret = rop_hash_del(redis_conn, FILE_NAME_HASH, fileid);
    if (ret != 0) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
            "rop_hash_del 操作失败\n");
        goto END;
    }

END:
    /*
    unshare：
        success: {"code":"018"}
        failed: {"code":"019"}
    */
    out = NULL;
    if (ret == 0) {
        out = return_status("018");
    } else {
        out = return_status("019");
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

int save_file(char *user, char *md5, char *filename) {
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL *conn = NULL;
    char *out = NULL;
    int ret2 = 0;
    char tmp[512] = {0};

    conn = msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
            "msql_conn err\n");
        ret = -1;
        goto END;
    }

    mysql_query(conn, "set names utf8");

    sprintf(sql_cmd,
            "select * from user_file_list where user = '%s' and md5 = '%s' and "
            "filename = '%s'",
            user, md5, filename);

    ret2 = process_result_one(conn, sql_cmd, NULL);
    if (ret2 == 2) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
            "%s[filename:%s, md5:%s]已存在\n", user, filename, md5);
        ret = -2;
        goto END;
    }

    sprintf(sql_cmd, "select count from file_info where md5 = '%s'", md5);
    ret2 = process_result_one(conn, sql_cmd, tmp);
    if (ret2 != 0) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n",
            sql_cmd);
        ret = -1;
        goto END;
    }

    int count = atoi(tmp);

    sprintf(sql_cmd, "update file_info set count = %d where md5 = '%s'",
            count + 1, md5);
    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n",
            sql_cmd);
        ret = -1;
        goto END;
    }

    struct timeval tv;
    struct tm *ptm;
    char time_str[128];

    gettimeofday(&tv, NULL);
    ptm = localtime(&tv.tv_sec);

    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", ptm);

    sprintf(sql_cmd,
            "insert into user_file_list(user, md5, createtime, filename, "
            "shared_status, pv) values ('%s', '%s', '%s', '%s', %d, %d)",
            user, md5, time_str, filename, 0, 0);
    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n",
            sql_cmd);
        ret = -1;
        goto END;
    }

    sprintf(sql_cmd, "select count from user_file_count where user = '%s'",
            user);
    count = 0;

    ret2 = process_result_one(conn, sql_cmd, tmp);
    if (ret2 == 1) {
        sprintf(sql_cmd,
                " insert into user_file_count (user, count) values('%s', %d)",
                user, 1);
    } else if (ret2 == 0) {
        count = atoi(tmp);
        sprintf(sql_cmd,
                "update user_file_count set count = %d where user = '%s'",
                count + 1, user);
    }

    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "%s 操作失败\n",
            sql_cmd);
        ret = -1;
        goto END;
    }

END:
    /*
    return ：0 success，-1 save failed，-2 file has existed
    save:
        success: {"code":"020"}
        file existed: {"code":"021"}
        failed: {"code":"022"}
    */
    out = NULL;
    if (ret == 0) {
        out = return_status("020");
    } else if (ret == -1) {
        out = return_status("022");
    } else if (ret == -2) {
        out = return_status("021");
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
    char cmd[20];
    char user[USER_NAME_LEN];
    char md5[MD5_LEN];
    char filename[FILE_NAME_LEN];

    read_cfg();

    while (FCGI_Accept() >= 0) {
        char *query = getenv("QUERY_STRING");

        query_parse_key_value(query, "cmd", cmd, NULL);
        LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "cmd = %s\n",
            cmd);

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
            LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
                "len = 0, No data from standard input\n");
        } else {
            char buf[4 * 1024] = {0};
            int ret = 0;
            ret = fread(buf, 1, len, stdin);
            if (ret == 0) {
                LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
                    "fread(buf, 1, len, stdin) err\n");
                continue;
            }

            LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "buf = %s\n",
                buf);

            get_json_info(buf, user, md5, filename);
            LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC,
                "user = %s, md5 = %s, filename = %s\n", user, md5, filename);

            if (strcmp(cmd, "pv") == 0) {
                pv_file(md5, filename);
            } else if (strcmp(cmd, "cancel") == 0) {
                cancel_share_file(user, md5, filename);
            } else if (strcmp(cmd, "save") == 0) {
                save_file(user, md5, filename);
            }
        }
    }

    return 0;
}
