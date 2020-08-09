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

#define REG_LOG_MODULE "cgi"
#define REG_LOG_PROC "reg"

// get register json info
int get_reg_info(char *reg_buf, char *user, char *nick_name, char *pwd,
                 char *tel, char *email) {
    int ret = 0;

    // {
    //     userName:xxxx,
    //     nickName:xxx,
    //     firstPwd:xxx,
    //     phone:xxx,
    //     email:xxx
    // }

    cJSON *root = cJSON_Parse(reg_buf);
    if (NULL == root) {
        LOG(REG_LOG_MODULE, REG_LOG_PROC, "cJSON_Parse err\n");
        ret = -1;
        goto END;
    }

    // userName
    cJSON *child1 = cJSON_GetObjectItem(root, "userName");
    if (NULL == child1) {
        LOG(REG_LOG_MODULE, REG_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }

    // LOG(REG_LOG_MODULE, REG_LOG_PROC, "child1->valuestring = %s\n",
    // child1->valuestring);
    strcpy(user, child1->valuestring);

    // nickName
    cJSON *child2 = cJSON_GetObjectItem(root, "nickName");
    if (NULL == child2) {
        LOG(REG_LOG_MODULE, REG_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }
    strcpy(nick_name, child2->valuestring);

    // pwd
    cJSON *child3 = cJSON_GetObjectItem(root, "firstPwd");
    if (NULL == child3) {
        LOG(REG_LOG_MODULE, REG_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }
    strcpy(pwd, child3->valuestring);

    // phone
    cJSON *child4 = cJSON_GetObjectItem(root, "phone");
    if (NULL == child4) {
        LOG(REG_LOG_MODULE, REG_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }
    strcpy(tel, child4->valuestring);

    // email
    cJSON *child5 = cJSON_GetObjectItem(root, "email");
    if (NULL == child5) {
        LOG(REG_LOG_MODULE, REG_LOG_PROC, "cJSON_GetObjectItem err\n");
        ret = -1;
        goto END;
    }
    strcpy(email, child5->valuestring);

END:
    if (root != NULL) {
        cJSON_Delete(root);
        root = NULL;
    }

    return ret;
}

// 0:success 1:failed 2:user has existed
int user_register(char *reg_buf) {
    int ret = 0;
    MYSQL *conn = NULL;

    char mysql_user[256] = {0};
    char mysql_pwd[256] = {0};
    char mysql_db[256] = {0};
    ret = get_mysql_info(mysql_user, mysql_pwd, mysql_db);
    if (ret != 0) {
        goto END;
    }
    LOG(REG_LOG_MODULE, REG_LOG_PROC,
        "mysql_user = %s, mysql_pwd = %s, mysql_db = %s\n", mysql_user,
        mysql_pwd, mysql_db);

    char user[128];
    char nick_name[128];
    char pwd[128];
    char tel[128];
    char email[128];
    ret = get_reg_info(reg_buf, user, nick_name, pwd, tel, email);
    if (ret != 0) {
        goto END;
    }
    LOG(REG_LOG_MODULE, REG_LOG_PROC,
        "user = %s, nick_name = %s, pwd = %s, tel = %s, email = %s\n", user,
        nick_name, pwd, tel, email);

    // connect the database
    conn = msql_conn(mysql_user, mysql_pwd, mysql_db);
    if (conn == NULL) {
        LOG(REG_LOG_MODULE, REG_LOG_PROC, "msql_conn err\n");
        ret = -1;
        goto END;
    }

    mysql_query(conn, "set names utf8");

    char sql_cmd[SQL_MAX_LEN] = {0};

    sprintf(sql_cmd, "select * from user where name = '%s'", user);

    // check if username has been used
    int ret2 = 0;
    ret2 = process_result_one(conn, sql_cmd, NULL);
    if (ret2 == 2) {
        LOG(REG_LOG_MODULE, REG_LOG_PROC, "[%s]user\n", user);
        ret = -2;
        goto END;
    }

    // current timestap
    struct timeval tv;
    struct tm *ptm;
    char time_str[128];

    gettimeofday(&tv, NULL);
    ptm = localtime(&tv.tv_sec);

    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", ptm);

    // insert register info
    sprintf(sql_cmd,
            "insert into user (name, nickname, password, phone, createtime, "
            "email) values ('%s', '%s', '%s', '%s', '%s', '%s')",
            user, nick_name, pwd, tel, time_str, email);

    if (mysql_query(conn, sql_cmd) != 0) {
        LOG(REG_LOG_MODULE, REG_LOG_PROC, "%s insert user failed %s\n", sql_cmd,
            mysql_error(conn));
        ret = -1;
        goto END;
    }

END:
    if (conn != NULL) {
        mysql_close(conn);
    }

    return ret;
}

int main() {
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
            LOG(REG_LOG_MODULE, REG_LOG_PROC,
                "len = 0, No data from standard input\n");
        } else {
            char buf[4 * 1024] = {0};
            int ret = 0;
            char *out = NULL;
            ret = fread(buf, 1, len, stdin);
            if (ret == 0) {
                LOG(REG_LOG_MODULE, REG_LOG_PROC,
                    "fread(buf, 1, len, stdin) err\n");
                continue;
            }

            LOG(REG_LOG_MODULE, REG_LOG_PROC, "buf = %s\n", buf);

            // register:
            //     success: {"code":"002"}
            //     user exists：{"code":"003"}
            //     failed: {"code":"004"}
            ret = user_register(buf);
            if (ret == 0) {
                out = return_status("002");
            } else if (ret == -1) {
                out = return_status("004");
            } else if (ret == -2) {
                out = return_status("003");
            }

            if (out != NULL) {
                printf(out);
                free(out);
            }
        }
    }

    return 0;
}
