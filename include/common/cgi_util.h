#ifndef _CGI_UTIL_H
#define _CGI_UTIL_H

#define FILE_NAME_LEN (256)
#define TEMP_BUF_MAX_LEN (512)
#define FILE_URL_LEN (512)
#define HOST_NAME_LEN (30)
#define USER_NAME_LEN (128)
#define TOKEN_LEN (128)
#define MD5_LEN (256)
#define PWD_LEN (256)
#define TIME_STRING_LEN (25)
#define SUFFIX_LEN (8)
#define PIC_NAME_LEN (10)
#define PIC_URL_LEN (256)

#define UTIL_LOG_MODULE "cgi"
#define UTIL_LOG_PROC "util"

int trim_space(char *inbuf);

char *memstr(char *full_data, int full_data_len, char *substr);

int query_parse_key_value(const char *query, const char *key, char *value,
                          int *value_len_p);

int get_file_suffix(const char *file_name, char *suffix);

void str_replace(char *strSrc, char *strFind, char *strReplace);

char *return_status(char *status_num);

int verify_token(char *user, char *token);

#endif
