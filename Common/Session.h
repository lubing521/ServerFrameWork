//////////////////////////////////////////////////////////////////////////
///Session ��Ϣ�� User��Ϣ��Gate��Ϣ,ģ�´�������������socket
//////////////////////////////////////////////////////////////////////////
#ifndef _SESSION_H_
#define _SESSION_H_
#include "GlobalDefine.h"
#include "List.h"

//io����
enum IOCP_REQUEST
{
	IOCP_REQUEST_NULL,
	IOCP_REQUEST_CONNECT,		//���ӳɹ�
	IOCP_REQUEST_ACCEPT,
	IOCP_REQUEST_SEND,
	IOCP_REQUEST_RECV,
	IOCP_REQUEST_MAX,
};


//����
typedef struct tag_TIOBUFF
{
	SOCKET			sock;
	char			szData[DATA_BUFSIZE];
}TIOBUFF, *LPTIOBUFF;

//i/o����
struct OVERLAPPEDEX : public OVERLAPPED 
{
	xe_uint8 bOperator;			//����

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

	// recv ���ۿ��� �ϼ��� �ϳ��� ��Ŷ�� �̾Ƴ���.
	char * ExtractPacket( char *pPacket )
	{
		int packetLen = (char *) memchr( Buffer, '!', bufLen ) - Buffer + 1;

		memcpy( pPacket, Buffer, packetLen );

		memmove( Buffer, Buffer + packetLen, DATA_BUFSIZE - packetLen );
		bufLen -= packetLen;

		return pPacket + packetLen;
	}
};


//�����Ϣ
class CUserInfo
{
public:
	SOCKET		sock;						//�����������������ӽ�����client socket��Ϣ
	char		szAddress[20];				// User's ip address 
};



//������Ϣ��һ�����ڱ���ĳ���������ӵ���Ϣ
class CGateInfo
{
public:
	SOCKET			sock;							//gate socket
	SOCKADDR_IN		addr;							//addr 
	

	list<CUserInfo*>	Userlist;				//�����
	
	list<TIOBUFF*>		sendBufList;			// buf��

	list<TIOBUFF*>		recvBuf;				//buf��
public:
	CGateInfo()
	{
		sock = INVALID_SOCKET;
	}

	virtual ~CGateInfo()
	{
		Close();
	}

	//�ر�ĳ���û�
	void CloseUser(char *szBuf)
	{
		xe_uint32 nSock = AnsiStrToVal(szBuf);
	}

	//�ر�����
	inline void	Close()
	{
		Userlist.clear();
		sendBufList.clear();
	}
};
#endif