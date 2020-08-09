#include "cgi_util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "cfg.h"
#include "redis_op.h"

int trim_space(char *inbuf) {
    int i = 0;
    int j = strlen(inbuf) - 1;

    char *str = inbuf;

    int count = 0;

    if (str == NULL) {
        LOG(UTIL_LOG_MODULE, UTIL_LOG_PROC, "inbuf   == NULL\n");
        return -1;
    }
    // space in front
    while (isspace(str[i]) && str[i] != '\0') {
        i++;
    }
    // space in end
    while (isspace(str[j]) && j > i) {
        j--;
    }

    count = j - i + 1;
    strncpy(inbuf, str + i, count);
    inbuf[count] = '\0';

    return 0;
}

// find the substr
char *memstr(char *full_data, int full_data_len, char *substr) {
    if (full_data == NULL || full_data_len <= 0 || substr == NULL) {
        return NULL;
    }

    if (*substr == '\0') {
        return NULL;
    }

    int sublen = strlen(substr);

    int i;
    char *cur = full_data;
    int last_possible = full_data_len - sublen + 1;
    for (i = 0; i < last_possible; i++) {
        if (*cur == *substr) {
            if (memcmp(cur, substr, sublen) == 0) {
                return cur;
            }
        }

        cur++;
    }

    return NULL;
}

// get method's arguments
int query_parse_key_value(const char *query, const char *key, char *value,
                          int *value_len_p) {
    char *temp = NULL;
    char *end = NULL;
    int value_len = 0;

    // temp points to key's begin in the query
    temp = strstr(query, key);
    if (temp == NULL) {
        LOG(UTIL_LOG_MODULE, UTIL_LOG_PROC, "Can not find key %s in query\n",
            key);
        return -1;
    }

    temp += strlen(key);
    temp++;
    // get value
    end = temp;
    while ('\0' != *end && '#' != *end && '&' != *end) {
        end++;
    }

    value_len = end - temp;
    strncpy(value, temp, value_len);
    value[value_len] = '\0';

    if (value_len_p != NULL) {
        *value_len_p = value_len;
    }

    return 0;
}

int get_file_suffix(const char *file_name, char *suffix) {
    const char *p = file_name;
    int len = 0;
    const char *q = NULL;
    const char *k = NULL;

    if (p == NULL) {
        return -1;
    }

    // find last char
    q = p;
    while (*q != '\0') {
        q++;
    }

    // find . from end
    k = q;
    while (*k != '.' && k != p) {
        k--;
    }

    if (*k == '.') {
        k++;
        len = q - k;
        if (len != 0) {
            strncpy(suffix, k, len);
            suffix[len] = '\0';
        } else {
            strncpy(suffix, "null", 5);
        }
    } else {
        strncpy(suffix, "null", 5);
    }

    return 0;
}

void str_replace(char *strSrc, char *strFind, char *strReplace) {
    while (*strSrc != '\0') {
        if (*strSrc == *strFind) {
            if (strncmp(strSrc, strFind, strlen(strFind)) == 0) {
                int i = 0;
                char *q = NULL;
                char *p = NULL;
                char *repl = NULL;
                int lastLen = 0;

                i = strlen(strFind);
                q = strSrc + i;
                p = q;
                repl = strReplace;

                while (*q++ != '\0') lastLen++;
                char *temp = (char *)malloc(lastLen + 1);
                int k = 0;
                for (k = 0; k < lastLen; k++) {
                    *(temp + k) = *(p + k);
                }
                *(temp + lastLen) = '\0';
                while (*repl != '\0') {
                    *strSrc++ = *repl++;
                }
                p = strSrc;
                char *pTemp = temp;
                while (*pTemp != '\0') {
                    *p++ = *pTemp++;
                }
                free(temp);
                *p = '\0';
            } else
                strSrc++;
        } else
            strSrc++;
    }
}

// return json format of status_num
char *return_status(char *status_num) {
    char *out = NULL;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "code", status_num);
    out = cJSON_Print(root);

    cJSON_Delete(root);

    return out;
}

// verify token by query redis
int verify_token(char *user, char *token) {
    int ret = 0;
    redisContext *redis_conn = NULL;
    char tmp_token[128] = {0};

    char redis_ip[30] = {0};
    char redis_port[10] = {0};

    get_cfg_value(CFG_PATH, "redis", "ip", redis_ip);
    get_cfg_value(CFG_PATH, "redis", "port", redis_port);

    redis_conn = rop_connectdb_nopwd(redis_ip, redis_port);
    if (redis_conn == NULL) {
        LOG(UTIL_LOG_MODULE, UTIL_LOG_PROC, "redis connected error\n");
        ret = -1;
        goto END;
    }

    ret = rop_get_string(redis_conn, user, tmp_token);
    if (ret == 0) {
        if (strcmp(token, tmp_token) != 0) {
            ret = -1;
        }
    }

END:
    if (redis_conn != NULL) {
        rop_disconnect(redis_conn);
    }

    return ret;
}
