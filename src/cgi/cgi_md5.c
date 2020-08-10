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

#define MD5_LOG_MODULE "cgi"
#define MD5_LOG_PROC "md5"

static char mysql_user[128] = {0};
static char mysql_pwd[128] = {0};
static char mysql_db[128] = {0};

// redis
// static char redis_ip[30] = {0};
// static char redis_port[10] = {0};

// read config
void read_cfg() {
    get_cfg_value(CFG_PATH, "mysql", "user", mysql_user);
    get_cfg_value(CFG_PATH, "mysql", "password", mysql_pwd);
    get_cfg_value(CFG_PATH, "mysql", "database", mysql_db);
    LOG(MD5_LOG_MODULE, MD5_LOG_PROC, "mysql:[user=%s,pwd=%s,database=%s]",
        mysql_user, mysql_pwd, mysql_db);

    // redis
    // get_cfg_value(CFG_PATH, "redis", "ip", redis_ip);
    // get_cfg_value(CFG_PATH, "redis", "port", redis_port);
    // LOG(MD5_LOG_MODULE, MD5_LOG_PROC, "redis:[ip=%s,port=%s]\n", redis_ip,
    // redis_port);
}

// get md5 info
int get_md5_info(char *buf, char *user, char *token, char *md5,
                 char *filename) {
    int ret = 0;

    //   {
    //     user:xxxx,
    //     token: xxxx,
    //     md5:xxx,
    //     fileName: xxx
    //    }

    cJSON *root = cJSON_Parse(buf);
    if (NULL == root) {
        LOG(MD5_LOG_MODULE, MD5_LOG_PROC, "cJSON_Parse err\n");
        ret = -1;
        goto END;
    }

    // user
    cJSON *child1 = cJSON_GetObjectItem(root, "user");
    if (NULL == child1) {
        LOG(MD5_LOG_MODULE, MD5_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    // LOG(MD5_LOG_MODULE, MD5_LOG_PROC, "child1->valuestring = %s\n",
    // child1->valuestring);
    strcpy(user, child1->valuestring);

    // MD5
    cJSON *child2 = cJSON_GetObjectItem(root, "md5");
    if (NULL == child2) {
        LOG(MD5_LOG_MODULE, MD5_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }
    strcpy(md5, child2->valuestring);

    // filename
    cJSON *child3 = cJSON_GetObjectItem(root, "fileName");
    if (NULL == child3) {
        LOG(MD5_LOG_MODULE, MD5_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }
    strcpy(filename, child3->valuestring);

    // token
    cJSON *child4 = cJSON_GetObjectItem(root, "token");
    if (NULL == child4) {
        LOG(MD5_LOG_MODULE, MD5_LOG_PROC, "cJSON_GetObjectItem err\n");
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

// 1second upload
// 0 success 1 failed 2 use has this file already
int deal_md5(char *user, char *md5, char *filename) {
    int ret = 0;
    MYSQL *conn = NULL;
    int ret2 = 0;
    char tmp[512] = {0};
    char sql_cmd[SQL_MAX_LEN] = {0};
    char *out = NULL;

    // connect the database
    conn = msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL) {
        LOG(MD5_LOG_MODULE, MD5_LOG_PROC, "msql_conn err\n");
        ret = -1;
        goto END;
    }

    mysql_query(conn, "set names utf8");

    // file exists: {"code":"004"}
    // success:  {"code":"005"}
    // failed:  {"code":"006"}

    // check mysql -> has this md5 ?
    sprintf(sql_cmd, "select count from file_info where md5 = '%s'", md5);

    ret2 = process_result_one(conn, sql_cmd, tmp);

    if (ret2 == 0) {
        // has
        int count = atoi(tmp);

        // check if user has this file
        sprintf(sql_cmd,
                "select * from user_file_list where user = '%s' and md5 = '%s' "
                "and filename = '%s'",
                user, md5, filename);

        ret2 = process_result_one(conn, sql_cmd, NULL);
        if (ret2 == 2) {
            // user has this file
            LOG(MD5_LOG_MODULE, MD5_LOG_PROC,
                "%s[filename:%s, md5:%s] has existed\n", user, filename, md5);
            ret = -2;
            goto END;
        }

        // change file_info count filed
        sprintf(sql_cmd, "update file_info set count = %d where md5 = '%s'",
                ++count, md5);
        if (mysql_query(conn, sql_cmd) != 0) {
            LOG(MD5_LOG_MODULE, MD5_LOG_PROC, "%s mysql failed %s\n", sql_cmd,
                mysql_error(conn));
            ret = -1;
            goto END;
        }

        // insert file info to user_file
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
            LOG(MD5_LOG_MODULE, MD5_LOG_PROC, "%s mysql failed %s\n", sql_cmd,
                mysql_error(conn));
            ret = -1;
            goto END;
        }

        // sql file number of user
        sprintf(sql_cmd, "select count from user_file_count where user = '%s'",
                user);
        count = 0;

        ret2 = process_result_one(conn, sql_cmd, tmp);
        if (ret2 == 1) {
            // insert
            sprintf(
                sql_cmd,
                " insert into user_file_count (user, count) values('%s', %d)",
                user, 1);
        } else if (ret2 == 0) {
            // update count
            count = atoi(tmp);
            sprintf(sql_cmd,
                    "update user_file_count set count = %d where user = '%s'",
                    count + 1, user);
        }

        if (mysql_query(conn, sql_cmd) != 0) {
            LOG(MD5_LOG_MODULE, MD5_LOG_PROC, "%s mysql failed %s\n", sql_cmd,
                mysql_error(conn));
            ret = -1;
            goto END;
        }

    } else if (1 == ret2) {
        // failed
        ret = -3;
        goto END;
    }

END:
    // ret：0 success，-1 error，-2 has this file already， -3 failed
    // 1second uplaod
    //     file exist{"code":"004"}
    //     success  {"code":"005"}
    //     failed  {"code":"006"}

    if (ret == 0) {
        out = return_status("005");
    } else if (ret == -2) {
        out = return_status("004");
    } else {
        out = return_status("006");
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
    read_cfg();

    while (FCGI_Accept() >= 0) {
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
            LOG(MD5_LOG_MODULE, MD5_LOG_PROC,
                "len = 0, No data from standard input\n");
        } else {
            char buf[4 * 1024] = {0};
            int ret = 0;
            ret = fread(buf, 1, len, stdin);
            if (ret == 0) {
                LOG(MD5_LOG_MODULE, MD5_LOG_PROC,
                    "fread(buf, 1, len, stdin) err\n");
                continue;
            }

            LOG(MD5_LOG_MODULE, MD5_LOG_PROC, "buf = %s\n", buf);

            //   {
            //     user:xxxx,
            //     token: xxxx,
            //     md5:xxx,
            //     fileName: xxx
            //    }

            char user[128] = {0};
            char md5[256] = {0};
            char token[256] = {0};
            char filename[128] = {0};
            ret = get_md5_info(buf, user, token, md5, filename);
            if (ret != 0) {
                LOG(MD5_LOG_MODULE, MD5_LOG_PROC, "get_md5_info() err\n");
                continue;
            }

            LOG(MD5_LOG_MODULE, MD5_LOG_PROC,
                "user = %s, token = %s, md5 = %s, filename = %s\n", user, token,
                md5, filename);

            ret = verify_token(user, token);
            if (ret == 0) {
                deal_md5(user, md5, filename);
            } else {
                char *out = return_status("111");
                if (out != NULL) {
                    printf(out);
                    free(out);
                }
            }
        }
    }
    return 0;
}
