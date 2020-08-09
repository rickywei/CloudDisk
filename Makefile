CC=gcc

CPPFLAGS=-I./include					\
         -I./include/common					\
		 -I/usr/include/fastdfs			\
		 -I/usr/include/fastcommon		\
		 -I/usr/local/include/hiredis/  \
		 -I/usr/include/mysql/

CFLAGS=-Wall

LIBS=-lfdfsclient	\
	 -lfastcommon	\
	 -lhiredis		\
	 -lfcgi         \
	 -lm            \
	 -lmysqlclient  


COMMON_PATH=src/common
CGI_BIN_PATH=bin_cgi
CGI_SRC_PATH=src/cgi


login=$(CGI_BIN_PATH)/login
register=$(CGI_BIN_PATH)/register
upload=$(CGI_BIN_PATH)/upload
md5=$(CGI_BIN_PATH)/md5
files=$(CGI_BIN_PATH)/files
filesop=$(CGI_BIN_PATH)/filesop
# sharefiles=$(CGI_BIN_PATH)/sharefiles
# sharefilesop=$(CGI_BIN_PATH)/sharefilesop


target=$(login)		\
	   $(register)	\
	   $(upload)	\
	   $(md5)		\
	   $(files)	\
	   $(filesop)	\
	   $(sharefiles)	\
	   $(sharefilesop)
ALL:$(target)


$(login):	$(CGI_SRC_PATH)/cgi_login.o \
			$(COMMON_PATH)/make_log.o  \
			$(COMMON_PATH)/cJSON.o \
			$(COMMON_PATH)/mysql_op.o \
			$(COMMON_PATH)/redis_op.o  \
			$(COMMON_PATH)/cfg.o \
			$(COMMON_PATH)/cgi_util.o \
			$(COMMON_PATH)/des.o \
			$(COMMON_PATH)/base64.o \
			$(COMMON_PATH)/md5.o
	$(CC) $^ -o $@ $(LIBS)

$(register):	$(CGI_SRC_PATH)/cgi_register.o \
				$(COMMON_PATH)/make_log.o  \
				$(COMMON_PATH)/cgi_util.o \
				$(COMMON_PATH)/cJSON.o \
				$(COMMON_PATH)/mysql_op.o \
				$(COMMON_PATH)/redis_op.o  \
				$(COMMON_PATH)/cfg.o
	$(CC) $^ -o $@ $(LIBS)

$(md5):		$(CGI_SRC_PATH)/cgi_md5.o \
			$(COMMON_PATH)/make_log.o  \
			$(COMMON_PATH)/cgi_util.o \
			$(COMMON_PATH)/cJSON.o \
			$(COMMON_PATH)/mysql_op.o \
			$(COMMON_PATH)/redis_op.o  \
			$(COMMON_PATH)/cfg.o
	$(CC) $^ -o $@ $(LIBS)

$(upload):$(CGI_SRC_PATH)/cgi_upload.o \
		  $(COMMON_PATH)/make_log.o  \
		  $(COMMON_PATH)/cgi_util.o \
		  $(COMMON_PATH)/cJSON.o \
		  $(COMMON_PATH)/mysql_op.o \
		  $(COMMON_PATH)/redis_op.o  \
		  $(COMMON_PATH)/cfg.o
	$(CC) $^ -o $@ $(LIBS)

$(files):	$(CGI_SRC_PATH)/cgi_files.o \
			$(COMMON_PATH)/make_log.o  \
			$(COMMON_PATH)/cgi_util.o \
			$(COMMON_PATH)/cJSON.o \
			$(COMMON_PATH)/mysql_op.o \
			$(COMMON_PATH)/redis_op.o  \
			$(COMMON_PATH)/cfg.o
	$(CC) $^ -o $@ $(LIBS)

$(filesop):$(CGI_SRC_PATH)/cgi_filesop.o \
			$(COMMON_PATH)/make_log.o  \
			$(COMMON_PATH)/cgi_util.o \
			$(COMMON_PATH)/cJSON.o \
			$(COMMON_PATH)/mysql_op.o \
			$(COMMON_PATH)/redis_op.o  \
			$(COMMON_PATH)/cfg.o
	$(CC) $^ -o $@ $(LIBS)

# $(sharefiles):	$(CGI_SRC_PATH)/cgi_sharefiles.o \
# 			$(COMMON_PATH)/make_log.o  \
# 			$(COMMON_PATH)/cgi_util.o \
# 			$(COMMON_PATH)/cJSON.o \
# 			$(COMMON_PATH)/mysql_op.o \
# 			$(COMMON_PATH)/redis_op.o  \
# 			$(COMMON_PATH)/cfg.o
# 	$(CC) $^ -o $@ $(LIBS)

# $(sharefilesop):	$(CGI_SRC_PATH)/cgi_sharefilesop.o \
# 			$(COMMON_PATH)/make_log.o  \
# 			$(COMMON_PATH)/cgi_util.o \
# 			$(COMMON_PATH)/cJSON.o \
# 			$(COMMON_PATH)/mysql_op.o \
# 			$(COMMON_PATH)/redis_op.o  \
# 			$(COMMON_PATH)/cfg.o
# 	$(CC) $^ -o $@ $(LIBS)

# =====================================================================


%.o:%.c
	$(CC) -c $< -o $@ $(CPPFLAGS) $(CFLAGS)


clean:
	-rm -rf *.o $(target) $(TEST_PATH)/*.o $(CGI_SRC_PATH)/*.o $(COMMON_PATH)/*.o

.PHONY:clean ALL
