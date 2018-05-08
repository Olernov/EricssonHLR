#pragma once

#define MAX_THREADS							16
#define MAX_DMS_RESPONSE_LEN				1000
#define ERROR_INIT_PARAMS					-1
#define INIT_FAIL							-2
#define ERROR_CMD_TOO_LONG					-3
#define CMD_NOTEXECUTED						-10
#define CMD_UNKNOWN							-30
#define EXCEPTION_CAUGHT					-999
#define OPERATION_SUCCESS					0
#define TRY_LATER							-333333
#define MAX_COMMAND_LEN						1020

#define SOCKET_TIMEOUT_SEC					10

#define NUM_OF_EXECUTE_COMMAND_PARAMS		1

const int mainThreadIndex = -1;