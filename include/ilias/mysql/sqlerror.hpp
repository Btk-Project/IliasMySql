/**
 * @file sqlerror.hpp
 * @author llhsdmd(llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-01-09
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#include <system_error>
#include <ilias/io/error.hpp>
#include <mariadb/mysqld_error.h>
#include <stdint.h>

#include "detail/global.hpp"

ILIAS_MYSQL_NS_BEGIN

#define SQL_ERROR_TABLE                                                                                                \
    SQL_ERROR_ROW(OK, OK, 0)                                                                                           \
    SQL_ERROR_ROW(INVALID_PARAMETER, INVALID_PARAMETER, 996)                                                           \
    SQL_ERROR_ROW(NOT_PREPARED, NOT_PREPARED, 997)                                                                     \
    SQL_ERROR_ROW(INVALID_INDEX, INVALID_INDEX, 998)                                                                   \
    SQL_ERROR_ROW(NO_MORE_DATA, NO_MORE_DATA, 999)                                                                     \
    SQL_ERROR_ROW(ER_HASHCHK, HASHCHK, 1000)                                                                           \
    SQL_ERROR_ROW(ER_NISAMCHK, NISAMCHK, 1001)                                                                         \
    SQL_ERROR_ROW(ER_NO, NO, 1002)                                                                                     \
    SQL_ERROR_ROW(ER_YES, YES, 1003)                                                                                   \
    SQL_ERROR_ROW(ER_CANT_CREATE_FILE, CANT_CREATE_FILE, 1004)                                                         \
    SQL_ERROR_ROW(ER_CANT_CREATE_TABLE, CANT_CREATE_TABLE, 1005)                                                       \
    SQL_ERROR_ROW(ER_CANT_CREATE_DB, CANT_CREATE_DB, 1006)                                                             \
    SQL_ERROR_ROW(ER_DB_CREATE_EXISTS, DB_CREATE_EXISTS, 1007)                                                         \
    SQL_ERROR_ROW(ER_DB_DROP_EXISTS, DB_DROP_EXISTS, 1008)                                                             \
    SQL_ERROR_ROW(ER_DB_DROP_DELETE, DB_DROP_DELETE, 1009)                                                             \
    SQL_ERROR_ROW(ER_DB_DROP_RMDIR, DB_DROP_RMDIR, 1010)                                                               \
    SQL_ERROR_ROW(ER_CANT_DELETE_FILE, CANT_DELETE_FILE, 1011)                                                         \
    SQL_ERROR_ROW(ER_CANT_FIND_SYSTEM_REC, CANT_FIND_SYSTEM_REC, 1012)                                                 \
    SQL_ERROR_ROW(ER_CANT_GET_STAT, CANT_GET_STAT, 1013)                                                               \
    SQL_ERROR_ROW(ER_CANT_GET_WD, CANT_GET_WD, 1014)                                                                   \
    SQL_ERROR_ROW(ER_CANT_LOCK, CANT_LOCK, 1015)                                                                       \
    SQL_ERROR_ROW(ER_CANT_OPEN_FILE, CANT_OPEN_FILE, 1016)                                                             \
    SQL_ERROR_ROW(ER_FILE_NOT_FOUND, FILE_NOT_FOUND, 1017)                                                             \
    SQL_ERROR_ROW(ER_CANT_READ_DIR, CANT_READ_DIR, 1018)                                                               \
    SQL_ERROR_ROW(ER_CANT_SET_WD, CANT_SET_WD, 1019)                                                                   \
    SQL_ERROR_ROW(ER_CHECKREAD, CHECKREAD, 1020)                                                                       \
    SQL_ERROR_ROW(ER_DISK_FULL, DISK_FULL, 1021)                                                                       \
    SQL_ERROR_ROW(ER_DUP_KEY, DUP_KEY, 1022)                                                                           \
    SQL_ERROR_ROW(ER_ERROR_ON_CLOSE, ERROR_ON_CLOSE, 1023)                                                             \
    SQL_ERROR_ROW(ER_ERROR_ON_READ, ERROR_ON_READ, 1024)                                                               \
    SQL_ERROR_ROW(ER_ERROR_ON_RENAME, ERROR_ON_RENAME, 1025)                                                           \
    SQL_ERROR_ROW(ER_ERROR_ON_WRITE, ERROR_ON_WRITE, 1026)                                                             \
    SQL_ERROR_ROW(ER_FILE_USED, FILE_USED, 1027)                                                                       \
    SQL_ERROR_ROW(ER_FILSORT_ABORT, FILSORT_ABORT, 1028)                                                               \
    SQL_ERROR_ROW(ER_FORM_NOT_FOUND, FORM_NOT_FOUND, 1029)                                                             \
    SQL_ERROR_ROW(ER_GET_ERRNO, GET_ERRNO, 1030)                                                                       \
    SQL_ERROR_ROW(ER_ILLEGAL_HA, ILLEGAL_HA, 1031)                                                                     \
    SQL_ERROR_ROW(ER_KEY_NOT_FOUND, KEY_NOT_FOUND, 1032)                                                               \
    SQL_ERROR_ROW(ER_NOT_FORM_FILE, NOT_FORM_FILE, 1033)                                                               \
    SQL_ERROR_ROW(ER_NOT_KEYFILE, NOT_KEYFILE, 1034)                                                                   \
    SQL_ERROR_ROW(ER_OLD_KEYFILE, OLD_KEYFILE, 1035)                                                                   \
    SQL_ERROR_ROW(ER_OPEN_AS_READONLY, OPEN_AS_READONLY, 1036)                                                         \
    SQL_ERROR_ROW(ER_OUTOFMEMORY, OUTOFMEMORY, 1037)                                                                   \
    SQL_ERROR_ROW(ER_OUT_OF_SORTMEMORY, OUT_OF_SORTMEMORY, 1038)                                                       \
    SQL_ERROR_ROW(ER_UNEXPECTED_EOF, UNEXPECTED_EOF, 1039)                                                             \
    SQL_ERROR_ROW(ER_CON_COUNT_ERROR, CON_COUNT_ERROR, 1040)                                                           \
    SQL_ERROR_ROW(ER_OUT_OF_RESOURCES, OUT_OF_RESOURCES, 1041)                                                         \
    SQL_ERROR_ROW(ER_BAD_HOST_ERROR, BAD_HOST_ERROR, 1042)                                                             \
    SQL_ERROR_ROW(ER_HANDSHAKE_ERROR, HANDSHAKE_ERROR, 1043)                                                           \
    SQL_ERROR_ROW(ER_DBACCESS_DENIED_ERROR, DBACCESS_DENIED_ERROR, 1044)                                               \
    SQL_ERROR_ROW(ER_ACCESS_DENIED_ERROR, ACCESS_DENIED_ERROR, 1045)                                                   \
    SQL_ERROR_ROW(ER_NO_DB_ERROR, NO_DB_ERROR, 1046)                                                                   \
    SQL_ERROR_ROW(ER_UNKNOWN_COM_ERROR, UNKNOWN_COM_ERROR, 1047)                                                       \
    SQL_ERROR_ROW(ER_BAD_NULL_ERROR, BAD_NULL_ERROR, 1048)                                                             \
    SQL_ERROR_ROW(ER_BAD_DB_ERROR, BAD_DB_ERROR, 1049)                                                                 \
    SQL_ERROR_ROW(ER_TABLE_EXISTS_ERROR, TABLE_EXISTS_ERROR, 1050)                                                     \
    SQL_ERROR_ROW(ER_BAD_TABLE_ERROR, BAD_TABLE_ERROR, 1051)                                                           \
    SQL_ERROR_ROW(ER_NON_UNIQ_ERROR, NON_UNIQ_ERROR, 1052)                                                             \
    SQL_ERROR_ROW(ER_SERVER_SHUTDOWN, SERVER_SHUTDOWN, 1053)                                                           \
    SQL_ERROR_ROW(ER_BAD_FIELD_ERROR, BAD_FIELD_ERROR, 1054)                                                           \
    SQL_ERROR_ROW(ER_WRONG_FIELD_WITH_GROUP, WRONG_FIELD_WITH_GROUP, 1055)                                             \
    SQL_ERROR_ROW(ER_WRONG_GROUP_FIELD, WRONG_GROUP_FIELD, 1056)                                                       \
    SQL_ERROR_ROW(ER_WRONG_SUM_SELECT, WRONG_SUM_SELECT, 1057)                                                         \
    SQL_ERROR_ROW(ER_WRONG_VALUE_COUNT, WRONG_VALUE_COUNT, 1058)                                                       \
    SQL_ERROR_ROW(ER_TOO_LONG_IDENT, TOO_LONG_IDENT, 1059)                                                             \
    SQL_ERROR_ROW(ER_DUP_FIELDNAME, DUP_FIELDNAME, 1060)                                                               \
    SQL_ERROR_ROW(ER_DUP_KEYNAME, DUP_KEYNAME, 1061)                                                                   \
    SQL_ERROR_ROW(ER_DUP_ENTRY, DUP_ENTRY, 1062)                                                                       \
    SQL_ERROR_ROW(ER_WRONG_FIELD_SPEC, WRONG_FIELD_SPEC, 1063)                                                         \
    SQL_ERROR_ROW(ER_PARSE_ERROR, PARSE_ERROR, 1064)                                                                   \
    SQL_ERROR_ROW(ER_EMPTY_QUERY, EMPTY_QUERY, 1065)                                                                   \
    SQL_ERROR_ROW(ER_NONUNIQ_TABLE, NONUNIQ_TABLE, 1066)                                                               \
    SQL_ERROR_ROW(ER_INVALID_DEFAULT, INVALID_DEFAULT, 1067)                                                           \
    SQL_ERROR_ROW(ER_MULTIPLE_PRI_KEY, MULTIPLE_PRI_KEY, 1068)                                                         \
    SQL_ERROR_ROW(ER_TOO_MANY_KEYS, TOO_MANY_KEYS, 1069)                                                               \
    SQL_ERROR_ROW(ER_TOO_MANY_KEY_PARTS, TOO_MANY_KEY_PARTS, 1070)                                                     \
    SQL_ERROR_ROW(ER_TOO_LONG_KEY, TOO_LONG_KEY, 1071)                                                                 \
    SQL_ERROR_ROW(ER_KEY_COLUMN_DOES_NOT_EXIST, KEY_COLUMN_DOES_NOT_EXIST, 1072)                                       \
    SQL_ERROR_ROW(ER_BLOB_USED_AS_KEY, BLOB_USED_AS_KEY, 1073)                                                         \
    SQL_ERROR_ROW(ER_TOO_BIG_FIELDLENGTH, TOO_BIG_FIELDLENGTH, 1074)                                                   \
    SQL_ERROR_ROW(ER_WRONG_AUTO_KEY, WRONG_AUTO_KEY, 1075)                                                             \
    SQL_ERROR_ROW(ER_BINLOG_CANT_DELETE_GTID_DOMAIN, BINLOG_CANT_DELETE_GTID_DOMAIN, 1076)                             \
    SQL_ERROR_ROW(ER_NORMAL_SHUTDOWN, NORMAL_SHUTDOWN, 1077)                                                           \
    SQL_ERROR_ROW(ER_GOT_SIGNAL, GOT_SIGNAL, 1078)                                                                     \
    SQL_ERROR_ROW(ER_SHUTDOWN_COMPLETE, SHUTDOWN_COMPLETE, 1079)                                                       \
    SQL_ERROR_ROW(ER_FORCING_CLOSE, FORCING_CLOSE, 1080)                                                               \
    SQL_ERROR_ROW(ER_IPSOCK_ERROR, IPSOCK_ERROR, 1081)                                                                 \
    SQL_ERROR_ROW(ER_NO_SUCH_INDEX, NO_SUCH_INDEX, 1082)                                                               \
    SQL_ERROR_ROW(ER_WRONG_FIELD_TERMINATORS, WRONG_FIELD_TERMINATORS, 1083)                                           \
    SQL_ERROR_ROW(ER_BLOBS_AND_NO_TERMINATED, BLOBS_AND_NO_TERMINATED, 1084)                                           \
    SQL_ERROR_ROW(ER_TEXTFILE_NOT_READABLE, TEXTFILE_NOT_READABLE, 1085)                                               \
    SQL_ERROR_ROW(ER_FILE_EXISTS_ERROR, FILE_EXISTS_ERROR, 1086)                                                       \
    SQL_ERROR_ROW(ER_LOAD_INFO, LOAD_INFO, 1087)                                                                       \
    SQL_ERROR_ROW(ER_ALTER_INFO, ALTER_INFO, 1088)                                                                     \
    SQL_ERROR_ROW(ER_WRONG_SUB_KEY, WRONG_SUB_KEY, 1089)                                                               \
    SQL_ERROR_ROW(ER_CANT_REMOVE_ALL_FIELDS, CANT_REMOVE_ALL_FIELDS, 1090)                                             \
    SQL_ERROR_ROW(ER_CANT_DROP_FIELD_OR_KEY, CANT_DROP_FIELD_OR_KEY, 1091)                                             \
    SQL_ERROR_ROW(ER_INSERT_INFO, INSERT_INFO, 1092)                                                                   \
    SQL_ERROR_ROW(ER_UPDATE_TABLE_USED, UPDATE_TABLE_USED, 1093)                                                       \
    SQL_ERROR_ROW(ER_NO_SUCH_THREAD, NO_SUCH_THREAD, 1094)                                                             \
    SQL_ERROR_ROW(ER_KILL_DENIED_ERROR, KILL_DENIED_ERROR, 1095)                                                       \
    SQL_ERROR_ROW(ER_NO_TABLES_USED, NO_TABLES_USED, 1096)                                                             \
    SQL_ERROR_ROW(ER_TOO_BIG_SET, TOO_BIG_SET, 1097)                                                                   \
    SQL_ERROR_ROW(ER_NO_UNIQUE_LOGFILE, NO_UNIQUE_LOGFILE, 1098)                                                       \
    SQL_ERROR_ROW(ER_TABLE_NOT_LOCKED_FOR_WRITE, TABLE_NOT_LOCKED_FOR_WRITE, 1099)                                     \
    SQL_ERROR_ROW(ER_TABLE_NOT_LOCKED, TABLE_NOT_LOCKED, 1100)                                                         \
    SQL_ERROR_ROW(ER_UNUSED_17, UNUSED_17, 1101)                                                                       \
    SQL_ERROR_ROW(ER_WRONG_DB_NAME, WRONG_DB_NAME, 1102)                                                               \
    SQL_ERROR_ROW(ER_WRONG_TABLE_NAME, WRONG_TABLE_NAME, 1103)                                                         \
    SQL_ERROR_ROW(ER_TOO_BIG_SELECT, TOO_BIG_SELECT, 1104)                                                             \
    SQL_ERROR_ROW(ER_UNKNOWN_ERROR, UNKNOWN_ERROR, 1105)                                                               \
    SQL_ERROR_ROW(ER_UNKNOWN_PROCEDURE, UNKNOWN_PROCEDURE, 1106)                                                       \
    SQL_ERROR_ROW(ER_WRONG_PARAMCOUNT_TO_PROCEDURE, WRONG_PARAMCOUNT_TO_PROCEDURE, 1107)                               \
    SQL_ERROR_ROW(ER_WRONG_PARAMETERS_TO_PROCEDURE, WRONG_PARAMETERS_TO_PROCEDURE, 1108)                               \
    SQL_ERROR_ROW(ER_UNKNOWN_TABLE, UNKNOWN_TABLE, 1109)                                                               \
    SQL_ERROR_ROW(ER_FIELD_SPECIFIED_TWICE, FIELD_SPECIFIED_TWICE, 1110)                                               \
    SQL_ERROR_ROW(ER_INVALID_GROUP_FUNC_USE, INVALID_GROUP_FUNC_USE, 1111)                                             \
    SQL_ERROR_ROW(ER_UNSUPPORTED_EXTENSION, UNSUPPORTED_EXTENSION, 1112)                                               \
    SQL_ERROR_ROW(ER_TABLE_MUST_HAVE_COLUMNS, TABLE_MUST_HAVE_COLUMNS, 1113)                                           \
    SQL_ERROR_ROW(ER_RECORD_FILE_FULL, RECORD_FILE_FULL, 1114)                                                         \
    SQL_ERROR_ROW(ER_UNKNOWN_CHARACTER_SET, UNKNOWN_CHARACTER_SET, 1115)                                               \
    SQL_ERROR_ROW(ER_TOO_MANY_TABLES, TOO_MANY_TABLES, 1116)                                                           \
    SQL_ERROR_ROW(ER_TOO_MANY_FIELDS, TOO_MANY_FIELDS, 1117)                                                           \
    SQL_ERROR_ROW(ER_TOO_BIG_ROWSIZE, TOO_BIG_ROWSIZE, 1118)                                                           \
    SQL_ERROR_ROW(ER_STACK_OVERRUN, STACK_OVERRUN, 1119)                                                               \
    SQL_ERROR_ROW(ER_WRONG_OUTER_JOIN, WRONG_OUTER_JOIN, 1120)                                                         \
    SQL_ERROR_ROW(ER_NULL_COLUMN_IN_INDEX, NULL_COLUMN_IN_INDEX, 1121)                                                 \
    SQL_ERROR_ROW(ER_CANT_FIND_UDF, CANT_FIND_UDF, 1122)                                                               \
    SQL_ERROR_ROW(ER_CANT_INITIALIZE_UDF, CANT_INITIALIZE_UDF, 1123)                                                   \
    SQL_ERROR_ROW(ER_UDF_NO_PATHS, UDF_NO_PATHS, 1124)                                                                 \
    SQL_ERROR_ROW(ER_UDF_EXISTS, UDF_EXISTS, 1125)                                                                     \
    SQL_ERROR_ROW(ER_CANT_OPEN_LIBRARY, CANT_OPEN_LIBRARY, 1126)                                                       \
    SQL_ERROR_ROW(ER_CANT_FIND_DL_ENTRY, CANT_FIND_DL_ENTRY, 1127)                                                     \
    SQL_ERROR_ROW(ER_FUNCTION_NOT_DEFINED, FUNCTION_NOT_DEFINED, 1128)                                                 \
    SQL_ERROR_ROW(ER_HOST_IS_BLOCKED, HOST_IS_BLOCKED, 1129)                                                           \
    SQL_ERROR_ROW(ER_HOST_NOT_PRIVILEGED, HOST_NOT_PRIVILEGED, 1130)                                                   \
    SQL_ERROR_ROW(ER_PASSWORD_ANONYMOUS_USER, PASSWORD_ANONYMOUS_USER, 1131)                                           \
    SQL_ERROR_ROW(ER_PASSWORD_NOT_ALLOWED, PASSWORD_NOT_ALLOWED, 1132)                                                 \
    SQL_ERROR_ROW(ER_PASSWORD_NO_MATCH, PASSWORD_NO_MATCH, 1133)                                                       \
    SQL_ERROR_ROW(ER_UPDATE_INFO, UPDATE_INFO, 1134)                                                                   \
    SQL_ERROR_ROW(ER_CANT_CREATE_THREAD, CANT_CREATE_THREAD, 1135)                                                     \
    SQL_ERROR_ROW(ER_WRONG_VALUE_COUNT_ON_ROW, WRONG_VALUE_COUNT_ON_ROW, 1136)                                         \
    SQL_ERROR_ROW(ER_CANT_REOPEN_TABLE, CANT_REOPEN_TABLE, 1137)                                                       \
    SQL_ERROR_ROW(ER_INVALID_USE_OF_NULL, INVALID_USE_OF_NULL, 1138)                                                   \
    SQL_ERROR_ROW(ER_REGEXP_ERROR, REGEXP_ERROR, 1139)                                                                 \
    SQL_ERROR_ROW(ER_MIX_OF_GROUP_FUNC_AND_FIELDS, MIX_OF_GROUP_FUNC_AND_FIELDS, 1140)                                 \
    SQL_ERROR_ROW(ER_NONEXISTING_GRANT, NONEXISTING_GRANT, 1141)                                                       \
    SQL_ERROR_ROW(ER_TABLEACCESS_DENIED_ERROR, TABLEACCESS_DENIED_ERROR, 1142)                                         \
    SQL_ERROR_ROW(ER_COLUMNACCESS_DENIED_ERROR, COLUMNACCESS_DENIED_ERROR, 1143)                                       \
    SQL_ERROR_ROW(ER_ILLEGAL_GRANT_FOR_TABLE, ILLEGAL_GRANT_FOR_TABLE, 1144)                                           \
    SQL_ERROR_ROW(ER_GRANT_WRONG_HOST_OR_USER, GRANT_WRONG_HOST_OR_USER, 1145)                                         \
    SQL_ERROR_ROW(ER_NO_SUCH_TABLE, NO_SUCH_TABLE, 1146)                                                               \
    SQL_ERROR_ROW(ER_NONEXISTING_TABLE_GRANT, NONEXISTING_TABLE_GRANT, 1147)                                           \
    SQL_ERROR_ROW(ER_NOT_ALLOWED_COMMAND, NOT_ALLOWED_COMMAND, 1148)                                                   \
    SQL_ERROR_ROW(ER_SYNTAX_ERROR, SYNTAX_ERROR, 1149)                                                                 \
    SQL_ERROR_ROW(ER_DELAYED_CANT_CHANGE_LOCK, DELAYED_CANT_CHANGE_LOCK, 1150)                                         \
    SQL_ERROR_ROW(ER_TOO_MANY_DELAYED_THREADS, TOO_MANY_DELAYED_THREADS, 1151)                                         \
    SQL_ERROR_ROW(ER_ABORTING_CONNECTION, ABORTING_CONNECTION, 1152)                                                   \
    SQL_ERROR_ROW(ER_NET_PACKET_TOO_LARGE, NET_PACKET_TOO_LARGE, 1153)                                                 \
    SQL_ERROR_ROW(ER_NET_READ_ERROR_FROM_PIPE, NET_READ_ERROR_FROM_PIPE, 1154)                                         \
    SQL_ERROR_ROW(ER_NET_FCNTL_ERROR, NET_FCNTL_ERROR, 1155)                                                           \
    SQL_ERROR_ROW(ER_NET_PACKETS_OUT_OF_ORDER, NET_PACKETS_OUT_OF_ORDER, 1156)                                         \
    SQL_ERROR_ROW(ER_NET_UNCOMPRESS_ERROR, NET_UNCOMPRESS_ERROR, 1157)                                                 \
    SQL_ERROR_ROW(ER_NET_READ_ERROR, NET_READ_ERROR, 1158)                                                             \
    SQL_ERROR_ROW(ER_NET_READ_INTERRUPTED, NET_READ_INTERRUPTED, 1159)                                                 \
    SQL_ERROR_ROW(ER_NET_ERROR_ON_WRITE, NET_ERROR_ON_WRITE, 1160)                                                     \
    SQL_ERROR_ROW(ER_NET_WRITE_INTERRUPTED, NET_WRITE_INTERRUPTED, 1161)                                               \
    SQL_ERROR_ROW(ER_TOO_LONG_STRING, TOO_LONG_STRING, 1162)                                                           \
    SQL_ERROR_ROW(ER_TABLE_CANT_HANDLE_BLOB, TABLE_CANT_HANDLE_BLOB, 1163)                                             \
    SQL_ERROR_ROW(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT, TABLE_CANT_HANDLE_AUTO_INCREMENT, 1164)                         \
    SQL_ERROR_ROW(ER_DELAYED_INSERT_TABLE_LOCKED, DELAYED_INSERT_TABLE_LOCKED, 1165)                                   \
    SQL_ERROR_ROW(ER_WRONG_COLUMN_NAME, WRONG_COLUMN_NAME, 1166)                                                       \
    SQL_ERROR_ROW(ER_WRONG_KEY_COLUMN, WRONG_KEY_COLUMN, 1167)                                                         \
    SQL_ERROR_ROW(ER_WRONG_MRG_TABLE, WRONG_MRG_TABLE, 1168)                                                           \
    SQL_ERROR_ROW(ER_DUP_UNIQUE, DUP_UNIQUE, 1169)                                                                     \
    SQL_ERROR_ROW(ER_BLOB_KEY_WITHOUT_LENGTH, BLOB_KEY_WITHOUT_LENGTH, 1170)                                           \
    SQL_ERROR_ROW(ER_PRIMARY_CANT_HAVE_NULL, PRIMARY_CANT_HAVE_NULL, 1171)                                             \
    SQL_ERROR_ROW(ER_TOO_MANY_ROWS, TOO_MANY_ROWS, 1172)                                                               \
    SQL_ERROR_ROW(ER_REQUIRES_PRIMARY_KEY, REQUIRES_PRIMARY_KEY, 1173)                                                 \
    SQL_ERROR_ROW(ER_NO_RAID_COMPILED, NO_RAID_COMPILED, 1174)                                                         \
    SQL_ERROR_ROW(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE, UPDATE_WITHOUT_KEY_IN_SAFE_MODE, 1175)                           \
    SQL_ERROR_ROW(ER_KEY_DOES_NOT_EXISTS, KEY_DOES_NOT_EXISTS, 1176)                                                   \
    SQL_ERROR_ROW(ER_CHECK_NO_SUCH_TABLE, CHECK_NO_SUCH_TABLE, 1177)                                                   \
    SQL_ERROR_ROW(ER_CHECK_NOT_IMPLEMENTED, CHECK_NOT_IMPLEMENTED, 1178)                                               \
    SQL_ERROR_ROW(ER_CANT_DO_THIS_DURING_AN_TRANSACTION, CANT_DO_THIS_DURING_AN_TRANSACTION, 1179)                     \
    SQL_ERROR_ROW(ER_ERROR_DURING_COMMIT, ERROR_DURING_COMMIT, 1180)                                                   \
    SQL_ERROR_ROW(ER_ERROR_DURING_ROLLBACK, ERROR_DURING_ROLLBACK, 1181)                                               \
    SQL_ERROR_ROW(ER_ERROR_DURING_FLUSH_LOGS, ERROR_DURING_FLUSH_LOGS, 1182)                                           \
    SQL_ERROR_ROW(ER_ERROR_DURING_CHECKPOINT, ERROR_DURING_CHECKPOINT, 1183)                                           \
    SQL_ERROR_ROW(ER_NEW_ABORTING_CONNECTION, NEW_ABORTING_CONNECTION, 1184)                                           \
    SQL_ERROR_ROW(ER_UNUSED_10, UNUSED_10, 1185)                                                                       \
    SQL_ERROR_ROW(ER_FLUSH_MASTER_BINLOG_CLOSED, FLUSH_MASTER_BINLOG_CLOSED, 1186)                                     \
    SQL_ERROR_ROW(ER_INDEX_REBUILD, INDEX_REBUILD, 1187)                                                               \
    SQL_ERROR_ROW(ER_MASTER, MASTER, 1188)                                                                             \
    SQL_ERROR_ROW(ER_MASTER_NET_READ, MASTER_NET_READ, 1189)                                                           \
    SQL_ERROR_ROW(ER_MASTER_NET_WRITE, MASTER_NET_WRITE, 1190)                                                         \
    SQL_ERROR_ROW(ER_FT_MATCHING_KEY_NOT_FOUND, FT_MATCHING_KEY_NOT_FOUND, 1191)                                       \
    SQL_ERROR_ROW(ER_LOCK_OR_ACTIVE_TRANSACTION, LOCK_OR_ACTIVE_TRANSACTION, 1192)                                     \
    SQL_ERROR_ROW(ER_UNKNOWN_SYSTEM_VARIABLE, UNKNOWN_SYSTEM_VARIABLE, 1193)                                           \
    SQL_ERROR_ROW(ER_CRASHED_ON_USAGE, CRASHED_ON_USAGE, 1194)                                                         \
    SQL_ERROR_ROW(ER_CRASHED_ON_REPAIR, CRASHED_ON_REPAIR, 1195)                                                       \
    SQL_ERROR_ROW(ER_WARNING_NOT_COMPLETE_ROLLBACK, WARNING_NOT_COMPLETE_ROLLBACK, 1196)                               \
    SQL_ERROR_ROW(ER_TRANS_CACHE_FULL, TRANS_CACHE_FULL, 1197)                                                         \
    SQL_ERROR_ROW(ER_SLAVE_MUST_STOP, SLAVE_MUST_STOP, 1198)                                                           \
    SQL_ERROR_ROW(ER_SLAVE_NOT_RUNNING, SLAVE_NOT_RUNNING, 1199)                                                       \
    SQL_ERROR_ROW(ER_BAD_SLAVE, BAD_SLAVE, 1200)                                                                       \
    SQL_ERROR_ROW(ER_MASTER_INFO, MASTER_INFO, 1201)                                                                   \
    SQL_ERROR_ROW(ER_SLAVE_THREAD, SLAVE_THREAD, 1202)                                                                 \
    SQL_ERROR_ROW(ER_TOO_MANY_USER_CONNECTIONS, TOO_MANY_USER_CONNECTIONS, 1203)                                       \
    SQL_ERROR_ROW(ER_SET_CONSTANTS_ONLY, SET_CONSTANTS_ONLY, 1204)                                                     \
    SQL_ERROR_ROW(ER_LOCK_WAIT_TIMEOUT, LOCK_WAIT_TIMEOUT, 1205)                                                       \
    SQL_ERROR_ROW(ER_LOCK_TABLE_FULL, LOCK_TABLE_FULL, 1206)                                                           \
    SQL_ERROR_ROW(ER_READ_ONLY_TRANSACTION, READ_ONLY_TRANSACTION, 1207)                                               \
    SQL_ERROR_ROW(ER_DROP_DB_WITH_READ_LOCK, DROP_DB_WITH_READ_LOCK, 1208)                                             \
    SQL_ERROR_ROW(ER_CREATE_DB_WITH_READ_LOCK, CREATE_DB_WITH_READ_LOCK, 1209)                                         \
    SQL_ERROR_ROW(ER_WRONG_ARGUMENTS, WRONG_ARGUMENTS, 1210)                                                           \
    SQL_ERROR_ROW(ER_NO_PERMISSION_TO_CREATE_USER, NO_PERMISSION_TO_CREATE_USER, 1211)                                 \
    SQL_ERROR_ROW(ER_UNION_TABLES_IN_DIFFERENT_DIR, UNION_TABLES_IN_DIFFERENT_DIR, 1212)                               \
    SQL_ERROR_ROW(ER_LOCK_DEADLOCK, LOCK_DEADLOCK, 1213)                                                               \
    SQL_ERROR_ROW(ER_TABLE_CANT_HANDLE_FT, TABLE_CANT_HANDLE_FT, 1214)                                                 \
    SQL_ERROR_ROW(ER_CANNOT_ADD_FOREIGN, CANNOT_ADD_FOREIGN, 1215)                                                     \
    SQL_ERROR_ROW(ER_NO_REFERENCED_ROW, NO_REFERENCED_ROW, 1216)                                                       \
    SQL_ERROR_ROW(ER_ROW_IS_REFERENCED, ROW_IS_REFERENCED, 1217)                                                       \
    SQL_ERROR_ROW(ER_CONNECT_TO_MASTER, CONNECT_TO_MASTER, 1218)                                                       \
    SQL_ERROR_ROW(ER_QUERY_ON_MASTER, QUERY_ON_MASTER, 1219)                                                           \
    SQL_ERROR_ROW(ER_ERROR_WHEN_EXECUTING_COMMAND, ERROR_WHEN_EXECUTING_COMMAND, 1220)                                 \
    SQL_ERROR_ROW(ER_WRONG_USAGE, WRONG_USAGE, 1221)                                                                   \
    SQL_ERROR_ROW(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT, WRONG_NUMBER_OF_COLUMNS_IN_SELECT, 1222)                       \
    SQL_ERROR_ROW(ER_CANT_UPDATE_WITH_READLOCK, CANT_UPDATE_WITH_READLOCK, 1223)                                       \
    SQL_ERROR_ROW(ER_MIXING_NOT_ALLOWED, MIXING_NOT_ALLOWED, 1224)                                                     \
    SQL_ERROR_ROW(ER_DUP_ARGUMENT, DUP_ARGUMENT, 1225)                                                                 \
    SQL_ERROR_ROW(ER_USER_LIMIT_REACHED, USER_LIMIT_REACHED, 1226)                                                     \
    SQL_ERROR_ROW(ER_SPECIFIC_ACCESS_DENIED_ERROR, SPECIFIC_ACCESS_DENIED_ERROR, 1227)                                 \
    SQL_ERROR_ROW(ER_LOCAL_VARIABLE, LOCAL_VARIABLE, 1228)                                                             \
    SQL_ERROR_ROW(ER_GLOBAL_VARIABLE, GLOBAL_VARIABLE, 1229)                                                           \
    SQL_ERROR_ROW(ER_NO_DEFAULT, NO_DEFAULT, 1230)                                                                     \
    SQL_ERROR_ROW(ER_WRONG_VALUE_FOR_VAR, WRONG_VALUE_FOR_VAR, 1231)                                                   \
    SQL_ERROR_ROW(ER_WRONG_TYPE_FOR_VAR, WRONG_TYPE_FOR_VAR, 1232)                                                     \
    SQL_ERROR_ROW(ER_VAR_CANT_BE_READ, VAR_CANT_BE_READ, 1233)                                                         \
    SQL_ERROR_ROW(ER_CANT_USE_OPTION_HERE, CANT_USE_OPTION_HERE, 1234)                                                 \
    SQL_ERROR_ROW(ER_NOT_SUPPORTED_YET, NOT_SUPPORTED_YET, 1235)                                                       \
    SQL_ERROR_ROW(ER_MASTER_FATAL_ERROR_READING_BINLOG, MASTER_FATAL_ERROR_READING_BINLOG, 1236)                       \
    SQL_ERROR_ROW(ER_SLAVE_IGNORED_TABLE, SLAVE_IGNORED_TABLE, 1237)                                                   \
    SQL_ERROR_ROW(ER_INCORRECT_GLOBAL_LOCAL_VAR, INCORRECT_GLOBAL_LOCAL_VAR, 1238)                                     \
    SQL_ERROR_ROW(ER_WRONG_FK_DEF, WRONG_FK_DEF, 1239)                                                                 \
    SQL_ERROR_ROW(ER_KEY_REF_DO_NOT_MATCH_TABLE_REF, KEY_REF_DO_NOT_MATCH_TABLE_REF, 1240)                             \
    SQL_ERROR_ROW(ER_OPERAND_COLUMNS, OPERAND_COLUMNS, 1241)                                                           \
    SQL_ERROR_ROW(ER_SUBQUERY_NO_1_ROW, SUBQUERY_NO_1_ROW, 1242)                                                       \
    SQL_ERROR_ROW(ER_UNKNOWN_STMT_HANDLER, UNKNOWN_STMT_HANDLER, 1243)                                                 \
    SQL_ERROR_ROW(ER_CORRUPT_HELP_DB, CORRUPT_HELP_DB, 1244)                                                           \
    SQL_ERROR_ROW(ER_CYCLIC_REFERENCE, CYCLIC_REFERENCE, 1245)                                                         \
    SQL_ERROR_ROW(ER_AUTO_CONVERT, AUTO_CONVERT, 1246)                                                                 \
    SQL_ERROR_ROW(ER_ILLEGAL_REFERENCE, ILLEGAL_REFERENCE, 1247)                                                       \
    SQL_ERROR_ROW(ER_DERIVED_MUST_HAVE_ALIAS, DERIVED_MUST_HAVE_ALIAS, 1248)                                           \
    SQL_ERROR_ROW(ER_SELECT_REDUCED, SELECT_REDUCED, 1249)                                                             \
    SQL_ERROR_ROW(ER_TABLENAME_NOT_ALLOWED_HERE, TABLENAME_NOT_ALLOWED_HERE, 1250)                                     \
    SQL_ERROR_ROW(ER_NOT_SUPPORTED_AUTH_MODE, NOT_SUPPORTED_AUTH_MODE, 1251)                                           \
    SQL_ERROR_ROW(ER_SPATIAL_CANT_HAVE_NULL, SPATIAL_CANT_HAVE_NULL, 1252)                                             \
    SQL_ERROR_ROW(ER_COLLATION_CHARSET_MISMATCH, COLLATION_CHARSET_MISMATCH, 1253)                                     \
    SQL_ERROR_ROW(ER_SLAVE_WAS_RUNNING, SLAVE_WAS_RUNNING, 1254)                                                       \
    SQL_ERROR_ROW(ER_SLAVE_WAS_NOT_RUNNING, SLAVE_WAS_NOT_RUNNING, 1255)                                               \
    SQL_ERROR_ROW(ER_TOO_BIG_FOR_UNCOMPRESS, TOO_BIG_FOR_UNCOMPRESS, 1256)                                             \
    SQL_ERROR_ROW(ER_ZLIB_Z_MEM_ERROR, ZLIB_Z_MEM_ERROR, 1257)                                                         \
    SQL_ERROR_ROW(ER_ZLIB_Z_BUF_ERROR, ZLIB_Z_BUF_ERROR, 1258)                                                         \
    SQL_ERROR_ROW(ER_ZLIB_Z_DATA_ERROR, ZLIB_Z_DATA_ERROR, 1259)                                                       \
    SQL_ERROR_ROW(ER_CUT_VALUE_GROUP_CONCAT, CUT_VALUE_GROUP_CONCAT, 1260)                                             \
    SQL_ERROR_ROW(ER_WARN_TOO_FEW_RECORDS, WARN_TOO_FEW_RECORDS, 1261)                                                 \
    SQL_ERROR_ROW(ER_WARN_TOO_MANY_RECORDS, WARN_TOO_MANY_RECORDS, 1262)                                               \
    SQL_ERROR_ROW(ER_WARN_NULL_TO_NOTNULL, WARN_NULL_TO_NOTNULL, 1263)                                                 \
    SQL_ERROR_ROW(ER_WARN_DATA_OUT_OF_RANGE, WARN_DATA_OUT_OF_RANGE, 1264)                                             \
    SQL_ERROR_ROW(WARN_DATA_TRUNCATED, SQLWARN_DATA_TRUNCATED, 1265)                                                   \
    SQL_ERROR_ROW(ER_WARN_USING_OTHER_HANDLER, WARN_USING_OTHER_HANDLER, 1266)                                         \
    SQL_ERROR_ROW(ER_CANT_AGGREGATE_2COLLATIONS, CANT_AGGREGATE_2COLLATIONS, 1267)                                     \
    SQL_ERROR_ROW(ER_DROP_USER, DROP_USER, 1268)                                                                       \
    SQL_ERROR_ROW(ER_REVOKE_GRANTS, REVOKE_GRANTS, 1269)                                                               \
    SQL_ERROR_ROW(ER_CANT_AGGREGATE_3COLLATIONS, CANT_AGGREGATE_3COLLATIONS, 1270)                                     \
    SQL_ERROR_ROW(ER_CANT_AGGREGATE_NCOLLATIONS, CANT_AGGREGATE_NCOLLATIONS, 1271)                                     \
    SQL_ERROR_ROW(ER_VARIABLE_IS_NOT_STRUCT, VARIABLE_IS_NOT_STRUCT, 1272)                                             \
    SQL_ERROR_ROW(ER_UNKNOWN_COLLATION, UNKNOWN_COLLATION, 1273)                                                       \
    SQL_ERROR_ROW(ER_SLAVE_IGNORED_SSL_PARAMS, SLAVE_IGNORED_SSL_PARAMS, 1274)                                         \
    SQL_ERROR_ROW(ER_SERVER_IS_IN_SECURE_AUTH_MODE, SERVER_IS_IN_SECURE_AUTH_MODE, 1275)                               \
    SQL_ERROR_ROW(ER_WARN_FIELD_RESOLVED, WARN_FIELD_RESOLVED, 1276)                                                   \
    SQL_ERROR_ROW(ER_BAD_SLAVE_UNTIL_COND, BAD_SLAVE_UNTIL_COND, 1277)                                                 \
    SQL_ERROR_ROW(ER_MISSING_SKIP_SLAVE, MISSING_SKIP_SLAVE, 1278)                                                     \
    SQL_ERROR_ROW(ER_UNTIL_COND_IGNORED, UNTIL_COND_IGNORED, 1279)                                                     \
    SQL_ERROR_ROW(ER_WRONG_NAME_FOR_INDEX, WRONG_NAME_FOR_INDEX, 1280)                                                 \
    SQL_ERROR_ROW(ER_WRONG_NAME_FOR_CATALOG, WRONG_NAME_FOR_CATALOG, 1281)                                             \
    SQL_ERROR_ROW(ER_WARN_QC_RESIZE, WARN_QC_RESIZE, 1282)                                                             \
    SQL_ERROR_ROW(ER_BAD_FT_COLUMN, BAD_FT_COLUMN, 1283)                                                               \
    SQL_ERROR_ROW(ER_UNKNOWN_KEY_CACHE, UNKNOWN_KEY_CACHE, 1284)                                                       \
    SQL_ERROR_ROW(ER_WARN_HOSTNAME_WONT_WORK, WARN_HOSTNAME_WONT_WORK, 1285)                                           \
    SQL_ERROR_ROW(ER_UNKNOWN_STORAGE_ENGINE, UNKNOWN_STORAGE_ENGINE, 1286)                                             \
    SQL_ERROR_ROW(ER_WARN_DEPRECATED_SYNTAX, WARN_DEPRECATED_SYNTAX, 1287)                                             \
    SQL_ERROR_ROW(ER_NON_UPDATABLE_TABLE, NON_UPDATABLE_TABLE, 1288)                                                   \
    SQL_ERROR_ROW(ER_FEATURE_DISABLED, FEATURE_DISABLED, 1289)                                                         \
    SQL_ERROR_ROW(ER_OPTION_PREVENTS_STATEMENT, OPTION_PREVENTS_STATEMENT, 1290)                                       \
    SQL_ERROR_ROW(ER_DUPLICATED_VALUE_IN_TYPE, DUPLICATED_VALUE_IN_TYPE, 1291)                                         \
    SQL_ERROR_ROW(ER_TRUNCATED_WRONG_VALUE, TRUNCATED_WRONG_VALUE, 1292)                                               \
    SQL_ERROR_ROW(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS, TOO_MUCH_AUTO_TIMESTAMP_COLS, 1293)                                 \
    SQL_ERROR_ROW(ER_INVALID_ON_UPDATE, INVALID_ON_UPDATE, 1294)                                                       \
    SQL_ERROR_ROW(ER_UNSUPPORTED_PS, UNSUPPORTED_PS, 1295)                                                             \
    SQL_ERROR_ROW(ER_GET_ERRMSG, GET_ERRMSG, 1296)                                                                     \
    SQL_ERROR_ROW(ER_GET_TEMPORARY_ERRMSG, GET_TEMPORARY_ERRMSG, 1297)                                                 \
    SQL_ERROR_ROW(ER_UNKNOWN_TIME_ZONE, UNKNOWN_TIME_ZONE, 1298)                                                       \
    SQL_ERROR_ROW(ER_WARN_INVALID_TIMESTAMP, WARN_INVALID_TIMESTAMP, 1299)                                             \
    SQL_ERROR_ROW(ER_INVALID_CHARACTER_STRING, INVALID_CHARACTER_STRING, 1300)                                         \
    SQL_ERROR_ROW(ER_WARN_ALLOWED_PACKET_OVERFLOWED, WARN_ALLOWED_PACKET_OVERFLOWED, 1301)                             \
    SQL_ERROR_ROW(ER_CONFLICTING_DECLARATIONS, CONFLICTING_DECLARATIONS, 1302)                                         \
    SQL_ERROR_ROW(ER_SP_NO_RECURSIVE_CREATE, SP_NO_RECURSIVE_CREATE, 1303)                                             \
    SQL_ERROR_ROW(ER_SP_ALREADY_EXISTS, SP_ALREADY_EXISTS, 1304)                                                       \
    SQL_ERROR_ROW(ER_SP_DOES_NOT_EXIST, SP_DOES_NOT_EXIST, 1305)                                                       \
    SQL_ERROR_ROW(ER_SP_DROP_FAILED, SP_DROP_FAILED, 1306)                                                             \
    SQL_ERROR_ROW(ER_SP_STORE_FAILED, SP_STORE_FAILED, 1307)                                                           \
    SQL_ERROR_ROW(ER_SP_LILABEL_MISMATCH, SP_LILABEL_MISMATCH, 1308)                                                   \
    SQL_ERROR_ROW(ER_SP_LABEL_REDEFINE, SP_LABEL_REDEFINE, 1309)                                                       \
    SQL_ERROR_ROW(ER_SP_LABEL_MISMATCH, SP_LABEL_MISMATCH, 1310)                                                       \
    SQL_ERROR_ROW(ER_SP_UNINIT_VAR, SP_UNINIT_VAR, 1311)                                                               \
    SQL_ERROR_ROW(ER_SP_BADSELECT, SP_BADSELECT, 1312)                                                                 \
    SQL_ERROR_ROW(ER_SP_BADRETURN, SP_BADRETURN, 1313)                                                                 \
    SQL_ERROR_ROW(ER_SP_BADSTATEMENT, SP_BADSTATEMENT, 1314)                                                           \
    SQL_ERROR_ROW(ER_UPDATE_LOG_DEPRECATED_IGNORED, UPDATE_LOG_DEPRECATED_IGNORED, 1315)                               \
    SQL_ERROR_ROW(ER_UPDATE_LOG_DEPRECATED_TRANSLATED, UPDATE_LOG_DEPRECATED_TRANSLATED, 1316)                         \
    SQL_ERROR_ROW(ER_QUERY_INTERRUPTED, QUERY_INTERRUPTED, 1317)                                                       \
    SQL_ERROR_ROW(ER_SP_WRONG_NO_OF_ARGS, SP_WRONG_NO_OF_ARGS, 1318)                                                   \
    SQL_ERROR_ROW(ER_SP_COND_MISMATCH, SP_COND_MISMATCH, 1319)                                                         \
    SQL_ERROR_ROW(ER_SP_NORETURN, SP_NORETURN, 1320)                                                                   \
    SQL_ERROR_ROW(ER_SP_NORETURNEND, SP_NORETURNEND, 1321)                                                             \
    SQL_ERROR_ROW(ER_SP_BAD_CURSOR_QUERY, SP_BAD_CURSOR_QUERY, 1322)                                                   \
    SQL_ERROR_ROW(ER_SP_BAD_CURSOR_SELECT, SP_BAD_CURSOR_SELECT, 1323)                                                 \
    SQL_ERROR_ROW(ER_SP_CURSOR_MISMATCH, SP_CURSOR_MISMATCH, 1324)                                                     \
    SQL_ERROR_ROW(ER_SP_CURSOR_ALREADY_OPEN, SP_CURSOR_ALREADY_OPEN, 1325)                                             \
    SQL_ERROR_ROW(ER_SP_CURSOR_NOT_OPEN, SP_CURSOR_NOT_OPEN, 1326)                                                     \
    SQL_ERROR_ROW(ER_SP_UNDECLARED_VAR, SP_UNDECLARED_VAR, 1327)                                                       \
    SQL_ERROR_ROW(ER_SP_WRONG_NO_OF_FETCH_ARGS, SP_WRONG_NO_OF_FETCH_ARGS, 1328)                                       \
    SQL_ERROR_ROW(ER_SP_FETCH_NO_DATA, SP_FETCH_NO_DATA, 1329)                                                         \
    SQL_ERROR_ROW(ER_SP_DUP_PARAM, SP_DUP_PARAM, 1330)                                                                 \
    SQL_ERROR_ROW(ER_SP_DUP_VAR, SP_DUP_VAR, 1331)                                                                     \
    SQL_ERROR_ROW(ER_SP_DUP_COND, SP_DUP_COND, 1332)                                                                   \
    SQL_ERROR_ROW(ER_SP_DUP_CURS, SP_DUP_CURS, 1333)                                                                   \
    SQL_ERROR_ROW(ER_SP_CANT_ALTER, SP_CANT_ALTER, 1334)                                                               \
    SQL_ERROR_ROW(ER_SP_SUBSELECT_NYI, SP_SUBSELECT_NYI, 1335)                                                         \
    SQL_ERROR_ROW(ER_STMT_NOT_ALLOWED_IN_SF_OR_TRG, STMT_NOT_ALLOWED_IN_SF_OR_TRG, 1336)                               \
    SQL_ERROR_ROW(ER_SP_VARCOND_AFTER_CURSHNDLR, SP_VARCOND_AFTER_CURSHNDLR, 1337)                                     \
    SQL_ERROR_ROW(ER_SP_CURSOR_AFTER_HANDLER, SP_CURSOR_AFTER_HANDLER, 1338)                                           \
    SQL_ERROR_ROW(ER_SP_CASE_NOT_FOUND, SP_CASE_NOT_FOUND, 1339)                                                       \
    SQL_ERROR_ROW(ER_FPARSER_TOO_BIG_FILE, FPARSER_TOO_BIG_FILE, 1340)                                                 \
    SQL_ERROR_ROW(ER_FPARSER_BAD_HEADER, FPARSER_BAD_HEADER, 1341)                                                     \
    SQL_ERROR_ROW(ER_FPARSER_EOF_IN_COMMENT, FPARSER_EOF_IN_COMMENT, 1342)                                             \
    SQL_ERROR_ROW(ER_FPARSER_ERROR_IN_PARAMETER, FPARSER_ERROR_IN_PARAMETER, 1343)                                     \
    SQL_ERROR_ROW(ER_FPARSER_EOF_IN_UNKNOWN_PARAMETER, FPARSER_EOF_IN_UNKNOWN_PARAMETER, 1344)                         \
    SQL_ERROR_ROW(ER_VIEW_NO_EXPLAIN, VIEW_NO_EXPLAIN, 1345)                                                           \
    SQL_ERROR_ROW(ER_FRM_UNKNOWN_TYPE, FRM_UNKNOWN_TYPE, 1346)                                                         \
    SQL_ERROR_ROW(ER_WRONG_OBJECT, WRONG_OBJECT, 1347)                                                                 \
    SQL_ERROR_ROW(ER_NONUPDATEABLE_COLUMN, NONUPDATEABLE_COLUMN, 1348)                                                 \
    SQL_ERROR_ROW(ER_VIEW_SELECT_DERIVED, VIEW_SELECT_DERIVED, 1349)                                                   \
    SQL_ERROR_ROW(ER_VIEW_SELECT_CLAUSE, VIEW_SELECT_CLAUSE, 1350)                                                     \
    SQL_ERROR_ROW(ER_VIEW_SELECT_VARIABLE, VIEW_SELECT_VARIABLE, 1351)                                                 \
    SQL_ERROR_ROW(ER_VIEW_SELECT_TMPTABLE, VIEW_SELECT_TMPTABLE, 1352)                                                 \
    SQL_ERROR_ROW(ER_VIEW_WRONG_LIST, VIEW_WRONG_LIST, 1353)                                                           \
    SQL_ERROR_ROW(ER_WARN_VIEW_MERGE, WARN_VIEW_MERGE, 1354)                                                           \
    SQL_ERROR_ROW(ER_WARN_VIEW_WITHOUT_KEY, WARN_VIEW_WITHOUT_KEY, 1355)                                               \
    SQL_ERROR_ROW(ER_VIEW_INVALID, VIEW_INVALID, 1356)                                                                 \
    SQL_ERROR_ROW(ER_SP_NO_DROP_SP, SP_NO_DROP_SP, 1357)                                                               \
    SQL_ERROR_ROW(ER_SP_GOTO_IN_HNDLR, SP_GOTO_IN_HNDLR, 1358)                                                         \
    SQL_ERROR_ROW(ER_TRG_ALREADY_EXISTS, TRG_ALREADY_EXISTS, 1359)                                                     \
    SQL_ERROR_ROW(ER_TRG_DOES_NOT_EXIST, TRG_DOES_NOT_EXIST, 1360)                                                     \
    SQL_ERROR_ROW(ER_TRG_ON_VIEW_OR_TEMP_TABLE, TRG_ON_VIEW_OR_TEMP_TABLE, 1361)                                       \
    SQL_ERROR_ROW(ER_TRG_CANT_CHANGE_ROW, TRG_CANT_CHANGE_ROW, 1362)                                                   \
    SQL_ERROR_ROW(ER_TRG_NO_SUCH_ROW_IN_TRG, TRG_NO_SUCH_ROW_IN_TRG, 1363)                                             \
    SQL_ERROR_ROW(ER_NO_DEFAULT_FOR_FIELD, NO_DEFAULT_FOR_FIELD, 1364)                                                 \
    SQL_ERROR_ROW(ER_DIVISION_BY_ZERO, DIVISION_BY_ZERO, 1365)                                                         \
    SQL_ERROR_ROW(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD, TRUNCATED_WRONG_VALUE_FOR_FIELD, 1366)                           \
    SQL_ERROR_ROW(ER_ILLEGAL_VALUE_FOR_TYPE, ILLEGAL_VALUE_FOR_TYPE, 1367)                                             \
    SQL_ERROR_ROW(ER_VIEW_NONUPD_CHECK, VIEW_NONUPD_CHECK, 1368)                                                       \
    SQL_ERROR_ROW(ER_VIEW_CHECK_FAILED, VIEW_CHECK_FAILED, 1369)                                                       \
    SQL_ERROR_ROW(ER_PROCACCESS_DENIED_ERROR, PROCACCESS_DENIED_ERROR, 1370)                                           \
    SQL_ERROR_ROW(ER_RELAY_LOG_FAIL, RELAY_LOG_FAIL, 1371)                                                             \
    SQL_ERROR_ROW(ER_PASSWD_LENGTH, PASSWD_LENGTH, 1372)                                                               \
    SQL_ERROR_ROW(ER_UNKNOWN_TARGET_BINLOG, UNKNOWN_TARGET_BINLOG, 1373)                                               \
    SQL_ERROR_ROW(ER_IO_ERR_LOG_INDEX_READ, IO_ERR_LOG_INDEX_READ, 1374)                                               \
    SQL_ERROR_ROW(ER_BINLOG_PURGE_PROHIBITED, BINLOG_PURGE_PROHIBITED, 1375)                                           \
    SQL_ERROR_ROW(ER_FSEEK_FAIL, FSEEK_FAIL, 1376)                                                                     \
    SQL_ERROR_ROW(ER_BINLOG_PURGE_FATAL_ERR, BINLOG_PURGE_FATAL_ERR, 1377)                                             \
    SQL_ERROR_ROW(ER_LOG_IN_USE, LOG_IN_USE, 1378)                                                                     \
    SQL_ERROR_ROW(ER_LOG_PURGE_UNKNOWN_ERR, LOG_PURGE_UNKNOWN_ERR, 1379)                                               \
    SQL_ERROR_ROW(ER_RELAY_LOG_INIT, RELAY_LOG_INIT, 1380)                                                             \
    SQL_ERROR_ROW(ER_NO_BINARY_LOGGING, NO_BINARY_LOGGING, 1381)                                                       \
    SQL_ERROR_ROW(ER_RESERVED_SYNTAX, RESERVED_SYNTAX, 1382)                                                           \
    SQL_ERROR_ROW(ER_WSAS_FAILED, WSAS_FAILED, 1383)                                                                   \
    SQL_ERROR_ROW(ER_DIFF_GROUPS_PROC, DIFF_GROUPS_PROC, 1384)                                                         \
    SQL_ERROR_ROW(ER_NO_GROUP_FOR_PROC, NO_GROUP_FOR_PROC, 1385)                                                       \
    SQL_ERROR_ROW(ER_ORDER_WITH_PROC, ORDER_WITH_PROC, 1386)                                                           \
    SQL_ERROR_ROW(ER_LOGGING_PROHIBIT_CHANGING_OF, LOGGING_PROHIBIT_CHANGING_OF, 1387)                                 \
    SQL_ERROR_ROW(ER_NO_FILE_MAPPING, NO_FILE_MAPPING, 1388)                                                           \
    SQL_ERROR_ROW(ER_WRONG_MAGIC, WRONG_MAGIC, 1389)                                                                   \
    SQL_ERROR_ROW(ER_PS_MANY_PARAM, PS_MANY_PARAM, 1390)                                                               \
    SQL_ERROR_ROW(ER_KEY_PART_0, KEY_PART_0, 1391)                                                                     \
    SQL_ERROR_ROW(ER_VIEW_CHECKSUM, VIEW_CHECKSUM, 1392)                                                               \
    SQL_ERROR_ROW(ER_VIEW_MULTIUPDATE, VIEW_MULTIUPDATE, 1393)                                                         \
    SQL_ERROR_ROW(ER_VIEW_NO_INSERT_FIELD_LIST, VIEW_NO_INSERT_FIELD_LIST, 1394)                                       \
    SQL_ERROR_ROW(ER_VIEW_DELETE_MERGE_VIEW, VIEW_DELETE_MERGE_VIEW, 1395)                                             \
    SQL_ERROR_ROW(ER_CANNOT_USER, CANNOT_USER, 1396)                                                                   \
    SQL_ERROR_ROW(ER_XAER_NOTA, XAER_NOTA, 1397)                                                                       \
    SQL_ERROR_ROW(ER_XAER_INVAL, XAER_INVAL, 1398)                                                                     \
    SQL_ERROR_ROW(ER_XAER_RMFAIL, XAER_RMFAIL, 1399)                                                                   \
    SQL_ERROR_ROW(ER_XAER_OUTSIDE, XAER_OUTSIDE, 1400)                                                                 \
    SQL_ERROR_ROW(ER_XAER_RMERR, XAER_RMERR, 1401)                                                                     \
    SQL_ERROR_ROW(ER_XA_RBROLLBACK, XA_RBROLLBACK, 1402)                                                               \
    SQL_ERROR_ROW(ER_NONEXISTING_PROC_GRANT, NONEXISTING_PROC_GRANT, 1403)                                             \
    SQL_ERROR_ROW(ER_PROC_AUTO_GRANT_FAIL, PROC_AUTO_GRANT_FAIL, 1404)                                                 \
    SQL_ERROR_ROW(ER_PROC_AUTO_REVOKE_FAIL, PROC_AUTO_REVOKE_FAIL, 1405)                                               \
    SQL_ERROR_ROW(ER_DATA_TOO_LONG, DATA_TOO_LONG, 1406)                                                               \
    SQL_ERROR_ROW(ER_SP_BAD_SQLSTATE, SP_BAD_SQLSTATE, 1407)                                                           \
    SQL_ERROR_ROW(ER_STARTUP, STARTUP, 1408)                                                                           \
    SQL_ERROR_ROW(ER_LOAD_FROM_FIXED_SIZE_ROWS_TO_VAR, LOAD_FROM_FIXED_SIZE_ROWS_TO_VAR, 1409)                         \
    SQL_ERROR_ROW(ER_CANT_CREATE_USER_WITH_GRANT, CANT_CREATE_USER_WITH_GRANT, 1410)                                   \
    SQL_ERROR_ROW(ER_WRONG_VALUE_FOR_TYPE, WRONG_VALUE_FOR_TYPE, 1411)                                                 \
    SQL_ERROR_ROW(ER_TABLE_DEF_CHANGED, TABLE_DEF_CHANGED, 1412)                                                       \
    SQL_ERROR_ROW(ER_SP_DUP_HANDLER, SP_DUP_HANDLER, 1413)                                                             \
    SQL_ERROR_ROW(ER_SP_NOT_VAR_ARG, SP_NOT_VAR_ARG, 1414)                                                             \
    SQL_ERROR_ROW(ER_SP_NO_RETSET, SP_NO_RETSET, 1415)                                                                 \
    SQL_ERROR_ROW(ER_CANT_CREATE_GEOMETRY_OBJECT, CANT_CREATE_GEOMETRY_OBJECT, 1416)                                   \
    SQL_ERROR_ROW(ER_FAILED_ROUTINE_BREAK_BINLOG, FAILED_ROUTINE_BREAK_BINLOG, 1417)                                   \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_ROUTINE, BINLOG_UNSAFE_ROUTINE, 1418)                                               \
    SQL_ERROR_ROW(ER_BINLOG_CREATE_ROUTINE_NEED_SUPER, BINLOG_CREATE_ROUTINE_NEED_SUPER, 1419)                         \
    SQL_ERROR_ROW(ER_EXEC_STMT_WITH_OPEN_CURSOR, EXEC_STMT_WITH_OPEN_CURSOR, 1420)                                     \
    SQL_ERROR_ROW(ER_STMT_HAS_NO_OPEN_CURSOR, STMT_HAS_NO_OPEN_CURSOR, 1421)                                           \
    SQL_ERROR_ROW(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, 1422)                           \
    SQL_ERROR_ROW(ER_NO_DEFAULT_FOR_VIEW_FIELD, NO_DEFAULT_FOR_VIEW_FIELD, 1423)                                       \
    SQL_ERROR_ROW(ER_SP_NO_RECURSION, SP_NO_RECURSION, 1424)                                                           \
    SQL_ERROR_ROW(ER_TOO_BIG_SCALE, TOO_BIG_SCALE, 1425)                                                               \
    SQL_ERROR_ROW(ER_TOO_BIG_PRECISION, TOO_BIG_PRECISION, 1426)                                                       \
    SQL_ERROR_ROW(ER_M_BIGGER_THAN_D, M_BIGGER_THAN_D, 1427)                                                           \
    SQL_ERROR_ROW(ER_WRONG_LOCK_OF_SYSTEM_TABLE, WRONG_LOCK_OF_SYSTEM_TABLE, 1428)                                     \
    SQL_ERROR_ROW(ER_CONNECT_TO_FOREIGN_DATA_SOURCE, CONNECT_TO_FOREIGN_DATA_SOURCE, 1429)                             \
    SQL_ERROR_ROW(ER_QUERY_ON_FOREIGN_DATA_SOURCE, QUERY_ON_FOREIGN_DATA_SOURCE, 1430)                                 \
    SQL_ERROR_ROW(ER_FOREIGN_DATA_SOURCE_DOESNT_EXIST, FOREIGN_DATA_SOURCE_DOESNT_EXIST, 1431)                         \
    SQL_ERROR_ROW(ER_FOREIGN_DATA_STRING_INVALID_CANT_CREATE, FOREIGN_DATA_STRING_INVALID_CANT_CREATE, 1432)           \
    SQL_ERROR_ROW(ER_FOREIGN_DATA_STRING_INVALID, FOREIGN_DATA_STRING_INVALID, 1433)                                   \
    SQL_ERROR_ROW(ER_CANT_CREATE_FEDERATED_TABLE, CANT_CREATE_FEDERATED_TABLE, 1434)                                   \
    SQL_ERROR_ROW(ER_TRG_IN_WRONG_SCHEMA, TRG_IN_WRONG_SCHEMA, 1435)                                                   \
    SQL_ERROR_ROW(ER_STACK_OVERRUN_NEED_MORE, STACK_OVERRUN_NEED_MORE, 1436)                                           \
    SQL_ERROR_ROW(ER_TOO_LONG_BODY, TOO_LONG_BODY, 1437)                                                               \
    SQL_ERROR_ROW(ER_WARN_CANT_DROP_DEFAULT_KEYCACHE, WARN_CANT_DROP_DEFAULT_KEYCACHE, 1438)                           \
    SQL_ERROR_ROW(ER_TOO_BIG_DISPLAYWIDTH, TOO_BIG_DISPLAYWIDTH, 1439)                                                 \
    SQL_ERROR_ROW(ER_XAER_DUPID, XAER_DUPID, 1440)                                                                     \
    SQL_ERROR_ROW(ER_DATETIME_FUNCTION_OVERFLOW, DATETIME_FUNCTION_OVERFLOW, 1441)                                     \
    SQL_ERROR_ROW(ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG, CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG, 1442)                   \
    SQL_ERROR_ROW(ER_VIEW_PREVENT_UPDATE, VIEW_PREVENT_UPDATE, 1443)                                                   \
    SQL_ERROR_ROW(ER_PS_NO_RECURSION, PS_NO_RECURSION, 1444)                                                           \
    SQL_ERROR_ROW(ER_SP_CANT_SET_AUTOCOMMIT, SP_CANT_SET_AUTOCOMMIT, 1445)                                             \
    SQL_ERROR_ROW(ER_MALFORMED_DEFINER, MALFORMED_DEFINER, 1446)                                                       \
    SQL_ERROR_ROW(ER_VIEW_FRM_NO_USER, VIEW_FRM_NO_USER, 1447)                                                         \
    SQL_ERROR_ROW(ER_VIEW_OTHER_USER, VIEW_OTHER_USER, 1448)                                                           \
    SQL_ERROR_ROW(ER_NO_SUCH_USER, NO_SUCH_USER, 1449)                                                                 \
    SQL_ERROR_ROW(ER_FORBID_SCHEMA_CHANGE, FORBID_SCHEMA_CHANGE, 1450)                                                 \
    SQL_ERROR_ROW(ER_ROW_IS_REFERENCED_2, ROW_IS_REFERENCED_2, 1451)                                                   \
    SQL_ERROR_ROW(ER_NO_REFERENCED_ROW_2, NO_REFERENCED_ROW_2, 1452)                                                   \
    SQL_ERROR_ROW(ER_SP_BAD_VAR_SHADOW, SP_BAD_VAR_SHADOW, 1453)                                                       \
    SQL_ERROR_ROW(ER_TRG_NO_DEFINER, TRG_NO_DEFINER, 1454)                                                             \
    SQL_ERROR_ROW(ER_OLD_FILE_FORMAT, OLD_FILE_FORMAT, 1455)                                                           \
    SQL_ERROR_ROW(ER_SP_RECURSION_LIMIT, SP_RECURSION_LIMIT, 1456)                                                     \
    SQL_ERROR_ROW(ER_SP_PROC_TABLE_CORRUPT, SP_PROC_TABLE_CORRUPT, 1457)                                               \
    SQL_ERROR_ROW(ER_SP_WRONG_NAME, SP_WRONG_NAME, 1458)                                                               \
    SQL_ERROR_ROW(ER_TABLE_NEEDS_UPGRADE, TABLE_NEEDS_UPGRADE, 1459)                                                   \
    SQL_ERROR_ROW(ER_SP_NO_AGGREGATE, SP_NO_AGGREGATE, 1460)                                                           \
    SQL_ERROR_ROW(ER_MAX_PREPARED_STMT_COUNT_REACHED, MAX_PREPARED_STMT_COUNT_REACHED, 1461)                           \
    SQL_ERROR_ROW(ER_VIEW_RECURSIVE, VIEW_RECURSIVE, 1462)                                                             \
    SQL_ERROR_ROW(ER_NON_GROUPING_FIELD_USED, NON_GROUPING_FIELD_USED, 1463)                                           \
    SQL_ERROR_ROW(ER_TABLE_CANT_HANDLE_SPKEYS, TABLE_CANT_HANDLE_SPKEYS, 1464)                                         \
    SQL_ERROR_ROW(ER_NO_TRIGGERS_ON_SYSTEM_SCHEMA, NO_TRIGGERS_ON_SYSTEM_SCHEMA, 1465)                                 \
    SQL_ERROR_ROW(ER_REMOVED_SPACES, REMOVED_SPACES, 1466)                                                             \
    SQL_ERROR_ROW(ER_AUTOINC_READ_FAILED, AUTOINC_READ_FAILED, 1467)                                                   \
    SQL_ERROR_ROW(ER_USERNAME, USERNAME, 1468)                                                                         \
    SQL_ERROR_ROW(ER_HOSTNAME, HOSTNAME, 1469)                                                                         \
    SQL_ERROR_ROW(ER_WRONG_STRING_LENGTH, WRONG_STRING_LENGTH, 1470)                                                   \
    SQL_ERROR_ROW(ER_NON_INSERTABLE_TABLE, NON_INSERTABLE_TABLE, 1471)                                                 \
    SQL_ERROR_ROW(ER_ADMIN_WRONG_MRG_TABLE, ADMIN_WRONG_MRG_TABLE, 1472)                                               \
    SQL_ERROR_ROW(ER_TOO_HIGH_LEVEL_OF_NESTING_FOR_SELECT, TOO_HIGH_LEVEL_OF_NESTING_FOR_SELECT, 1473)                 \
    SQL_ERROR_ROW(ER_NAME_BECOMES_EMPTY, NAME_BECOMES_EMPTY, 1474)                                                     \
    SQL_ERROR_ROW(ER_AMBIGUOUS_FIELD_TERM, AMBIGUOUS_FIELD_TERM, 1475)                                                 \
    SQL_ERROR_ROW(ER_FOREIGN_SERVER_EXISTS, FOREIGN_SERVER_EXISTS, 1476)                                               \
    SQL_ERROR_ROW(ER_FOREIGN_SERVER_DOESNT_EXIST, FOREIGN_SERVER_DOESNT_EXIST, 1477)                                   \
    SQL_ERROR_ROW(ER_ILLEGAL_HA_CREATE_OPTION, ILLEGAL_HA_CREATE_OPTION, 1478)                                         \
    SQL_ERROR_ROW(ER_PARTITION_REQUIRES_VALUES_ERROR, PARTITION_REQUIRES_VALUES_ERROR, 1479)                           \
    SQL_ERROR_ROW(ER_PARTITION_WRONG_VALUES_ERROR, PARTITION_WRONG_VALUES_ERROR, 1480)                                 \
    SQL_ERROR_ROW(ER_PARTITION_MAXVALUE_ERROR, PARTITION_MAXVALUE_ERROR, 1481)                                         \
    SQL_ERROR_ROW(ER_PARTITION_SUBPARTITION_ERROR, PARTITION_SUBPARTITION_ERROR, 1482)                                 \
    SQL_ERROR_ROW(ER_PARTITION_SUBPART_MIX_ERROR, PARTITION_SUBPART_MIX_ERROR, 1483)                                   \
    SQL_ERROR_ROW(ER_PARTITION_WRONG_NO_PART_ERROR, PARTITION_WRONG_NO_PART_ERROR, 1484)                               \
    SQL_ERROR_ROW(ER_PARTITION_WRONG_NO_SUBPART_ERROR, PARTITION_WRONG_NO_SUBPART_ERROR, 1485)                         \
    SQL_ERROR_ROW(ER_WRONG_EXPR_IN_PARTITION_FUNC_ERROR, WRONG_EXPR_IN_PARTITION_FUNC_ERROR, 1486)                     \
    SQL_ERROR_ROW(ER_NOT_CONSTANT_EXPRESSION, NOT_CONSTANT_EXPRESSION, 1487)                                           \
    SQL_ERROR_ROW(ER_FIELD_NOT_FOUND_PART_ERROR, FIELD_NOT_FOUND_PART_ERROR, 1488)                                     \
    SQL_ERROR_ROW(ER_LIST_OF_FIELDS_ONLY_IN_HASH_ERROR, LIST_OF_FIELDS_ONLY_IN_HASH_ERROR, 1489)                       \
    SQL_ERROR_ROW(ER_INCONSISTENT_PARTITION_INFO_ERROR, INCONSISTENT_PARTITION_INFO_ERROR, 1490)                       \
    SQL_ERROR_ROW(ER_PARTITION_FUNC_NOT_ALLOWED_ERROR, PARTITION_FUNC_NOT_ALLOWED_ERROR, 1491)                         \
    SQL_ERROR_ROW(ER_PARTITIONS_MUST_BE_DEFINED_ERROR, PARTITIONS_MUST_BE_DEFINED_ERROR, 1492)                         \
    SQL_ERROR_ROW(ER_RANGE_NOT_INCREASING_ERROR, RANGE_NOT_INCREASING_ERROR, 1493)                                     \
    SQL_ERROR_ROW(ER_INCONSISTENT_TYPE_OF_FUNCTIONS_ERROR, INCONSISTENT_TYPE_OF_FUNCTIONS_ERROR, 1494)                 \
    SQL_ERROR_ROW(ER_MULTIPLE_DEF_CONST_IN_LIST_PART_ERROR, MULTIPLE_DEF_CONST_IN_LIST_PART_ERROR, 1495)               \
    SQL_ERROR_ROW(ER_PARTITION_ENTRY_ERROR, PARTITION_ENTRY_ERROR, 1496)                                               \
    SQL_ERROR_ROW(ER_MIX_HANDLER_ERROR, MIX_HANDLER_ERROR, 1497)                                                       \
    SQL_ERROR_ROW(ER_PARTITION_NOT_DEFINED_ERROR, PARTITION_NOT_DEFINED_ERROR, 1498)                                   \
    SQL_ERROR_ROW(ER_TOO_MANY_PARTITIONS_ERROR, TOO_MANY_PARTITIONS_ERROR, 1499)                                       \
    SQL_ERROR_ROW(ER_SUBPARTITION_ERROR, SUBPARTITION_ERROR, 1500)                                                     \
    SQL_ERROR_ROW(ER_CANT_CREATE_HANDLER_FILE, CANT_CREATE_HANDLER_FILE, 1501)                                         \
    SQL_ERROR_ROW(ER_BLOB_FIELD_IN_PART_FUNC_ERROR, BLOB_FIELD_IN_PART_FUNC_ERROR, 1502)                               \
    SQL_ERROR_ROW(ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF, UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF, 1503)                         \
    SQL_ERROR_ROW(ER_NO_PARTS_ERROR, NO_PARTS_ERROR, 1504)                                                             \
    SQL_ERROR_ROW(ER_PARTITION_MGMT_ON_NONPARTITIONED, PARTITION_MGMT_ON_NONPARTITIONED, 1505)                         \
    SQL_ERROR_ROW(ER_FEATURE_NOT_SUPPORTED_WITH_PARTITIONING, FEATURE_NOT_SUPPORTED_WITH_PARTITIONING, 1506)           \
    SQL_ERROR_ROW(ER_PARTITION_DOES_NOT_EXIST, PARTITION_DOES_NOT_EXIST, 1507)                                         \
    SQL_ERROR_ROW(ER_DROP_LAST_PARTITION, DROP_LAST_PARTITION, 1508)                                                   \
    SQL_ERROR_ROW(ER_COALESCE_ONLY_ON_HASH_PARTITION, COALESCE_ONLY_ON_HASH_PARTITION, 1509)                           \
    SQL_ERROR_ROW(ER_REORG_HASH_ONLY_ON_SAME_NO, REORG_HASH_ONLY_ON_SAME_NO, 1510)                                     \
    SQL_ERROR_ROW(ER_REORG_NO_PARAM_ERROR, REORG_NO_PARAM_ERROR, 1511)                                                 \
    SQL_ERROR_ROW(ER_ONLY_ON_RANGE_LIST_PARTITION, ONLY_ON_RANGE_LIST_PARTITION, 1512)                                 \
    SQL_ERROR_ROW(ER_ADD_PARTITION_SUBPART_ERROR, ADD_PARTITION_SUBPART_ERROR, 1513)                                   \
    SQL_ERROR_ROW(ER_ADD_PARTITION_NO_NEW_PARTITION, ADD_PARTITION_NO_NEW_PARTITION, 1514)                             \
    SQL_ERROR_ROW(ER_COALESCE_PARTITION_NO_PARTITION, COALESCE_PARTITION_NO_PARTITION, 1515)                           \
    SQL_ERROR_ROW(ER_REORG_PARTITION_NOT_EXIST, REORG_PARTITION_NOT_EXIST, 1516)                                       \
    SQL_ERROR_ROW(ER_SAME_NAME_PARTITION, SAME_NAME_PARTITION, 1517)                                                   \
    SQL_ERROR_ROW(ER_NO_BINLOG_ERROR, NO_BINLOG_ERROR, 1518)                                                           \
    SQL_ERROR_ROW(ER_CONSECUTIVE_REORG_PARTITIONS, CONSECUTIVE_REORG_PARTITIONS, 1519)                                 \
    SQL_ERROR_ROW(ER_REORG_OUTSIDE_RANGE, REORG_OUTSIDE_RANGE, 1520)                                                   \
    SQL_ERROR_ROW(ER_PARTITION_FUNCTION_FAILURE, PARTITION_FUNCTION_FAILURE, 1521)                                     \
    SQL_ERROR_ROW(ER_PART_STATE_ERROR, PART_STATE_ERROR, 1522)                                                         \
    SQL_ERROR_ROW(ER_LIMITED_PART_RANGE, LIMITED_PART_RANGE, 1523)                                                     \
    SQL_ERROR_ROW(ER_PLUGIN_IS_NOT_LOADED, PLUGIN_IS_NOT_LOADED, 1524)                                                 \
    SQL_ERROR_ROW(ER_WRONG_VALUE, WRONG_VALUE, 1525)                                                                   \
    SQL_ERROR_ROW(ER_NO_PARTITION_FOR_GIVEN_VALUE, NO_PARTITION_FOR_GIVEN_VALUE, 1526)                                 \
    SQL_ERROR_ROW(ER_FILEGROUP_OPTION_ONLY_ONCE, FILEGROUP_OPTION_ONLY_ONCE, 1527)                                     \
    SQL_ERROR_ROW(ER_CREATE_FILEGROUP_FAILED, CREATE_FILEGROUP_FAILED, 1528)                                           \
    SQL_ERROR_ROW(ER_DROP_FILEGROUP_FAILED, DROP_FILEGROUP_FAILED, 1529)                                               \
    SQL_ERROR_ROW(ER_TABLESPACE_AUTO_EXTEND_ERROR, TABLESPACE_AUTO_EXTEND_ERROR, 1530)                                 \
    SQL_ERROR_ROW(ER_WRONG_SIZE_NUMBER, WRONG_SIZE_NUMBER, 1531)                                                       \
    SQL_ERROR_ROW(ER_SIZE_OVERFLOW_ERROR, SIZE_OVERFLOW_ERROR, 1532)                                                   \
    SQL_ERROR_ROW(ER_ALTER_FILEGROUP_FAILED, ALTER_FILEGROUP_FAILED, 1533)                                             \
    SQL_ERROR_ROW(ER_BINLOG_ROW_LOGGING_FAILED, BINLOG_ROW_LOGGING_FAILED, 1534)                                       \
    SQL_ERROR_ROW(ER_BINLOG_ROW_WRONG_TABLE_DEF, BINLOG_ROW_WRONG_TABLE_DEF, 1535)                                     \
    SQL_ERROR_ROW(ER_BINLOG_ROW_RBR_TO_SBR, BINLOG_ROW_RBR_TO_SBR, 1536)                                               \
    SQL_ERROR_ROW(ER_EVENT_ALREADY_EXISTS, EVENT_ALREADY_EXISTS, 1537)                                                 \
    SQL_ERROR_ROW(ER_EVENT_STORE_FAILED, EVENT_STORE_FAILED, 1538)                                                     \
    SQL_ERROR_ROW(ER_EVENT_DOES_NOT_EXIST, EVENT_DOES_NOT_EXIST, 1539)                                                 \
    SQL_ERROR_ROW(ER_EVENT_CANT_ALTER, EVENT_CANT_ALTER, 1540)                                                         \
    SQL_ERROR_ROW(ER_EVENT_DROP_FAILED, EVENT_DROP_FAILED, 1541)                                                       \
    SQL_ERROR_ROW(ER_EVENT_INTERVAL_NOT_POSITIVE_OR_TOO_BIG, EVENT_INTERVAL_NOT_POSITIVE_OR_TOO_BIG, 1542)             \
    SQL_ERROR_ROW(ER_EVENT_ENDS_BEFORE_STARTS, EVENT_ENDS_BEFORE_STARTS, 1543)                                         \
    SQL_ERROR_ROW(ER_EVENT_EXEC_TIME_IN_THE_PAST, EVENT_EXEC_TIME_IN_THE_PAST, 1544)                                   \
    SQL_ERROR_ROW(ER_EVENT_OPEN_TABLE_FAILED, EVENT_OPEN_TABLE_FAILED, 1545)                                           \
    SQL_ERROR_ROW(ER_EVENT_NEITHER_M_EXPR_NOR_M_AT, EVENT_NEITHER_M_EXPR_NOR_M_AT, 1546)                               \
    SQL_ERROR_ROW(ER_UNUSED_2, UNUSED_2, 1547)                                                                         \
    SQL_ERROR_ROW(ER_UNUSED_3, UNUSED_3, 1548)                                                                         \
    SQL_ERROR_ROW(ER_EVENT_CANNOT_DELETE, EVENT_CANNOT_DELETE, 1549)                                                   \
    SQL_ERROR_ROW(ER_EVENT_COMPILE_ERROR, EVENT_COMPILE_ERROR, 1550)                                                   \
    SQL_ERROR_ROW(ER_EVENT_SAME_NAME, EVENT_SAME_NAME, 1551)                                                           \
    SQL_ERROR_ROW(ER_EVENT_DATA_TOO_LONG, EVENT_DATA_TOO_LONG, 1552)                                                   \
    SQL_ERROR_ROW(ER_DROP_INDEX_FK, DROP_INDEX_FK, 1553)                                                               \
    SQL_ERROR_ROW(ER_WARN_DEPRECATED_SYNTAX_WITH_VER, WARN_DEPRECATED_SYNTAX_WITH_VER, 1554)                           \
    SQL_ERROR_ROW(ER_CANT_WRITE_LOCK_LOG_TABLE, CANT_WRITE_LOCK_LOG_TABLE, 1555)                                       \
    SQL_ERROR_ROW(ER_CANT_LOCK_LOG_TABLE, CANT_LOCK_LOG_TABLE, 1556)                                                   \
    SQL_ERROR_ROW(ER_UNUSED_4, UNUSED_4, 1557)                                                                         \
    SQL_ERROR_ROW(ER_COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE, COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE, 1558)                 \
    SQL_ERROR_ROW(ER_TEMP_TABLE_PREVENTS_SWITCH_OUT_OF_RBR, TEMP_TABLE_PREVENTS_SWITCH_OUT_OF_RBR, 1559)               \
    SQL_ERROR_ROW(ER_STORED_FUNCTION_PREVENTS_SWITCH_BINLOG_FORMAT, STORED_FUNCTION_PREVENTS_SWITCH_BINLOG_FORMAT,     \
                  1560)                                                                                                \
    SQL_ERROR_ROW(ER_UNUSED_13, UNUSED_13, 1561)                                                                       \
    SQL_ERROR_ROW(ER_PARTITION_NO_TEMPORARY, PARTITION_NO_TEMPORARY, 1562)                                             \
    SQL_ERROR_ROW(ER_PARTITION_CONST_DOMAIN_ERROR, PARTITION_CONST_DOMAIN_ERROR, 1563)                                 \
    SQL_ERROR_ROW(ER_PARTITION_FUNCTION_IS_NOT_ALLOWED, PARTITION_FUNCTION_IS_NOT_ALLOWED, 1564)                       \
    SQL_ERROR_ROW(ER_DDL_LOG_ERROR, DDL_LOG_ERROR, 1565)                                                               \
    SQL_ERROR_ROW(ER_NULL_IN_VALUES_LESS_THAN, NULL_IN_VALUES_LESS_THAN, 1566)                                         \
    SQL_ERROR_ROW(ER_WRONG_PARTITION_NAME, WRONG_PARTITION_NAME, 1567)                                                 \
    SQL_ERROR_ROW(ER_CANT_CHANGE_TX_CHARACTERISTICS, CANT_CHANGE_TX_CHARACTERISTICS, 1568)                             \
    SQL_ERROR_ROW(ER_DUP_ENTRY_AUTOINCREMENT_CASE, DUP_ENTRY_AUTOINCREMENT_CASE, 1569)                                 \
    SQL_ERROR_ROW(ER_EVENT_MODIFY_QUEUE_ERROR, EVENT_MODIFY_QUEUE_ERROR, 1570)                                         \
    SQL_ERROR_ROW(ER_EVENT_SET_VAR_ERROR, EVENT_SET_VAR_ERROR, 1571)                                                   \
    SQL_ERROR_ROW(ER_PARTITION_MERGE_ERROR, PARTITION_MERGE_ERROR, 1572)                                               \
    SQL_ERROR_ROW(ER_CANT_ACTIVATE_LOG, CANT_ACTIVATE_LOG, 1573)                                                       \
    SQL_ERROR_ROW(ER_RBR_NOT_AVAILABLE, RBR_NOT_AVAILABLE, 1574)                                                       \
    SQL_ERROR_ROW(ER_BASE64_DECODE_ERROR, BASE64_DECODE_ERROR, 1575)                                                   \
    SQL_ERROR_ROW(ER_EVENT_RECURSION_FORBIDDEN, EVENT_RECURSION_FORBIDDEN, 1576)                                       \
    SQL_ERROR_ROW(ER_EVENTS_DB_ERROR, EVENTS_DB_ERROR, 1577)                                                           \
    SQL_ERROR_ROW(ER_ONLY_INTEGERS_ALLOWED, ONLY_INTEGERS_ALLOWED, 1578)                                               \
    SQL_ERROR_ROW(ER_UNSUPORTED_LOG_ENGINE, UNSUPORTED_LOG_ENGINE, 1579)                                               \
    SQL_ERROR_ROW(ER_BAD_LOG_STATEMENT, BAD_LOG_STATEMENT, 1580)                                                       \
    SQL_ERROR_ROW(ER_CANT_RENAME_LOG_TABLE, CANT_RENAME_LOG_TABLE, 1581)                                               \
    SQL_ERROR_ROW(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, WRONG_PARAMCOUNT_TO_NATIVE_FCT, 1582)                             \
    SQL_ERROR_ROW(ER_WRONG_PARAMETERS_TO_NATIVE_FCT, WRONG_PARAMETERS_TO_NATIVE_FCT, 1583)                             \
    SQL_ERROR_ROW(ER_WRONG_PARAMETERS_TO_STORED_FCT, WRONG_PARAMETERS_TO_STORED_FCT, 1584)                             \
    SQL_ERROR_ROW(ER_NATIVE_FCT_NAME_COLLISION, NATIVE_FCT_NAME_COLLISION, 1585)                                       \
    SQL_ERROR_ROW(ER_DUP_ENTRY_WITH_KEY_NAME, DUP_ENTRY_WITH_KEY_NAME, 1586)                                           \
    SQL_ERROR_ROW(ER_BINLOG_PURGE_EMFILE, BINLOG_PURGE_EMFILE, 1587)                                                   \
    SQL_ERROR_ROW(ER_EVENT_CANNOT_CREATE_IN_THE_PAST, EVENT_CANNOT_CREATE_IN_THE_PAST, 1588)                           \
    SQL_ERROR_ROW(ER_EVENT_CANNOT_ALTER_IN_THE_PAST, EVENT_CANNOT_ALTER_IN_THE_PAST, 1589)                             \
    SQL_ERROR_ROW(ER_SLAVE_INCIDENT, SLAVE_INCIDENT, 1590)                                                             \
    SQL_ERROR_ROW(ER_NO_PARTITION_FOR_GIVEN_VALUE_SILENT, NO_PARTITION_FOR_GIVEN_VALUE_SILENT, 1591)                   \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_STATEMENT, BINLOG_UNSAFE_STATEMENT, 1592)                                           \
    SQL_ERROR_ROW(ER_SLAVE_FATAL_ERROR, SLAVE_FATAL_ERROR, 1593)                                                       \
    SQL_ERROR_ROW(ER_SLAVE_RELAY_LOG_READ_FAILURE, SLAVE_RELAY_LOG_READ_FAILURE, 1594)                                 \
    SQL_ERROR_ROW(ER_SLAVE_RELAY_LOG_WRITE_FAILURE, SLAVE_RELAY_LOG_WRITE_FAILURE, 1595)                               \
    SQL_ERROR_ROW(ER_SLAVE_CREATE_EVENT_FAILURE, SLAVE_CREATE_EVENT_FAILURE, 1596)                                     \
    SQL_ERROR_ROW(ER_SLAVE_MASTER_COM_FAILURE, SLAVE_MASTER_COM_FAILURE, 1597)                                         \
    SQL_ERROR_ROW(ER_BINLOG_LOGGING_IMPOSSIBLE, BINLOG_LOGGING_IMPOSSIBLE, 1598)                                       \
    SQL_ERROR_ROW(ER_VIEW_NO_CREATION_CTX, VIEW_NO_CREATION_CTX, 1599)                                                 \
    SQL_ERROR_ROW(ER_VIEW_INVALID_CREATION_CTX, VIEW_INVALID_CREATION_CTX, 1600)                                       \
    SQL_ERROR_ROW(ER_SR_INVALID_CREATION_CTX, SR_INVALID_CREATION_CTX, 1601)                                           \
    SQL_ERROR_ROW(ER_TRG_CORRUPTED_FILE, TRG_CORRUPTED_FILE, 1602)                                                     \
    SQL_ERROR_ROW(ER_TRG_NO_CREATION_CTX, TRG_NO_CREATION_CTX, 1603)                                                   \
    SQL_ERROR_ROW(ER_TRG_INVALID_CREATION_CTX, TRG_INVALID_CREATION_CTX, 1604)                                         \
    SQL_ERROR_ROW(ER_EVENT_INVALID_CREATION_CTX, EVENT_INVALID_CREATION_CTX, 1605)                                     \
    SQL_ERROR_ROW(ER_TRG_CANT_OPEN_TABLE, TRG_CANT_OPEN_TABLE, 1606)                                                   \
    SQL_ERROR_ROW(ER_CANT_CREATE_SROUTINE, CANT_CREATE_SROUTINE, 1607)                                                 \
    SQL_ERROR_ROW(ER_UNUSED_11, UNUSED_11, 1608)                                                                       \
    SQL_ERROR_ROW(ER_NO_FORMAT_DESCRIPTION_EVENT_BEFORE_BINLOG_STATEMENT,                                              \
                  NO_FORMAT_DESCRIPTION_EVENT_BEFORE_BINLOG_STATEMENT, 1609)                                           \
    SQL_ERROR_ROW(ER_SLAVE_CORRUPT_EVENT, SLAVE_CORRUPT_EVENT, 1610)                                                   \
    SQL_ERROR_ROW(ER_LOAD_DATA_INVALID_COLUMN, LOAD_DATA_INVALID_COLUMN, 1611)                                         \
    SQL_ERROR_ROW(ER_LOG_PURGE_NO_FILE, LOG_PURGE_NO_FILE, 1612)                                                       \
    SQL_ERROR_ROW(ER_XA_RBTIMEOUT, XA_RBTIMEOUT, 1613)                                                                 \
    SQL_ERROR_ROW(ER_XA_RBDEADLOCK, XA_RBDEADLOCK, 1614)                                                               \
    SQL_ERROR_ROW(ER_NEED_REPREPARE, NEED_REPREPARE, 1615)                                                             \
    SQL_ERROR_ROW(ER_DELAYED_NOT_SUPPORTED, DELAYED_NOT_SUPPORTED, 1616)                                               \
    SQL_ERROR_ROW(WARN_NO_MASTER_INFO, SQLWARN_NO_MASTER_INFO, 1617)                                                   \
    SQL_ERROR_ROW(WARN_OPTION_IGNORED, SQLWARN_OPTION_IGNORED, 1618)                                                   \
    SQL_ERROR_ROW(ER_PLUGIN_DELETE_BUILTIN, PLUGIN_DELETE_BUILTIN, 1619)                                               \
    SQL_ERROR_ROW(WARN_PLUGIN_BUSY, SQLWARN_PLUGIN_BUSY, 1620)                                                         \
    SQL_ERROR_ROW(ER_VARIABLE_IS_READONLY, VARIABLE_IS_READONLY, 1621)                                                 \
    SQL_ERROR_ROW(ER_WARN_ENGINE_TRANSACTION_ROLLBACK, WARN_ENGINE_TRANSACTION_ROLLBACK, 1622)                         \
    SQL_ERROR_ROW(ER_SLAVE_HEARTBEAT_FAILURE, SLAVE_HEARTBEAT_FAILURE, 1623)                                           \
    SQL_ERROR_ROW(ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE, SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE, 1624)                     \
    SQL_ERROR_ROW(ER_UNUSED_14, UNUSED_14, 1625)                                                                       \
    SQL_ERROR_ROW(ER_CONFLICT_FN_PARSE_ERROR, CONFLICT_FN_PARSE_ERROR, 1626)                                           \
    SQL_ERROR_ROW(ER_EXCEPTIONS_WRITE_ERROR, EXCEPTIONS_WRITE_ERROR, 1627)                                             \
    SQL_ERROR_ROW(ER_TOO_LONG_TABLE_COMMENT, TOO_LONG_TABLE_COMMENT, 1628)                                             \
    SQL_ERROR_ROW(ER_TOO_LONG_FIELD_COMMENT, TOO_LONG_FIELD_COMMENT, 1629)                                             \
    SQL_ERROR_ROW(ER_FUNC_INEXISTENT_NAME_COLLISION, FUNC_INEXISTENT_NAME_COLLISION, 1630)                             \
    SQL_ERROR_ROW(ER_DATABASE_NAME, DATABASE_NAME, 1631)                                                               \
    SQL_ERROR_ROW(ER_TABLE_NAME, TABLE_NAME, 1632)                                                                     \
    SQL_ERROR_ROW(ER_PARTITION_NAME, PARTITION_NAME, 1633)                                                             \
    SQL_ERROR_ROW(ER_SUBPARTITION_NAME, SUBPARTITION_NAME, 1634)                                                       \
    SQL_ERROR_ROW(ER_TEMPORARY_NAME, TEMPORARY_NAME, 1635)                                                             \
    SQL_ERROR_ROW(ER_RENAMED_NAME, RENAMED_NAME, 1636)                                                                 \
    SQL_ERROR_ROW(ER_TOO_MANY_CONCURRENT_TRXS, TOO_MANY_CONCURRENT_TRXS, 1637)                                         \
    SQL_ERROR_ROW(WARN_NON_ASCII_SEPARATOR_NOT_IMPLEMENTED, SQLWARN_NON_ASCII_SEPARATOR_NOT_IMPLEMENTED, 1638)         \
    SQL_ERROR_ROW(ER_DEBUG_SYNC_TIMEOUT, DEBUG_SYNC_TIMEOUT, 1639)                                                     \
    SQL_ERROR_ROW(ER_DEBUG_SYNC_HIT_LIMIT, DEBUG_SYNC_HIT_LIMIT, 1640)                                                 \
    SQL_ERROR_ROW(ER_DUP_SIGNAL_SET, DUP_SIGNAL_SET, 1641)                                                             \
    SQL_ERROR_ROW(ER_SIGNAL_WARN, SIGNAL_WARN, 1642)                                                                   \
    SQL_ERROR_ROW(ER_SIGNAL_NOT_FOUND, SIGNAL_NOT_FOUND, 1643)                                                         \
    SQL_ERROR_ROW(ER_SIGNAL_EXCEPTION, SIGNAL_EXCEPTION, 1644)                                                         \
    SQL_ERROR_ROW(ER_RESIGNAL_WITHOUT_ACTIVE_HANDLER, RESIGNAL_WITHOUT_ACTIVE_HANDLER, 1645)                           \
    SQL_ERROR_ROW(ER_SIGNAL_BAD_CONDITION_TYPE, SIGNAL_BAD_CONDITION_TYPE, 1646)                                       \
    SQL_ERROR_ROW(WARN_COND_ITEM_TRUNCATED, SQLWARN_COND_ITEM_TRUNCATED, 1647)                                         \
    SQL_ERROR_ROW(ER_COND_ITEM_TOO_LONG, COND_ITEM_TOO_LONG, 1648)                                                     \
    SQL_ERROR_ROW(ER_UNKNOWN_LOCALE, UNKNOWN_LOCALE, 1649)                                                             \
    SQL_ERROR_ROW(ER_SLAVE_IGNORE_SERVER_IDS, SLAVE_IGNORE_SERVER_IDS, 1650)                                           \
    SQL_ERROR_ROW(ER_QUERY_CACHE_DISABLED, QUERY_CACHE_DISABLED, 1651)                                                 \
    SQL_ERROR_ROW(ER_SAME_NAME_PARTITION_FIELD, SAME_NAME_PARTITION_FIELD, 1652)                                       \
    SQL_ERROR_ROW(ER_PARTITION_COLUMN_LIST_ERROR, PARTITION_COLUMN_LIST_ERROR, 1653)                                   \
    SQL_ERROR_ROW(ER_WRONG_TYPE_COLUMN_VALUE_ERROR, WRONG_TYPE_COLUMN_VALUE_ERROR, 1654)                               \
    SQL_ERROR_ROW(ER_TOO_MANY_PARTITION_FUNC_FIELDS_ERROR, TOO_MANY_PARTITION_FUNC_FIELDS_ERROR, 1655)                 \
    SQL_ERROR_ROW(ER_MAXVALUE_IN_VALUES_IN, MAXVALUE_IN_VALUES_IN, 1656)                                               \
    SQL_ERROR_ROW(ER_TOO_MANY_VALUES_ERROR, TOO_MANY_VALUES_ERROR, 1657)                                               \
    SQL_ERROR_ROW(ER_ROW_SINGLE_PARTITION_FIELD_ERROR, ROW_SINGLE_PARTITION_FIELD_ERROR, 1658)                         \
    SQL_ERROR_ROW(ER_FIELD_TYPE_NOT_ALLOWED_AS_PARTITION_FIELD, FIELD_TYPE_NOT_ALLOWED_AS_PARTITION_FIELD, 1659)       \
    SQL_ERROR_ROW(ER_PARTITION_FIELDS_TOO_LONG, PARTITION_FIELDS_TOO_LONG, 1660)                                       \
    SQL_ERROR_ROW(ER_BINLOG_ROW_ENGINE_AND_STMT_ENGINE, BINLOG_ROW_ENGINE_AND_STMT_ENGINE, 1661)                       \
    SQL_ERROR_ROW(ER_BINLOG_ROW_MODE_AND_STMT_ENGINE, BINLOG_ROW_MODE_AND_STMT_ENGINE, 1662)                           \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_AND_STMT_ENGINE, BINLOG_UNSAFE_AND_STMT_ENGINE, 1663)                               \
    SQL_ERROR_ROW(ER_BINLOG_ROW_INJECTION_AND_STMT_ENGINE, BINLOG_ROW_INJECTION_AND_STMT_ENGINE, 1664)                 \
    SQL_ERROR_ROW(ER_BINLOG_STMT_MODE_AND_ROW_ENGINE, BINLOG_STMT_MODE_AND_ROW_ENGINE, 1665)                           \
    SQL_ERROR_ROW(ER_BINLOG_ROW_INJECTION_AND_STMT_MODE, BINLOG_ROW_INJECTION_AND_STMT_MODE, 1666)                     \
    SQL_ERROR_ROW(ER_BINLOG_MULTIPLE_ENGINES_AND_SELF_LOGGING_ENGINE, BINLOG_MULTIPLE_ENGINES_AND_SELF_LOGGING_ENGINE, \
                  1667)                                                                                                \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_LIMIT, BINLOG_UNSAFE_LIMIT, 1668)                                                   \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_INSERT_DELAYED, BINLOG_UNSAFE_INSERT_DELAYED, 1669)                                 \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_SYSTEM_TABLE, BINLOG_UNSAFE_SYSTEM_TABLE, 1670)                                     \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_AUTOINC_COLUMNS, BINLOG_UNSAFE_AUTOINC_COLUMNS, 1671)                               \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_UDF, BINLOG_UNSAFE_UDF, 1672)                                                       \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_SYSTEM_VARIABLE, BINLOG_UNSAFE_SYSTEM_VARIABLE, 1673)                               \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_SYSTEM_FUNCTION, BINLOG_UNSAFE_SYSTEM_FUNCTION, 1674)                               \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_NONTRANS_AFTER_TRANS, BINLOG_UNSAFE_NONTRANS_AFTER_TRANS, 1675)                     \
    SQL_ERROR_ROW(ER_MESSAGE_AND_STATEMENT, MESSAGE_AND_STATEMENT, 1676)                                               \
    SQL_ERROR_ROW(ER_SLAVE_CONVERSION_FAILED, SLAVE_CONVERSION_FAILED, 1677)                                           \
    SQL_ERROR_ROW(ER_SLAVE_CANT_CREATE_CONVERSION, SLAVE_CANT_CREATE_CONVERSION, 1678)                                 \
    SQL_ERROR_ROW(ER_INSIDE_TRANSACTION_PREVENTS_SWITCH_BINLOG_FORMAT,                                                 \
                  INSIDE_TRANSACTION_PREVENTS_SWITCH_BINLOG_FORMAT, 1679)                                              \
    SQL_ERROR_ROW(ER_PATH_LENGTH, PATH_LENGTH, 1680)                                                                   \
    SQL_ERROR_ROW(ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT, WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT, 1681)               \
    SQL_ERROR_ROW(ER_WRONG_NATIVE_TABLE_STRUCTURE, WRONG_NATIVE_TABLE_STRUCTURE, 1682)                                 \
    SQL_ERROR_ROW(ER_WRONG_PERFSCHEMA_USAGE, WRONG_PERFSCHEMA_USAGE, 1683)                                             \
    SQL_ERROR_ROW(ER_WARN_I_S_SKIPPED_TABLE, WARN_I_S_SKIPPED_TABLE, 1684)                                             \
    SQL_ERROR_ROW(ER_INSIDE_TRANSACTION_PREVENTS_SWITCH_BINLOG_DIRECT,                                                 \
                  INSIDE_TRANSACTION_PREVENTS_SWITCH_BINLOG_DIRECT, 1685)                                              \
    SQL_ERROR_ROW(ER_STORED_FUNCTION_PREVENTS_SWITCH_BINLOG_DIRECT, STORED_FUNCTION_PREVENTS_SWITCH_BINLOG_DIRECT,     \
                  1686)                                                                                                \
    SQL_ERROR_ROW(ER_SPATIAL_MUST_HAVE_GEOM_COL, SPATIAL_MUST_HAVE_GEOM_COL, 1687)                                     \
    SQL_ERROR_ROW(ER_TOO_LONG_INDEX_COMMENT, TOO_LONG_INDEX_COMMENT, 1688)                                             \
    SQL_ERROR_ROW(ER_LOCK_ABORTED, LOCK_ABORTED, 1689)                                                                 \
    SQL_ERROR_ROW(ER_DATA_OUT_OF_RANGE, DATA_OUT_OF_RANGE, 1690)                                                       \
    SQL_ERROR_ROW(ER_WRONG_SPVAR_TYPE_IN_LIMIT, WRONG_SPVAR_TYPE_IN_LIMIT, 1691)                                       \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_MULTIPLE_ENGINES_AND_SELF_LOGGING_ENGINE,                                           \
                  BINLOG_UNSAFE_MULTIPLE_ENGINES_AND_SELF_LOGGING_ENGINE, 1692)                                        \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_MIXED_STATEMENT, BINLOG_UNSAFE_MIXED_STATEMENT, 1693)                               \
    SQL_ERROR_ROW(ER_INSIDE_TRANSACTION_PREVENTS_SWITCH_SQL_LOG_BIN, INSIDE_TRANSACTION_PREVENTS_SWITCH_SQL_LOG_BIN,   \
                  1694)                                                                                                \
    SQL_ERROR_ROW(ER_STORED_FUNCTION_PREVENTS_SWITCH_SQL_LOG_BIN, STORED_FUNCTION_PREVENTS_SWITCH_SQL_LOG_BIN, 1695)   \
    SQL_ERROR_ROW(ER_FAILED_READ_FROM_PAR_FILE, FAILED_READ_FROM_PAR_FILE, 1696)                                       \
    SQL_ERROR_ROW(ER_VALUES_IS_NOT_INT_TYPE_ERROR, VALUES_IS_NOT_INT_TYPE_ERROR, 1697)                                 \
    SQL_ERROR_ROW(ER_ACCESS_DENIED_NO_PASSWORD_ERROR, ACCESS_DENIED_NO_PASSWORD_ERROR, 1698)                           \
    SQL_ERROR_ROW(ER_SET_PASSWORD_AUTH_PLUGIN, SET_PASSWORD_AUTH_PLUGIN, 1699)                                         \
    SQL_ERROR_ROW(ER_GRANT_PLUGIN_USER_EXISTS, GRANT_PLUGIN_USER_EXISTS, 1700)                                         \
    SQL_ERROR_ROW(ER_TRUNCATE_ILLEGAL_FK, TRUNCATE_ILLEGAL_FK, 1701)                                                   \
    SQL_ERROR_ROW(ER_PLUGIN_IS_PERMANENT, PLUGIN_IS_PERMANENT, 1702)                                                   \
    SQL_ERROR_ROW(ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE_MIN, SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE_MIN, 1703)             \
    SQL_ERROR_ROW(ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE_MAX, SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE_MAX, 1704)             \
    SQL_ERROR_ROW(ER_STMT_CACHE_FULL, STMT_CACHE_FULL, 1705)                                                           \
    SQL_ERROR_ROW(ER_MULTI_UPDATE_KEY_CONFLICT, MULTI_UPDATE_KEY_CONFLICT, 1706)                                       \
    SQL_ERROR_ROW(ER_TABLE_NEEDS_REBUILD, TABLE_NEEDS_REBUILD, 1707)                                                   \
    SQL_ERROR_ROW(WARN_OPTION_BELOW_LIMIT, SQLWARN_OPTION_BELOW_LIMIT, 1708)                                           \
    SQL_ERROR_ROW(ER_INDEX_COLUMN_TOO_LONG, INDEX_COLUMN_TOO_LONG, 1709)                                               \
    SQL_ERROR_ROW(ER_ERROR_IN_TRIGGER_BODY, ERROR_IN_TRIGGER_BODY, 1710)                                               \
    SQL_ERROR_ROW(ER_ERROR_IN_UNKNOWN_TRIGGER_BODY, ERROR_IN_UNKNOWN_TRIGGER_BODY, 1711)                               \
    SQL_ERROR_ROW(ER_INDEX_CORRUPT, INDEX_CORRUPT, 1712)                                                               \
    SQL_ERROR_ROW(ER_UNDO_RECORD_TOO_BIG, UNDO_RECORD_TOO_BIG, 1713)                                                   \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_INSERT_IGNORE_SELECT, BINLOG_UNSAFE_INSERT_IGNORE_SELECT, 1714)                     \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_INSERT_SELECT_UPDATE, BINLOG_UNSAFE_INSERT_SELECT_UPDATE, 1715)                     \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_REPLACE_SELECT, BINLOG_UNSAFE_REPLACE_SELECT, 1716)                                 \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_CREATE_IGNORE_SELECT, BINLOG_UNSAFE_CREATE_IGNORE_SELECT, 1717)                     \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_CREATE_REPLACE_SELECT, BINLOG_UNSAFE_CREATE_REPLACE_SELECT, 1718)                   \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_UPDATE_IGNORE, BINLOG_UNSAFE_UPDATE_IGNORE, 1719)                                   \
    SQL_ERROR_ROW(ER_UNUSED_15, UNUSED_15, 1720)                                                                       \
    SQL_ERROR_ROW(ER_UNUSED_16, UNUSED_16, 1721)                                                                       \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_WRITE_AUTOINC_SELECT, BINLOG_UNSAFE_WRITE_AUTOINC_SELECT, 1722)                     \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_CREATE_SELECT_AUTOINC, BINLOG_UNSAFE_CREATE_SELECT_AUTOINC, 1723)                   \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_INSERT_TWO_KEYS, BINLOG_UNSAFE_INSERT_TWO_KEYS, 1724)                               \
    SQL_ERROR_ROW(ER_UNUSED_28, UNUSED_28, 1725)                                                                       \
    SQL_ERROR_ROW(ER_VERS_NOT_ALLOWED, VERS_NOT_ALLOWED, 1726)                                                         \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_AUTOINC_NOT_FIRST, BINLOG_UNSAFE_AUTOINC_NOT_FIRST, 1727)                           \
    SQL_ERROR_ROW(ER_CANNOT_LOAD_FROM_TABLE_V2, CANNOT_LOAD_FROM_TABLE_V2, 1728)                                       \
    SQL_ERROR_ROW(ER_MASTER_DELAY_VALUE_OUT_OF_RANGE, MASTER_DELAY_VALUE_OUT_OF_RANGE, 1729)                           \
    SQL_ERROR_ROW(ER_ONLY_FD_AND_RBR_EVENTS_ALLOWED_IN_BINLOG_STATEMENT,                                               \
                  ONLY_FD_AND_RBR_EVENTS_ALLOWED_IN_BINLOG_STATEMENT, 1730)                                            \
    SQL_ERROR_ROW(ER_PARTITION_EXCHANGE_DIFFERENT_OPTION, PARTITION_EXCHANGE_DIFFERENT_OPTION, 1731)                   \
    SQL_ERROR_ROW(ER_PARTITION_EXCHANGE_PART_TABLE, PARTITION_EXCHANGE_PART_TABLE, 1732)                               \
    SQL_ERROR_ROW(ER_PARTITION_EXCHANGE_TEMP_TABLE, PARTITION_EXCHANGE_TEMP_TABLE, 1733)                               \
    SQL_ERROR_ROW(ER_PARTITION_INSTEAD_OF_SUBPARTITION, PARTITION_INSTEAD_OF_SUBPARTITION, 1734)                       \
    SQL_ERROR_ROW(ER_UNKNOWN_PARTITION, UNKNOWN_PARTITION, 1735)                                                       \
    SQL_ERROR_ROW(ER_TABLES_DIFFERENT_METADATA, TABLES_DIFFERENT_METADATA, 1736)                                       \
    SQL_ERROR_ROW(ER_ROW_DOES_NOT_MATCH_PARTITION, ROW_DOES_NOT_MATCH_PARTITION, 1737)                                 \
    SQL_ERROR_ROW(ER_BINLOG_CACHE_SIZE_GREATER_THAN_MAX, BINLOG_CACHE_SIZE_GREATER_THAN_MAX, 1738)                     \
    SQL_ERROR_ROW(ER_WARN_INDEX_NOT_APPLICABLE, WARN_INDEX_NOT_APPLICABLE, 1739)                                       \
    SQL_ERROR_ROW(ER_PARTITION_EXCHANGE_FOREIGN_KEY, PARTITION_EXCHANGE_FOREIGN_KEY, 1740)                             \
    SQL_ERROR_ROW(ER_NO_SUCH_KEY_VALUE, NO_SUCH_KEY_VALUE, 1741)                                                       \
    SQL_ERROR_ROW(ER_VALUE_TOO_LONG, VALUE_TOO_LONG, 1742)                                                             \
    SQL_ERROR_ROW(ER_NETWORK_READ_EVENT_CHECKSUM_FAILURE, NETWORK_READ_EVENT_CHECKSUM_FAILURE, 1743)                   \
    SQL_ERROR_ROW(ER_BINLOG_READ_EVENT_CHECKSUM_FAILURE, BINLOG_READ_EVENT_CHECKSUM_FAILURE, 1744)                     \
    SQL_ERROR_ROW(ER_BINLOG_STMT_CACHE_SIZE_GREATER_THAN_MAX, BINLOG_STMT_CACHE_SIZE_GREATER_THAN_MAX, 1745)           \
    SQL_ERROR_ROW(ER_CANT_UPDATE_TABLE_IN_CREATE_TABLE_SELECT, CANT_UPDATE_TABLE_IN_CREATE_TABLE_SELECT, 1746)         \
    SQL_ERROR_ROW(ER_PARTITION_CLAUSE_ON_NONPARTITIONED, PARTITION_CLAUSE_ON_NONPARTITIONED, 1747)                     \
    SQL_ERROR_ROW(ER_ROW_DOES_NOT_MATCH_GIVEN_PARTITION_SET, ROW_DOES_NOT_MATCH_GIVEN_PARTITION_SET, 1748)             \
    SQL_ERROR_ROW(ER_UNUSED_5, UNUSED_5, 1749)                                                                         \
    SQL_ERROR_ROW(ER_CHANGE_RPL_INFO_REPOSITORY_FAILURE, CHANGE_RPL_INFO_REPOSITORY_FAILURE, 1750)                     \
    SQL_ERROR_ROW(ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_CREATED_TEMP_TABLE,                                            \
                  WARNING_NOT_COMPLETE_ROLLBACK_WITH_CREATED_TEMP_TABLE, 1751)                                         \
    SQL_ERROR_ROW(ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_DROPPED_TEMP_TABLE,                                            \
                  WARNING_NOT_COMPLETE_ROLLBACK_WITH_DROPPED_TEMP_TABLE, 1752)                                         \
    SQL_ERROR_ROW(ER_MTS_FEATURE_IS_NOT_SUPPORTED, MTS_FEATURE_IS_NOT_SUPPORTED, 1753)                                 \
    SQL_ERROR_ROW(ER_MTS_UPDATED_DBS_GREATER_MAX, MTS_UPDATED_DBS_GREATER_MAX, 1754)                                   \
    SQL_ERROR_ROW(ER_MTS_CANT_PARALLEL, MTS_CANT_PARALLEL, 1755)                                                       \
    SQL_ERROR_ROW(ER_MTS_INCONSISTENT_DATA, MTS_INCONSISTENT_DATA, 1756)                                               \
    SQL_ERROR_ROW(ER_FULLTEXT_NOT_SUPPORTED_WITH_PARTITIONING, FULLTEXT_NOT_SUPPORTED_WITH_PARTITIONING, 1757)         \
    SQL_ERROR_ROW(ER_DA_INVALID_CONDITION_NUMBER, DA_INVALID_CONDITION_NUMBER, 1758)                                   \
    SQL_ERROR_ROW(ER_INSECURE_PLAIN_TEXT, INSECURE_PLAIN_TEXT, 1759)                                                   \
    SQL_ERROR_ROW(ER_INSECURE_CHANGE_MASTER, INSECURE_CHANGE_MASTER, 1760)                                             \
    SQL_ERROR_ROW(ER_FOREIGN_DUPLICATE_KEY_WITH_CHILD_INFO, FOREIGN_DUPLICATE_KEY_WITH_CHILD_INFO, 1761)               \
    SQL_ERROR_ROW(ER_FOREIGN_DUPLICATE_KEY_WITHOUT_CHILD_INFO, FOREIGN_DUPLICATE_KEY_WITHOUT_CHILD_INFO, 1762)         \
    SQL_ERROR_ROW(ER_SQLTHREAD_WITH_SECURE_SLAVE, SQLTHREAD_WITH_SECURE_SLAVE, 1763)                                   \
    SQL_ERROR_ROW(ER_TABLE_HAS_NO_FT, TABLE_HAS_NO_FT, 1764)                                                           \
    SQL_ERROR_ROW(ER_VARIABLE_NOT_SETTABLE_IN_SF_OR_TRIGGER, VARIABLE_NOT_SETTABLE_IN_SF_OR_TRIGGER, 1765)             \
    SQL_ERROR_ROW(ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION, VARIABLE_NOT_SETTABLE_IN_TRANSACTION, 1766)                 \
    SQL_ERROR_ROW(ER_GTID_NEXT_IS_NOT_IN_GTID_NEXT_LIST, GTID_NEXT_IS_NOT_IN_GTID_NEXT_LIST, 1767)                     \
    SQL_ERROR_ROW(ER_CANT_CHANGE_GTID_NEXT_IN_TRANSACTION_WHEN_GTID_NEXT_LIST_IS_NULL,                                 \
                  CANT_CHANGE_GTID_NEXT_IN_TRANSACTION_WHEN_GTID_NEXT_LIST_IS_NULL, 1768)                              \
    SQL_ERROR_ROW(ER_SET_STATEMENT_CANNOT_INVOKE_FUNCTION, SET_STATEMENT_CANNOT_INVOKE_FUNCTION, 1769)                 \
    SQL_ERROR_ROW(ER_GTID_NEXT_CANT_BE_AUTOMATIC_IF_GTID_NEXT_LIST_IS_NON_NULL,                                        \
                  GTID_NEXT_CANT_BE_AUTOMATIC_IF_GTID_NEXT_LIST_IS_NON_NULL, 1770)                                     \
    SQL_ERROR_ROW(ER_SKIPPING_LOGGED_TRANSACTION, SKIPPING_LOGGED_TRANSACTION, 1771)                                   \
    SQL_ERROR_ROW(ER_MALFORMED_GTID_SET_SPECIFICATION, MALFORMED_GTID_SET_SPECIFICATION, 1772)                         \
    SQL_ERROR_ROW(ER_MALFORMED_GTID_SET_ENCODING, MALFORMED_GTID_SET_ENCODING, 1773)                                   \
    SQL_ERROR_ROW(ER_MALFORMED_GTID_SPECIFICATION, MALFORMED_GTID_SPECIFICATION, 1774)                                 \
    SQL_ERROR_ROW(ER_GNO_EXHAUSTED, GNO_EXHAUSTED, 1775)                                                               \
    SQL_ERROR_ROW(ER_BAD_SLAVE_AUTO_POSITION, BAD_SLAVE_AUTO_POSITION, 1776)                                           \
    SQL_ERROR_ROW(ER_AUTO_POSITION_REQUIRES_GTID_MODE_ON, AUTO_POSITION_REQUIRES_GTID_MODE_ON, 1777)                   \
    SQL_ERROR_ROW(ER_CANT_DO_IMPLICIT_COMMIT_IN_TRX_WHEN_GTID_NEXT_IS_SET,                                             \
                  CANT_DO_IMPLICIT_COMMIT_IN_TRX_WHEN_GTID_NEXT_IS_SET, 1778)                                          \
    SQL_ERROR_ROW(ER_GTID_MODE_2_OR_3_REQUIRES_ENFORCE_GTID_CONSISTENCY_ON,                                            \
                  GTID_MODE_2_OR_3_REQUIRES_ENFORCE_GTID_CONSISTENCY_ON, 1779)                                         \
    SQL_ERROR_ROW(ER_GTID_MODE_REQUIRES_BINLOG, GTID_MODE_REQUIRES_BINLOG, 1780)                                       \
    SQL_ERROR_ROW(ER_CANT_SET_GTID_NEXT_TO_GTID_WHEN_GTID_MODE_IS_OFF,                                                 \
                  CANT_SET_GTID_NEXT_TO_GTID_WHEN_GTID_MODE_IS_OFF, 1781)                                              \
    SQL_ERROR_ROW(ER_CANT_SET_GTID_NEXT_TO_ANONYMOUS_WHEN_GTID_MODE_IS_ON,                                             \
                  CANT_SET_GTID_NEXT_TO_ANONYMOUS_WHEN_GTID_MODE_IS_ON, 1782)                                          \
    SQL_ERROR_ROW(ER_CANT_SET_GTID_NEXT_LIST_TO_NON_NULL_WHEN_GTID_MODE_IS_OFF,                                        \
                  CANT_SET_GTID_NEXT_LIST_TO_NON_NULL_WHEN_GTID_MODE_IS_OFF, 1783)                                     \
    SQL_ERROR_ROW(ER_FOUND_GTID_EVENT_WHEN_GTID_MODE_IS_OFF, FOUND_GTID_EVENT_WHEN_GTID_MODE_IS_OFF, 1784)             \
    SQL_ERROR_ROW(ER_GTID_UNSAFE_NON_TRANSACTIONAL_TABLE, GTID_UNSAFE_NON_TRANSACTIONAL_TABLE, 1785)                   \
    SQL_ERROR_ROW(ER_GTID_UNSAFE_CREATE_SELECT, GTID_UNSAFE_CREATE_SELECT, 1786)                                       \
    SQL_ERROR_ROW(ER_GTID_UNSAFE_CREATE_DROP_TEMPORARY_TABLE_IN_TRANSACTION,                                           \
                  GTID_UNSAFE_CREATE_DROP_TEMPORARY_TABLE_IN_TRANSACTION, 1787)                                        \
    SQL_ERROR_ROW(ER_GTID_MODE_CAN_ONLY_CHANGE_ONE_STEP_AT_A_TIME, GTID_MODE_CAN_ONLY_CHANGE_ONE_STEP_AT_A_TIME, 1788) \
    SQL_ERROR_ROW(ER_MASTER_HAS_PURGED_REQUIRED_GTIDS, MASTER_HAS_PURGED_REQUIRED_GTIDS, 1789)                         \
    SQL_ERROR_ROW(ER_CANT_SET_GTID_NEXT_WHEN_OWNING_GTID, CANT_SET_GTID_NEXT_WHEN_OWNING_GTID, 1790)                   \
    SQL_ERROR_ROW(ER_UNKNOWN_EXPLAIN_FORMAT, UNKNOWN_EXPLAIN_FORMAT, 1791)                                             \
    SQL_ERROR_ROW(ER_CANT_EXECUTE_IN_READ_ONLY_TRANSACTION, CANT_EXECUTE_IN_READ_ONLY_TRANSACTION, 1792)               \
    SQL_ERROR_ROW(ER_TOO_LONG_TABLE_PARTITION_COMMENT, TOO_LONG_TABLE_PARTITION_COMMENT, 1793)                         \
    SQL_ERROR_ROW(ER_SLAVE_CONFIGURATION, SLAVE_CONFIGURATION, 1794)                                                   \
    SQL_ERROR_ROW(ER_INNODB_FT_LIMIT, INNODB_FT_LIMIT, 1795)                                                           \
    SQL_ERROR_ROW(ER_INNODB_NO_FT_TEMP_TABLE, INNODB_NO_FT_TEMP_TABLE, 1796)                                           \
    SQL_ERROR_ROW(ER_INNODB_FT_WRONG_DOCID_COLUMN, INNODB_FT_WRONG_DOCID_COLUMN, 1797)                                 \
    SQL_ERROR_ROW(ER_INNODB_FT_WRONG_DOCID_INDEX, INNODB_FT_WRONG_DOCID_INDEX, 1798)                                   \
    SQL_ERROR_ROW(ER_INNODB_ONLINE_LOG_TOO_BIG, INNODB_ONLINE_LOG_TOO_BIG, 1799)                                       \
    SQL_ERROR_ROW(ER_UNKNOWN_ALTER_ALGORITHM, UNKNOWN_ALTER_ALGORITHM, 1800)                                           \
    SQL_ERROR_ROW(ER_UNKNOWN_ALTER_LOCK, UNKNOWN_ALTER_LOCK, 1801)                                                     \
    SQL_ERROR_ROW(ER_MTS_CHANGE_MASTER_CANT_RUN_WITH_GAPS, MTS_CHANGE_MASTER_CANT_RUN_WITH_GAPS, 1802)                 \
    SQL_ERROR_ROW(ER_MTS_RECOVERY_FAILURE, MTS_RECOVERY_FAILURE, 1803)                                                 \
    SQL_ERROR_ROW(ER_MTS_RESET_WORKERS, MTS_RESET_WORKERS, 1804)                                                       \
    SQL_ERROR_ROW(ER_COL_COUNT_DOESNT_MATCH_CORRUPTED_V2, COL_COUNT_DOESNT_MATCH_CORRUPTED_V2, 1805)                   \
    SQL_ERROR_ROW(ER_SLAVE_SILENT_RETRY_TRANSACTION, SLAVE_SILENT_RETRY_TRANSACTION, 1806)                             \
    SQL_ERROR_ROW(ER_UNUSED_22, UNUSED_22, 1807)                                                                       \
    SQL_ERROR_ROW(ER_TABLE_SCHEMA_MISMATCH, TABLE_SCHEMA_MISMATCH, 1808)                                               \
    SQL_ERROR_ROW(ER_TABLE_IN_SYSTEM_TABLESPACE, TABLE_IN_SYSTEM_TABLESPACE, 1809)                                     \
    SQL_ERROR_ROW(ER_IO_READ_ERROR, IO_READ_ERROR, 1810)                                                               \
    SQL_ERROR_ROW(ER_IO_WRITE_ERROR, IO_WRITE_ERROR, 1811)                                                             \
    SQL_ERROR_ROW(ER_TABLESPACE_MISSING, TABLESPACE_MISSING, 1812)                                                     \
    SQL_ERROR_ROW(ER_TABLESPACE_EXISTS, TABLESPACE_EXISTS, 1813)                                                       \
    SQL_ERROR_ROW(ER_TABLESPACE_DISCARDED, TABLESPACE_DISCARDED, 1814)                                                 \
    SQL_ERROR_ROW(ER_INTERNAL_ERROR, INTERNAL_ERROR, 1815)                                                             \
    SQL_ERROR_ROW(ER_INNODB_IMPORT_ERROR, INNODB_IMPORT_ERROR, 1816)                                                   \
    SQL_ERROR_ROW(ER_INNODB_INDEX_CORRUPT, INNODB_INDEX_CORRUPT, 1817)                                                 \
    SQL_ERROR_ROW(ER_INVALID_YEAR_COLUMN_LENGTH, INVALID_YEAR_COLUMN_LENGTH, 1818)                                     \
    SQL_ERROR_ROW(ER_NOT_VALID_PASSWORD, NOT_VALID_PASSWORD, 1819)                                                     \
    SQL_ERROR_ROW(ER_MUST_CHANGE_PASSWORD, MUST_CHANGE_PASSWORD, 1820)                                                 \
    SQL_ERROR_ROW(ER_FK_NO_INDEX_CHILD, FK_NO_INDEX_CHILD, 1821)                                                       \
    SQL_ERROR_ROW(ER_FK_NO_INDEX_PARENT, FK_NO_INDEX_PARENT, 1822)                                                     \
    SQL_ERROR_ROW(ER_FK_FAIL_ADD_SYSTEM, FK_FAIL_ADD_SYSTEM, 1823)                                                     \
    SQL_ERROR_ROW(ER_FK_CANNOT_OPEN_PARENT, FK_CANNOT_OPEN_PARENT, 1824)                                               \
    SQL_ERROR_ROW(ER_FK_INCORRECT_OPTION, FK_INCORRECT_OPTION, 1825)                                                   \
    SQL_ERROR_ROW(ER_DUP_CONSTRAINT_NAME, DUP_CONSTRAINT_NAME, 1826)                                                   \
    SQL_ERROR_ROW(ER_PASSWORD_FORMAT, PASSWORD_FORMAT, 1827)                                                           \
    SQL_ERROR_ROW(ER_FK_COLUMN_CANNOT_DROP, FK_COLUMN_CANNOT_DROP, 1828)                                               \
    SQL_ERROR_ROW(ER_FK_COLUMN_CANNOT_DROP_CHILD, FK_COLUMN_CANNOT_DROP_CHILD, 1829)                                   \
    SQL_ERROR_ROW(ER_FK_COLUMN_NOT_NULL, FK_COLUMN_NOT_NULL, 1830)                                                     \
    SQL_ERROR_ROW(ER_DUP_INDEX, DUP_INDEX, 1831)                                                                       \
    SQL_ERROR_ROW(ER_FK_COLUMN_CANNOT_CHANGE, FK_COLUMN_CANNOT_CHANGE, 1832)                                           \
    SQL_ERROR_ROW(ER_FK_COLUMN_CANNOT_CHANGE_CHILD, FK_COLUMN_CANNOT_CHANGE_CHILD, 1833)                               \
    SQL_ERROR_ROW(ER_FK_CANNOT_DELETE_PARENT, FK_CANNOT_DELETE_PARENT, 1834)                                           \
    SQL_ERROR_ROW(ER_MALFORMED_PACKET, MALFORMED_PACKET, 1835)                                                         \
    SQL_ERROR_ROW(ER_READ_ONLY_MODE, READ_ONLY_MODE, 1836)                                                             \
    SQL_ERROR_ROW(ER_GTID_NEXT_TYPE_UNDEFINED_GROUP, GTID_NEXT_TYPE_UNDEFINED_GROUP, 1837)                             \
    SQL_ERROR_ROW(ER_VARIABLE_NOT_SETTABLE_IN_SP, VARIABLE_NOT_SETTABLE_IN_SP, 1838)                                   \
    SQL_ERROR_ROW(ER_CANT_SET_GTID_PURGED_WHEN_GTID_MODE_IS_OFF, CANT_SET_GTID_PURGED_WHEN_GTID_MODE_IS_OFF, 1839)     \
    SQL_ERROR_ROW(ER_CANT_SET_GTID_PURGED_WHEN_GTID_EXECUTED_IS_NOT_EMPTY,                                             \
                  CANT_SET_GTID_PURGED_WHEN_GTID_EXECUTED_IS_NOT_EMPTY, 1840)                                          \
    SQL_ERROR_ROW(ER_CANT_SET_GTID_PURGED_WHEN_OWNED_GTIDS_IS_NOT_EMPTY,                                               \
                  CANT_SET_GTID_PURGED_WHEN_OWNED_GTIDS_IS_NOT_EMPTY, 1841)                                            \
    SQL_ERROR_ROW(ER_GTID_PURGED_WAS_CHANGED, GTID_PURGED_WAS_CHANGED, 1842)                                           \
    SQL_ERROR_ROW(ER_GTID_EXECUTED_WAS_CHANGED, GTID_EXECUTED_WAS_CHANGED, 1843)                                       \
    SQL_ERROR_ROW(ER_BINLOG_STMT_MODE_AND_NO_REPL_TABLES, BINLOG_STMT_MODE_AND_NO_REPL_TABLES, 1844)                   \
    SQL_ERROR_ROW(ER_ALTER_OPERATION_NOT_SUPPORTED, ALTER_OPERATION_NOT_SUPPORTED, 1845)                               \
    SQL_ERROR_ROW(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, ALTER_OPERATION_NOT_SUPPORTED_REASON, 1846)                 \
    SQL_ERROR_ROW(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_COPY, ALTER_OPERATION_NOT_SUPPORTED_REASON_COPY, 1847)       \
    SQL_ERROR_ROW(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_PARTITION, ALTER_OPERATION_NOT_SUPPORTED_REASON_PARTITION,   \
                  1848)                                                                                                \
    SQL_ERROR_ROW(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_FK_RENAME, ALTER_OPERATION_NOT_SUPPORTED_REASON_FK_RENAME,   \
                  1849)                                                                                                \
    SQL_ERROR_ROW(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_COLUMN_TYPE,                                                 \
                  ALTER_OPERATION_NOT_SUPPORTED_REASON_COLUMN_TYPE, 1850)                                              \
    SQL_ERROR_ROW(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_FK_CHECK, ALTER_OPERATION_NOT_SUPPORTED_REASON_FK_CHECK,     \
                  1851)                                                                                                \
    SQL_ERROR_ROW(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_IGNORE, ALTER_OPERATION_NOT_SUPPORTED_REASON_IGNORE, 1852)   \
    SQL_ERROR_ROW(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_NOPK, ALTER_OPERATION_NOT_SUPPORTED_REASON_NOPK, 1853)       \
    SQL_ERROR_ROW(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_AUTOINC, ALTER_OPERATION_NOT_SUPPORTED_REASON_AUTOINC, 1854) \
    SQL_ERROR_ROW(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_HIDDEN_FTS, ALTER_OPERATION_NOT_SUPPORTED_REASON_HIDDEN_FTS, \
                  1855)                                                                                                \
    SQL_ERROR_ROW(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_CHANGE_FTS, ALTER_OPERATION_NOT_SUPPORTED_REASON_CHANGE_FTS, \
                  1856)                                                                                                \
    SQL_ERROR_ROW(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_FTS, ALTER_OPERATION_NOT_SUPPORTED_REASON_FTS, 1857)         \
    SQL_ERROR_ROW(ER_SQL_SLAVE_SKIP_COUNTER_NOT_SETTABLE_IN_GTID_MODE,                                                 \
                  SQL_SLAVE_SKIP_COUNTER_NOT_SETTABLE_IN_GTID_MODE, 1858)                                              \
    SQL_ERROR_ROW(ER_DUP_UNKNOWN_IN_INDEX, DUP_UNKNOWN_IN_INDEX, 1859)                                                 \
    SQL_ERROR_ROW(ER_IDENT_CAUSES_TOO_LONG_PATH, IDENT_CAUSES_TOO_LONG_PATH, 1860)                                     \
    SQL_ERROR_ROW(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_NOT_NULL, ALTER_OPERATION_NOT_SUPPORTED_REASON_NOT_NULL,     \
                  1861)                                                                                                \
    SQL_ERROR_ROW(ER_MUST_CHANGE_PASSWORD_LOGIN, MUST_CHANGE_PASSWORD_LOGIN, 1862)                                     \
    SQL_ERROR_ROW(ER_ROW_IN_WRONG_PARTITION, ROW_IN_WRONG_PARTITION, 1863)                                             \
    SQL_ERROR_ROW(ER_MTS_EVENT_BIGGER_PENDING_JOBS_SIZE_MAX, MTS_EVENT_BIGGER_PENDING_JOBS_SIZE_MAX, 1864)             \
    SQL_ERROR_ROW(ER_INNODB_NO_FT_USES_PARSER, INNODB_NO_FT_USES_PARSER, 1865)                                         \
    SQL_ERROR_ROW(ER_BINLOG_LOGICAL_CORRUPTION, BINLOG_LOGICAL_CORRUPTION, 1866)                                       \
    SQL_ERROR_ROW(ER_WARN_PURGE_LOG_IN_USE, WARN_PURGE_LOG_IN_USE, 1867)                                               \
    SQL_ERROR_ROW(ER_WARN_PURGE_LOG_IS_ACTIVE, WARN_PURGE_LOG_IS_ACTIVE, 1868)                                         \
    SQL_ERROR_ROW(ER_AUTO_INCREMENT_CONFLICT, AUTO_INCREMENT_CONFLICT, 1869)                                           \
    SQL_ERROR_ROW(WARN_ON_BLOCKHOLE_IN_RBR, SQLWARN_ON_BLOCKHOLE_IN_RBR, 1870)                                         \
    SQL_ERROR_ROW(ER_SLAVE_MI_INIT_REPOSITORY, SLAVE_MI_INIT_REPOSITORY, 1871)                                         \
    SQL_ERROR_ROW(ER_SLAVE_RLI_INIT_REPOSITORY, SLAVE_RLI_INIT_REPOSITORY, 1872)                                       \
    SQL_ERROR_ROW(ER_ACCESS_DENIED_CHANGE_USER_ERROR, ACCESS_DENIED_CHANGE_USER_ERROR, 1873)                           \
    SQL_ERROR_ROW(ER_INNODB_READ_ONLY, INNODB_READ_ONLY, 1874)                                                         \
    SQL_ERROR_ROW(ER_STOP_SLAVE_SQL_THREAD_TIMEOUT, STOP_SLAVE_SQL_THREAD_TIMEOUT, 1875)                               \
    SQL_ERROR_ROW(ER_STOP_SLAVE_IO_THREAD_TIMEOUT, STOP_SLAVE_IO_THREAD_TIMEOUT, 1876)                                 \
    SQL_ERROR_ROW(ER_TABLE_CORRUPT, TABLE_CORRUPT, 1877)                                                               \
    SQL_ERROR_ROW(ER_TEMP_FILE_WRITE_FAILURE, TEMP_FILE_WRITE_FAILURE, 1878)                                           \
    SQL_ERROR_ROW(ER_INNODB_FT_AUX_NOT_HEX_ID, INNODB_FT_AUX_NOT_HEX_ID, 1879)                                         \
    /* SQL_ERROR_ROW(ER_LAST_MYSQL_ERROR_MESSAGE, LAST_MYSQL_ERROR_MESSAGE, 1880) */                                   \
    SQL_ERROR_ROW(ER_ERROR_LAST_SECTION_1, ERROR_LAST_SECTION_1, 1880)                                                 \
    SQL_ERROR_ROW(ER_ERROR_FIRST_SECTION_2, ERROR_FIRST_SECTION_2, 1900)                                               \
    /* SQL_ERROR_ROW(ER_UNUSED_18, UNUSED_18, 1900) */                                                                 \
    SQL_ERROR_ROW(ER_GENERATED_COLUMN_FUNCTION_IS_NOT_ALLOWED, GENERATED_COLUMN_FUNCTION_IS_NOT_ALLOWED, 1901)         \
    SQL_ERROR_ROW(ER_UNUSED_19, UNUSED_19, 1902)                                                                       \
    SQL_ERROR_ROW(ER_PRIMARY_KEY_BASED_ON_GENERATED_COLUMN, PRIMARY_KEY_BASED_ON_GENERATED_COLUMN, 1903)               \
    SQL_ERROR_ROW(ER_KEY_BASED_ON_GENERATED_VIRTUAL_COLUMN, KEY_BASED_ON_GENERATED_VIRTUAL_COLUMN, 1904)               \
    SQL_ERROR_ROW(ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN, WRONG_FK_OPTION_FOR_GENERATED_COLUMN, 1905)                 \
    SQL_ERROR_ROW(ER_WARNING_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN, WARNING_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN,   \
                  1906)                                                                                                \
    SQL_ERROR_ROW(ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN, UNSUPPORTED_ACTION_ON_GENERATED_COLUMN, 1907)             \
    SQL_ERROR_ROW(ER_UNUSED_20, UNUSED_20, 1908)                                                                       \
    SQL_ERROR_ROW(ER_UNUSED_21, UNUSED_21, 1909)                                                                       \
    SQL_ERROR_ROW(ER_UNSUPPORTED_ENGINE_FOR_GENERATED_COLUMNS, UNSUPPORTED_ENGINE_FOR_GENERATED_COLUMNS, 1910)         \
    SQL_ERROR_ROW(ER_UNKNOWN_OPTION, UNKNOWN_OPTION, 1911)                                                             \
    SQL_ERROR_ROW(ER_BAD_OPTION_VALUE, BAD_OPTION_VALUE, 1912)                                                         \
    SQL_ERROR_ROW(ER_UNUSED_6, UNUSED_6, 1913)                                                                         \
    SQL_ERROR_ROW(ER_UNUSED_7, UNUSED_7, 1914)                                                                         \
    SQL_ERROR_ROW(ER_UNUSED_8, UNUSED_8, 1915)                                                                         \
    SQL_ERROR_ROW(ER_DATA_OVERFLOW, DATA_OVERFLOW, 1916)                                                               \
    SQL_ERROR_ROW(ER_DATA_TRUNCATED, DATA_TRUNCATED, 1917)                                                             \
    SQL_ERROR_ROW(ER_BAD_DATA, BAD_DATA, 1918)                                                                         \
    SQL_ERROR_ROW(ER_DYN_COL_WRONG_FORMAT, DYN_COL_WRONG_FORMAT, 1919)                                                 \
    SQL_ERROR_ROW(ER_DYN_COL_IMPLEMENTATION_LIMIT, DYN_COL_IMPLEMENTATION_LIMIT, 1920)                                 \
    SQL_ERROR_ROW(ER_DYN_COL_DATA, DYN_COL_DATA, 1921)                                                                 \
    SQL_ERROR_ROW(ER_DYN_COL_WRONG_CHARSET, DYN_COL_WRONG_CHARSET, 1922)                                               \
    SQL_ERROR_ROW(ER_ILLEGAL_SUBQUERY_OPTIMIZER_SWITCHES, ILLEGAL_SUBQUERY_OPTIMIZER_SWITCHES, 1923)                   \
    SQL_ERROR_ROW(ER_QUERY_CACHE_IS_DISABLED, QUERY_CACHE_IS_DISABLED, 1924)                                           \
    SQL_ERROR_ROW(ER_QUERY_CACHE_IS_GLOBALY_DISABLED, QUERY_CACHE_IS_GLOBALY_DISABLED, 1925)                           \
    SQL_ERROR_ROW(ER_VIEW_ORDERBY_IGNORED, VIEW_ORDERBY_IGNORED, 1926)                                                 \
    SQL_ERROR_ROW(ER_CONNECTION_KILLED, CONNECTION_KILLED, 1927)                                                       \
    SQL_ERROR_ROW(ER_UNUSED_12, UNUSED_12, 1928)                                                                       \
    SQL_ERROR_ROW(ER_INSIDE_TRANSACTION_PREVENTS_SWITCH_SKIP_REPLICATION,                                              \
                  INSIDE_TRANSACTION_PREVENTS_SWITCH_SKIP_REPLICATION, 1929)                                           \
    SQL_ERROR_ROW(ER_STORED_FUNCTION_PREVENTS_SWITCH_SKIP_REPLICATION,                                                 \
                  STORED_FUNCTION_PREVENTS_SWITCH_SKIP_REPLICATION, 1930)                                              \
    SQL_ERROR_ROW(ER_QUERY_EXCEEDED_ROWS_EXAMINED_LIMIT, QUERY_EXCEEDED_ROWS_EXAMINED_LIMIT, 1931)                     \
    SQL_ERROR_ROW(ER_NO_SUCH_TABLE_IN_ENGINE, NO_SUCH_TABLE_IN_ENGINE, 1932)                                           \
    SQL_ERROR_ROW(ER_TARGET_NOT_EXPLAINABLE, TARGET_NOT_EXPLAINABLE, 1933)                                             \
    SQL_ERROR_ROW(ER_CONNECTION_ALREADY_EXISTS, CONNECTION_ALREADY_EXISTS, 1934)                                       \
    SQL_ERROR_ROW(ER_MASTER_LOG_PREFIX, MASTER_LOG_PREFIX, 1935)                                                       \
    SQL_ERROR_ROW(ER_CANT_START_STOP_SLAVE, CANT_START_STOP_SLAVE, 1936)                                               \
    SQL_ERROR_ROW(ER_SLAVE_STARTED, SLAVE_STARTED, 1937)                                                               \
    SQL_ERROR_ROW(ER_SLAVE_STOPPED, SLAVE_STOPPED, 1938)                                                               \
    SQL_ERROR_ROW(ER_SQL_DISCOVER_ERROR, SQL_DISCOVER_ERROR, 1939)                                                     \
    SQL_ERROR_ROW(ER_FAILED_GTID_STATE_INIT, FAILED_GTID_STATE_INIT, 1940)                                             \
    SQL_ERROR_ROW(ER_INCORRECT_GTID_STATE, INCORRECT_GTID_STATE, 1941)                                                 \
    SQL_ERROR_ROW(ER_CANNOT_UPDATE_GTID_STATE, CANNOT_UPDATE_GTID_STATE, 1942)                                         \
    SQL_ERROR_ROW(ER_DUPLICATE_GTID_DOMAIN, DUPLICATE_GTID_DOMAIN, 1943)                                               \
    SQL_ERROR_ROW(ER_GTID_OPEN_TABLE_FAILED, GTID_OPEN_TABLE_FAILED, 1944)                                             \
    SQL_ERROR_ROW(ER_GTID_POSITION_NOT_FOUND_IN_BINLOG, GTID_POSITION_NOT_FOUND_IN_BINLOG, 1945)                       \
    SQL_ERROR_ROW(ER_CANNOT_LOAD_SLAVE_GTID_STATE, CANNOT_LOAD_SLAVE_GTID_STATE, 1946)                                 \
    SQL_ERROR_ROW(ER_MASTER_GTID_POS_CONFLICTS_WITH_BINLOG, MASTER_GTID_POS_CONFLICTS_WITH_BINLOG, 1947)               \
    SQL_ERROR_ROW(ER_MASTER_GTID_POS_MISSING_DOMAIN, MASTER_GTID_POS_MISSING_DOMAIN, 1948)                             \
    SQL_ERROR_ROW(ER_UNTIL_REQUIRES_USING_GTID, UNTIL_REQUIRES_USING_GTID, 1949)                                       \
    SQL_ERROR_ROW(ER_GTID_STRICT_OUT_OF_ORDER, GTID_STRICT_OUT_OF_ORDER, 1950)                                         \
    SQL_ERROR_ROW(ER_GTID_START_FROM_BINLOG_HOLE, GTID_START_FROM_BINLOG_HOLE, 1951)                                   \
    SQL_ERROR_ROW(ER_SLAVE_UNEXPECTED_MASTER_SWITCH, SLAVE_UNEXPECTED_MASTER_SWITCH, 1952)                             \
    SQL_ERROR_ROW(ER_INSIDE_TRANSACTION_PREVENTS_SWITCH_GTID_DOMAIN_ID_SEQ_NO,                                         \
                  INSIDE_TRANSACTION_PREVENTS_SWITCH_GTID_DOMAIN_ID_SEQ_NO, 1953)                                      \
    SQL_ERROR_ROW(ER_STORED_FUNCTION_PREVENTS_SWITCH_GTID_DOMAIN_ID_SEQ_NO,                                            \
                  STORED_FUNCTION_PREVENTS_SWITCH_GTID_DOMAIN_ID_SEQ_NO, 1954)                                         \
    SQL_ERROR_ROW(ER_GTID_POSITION_NOT_FOUND_IN_BINLOG2, GTID_POSITION_NOT_FOUND_IN_BINLOG2, 1955)                     \
    SQL_ERROR_ROW(ER_BINLOG_MUST_BE_EMPTY, BINLOG_MUST_BE_EMPTY, 1956)                                                 \
    SQL_ERROR_ROW(ER_NO_SUCH_QUERY, NO_SUCH_QUERY, 1957)                                                               \
    SQL_ERROR_ROW(ER_BAD_BASE64_DATA, BAD_BASE64_DATA, 1958)                                                           \
    SQL_ERROR_ROW(ER_INVALID_ROLE, INVALID_ROLE, 1959)                                                                 \
    SQL_ERROR_ROW(ER_INVALID_CURRENT_USER, INVALID_CURRENT_USER, 1960)                                                 \
    SQL_ERROR_ROW(ER_CANNOT_GRANT_ROLE, CANNOT_GRANT_ROLE, 1961)                                                       \
    SQL_ERROR_ROW(ER_CANNOT_REVOKE_ROLE, CANNOT_REVOKE_ROLE, 1962)                                                     \
    SQL_ERROR_ROW(ER_CHANGE_SLAVE_PARALLEL_THREADS_ACTIVE, CHANGE_SLAVE_PARALLEL_THREADS_ACTIVE, 1963)                 \
    SQL_ERROR_ROW(ER_PRIOR_COMMIT_FAILED, PRIOR_COMMIT_FAILED, 1964)                                                   \
    SQL_ERROR_ROW(ER_IT_IS_A_VIEW, IT_IS_A_VIEW, 1965)                                                                 \
    SQL_ERROR_ROW(ER_SLAVE_SKIP_NOT_IN_GTID, SLAVE_SKIP_NOT_IN_GTID, 1966)                                             \
    SQL_ERROR_ROW(ER_TABLE_DEFINITION_TOO_BIG, TABLE_DEFINITION_TOO_BIG, 1967)                                         \
    SQL_ERROR_ROW(ER_PLUGIN_INSTALLED, PLUGIN_INSTALLED, 1968)                                                         \
    SQL_ERROR_ROW(ER_STATEMENT_TIMEOUT, STATEMENT_TIMEOUT, 1969)                                                       \
    SQL_ERROR_ROW(ER_SUBQUERIES_NOT_SUPPORTED, SUBQUERIES_NOT_SUPPORTED, 1970)                                         \
    SQL_ERROR_ROW(ER_SET_STATEMENT_NOT_SUPPORTED, SET_STATEMENT_NOT_SUPPORTED, 1971)                                   \
    SQL_ERROR_ROW(ER_UNUSED_9, UNUSED_9, 1972)                                                                         \
    SQL_ERROR_ROW(ER_USER_CREATE_EXISTS, USER_CREATE_EXISTS, 1973)                                                     \
    SQL_ERROR_ROW(ER_USER_DROP_EXISTS, USER_DROP_EXISTS, 1974)                                                         \
    SQL_ERROR_ROW(ER_ROLE_CREATE_EXISTS, ROLE_CREATE_EXISTS, 1975)                                                     \
    SQL_ERROR_ROW(ER_ROLE_DROP_EXISTS, ROLE_DROP_EXISTS, 1976)                                                         \
    SQL_ERROR_ROW(ER_CANNOT_CONVERT_CHARACTER, CANNOT_CONVERT_CHARACTER, 1977)                                         \
    SQL_ERROR_ROW(ER_INVALID_DEFAULT_VALUE_FOR_FIELD, INVALID_DEFAULT_VALUE_FOR_FIELD, 1978)                           \
    SQL_ERROR_ROW(ER_KILL_QUERY_DENIED_ERROR, KILL_QUERY_DENIED_ERROR, 1979)                                           \
    SQL_ERROR_ROW(ER_NO_EIS_FOR_FIELD, NO_EIS_FOR_FIELD, 1980)                                                         \
    SQL_ERROR_ROW(ER_WARN_AGGFUNC_DEPENDENCE, WARN_AGGFUNC_DEPENDENCE, 1981)                                           \
    /* SQL_ERROR_ROW(WARN_INNODB_PARTITION_OPTION_IGNORED, SQLWARN_INNODB_PARTITION_OPTION_IGNORED, 1982) */           \
    SQL_ERROR_ROW(ER_ERROR_LAST_SECTION_2, ERROR_LAST_SECTION_2, 1982)                                                 \
    SQL_ERROR_ROW(ER_ERROR_FIRST_SECTION_3, ERROR_FIRST_SECTION_3, 2000)                                               \
    /* SQL_ERROR_ROW(ER_ERROR_LAST_SECTION_3, ERROR_LAST_SECTION_3, 2000) */                                           \
    SQL_ERROR_ROW(ER_ERROR_FIRST_SECTION_4, ERROR_FIRST_SECTION_4, 3000)                                               \
    /* SQL_ERROR_ROW(ER_FILE_CORRUPT, FILE_CORRUPT, 3000) */                                                           \
    SQL_ERROR_ROW(ER_ERROR_ON_MASTER, ERROR_ON_MASTER, 3001)                                                           \
    SQL_ERROR_ROW(ER_INCONSISTENT_ERROR, INCONSISTENT_ERROR, 3002)                                                     \
    SQL_ERROR_ROW(ER_STORAGE_ENGINE_NOT_LOADED, STORAGE_ENGINE_NOT_LOADED, 3003)                                       \
    SQL_ERROR_ROW(ER_GET_STACKED_DA_WITHOUT_ACTIVE_HANDLER, GET_STACKED_DA_WITHOUT_ACTIVE_HANDLER, 3004)               \
    SQL_ERROR_ROW(ER_WARN_LEGACY_SYNTAX_CONVERTED, WARN_LEGACY_SYNTAX_CONVERTED, 3005)                                 \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_FULLTEXT_PLUGIN, BINLOG_UNSAFE_FULLTEXT_PLUGIN, 3006)                               \
    SQL_ERROR_ROW(ER_CANNOT_DISCARD_TEMPORARY_TABLE, CANNOT_DISCARD_TEMPORARY_TABLE, 3007)                             \
    SQL_ERROR_ROW(ER_FK_DEPTH_EXCEEDED, FK_DEPTH_EXCEEDED, 3008)                                                       \
    SQL_ERROR_ROW(ER_COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE_V2, COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE_V2, 3009)           \
    SQL_ERROR_ROW(ER_WARN_TRIGGER_DOESNT_HAVE_CREATED, WARN_TRIGGER_DOESNT_HAVE_CREATED, 3010)                         \
    SQL_ERROR_ROW(ER_REFERENCED_TRG_DOES_NOT_EXIST_MYSQL, REFERENCED_TRG_DOES_NOT_EXIST_MYSQL, 3011)                   \
    SQL_ERROR_ROW(ER_EXPLAIN_NOT_SUPPORTED, EXPLAIN_NOT_SUPPORTED, 3012)                                               \
    SQL_ERROR_ROW(ER_INVALID_FIELD_SIZE, INVALID_FIELD_SIZE, 3013)                                                     \
    SQL_ERROR_ROW(ER_MISSING_HA_CREATE_OPTION, MISSING_HA_CREATE_OPTION, 3014)                                         \
    SQL_ERROR_ROW(ER_ENGINE_OUT_OF_MEMORY, ENGINE_OUT_OF_MEMORY, 3015)                                                 \
    SQL_ERROR_ROW(ER_PASSWORD_EXPIRE_ANONYMOUS_USER, PASSWORD_EXPIRE_ANONYMOUS_USER, 3016)                             \
    SQL_ERROR_ROW(ER_SLAVE_SQL_THREAD_MUST_STOP, SLAVE_SQL_THREAD_MUST_STOP, 3017)                                     \
    SQL_ERROR_ROW(ER_NO_FT_MATERIALIZED_SUBQUERY, NO_FT_MATERIALIZED_SUBQUERY, 3018)                                   \
    SQL_ERROR_ROW(ER_INNODB_UNDO_LOG_FULL, INNODB_UNDO_LOG_FULL, 3019)                                                 \
    SQL_ERROR_ROW(ER_INVALID_ARGUMENT_FOR_LOGARITHM, INVALID_ARGUMENT_FOR_LOGARITHM, 3020)                             \
    SQL_ERROR_ROW(ER_SLAVE_CHANNEL_IO_THREAD_MUST_STOP, SLAVE_CHANNEL_IO_THREAD_MUST_STOP, 3021)                       \
    SQL_ERROR_ROW(ER_WARN_OPEN_TEMP_TABLES_MUST_BE_ZERO, WARN_OPEN_TEMP_TABLES_MUST_BE_ZERO, 3022)                     \
    SQL_ERROR_ROW(ER_WARN_ONLY_MASTER_LOG_FILE_NO_POS, WARN_ONLY_MASTER_LOG_FILE_NO_POS, 3023)                         \
    SQL_ERROR_ROW(ER_QUERY_TIMEOUT, QUERY_TIMEOUT, 3024)                                                               \
    SQL_ERROR_ROW(ER_NON_RO_SELECT_DISABLE_TIMER, NON_RO_SELECT_DISABLE_TIMER, 3025)                                   \
    SQL_ERROR_ROW(ER_DUP_LIST_ENTRY, DUP_LIST_ENTRY, 3026)                                                             \
    SQL_ERROR_ROW(ER_SQL_MODE_NO_EFFECT, SQL_MODE_NO_EFFECT, 3027)                                                     \
    SQL_ERROR_ROW(ER_AGGREGATE_ORDER_FOR_UNION, AGGREGATE_ORDER_FOR_UNION, 3028)                                       \
    SQL_ERROR_ROW(ER_AGGREGATE_ORDER_NON_AGG_QUERY, AGGREGATE_ORDER_NON_AGG_QUERY, 3029)                               \
    SQL_ERROR_ROW(ER_SLAVE_WORKER_STOPPED_PREVIOUS_THD_ERROR, SLAVE_WORKER_STOPPED_PREVIOUS_THD_ERROR, 3030)           \
    SQL_ERROR_ROW(ER_DONT_SUPPORT_SLAVE_PRESERVE_COMMIT_ORDER, DONT_SUPPORT_SLAVE_PRESERVE_COMMIT_ORDER, 3031)         \
    SQL_ERROR_ROW(ER_SERVER_OFFLINE_MODE, SERVER_OFFLINE_MODE, 3032)                                                   \
    SQL_ERROR_ROW(ER_GIS_DIFFERENT_SRIDS, GIS_DIFFERENT_SRIDS, 3033)                                                   \
    SQL_ERROR_ROW(ER_GIS_UNSUPPORTED_ARGUMENT, GIS_UNSUPPORTED_ARGUMENT, 3034)                                         \
    SQL_ERROR_ROW(ER_GIS_UNKNOWN_ERROR, GIS_UNKNOWN_ERROR, 3035)                                                       \
    SQL_ERROR_ROW(ER_GIS_UNKNOWN_EXCEPTION, GIS_UNKNOWN_EXCEPTION, 3036)                                               \
    SQL_ERROR_ROW(ER_GIS_INVALID_DATA, GIS_INVALID_DATA, 3037)                                                         \
    SQL_ERROR_ROW(ER_BOOST_GEOMETRY_EMPTY_INPUT_EXCEPTION, BOOST_GEOMETRY_EMPTY_INPUT_EXCEPTION, 3038)                 \
    SQL_ERROR_ROW(ER_BOOST_GEOMETRY_CENTROID_EXCEPTION, BOOST_GEOMETRY_CENTROID_EXCEPTION, 3039)                       \
    SQL_ERROR_ROW(ER_BOOST_GEOMETRY_OVERLAY_INVALID_INPUT_EXCEPTION, BOOST_GEOMETRY_OVERLAY_INVALID_INPUT_EXCEPTION,   \
                  3040)                                                                                                \
    SQL_ERROR_ROW(ER_BOOST_GEOMETRY_TURN_INFO_EXCEPTION, BOOST_GEOMETRY_TURN_INFO_EXCEPTION, 3041)                     \
    SQL_ERROR_ROW(ER_BOOST_GEOMETRY_SELF_INTERSECTION_POINT_EXCEPTION,                                                 \
                  BOOST_GEOMETRY_SELF_INTERSECTION_POINT_EXCEPTION, 3042)                                              \
    SQL_ERROR_ROW(ER_BOOST_GEOMETRY_UNKNOWN_EXCEPTION, BOOST_GEOMETRY_UNKNOWN_EXCEPTION, 3043)                         \
    SQL_ERROR_ROW(ER_STD_BAD_ALLOC_ERROR, STD_BAD_ALLOC_ERROR, 3044)                                                   \
    SQL_ERROR_ROW(ER_STD_DOMAIN_ERROR, STD_DOMAIN_ERROR, 3045)                                                         \
    SQL_ERROR_ROW(ER_STD_LENGTH_ERROR, STD_LENGTH_ERROR, 3046)                                                         \
    SQL_ERROR_ROW(ER_STD_INVALID_ARGUMENT, STD_INVALID_ARGUMENT, 3047)                                                 \
    SQL_ERROR_ROW(ER_STD_OUT_OF_RANGE_ERROR, STD_OUT_OF_RANGE_ERROR, 3048)                                             \
    SQL_ERROR_ROW(ER_STD_OVERFLOW_ERROR, STD_OVERFLOW_ERROR, 3049)                                                     \
    SQL_ERROR_ROW(ER_STD_RANGE_ERROR, STD_RANGE_ERROR, 3050)                                                           \
    SQL_ERROR_ROW(ER_STD_UNDERFLOW_ERROR, STD_UNDERFLOW_ERROR, 3051)                                                   \
    SQL_ERROR_ROW(ER_STD_LOGIC_ERROR, STD_LOGIC_ERROR, 3052)                                                           \
    SQL_ERROR_ROW(ER_STD_RUNTIME_ERROR, STD_RUNTIME_ERROR, 3053)                                                       \
    SQL_ERROR_ROW(ER_STD_UNKNOWN_EXCEPTION, STD_UNKNOWN_EXCEPTION, 3054)                                               \
    SQL_ERROR_ROW(ER_GIS_DATA_WRONG_ENDIANESS, GIS_DATA_WRONG_ENDIANESS, 3055)                                         \
    SQL_ERROR_ROW(ER_CHANGE_MASTER_PASSWORD_LENGTH, CHANGE_MASTER_PASSWORD_LENGTH, 3056)                               \
    SQL_ERROR_ROW(ER_USER_LOCK_WRONG_NAME, USER_LOCK_WRONG_NAME, 3057)                                                 \
    SQL_ERROR_ROW(ER_USER_LOCK_DEADLOCK, USER_LOCK_DEADLOCK, 3058)                                                     \
    SQL_ERROR_ROW(ER_REPLACE_INACCESSIBLE_ROWS, REPLACE_INACCESSIBLE_ROWS, 3059)                                       \
    /* SQL_ERROR_ROW(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_GIS, ALTER_OPERATION_NOT_SUPPORTED_REASON_GIS, 3060) */   \
    SQL_ERROR_ROW(ER_ERROR_LAST_SECTION_4, ERROR_LAST_SECTION_4, 3060)                                                 \
    SQL_ERROR_ROW(ER_ERROR_FIRST_SECTION_5, ERROR_FIRST_SECTION_5, 4000)                                               \
    /* SQL_ERROR_ROW(ER_UNUSED_26, UNUSED_26, 4000) */                                                                 \
    SQL_ERROR_ROW(ER_UNUSED_27, UNUSED_27, 4001)                                                                       \
    SQL_ERROR_ROW(ER_WITH_COL_WRONG_LIST, WITH_COL_WRONG_LIST, 4002)                                                   \
    SQL_ERROR_ROW(ER_TOO_MANY_DEFINITIONS_IN_WITH_CLAUSE, TOO_MANY_DEFINITIONS_IN_WITH_CLAUSE, 4003)                   \
    SQL_ERROR_ROW(ER_DUP_QUERY_NAME, DUP_QUERY_NAME, 4004)                                                             \
    SQL_ERROR_ROW(ER_RECURSIVE_WITHOUT_ANCHORS, RECURSIVE_WITHOUT_ANCHORS, 4005)                                       \
    SQL_ERROR_ROW(ER_UNACCEPTABLE_MUTUAL_RECURSION, UNACCEPTABLE_MUTUAL_RECURSION, 4006)                               \
    SQL_ERROR_ROW(ER_REF_TO_RECURSIVE_WITH_TABLE_IN_DERIVED, REF_TO_RECURSIVE_WITH_TABLE_IN_DERIVED, 4007)             \
    SQL_ERROR_ROW(ER_NOT_STANDARD_COMPLIANT_RECURSIVE, NOT_STANDARD_COMPLIANT_RECURSIVE, 4008)                         \
    SQL_ERROR_ROW(ER_WRONG_WINDOW_SPEC_NAME, WRONG_WINDOW_SPEC_NAME, 4009)                                             \
    SQL_ERROR_ROW(ER_DUP_WINDOW_NAME, DUP_WINDOW_NAME, 4010)                                                           \
    SQL_ERROR_ROW(ER_PARTITION_LIST_IN_REFERENCING_WINDOW_SPEC, PARTITION_LIST_IN_REFERENCING_WINDOW_SPEC, 4011)       \
    SQL_ERROR_ROW(ER_ORDER_LIST_IN_REFERENCING_WINDOW_SPEC, ORDER_LIST_IN_REFERENCING_WINDOW_SPEC, 4012)               \
    SQL_ERROR_ROW(ER_WINDOW_FRAME_IN_REFERENCED_WINDOW_SPEC, WINDOW_FRAME_IN_REFERENCED_WINDOW_SPEC, 4013)             \
    SQL_ERROR_ROW(ER_BAD_COMBINATION_OF_WINDOW_FRAME_BOUND_SPECS, BAD_COMBINATION_OF_WINDOW_FRAME_BOUND_SPECS, 4014)   \
    SQL_ERROR_ROW(ER_WRONG_PLACEMENT_OF_WINDOW_FUNCTION, WRONG_PLACEMENT_OF_WINDOW_FUNCTION, 4015)                     \
    SQL_ERROR_ROW(ER_WINDOW_FUNCTION_IN_WINDOW_SPEC, WINDOW_FUNCTION_IN_WINDOW_SPEC, 4016)                             \
    SQL_ERROR_ROW(ER_NOT_ALLOWED_WINDOW_FRAME, NOT_ALLOWED_WINDOW_FRAME, 4017)                                         \
    SQL_ERROR_ROW(ER_NO_ORDER_LIST_IN_WINDOW_SPEC, NO_ORDER_LIST_IN_WINDOW_SPEC, 4018)                                 \
    SQL_ERROR_ROW(ER_RANGE_FRAME_NEEDS_SIMPLE_ORDERBY, RANGE_FRAME_NEEDS_SIMPLE_ORDERBY, 4019)                         \
    SQL_ERROR_ROW(ER_WRONG_TYPE_FOR_ROWS_FRAME, WRONG_TYPE_FOR_ROWS_FRAME, 4020)                                       \
    SQL_ERROR_ROW(ER_WRONG_TYPE_FOR_RANGE_FRAME, WRONG_TYPE_FOR_RANGE_FRAME, 4021)                                     \
    SQL_ERROR_ROW(ER_FRAME_EXCLUSION_NOT_SUPPORTED, FRAME_EXCLUSION_NOT_SUPPORTED, 4022)                               \
    SQL_ERROR_ROW(ER_WINDOW_FUNCTION_DONT_HAVE_FRAME, WINDOW_FUNCTION_DONT_HAVE_FRAME, 4023)                           \
    SQL_ERROR_ROW(ER_INVALID_NTILE_ARGUMENT, INVALID_NTILE_ARGUMENT, 4024)                                             \
    SQL_ERROR_ROW(ER_CONSTRAINT_FAILED, CONSTRAINT_FAILED, 4025)                                                       \
    SQL_ERROR_ROW(ER_EXPRESSION_IS_TOO_BIG, EXPRESSION_IS_TOO_BIG, 4026)                                               \
    SQL_ERROR_ROW(ER_ERROR_EVALUATING_EXPRESSION, ERROR_EVALUATING_EXPRESSION, 4027)                                   \
    SQL_ERROR_ROW(ER_CALCULATING_DEFAULT_VALUE, CALCULATING_DEFAULT_VALUE, 4028)                                       \
    SQL_ERROR_ROW(ER_EXPRESSION_REFERS_TO_UNINIT_FIELD, EXPRESSION_REFERS_TO_UNINIT_FIELD, 4029)                       \
    SQL_ERROR_ROW(ER_PARTITION_DEFAULT_ERROR, PARTITION_DEFAULT_ERROR, 4030)                                           \
    SQL_ERROR_ROW(ER_REFERENCED_TRG_DOES_NOT_EXIST, REFERENCED_TRG_DOES_NOT_EXIST, 4031)                               \
    SQL_ERROR_ROW(ER_INVALID_DEFAULT_PARAM, INVALID_DEFAULT_PARAM, 4032)                                               \
    SQL_ERROR_ROW(ER_BINLOG_NON_SUPPORTED_BULK, BINLOG_NON_SUPPORTED_BULK, 4033)                                       \
    SQL_ERROR_ROW(ER_BINLOG_UNCOMPRESS_ERROR, BINLOG_UNCOMPRESS_ERROR, 4034)                                           \
    SQL_ERROR_ROW(ER_JSON_BAD_CHR, JSON_BAD_CHR, 4035)                                                                 \
    SQL_ERROR_ROW(ER_JSON_NOT_JSON_CHR, JSON_NOT_JSON_CHR, 4036)                                                       \
    SQL_ERROR_ROW(ER_JSON_EOS, JSON_EOS, 4037)                                                                         \
    SQL_ERROR_ROW(ER_JSON_SYNTAX, JSON_SYNTAX, 4038)                                                                   \
    SQL_ERROR_ROW(ER_JSON_ESCAPING, JSON_ESCAPING, 4039)                                                               \
    SQL_ERROR_ROW(ER_JSON_DEPTH, JSON_DEPTH, 4040)                                                                     \
    SQL_ERROR_ROW(ER_JSON_PATH_EOS, JSON_PATH_EOS, 4041)                                                               \
    SQL_ERROR_ROW(ER_JSON_PATH_SYNTAX, JSON_PATH_SYNTAX, 4042)                                                         \
    SQL_ERROR_ROW(ER_JSON_PATH_DEPTH, JSON_PATH_DEPTH, 4043)                                                           \
    SQL_ERROR_ROW(ER_JSON_PATH_NO_WILDCARD, JSON_PATH_NO_WILDCARD, 4044)                                               \
    SQL_ERROR_ROW(ER_JSON_PATH_ARRAY, JSON_PATH_ARRAY, 4045)                                                           \
    SQL_ERROR_ROW(ER_JSON_ONE_OR_ALL, JSON_ONE_OR_ALL, 4046)                                                           \
    SQL_ERROR_ROW(ER_UNSUPPORTED_COMPRESSED_TABLE, UNSUPPORTED_COMPRESSED_TABLE, 4047)                                 \
    SQL_ERROR_ROW(ER_GEOJSON_INCORRECT, GEOJSON_INCORRECT, 4048)                                                       \
    SQL_ERROR_ROW(ER_GEOJSON_TOO_FEW_POINTS, GEOJSON_TOO_FEW_POINTS, 4049)                                             \
    SQL_ERROR_ROW(ER_GEOJSON_NOT_CLOSED, GEOJSON_NOT_CLOSED, 4050)                                                     \
    SQL_ERROR_ROW(ER_JSON_PATH_EMPTY, JSON_PATH_EMPTY, 4051)                                                           \
    SQL_ERROR_ROW(ER_SLAVE_SAME_ID, SLAVE_SAME_ID, 4052)                                                               \
    SQL_ERROR_ROW(ER_FLASHBACK_NOT_SUPPORTED, FLASHBACK_NOT_SUPPORTED, 4053)                                           \
    SQL_ERROR_ROW(ER_KEYS_OUT_OF_ORDER, KEYS_OUT_OF_ORDER, 4054)                                                       \
    SQL_ERROR_ROW(ER_OVERLAPPING_KEYS, OVERLAPPING_KEYS, 4055)                                                         \
    SQL_ERROR_ROW(ER_REQUIRE_ROW_BINLOG_FORMAT, REQUIRE_ROW_BINLOG_FORMAT, 4056)                                       \
    SQL_ERROR_ROW(ER_ISOLATION_MODE_NOT_SUPPORTED, ISOLATION_MODE_NOT_SUPPORTED, 4057)                                 \
    SQL_ERROR_ROW(ER_ON_DUPLICATE_DISABLED, ON_DUPLICATE_DISABLED, 4058)                                               \
    SQL_ERROR_ROW(ER_UPDATES_WITH_CONSISTENT_SNAPSHOT, UPDATES_WITH_CONSISTENT_SNAPSHOT, 4059)                         \
    SQL_ERROR_ROW(ER_ROLLBACK_ONLY, ROLLBACK_ONLY, 4060)                                                               \
    SQL_ERROR_ROW(ER_ROLLBACK_TO_SAVEPOINT, ROLLBACK_TO_SAVEPOINT, 4061)                                               \
    SQL_ERROR_ROW(ER_ISOLATION_LEVEL_WITH_CONSISTENT_SNAPSHOT, ISOLATION_LEVEL_WITH_CONSISTENT_SNAPSHOT, 4062)         \
    SQL_ERROR_ROW(ER_UNSUPPORTED_COLLATION, UNSUPPORTED_COLLATION, 4063)                                               \
    SQL_ERROR_ROW(ER_METADATA_INCONSISTENCY, METADATA_INCONSISTENCY, 4064)                                             \
    SQL_ERROR_ROW(ER_CF_DIFFERENT, CF_DIFFERENT, 4065)                                                                 \
    SQL_ERROR_ROW(ER_RDB_TTL_DURATION_FORMAT, RDB_TTL_DURATION_FORMAT, 4066)                                           \
    SQL_ERROR_ROW(ER_RDB_STATUS_GENERAL, RDB_STATUS_GENERAL, 4067)                                                     \
    SQL_ERROR_ROW(ER_RDB_STATUS_MSG, RDB_STATUS_MSG, 4068)                                                             \
    SQL_ERROR_ROW(ER_RDB_TTL_UNSUPPORTED, RDB_TTL_UNSUPPORTED, 4069)                                                   \
    SQL_ERROR_ROW(ER_RDB_TTL_COL_FORMAT, RDB_TTL_COL_FORMAT, 4070)                                                     \
    SQL_ERROR_ROW(ER_PER_INDEX_CF_DEPRECATED, PER_INDEX_CF_DEPRECATED, 4071)                                           \
    SQL_ERROR_ROW(ER_KEY_CREATE_DURING_ALTER, KEY_CREATE_DURING_ALTER, 4072)                                           \
    SQL_ERROR_ROW(ER_SK_POPULATE_DURING_ALTER, SK_POPULATE_DURING_ALTER, 4073)                                         \
    SQL_ERROR_ROW(ER_SUM_FUNC_WITH_WINDOW_FUNC_AS_ARG, SUM_FUNC_WITH_WINDOW_FUNC_AS_ARG, 4074)                         \
    SQL_ERROR_ROW(ER_NET_OK_PACKET_TOO_LARGE, NET_OK_PACKET_TOO_LARGE, 4075)                                           \
    SQL_ERROR_ROW(ER_GEOJSON_EMPTY_COORDINATES, GEOJSON_EMPTY_COORDINATES, 4076)                                       \
    SQL_ERROR_ROW(ER_MYROCKS_CANT_NOPAD_COLLATION, MYROCKS_CANT_NOPAD_COLLATION, 4077)                                 \
    SQL_ERROR_ROW(ER_ILLEGAL_PARAMETER_DATA_TYPES2_FOR_OPERATION, ILLEGAL_PARAMETER_DATA_TYPES2_FOR_OPERATION, 4078)   \
    SQL_ERROR_ROW(ER_ILLEGAL_PARAMETER_DATA_TYPE_FOR_OPERATION, ILLEGAL_PARAMETER_DATA_TYPE_FOR_OPERATION, 4079)       \
    SQL_ERROR_ROW(ER_WRONG_PARAMCOUNT_TO_CURSOR, WRONG_PARAMCOUNT_TO_CURSOR, 4080)                                     \
    SQL_ERROR_ROW(ER_UNKNOWN_STRUCTURED_VARIABLE, UNKNOWN_STRUCTURED_VARIABLE, 4081)                                   \
    SQL_ERROR_ROW(ER_ROW_VARIABLE_DOES_NOT_HAVE_FIELD, ROW_VARIABLE_DOES_NOT_HAVE_FIELD, 4082)                         \
    SQL_ERROR_ROW(ER_END_IDENTIFIER_DOES_NOT_MATCH, END_IDENTIFIER_DOES_NOT_MATCH, 4083)                               \
    SQL_ERROR_ROW(ER_SEQUENCE_RUN_OUT, SEQUENCE_RUN_OUT, 4084)                                                         \
    SQL_ERROR_ROW(ER_SEQUENCE_INVALID_DATA, SEQUENCE_INVALID_DATA, 4085)                                               \
    SQL_ERROR_ROW(ER_SEQUENCE_INVALID_TABLE_STRUCTURE, SEQUENCE_INVALID_TABLE_STRUCTURE, 4086)                         \
    SQL_ERROR_ROW(ER_SEQUENCE_ACCESS_ERROR, SEQUENCE_ACCESS_ERROR, 4087)                                               \
    SQL_ERROR_ROW(ER_SEQUENCE_BINLOG_FORMAT, SEQUENCE_BINLOG_FORMAT, 4088)                                             \
    SQL_ERROR_ROW(ER_NOT_SEQUENCE, NOT_SEQUENCE, 4089)                                                                 \
    SQL_ERROR_ROW(ER_NOT_SEQUENCE2, NOT_SEQUENCE2, 4090)                                                               \
    SQL_ERROR_ROW(ER_UNKNOWN_SEQUENCES, UNKNOWN_SEQUENCES, 4091)                                                       \
    SQL_ERROR_ROW(ER_UNKNOWN_VIEW, UNKNOWN_VIEW, 4092)                                                                 \
    SQL_ERROR_ROW(ER_WRONG_INSERT_INTO_SEQUENCE, WRONG_INSERT_INTO_SEQUENCE, 4093)                                     \
    SQL_ERROR_ROW(ER_SP_STACK_TRACE, SP_STACK_TRACE, 4094)                                                             \
    SQL_ERROR_ROW(ER_PACKAGE_ROUTINE_IN_SPEC_NOT_DEFINED_IN_BODY, PACKAGE_ROUTINE_IN_SPEC_NOT_DEFINED_IN_BODY, 4095)   \
    SQL_ERROR_ROW(ER_PACKAGE_ROUTINE_FORWARD_DECLARATION_NOT_DEFINED, PACKAGE_ROUTINE_FORWARD_DECLARATION_NOT_DEFINED, \
                  4096)                                                                                                \
    SQL_ERROR_ROW(ER_COMPRESSED_COLUMN_USED_AS_KEY, COMPRESSED_COLUMN_USED_AS_KEY, 4097)                               \
    SQL_ERROR_ROW(ER_UNKNOWN_COMPRESSION_METHOD, UNKNOWN_COMPRESSION_METHOD, 4098)                                     \
    SQL_ERROR_ROW(ER_WRONG_NUMBER_OF_VALUES_IN_TVC, WRONG_NUMBER_OF_VALUES_IN_TVC, 4099)                               \
    SQL_ERROR_ROW(ER_FIELD_REFERENCE_IN_TVC, FIELD_REFERENCE_IN_TVC, 4100)                                             \
    SQL_ERROR_ROW(ER_WRONG_TYPE_FOR_PERCENTILE_FUNC, WRONG_TYPE_FOR_PERCENTILE_FUNC, 4101)                             \
    SQL_ERROR_ROW(ER_ARGUMENT_NOT_CONSTANT, ARGUMENT_NOT_CONSTANT, 4102)                                               \
    SQL_ERROR_ROW(ER_ARGUMENT_OUT_OF_RANGE, ARGUMENT_OUT_OF_RANGE, 4103)                                               \
    SQL_ERROR_ROW(ER_WRONG_TYPE_OF_ARGUMENT, WRONG_TYPE_OF_ARGUMENT, 4104)                                             \
    SQL_ERROR_ROW(ER_NOT_AGGREGATE_FUNCTION, NOT_AGGREGATE_FUNCTION, 4105)                                             \
    SQL_ERROR_ROW(ER_INVALID_AGGREGATE_FUNCTION, INVALID_AGGREGATE_FUNCTION, 4106)                                     \
    SQL_ERROR_ROW(ER_INVALID_VALUE_TO_LIMIT, INVALID_VALUE_TO_LIMIT, 4107)                                             \
    SQL_ERROR_ROW(ER_INVISIBLE_NOT_NULL_WITHOUT_DEFAULT, INVISIBLE_NOT_NULL_WITHOUT_DEFAULT, 4108)                     \
    SQL_ERROR_ROW(ER_UPDATE_INFO_WITH_SYSTEM_VERSIONING, UPDATE_INFO_WITH_SYSTEM_VERSIONING, 4109)                     \
    SQL_ERROR_ROW(ER_VERS_FIELD_WRONG_TYPE, VERS_FIELD_WRONG_TYPE, 4110)                                               \
    SQL_ERROR_ROW(ER_VERS_ENGINE_UNSUPPORTED, VERS_ENGINE_UNSUPPORTED, 4111)                                           \
    SQL_ERROR_ROW(ER_UNUSED_23, UNUSED_23, 4112)                                                                       \
    SQL_ERROR_ROW(ER_PARTITION_WRONG_TYPE, PARTITION_WRONG_TYPE, 4113)                                                 \
    SQL_ERROR_ROW(WARN_VERS_PART_FULL, SQLWARN_VERS_PART_FULL, 4114)                                                   \
    SQL_ERROR_ROW(WARN_VERS_PARAMETERS, SQLWARN_VERS_PARAMETERS, 4115)                                                 \
    SQL_ERROR_ROW(ER_VERS_DROP_PARTITION_INTERVAL, VERS_DROP_PARTITION_INTERVAL, 4116)                                 \
    SQL_ERROR_ROW(ER_UNUSED_25, UNUSED_25, 4117)                                                                       \
    SQL_ERROR_ROW(WARN_VERS_PART_NON_HISTORICAL, SQLWARN_VERS_PART_NON_HISTORICAL, 4118)                               \
    SQL_ERROR_ROW(ER_VERS_ALTER_NOT_ALLOWED, VERS_ALTER_NOT_ALLOWED, 4119)                                             \
    SQL_ERROR_ROW(ER_VERS_ALTER_ENGINE_PROHIBITED, VERS_ALTER_ENGINE_PROHIBITED, 4120)                                 \
    SQL_ERROR_ROW(ER_VERS_RANGE_PROHIBITED, VERS_RANGE_PROHIBITED, 4121)                                               \
    SQL_ERROR_ROW(ER_CONFLICTING_FOR_SYSTEM_TIME, CONFLICTING_FOR_SYSTEM_TIME, 4122)                                   \
    SQL_ERROR_ROW(ER_VERS_TABLE_MUST_HAVE_COLUMNS, VERS_TABLE_MUST_HAVE_COLUMNS, 4123)                                 \
    SQL_ERROR_ROW(ER_VERS_NOT_VERSIONED, VERS_NOT_VERSIONED, 4124)                                                     \
    SQL_ERROR_ROW(ER_MISSING, MISSING, 4125)                                                                           \
    SQL_ERROR_ROW(ER_VERS_PERIOD_COLUMNS, VERS_PERIOD_COLUMNS, 4126)                                                   \
    SQL_ERROR_ROW(ER_PART_WRONG_VALUE, PART_WRONG_VALUE, 4127)                                                         \
    SQL_ERROR_ROW(ER_VERS_WRONG_PARTS, VERS_WRONG_PARTS, 4128)                                                         \
    SQL_ERROR_ROW(ER_VERS_NO_TRX_ID, VERS_NO_TRX_ID, 4129)                                                             \
    SQL_ERROR_ROW(ER_VERS_ALTER_SYSTEM_FIELD, VERS_ALTER_SYSTEM_FIELD, 4130)                                           \
    SQL_ERROR_ROW(ER_DROP_VERSIONING_SYSTEM_TIME_PARTITION, DROP_VERSIONING_SYSTEM_TIME_PARTITION, 4131)               \
    SQL_ERROR_ROW(ER_VERS_DB_NOT_SUPPORTED, VERS_DB_NOT_SUPPORTED, 4132)                                               \
    SQL_ERROR_ROW(ER_VERS_TRT_IS_DISABLED, VERS_TRT_IS_DISABLED, 4133)                                                 \
    SQL_ERROR_ROW(ER_VERS_DUPLICATE_ROW_START_END, VERS_DUPLICATE_ROW_START_END, 4134)                                 \
    SQL_ERROR_ROW(ER_VERS_ALREADY_VERSIONED, VERS_ALREADY_VERSIONED, 4135)                                             \
    SQL_ERROR_ROW(ER_UNUSED_24, UNUSED_24, 4136)                                                                       \
    SQL_ERROR_ROW(ER_VERS_NOT_SUPPORTED, VERS_NOT_SUPPORTED, 4137)                                                     \
    SQL_ERROR_ROW(ER_VERS_TRX_PART_HISTORIC_ROW_NOT_SUPPORTED, VERS_TRX_PART_HISTORIC_ROW_NOT_SUPPORTED, 4138)         \
    SQL_ERROR_ROW(ER_INDEX_FILE_FULL, INDEX_FILE_FULL, 4139)                                                           \
    SQL_ERROR_ROW(ER_UPDATED_COLUMN_ONLY_ONCE, UPDATED_COLUMN_ONLY_ONCE, 4140)                                         \
    SQL_ERROR_ROW(ER_EMPTY_ROW_IN_TVC, EMPTY_ROW_IN_TVC, 4141)                                                         \
    SQL_ERROR_ROW(ER_VERS_QUERY_IN_PARTITION, VERS_QUERY_IN_PARTITION, 4142)                                           \
    SQL_ERROR_ROW(ER_KEY_DOESNT_SUPPORT, KEY_DOESNT_SUPPORT, 4143)                                                     \
    SQL_ERROR_ROW(ER_ALTER_OPERATION_TABLE_OPTIONS_NEED_REBUILD, ALTER_OPERATION_TABLE_OPTIONS_NEED_REBUILD, 4144)     \
    SQL_ERROR_ROW(ER_BACKUP_LOCK_IS_ACTIVE, BACKUP_LOCK_IS_ACTIVE, 4145)                                               \
    SQL_ERROR_ROW(ER_BACKUP_NOT_RUNNING, BACKUP_NOT_RUNNING, 4146)                                                     \
    SQL_ERROR_ROW(ER_BACKUP_WRONG_STAGE, BACKUP_WRONG_STAGE, 4147)                                                     \
    SQL_ERROR_ROW(ER_BACKUP_STAGE_FAILED, BACKUP_STAGE_FAILED, 4148)                                                   \
    SQL_ERROR_ROW(ER_BACKUP_UNKNOWN_STAGE, BACKUP_UNKNOWN_STAGE, 4149)                                                 \
    SQL_ERROR_ROW(ER_USER_IS_BLOCKED, USER_IS_BLOCKED, 4150)                                                           \
    SQL_ERROR_ROW(ER_ACCOUNT_HAS_BEEN_LOCKED, ACCOUNT_HAS_BEEN_LOCKED, 4151)                                           \
    SQL_ERROR_ROW(ER_PERIOD_TEMPORARY_NOT_ALLOWED, PERIOD_TEMPORARY_NOT_ALLOWED, 4152)                                 \
    SQL_ERROR_ROW(ER_PERIOD_TYPES_MISMATCH, PERIOD_TYPES_MISMATCH, 4153)                                               \
    SQL_ERROR_ROW(ER_MORE_THAN_ONE_PERIOD, MORE_THAN_ONE_PERIOD, 4154)                                                 \
    SQL_ERROR_ROW(ER_PERIOD_FIELD_WRONG_ATTRIBUTES, PERIOD_FIELD_WRONG_ATTRIBUTES, 4155)                               \
    SQL_ERROR_ROW(ER_PERIOD_NOT_FOUND, PERIOD_NOT_FOUND, 4156)                                                         \
    SQL_ERROR_ROW(ER_PERIOD_COLUMNS_UPDATED, PERIOD_COLUMNS_UPDATED, 4157)                                             \
    SQL_ERROR_ROW(ER_PERIOD_CONSTRAINT_DROP, PERIOD_CONSTRAINT_DROP, 4158)                                             \
    SQL_ERROR_ROW(ER_TOO_LONG_KEYPART, TOO_LONG_KEYPART, 4159)                                                         \
    SQL_ERROR_ROW(ER_TOO_LONG_DATABASE_COMMENT, TOO_LONG_DATABASE_COMMENT, 4160)                                       \
    SQL_ERROR_ROW(ER_UNKNOWN_DATA_TYPE, UNKNOWN_DATA_TYPE, 4161)                                                       \
    SQL_ERROR_ROW(ER_UNKNOWN_OPERATOR, UNKNOWN_OPERATOR, 4162)                                                         \
    SQL_ERROR_ROW(ER_WARN_HISTORY_ROW_START_TIME, WARN_HISTORY_ROW_START_TIME, 4163)                                   \
    SQL_ERROR_ROW(ER_PART_STARTS_BEYOND_INTERVAL, PART_STARTS_BEYOND_INTERVAL, 4164)                                   \
    SQL_ERROR_ROW(ER_GALERA_REPLICATION_NOT_SUPPORTED, GALERA_REPLICATION_NOT_SUPPORTED, 4165)                         \
    SQL_ERROR_ROW(ER_LOAD_INFILE_CAPABILITY_DISABLED, LOAD_INFILE_CAPABILITY_DISABLED, 4166)                           \
    SQL_ERROR_ROW(ER_NO_SECURE_TRANSPORTS_CONFIGURED, NO_SECURE_TRANSPORTS_CONFIGURED, 4167)                           \
    SQL_ERROR_ROW(ER_SLAVE_IGNORED_SHARED_TABLE, SLAVE_IGNORED_SHARED_TABLE, 4168)                                     \
    SQL_ERROR_ROW(ER_NO_AUTOINCREMENT_WITH_UNIQUE, NO_AUTOINCREMENT_WITH_UNIQUE, 4169)                                 \
    SQL_ERROR_ROW(ER_KEY_CONTAINS_PERIOD_FIELDS, KEY_CONTAINS_PERIOD_FIELDS, 4170)                                     \
    SQL_ERROR_ROW(ER_KEY_CANT_HAVE_WITHOUT_OVERLAPS, KEY_CANT_HAVE_WITHOUT_OVERLAPS, 4171)                             \
    SQL_ERROR_ROW(ER_NOT_ALLOWED_IN_THIS_CONTEXT, NOT_ALLOWED_IN_THIS_CONTEXT, 4172)                                   \
    SQL_ERROR_ROW(ER_DATA_WAS_COMMITED_UNDER_ROLLBACK, DATA_WAS_COMMITED_UNDER_ROLLBACK, 4173)                         \
    SQL_ERROR_ROW(ER_PK_INDEX_CANT_BE_IGNORED, PK_INDEX_CANT_BE_IGNORED, 4174)                                         \
    SQL_ERROR_ROW(ER_BINLOG_UNSAFE_SKIP_LOCKED, BINLOG_UNSAFE_SKIP_LOCKED, 4175)                                       \
    SQL_ERROR_ROW(ER_JSON_TABLE_ERROR_ON_FIELD, JSON_TABLE_ERROR_ON_FIELD, 4176)                                       \
    SQL_ERROR_ROW(ER_JSON_TABLE_ALIAS_REQUIRED, JSON_TABLE_ALIAS_REQUIRED, 4177)                                       \
    SQL_ERROR_ROW(ER_JSON_TABLE_SCALAR_EXPECTED, JSON_TABLE_SCALAR_EXPECTED, 4178)                                     \
    SQL_ERROR_ROW(ER_JSON_TABLE_MULTIPLE_MATCHES, JSON_TABLE_MULTIPLE_MATCHES, 4179)                                   \
    SQL_ERROR_ROW(ER_WITH_TIES_NEEDS_ORDER, WITH_TIES_NEEDS_ORDER, 4180)                                               \
    SQL_ERROR_ROW(ER_REMOVED_ORPHAN_TRIGGER, REMOVED_ORPHAN_TRIGGER, 4181)                                             \
    SQL_ERROR_ROW(ER_STORAGE_ENGINE_DISABLED, STORAGE_ENGINE_DISABLED, 4182)                                           \
    SQL_ERROR_ROW(WARN_SFORMAT_ERROR, SQLWARN_SFORMAT_ERROR, 4183)                                                     \
    SQL_ERROR_ROW(ER_PARTITION_CONVERT_SUBPARTITIONED, PARTITION_CONVERT_SUBPARTITIONED, 4184)                         \
    SQL_ERROR_ROW(ER_PROVIDER_NOT_LOADED, PROVIDER_NOT_LOADED, 4185)                                                   \
    SQL_ERROR_ROW(ER_JSON_HISTOGRAM_PARSE_FAILED, JSON_HISTOGRAM_PARSE_FAILED, 4186)                                   \
    SQL_ERROR_ROW(ER_SF_OUT_INOUT_ARG_NOT_ALLOWED, SF_OUT_INOUT_ARG_NOT_ALLOWED, 4187)                                 \
    SQL_ERROR_ROW(ER_INCONSISTENT_SLAVE_TEMP_TABLE, INCONSISTENT_SLAVE_TEMP_TABLE, 4188)

class SqlErrorCategory;

class SqlError {
public:
    enum Code : uint32_t {
#define SQL_ERROR_ROW(RowDef, Def, Code) Def = Code,
        SQL_ERROR_TABLE
#undef SQL_ERROR_ROW
    };
    SqlError(int64_t err, const std::error_category &c);
    SqlError(Code err, const std::string &message = "");
    SqlError(const SqlError &err);
    SqlError();
    ~SqlError();
    auto isOk() const -> bool;
    auto value() const -> int64_t;
    auto message() const -> std::string;
    auto category() const -> const std::error_category &;
    auto toString() const -> std::string;
    auto error() const -> Code;
    auto operator=(const SqlError &) -> SqlError & = default;

private:
    int                        mErr      = OK;
    const std::error_category *mCategory = nullptr;
    std::string                mMessage;
};

class SqlErrorCategory final : public std::error_category {
public:
    SqlErrorCategory();
    ~SqlErrorCategory();
    auto        message(int value) const -> std::string override;
    auto        name() const noexcept -> const char        *override;
    static auto instance() -> const SqlErrorCategory &;
};

inline SqlError::SqlError(int64_t err, const std::error_category &c) : mErr(err), mCategory(&c) {
}
inline SqlError::SqlError(Code err, const std::string &message)
    : mErr(err), mCategory(&SqlErrorCategory::instance()), mMessage(message) {
}
inline SqlError::SqlError(const SqlError &err) : mErr(err.mErr), mCategory(err.mCategory), mMessage(err.mMessage) {
}
inline SqlError::SqlError() : mErr(OK), mCategory(&SqlErrorCategory::instance()) {
}
inline SqlError::~SqlError() {
}
inline auto SqlError::isOk() const -> bool {
    return mErr == OK;
}
inline auto SqlError::value() const -> int64_t {
    return mErr;
}

inline auto SqlError::error() const -> Code {
    return (Code)mErr;
}

inline auto SqlError::message() const -> std::string {
    if (mMessage != "") {
        return mMessage;
    }
    std::string_view rawErrorName;
    switch (mErr) {
        // FIXME: mariadb has same error code, can't use switch case, only first case is executed.
#define SQL_ERROR_ROW(RowDef, Def, Code)                                                                               \
    case (Code):                                                                                                       \
        rawErrorName = #RowDef;                                                                                        \
        break;
        SQL_ERROR_TABLE
#undef SQL_ERROR_ROW
    }

    return std::string(rawErrorName) + "(" + std::to_string(mErr) + ")";
}

inline auto SqlError::category() const -> const std::error_category & {
    return *mCategory;
}

inline auto SqlError::toString() const -> std::string {
    if (mMessage != "") {
        return mMessage;
    }
    std::string_view rawErrorName;
    switch (mErr) {
        // FIXME: mariadb has same error code, can't use switch case, only first case is executed.
#define SQL_ERROR_ROW(RowDef, Def, Code)                                                                               \
    case (Code):                                                                                                       \
        rawErrorName = #RowDef;                                                                                        \
        break;
        SQL_ERROR_TABLE
#undef SQL_ERROR_ROW
    }

    return std::string(rawErrorName);
}

inline SqlErrorCategory::SqlErrorCategory() {
}

inline SqlErrorCategory::~SqlErrorCategory() {
}

inline auto SqlErrorCategory::instance() -> const SqlErrorCategory & {
    static SqlErrorCategory c;
    return c;
}

inline auto SqlErrorCategory::message(int value) const -> std::string {
    return SqlError(value, *this).message();
}

inline auto SqlErrorCategory::name() const noexcept -> const char * {
    return "sql";
}

ILIAS_DECLARE_ERROR(SqlError::Code, SqlErrorCategory)
#undef SQL_ERROR_TABLE
inline auto make_error_code(SqlError::Code t) noexcept -> std::error_code {
    return {static_cast<int>(t), _ilias_error_category_of(t)};
}

ILIAS_MYSQL_NS_END

template <>
struct std::is_error_code_enum<ILIAS_MYSQL_COMPLETE_NAMESPACE::SqlError::Code> : std::true_type {};