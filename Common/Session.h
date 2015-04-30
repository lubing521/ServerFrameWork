//////////////////////////////////////////////////////////////////////////
///Session 信息和 User信息和Gate信息,模仿传奇世界来管理socket
//////////////////////////////////////////////////////////////////////////
#ifndef _SESSION_H_
#define _SESSION_H_
#include "GlobalDefine.h"
#include "List.h"

//io操作
enum IOCP_REQUEST
{
	IOCP_REQUEST_NULL,
	IOCP_REQUEST_CONNECT,		//连接成功
	IOCP_REQUEST_ACCEPT,
	IOCP_REQUEST_SEND,
	IOCP_REQUEST_RECV,
	IOCP_REQUEST_MAX,
};


//发包
typedef struct tag_TIOBUFF
{
	SOCKET			sock;
	char			szData[DATA_BUFSIZE];
}TIOBUFF, *LPTIOBUFF;

//i/o请求
struct OVERLAPPEDEX : public OVERLAPPED 
{
	xe_uint8 bOperator;			//操作

	TIOBUFF  pIOBuf;			//io buf
};

//SESSION
class CSessionInfo
{
public:
	SOCKET						sock;

	OVERLAPPED					Overlapped;
	WSABUF						DataBuf;
	CHAR						Buffer[DATA_BUFSIZE];
	int							bufLen;

public:
	// ORZ:
	CSessionInfo()
	{
		bufLen	= 0;
	}

	int  Recv()
	{
		DWORD nRecvBytes = 0, nFlags = 0;

		DataBuf.len = DATA_BUFSIZE - bufLen;
		DataBuf.buf = Buffer + bufLen;

		memset( &Overlapped, 0, sizeof( Overlapped ) );

		return WSARecv( sock, &DataBuf, 1, &nRecvBytes, &nFlags, &Overlapped, 0 );
	}

	bool HasCompletionPacket()
	{
		return memchr( Buffer, '!', bufLen ) ? true : false;
	}

	// recv 滚欺俊辑 肯己等 窍唱狼 菩哦阑 惶酒辰促.
	char * ExtractPacket( char *pPacket )
	{
		int packetLen = (char *) memchr( Buffer, '!', bufLen ) - Buffer + 1;

		memcpy( pPacket, Buffer, packetLen );

		memmove( Buffer, Buffer + packetLen, DATA_BUFSIZE - packetLen );
		bufLen -= packetLen;

		return pPacket + packetLen;
	}
};


//玩家信息
class CUserInfo
{
public:
	SOCKET		sock;						//其他服务器保存连接进来的client socket消息
	char		szAddress[20];				// User's ip address 
};



//网关消息，一般用于保存某个网关连接的消息
class CGateInfo
{
public:
	SOCKET			sock;							//gate socket
	SOCKADDR_IN		addr;							//addr 
	

	list<CUserInfo*>	Userlist;				//玩家链
	
	list<TIOBUFF*>		sendBufList;			// buf链

	list<TIOBUFF*>		recvBuf;				//buf链
public:
	CGateInfo()
	{
		sock = INVALID_SOCKET;
	}

	virtual ~CGateInfo()
	{
		Close();
	}

	//关闭某个用户
	void CloseUser(char *szBuf)
	{
		xe_uint32 nSock = AnsiStrToVal(szBuf);
	}

	//关闭所有
	inline void	Close()
	{
		Userlist.clear();
		sendBufList.clear();
	}
};
#endif