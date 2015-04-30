#ifndef GLOBAL_DEFINE_H_
#define GLOBAL_DEFINE_H_

#include <winsock2.h>		// 必须在windows.h之前包含， 否则网络模块编译会出错
#include <MSWSock.h>
#include <windows.h> 
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <vector>
#include <map>
#include <list>
#include "lock.h"
using namespace std;

#pragma comment(lib,"ws2_32.lib")

#pragma warning(disable:4996)
#pragma warning(disable:4244)
//1字节
#ifndef xe_int8
#define xe_int8		char
#endif

#ifndef xe_uint8
#define xe_uint8	unsigned char
#endif

//2个字节
#ifndef xe_int16
#define xe_int16	short
#endif

//2个字节
#ifndef xe_uint16
#define xe_uint16	unsigned short
#endif


//4个字节
#ifndef xe_int32
#define xe_int32	int
#endif


#ifndef xe_uint32
#define xe_uint32	unsigned int
#endif

//8字节
#ifndef xe_int64
#define xe_int64	long long
#endif

#ifndef xe_uint64
#define xe_uint64	unsigned long long
#endif

#ifndef xe_bool
#define xe_bool		bool
#endif

#ifndef xe_float
#define xe_float	float
#endif

#ifndef xe_double
#define xe_double	double
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef SAFE_FREE
#define SAFE_FREE(p)	{if(p) free(p);p=NULL;}
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)	{if(p) delete p;p=NULL;}
#endif

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(ptr) {if(ptr){delete []ptr;ptr=NULL;}}
#endif

// 释放句柄宏
#ifndef	RELEASE_HANDLE
#define RELEASE_HANDLE(x)               {if(x != NULL && x!=INVALID_HANDLE_VALUE){ CloseHandle(x);x = NULL;}}
#endif

// 释放Socket宏
#ifndef RELEASE_SOCKET
#define RELEASE_SOCKET(x)               {if(x !=INVALID_SOCKET) { closesocket(x);x=INVALID_SOCKET;}}
#endif

// 默认端口
#define DEFAULT_PORT          12345    
// 默认IP地址
#define DEFAULT_IP            "127.0.0.1"


// 同时投递的Accept请求的数量(看同时链接的最大数量),
#define MAX_POST_ACCEPT              100
// 传递给Worker线程的退出信号
#define EXIT_CODE                    NULL


#define MARK_CRC		0x1234
#define MAX_BUFFER		8180					//8180(msg len)+sizeof(stPackHeader) <= 8192
#define DATA_BUFSIZE	4080					//整个包不超过4096

#define PACKET_KEEPALIVE				"%++$"	//心跳包的内容

//内存对齐
#pragma pack(push)
#pragma pack(1)


//包头
struct stPackHeader
{
	xe_uint16 wCrc;				//crc
	xe_uint16 wSize;			//出了包头的大小
	xe_uint8  bCmdGroup;		//主消息
	xe_uint8  bCmd;				//子消息
};

//包尾
#define PACKEND "$end$"
#define PACKENDLEN strlen(PACKEND)


#pragma pack(pop)



//中文处理，防止繁体字
//字符串查找

char * xe_strstr(const char* src,const char* szFind);

//字符查找
char * xe_strchr(const char* src,int ch);


//字符串转换为整数和整数转换为字符串
xe_int32	AnsiStrToVal(const char *nptr);
char*		ValToAnsiStr(xe_uint32 val, char *buf);

int			memlen(const char *str);
#endif