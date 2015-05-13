//////////////////////////////////////////////////////////////////////////
///Session 信息和 User信息和Gate信息,模仿传奇世界来管理socket
//////////////////////////////////////////////////////////////////////////
#ifndef _SESSION_H_
#define _SESSION_H_
#include "GlobalDefine.h"
#include "List.h"
#include <list>

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
	SOCKET	 sock;
	xe_uint8 bOperator;			//操作

	TIOBUFF  pIOBuf;			//io buf
};

//SESSION
class CSessionInfo
{
public:
	SOCKET						sock;

	std::list<TIOBUFF*>			m_recvbuf;
	std::list<TIOBUFF*>			m_sendbuf;

	volatile bool				m_bRemove;

	CLock						m_recvcs;
	CLock						m_sendcs;
public:
	// ORZ:
	CSessionInfo()
	{
		m_bRemove = false;
	}
	~CSessionInfo()
	{
		if (sock != INVALID_SOCKET)
			closesocket(sock);

		sock = INVALID_SOCKET;

		ClearBuf();
	}

	void Remove()
	{
		m_bRemove = true;
	}

	bool IsRemove()
	{
		return m_bRemove;
	}

	void Push_RecvBack(TIOBUFF *pBuf)
	{
		if (pBuf != NULL)
			return;

		CAutoLock cs(&m_recvcs);
		m_recvbuf.push_back(pBuf);
	}

	TIOBUFF *ExtractRecvbPacket()
	{
		CAutoLock cs(&m_recvcs);
		TIOBUFF *pBuf = m_recvbuf.front();
		m_recvbuf.pop_front();

		return pBuf;
	}

	void Push_SendBack(TIOBUFF *pBuf)
	{
		if (pBuf != NULL)
			return;

		CAutoLock cs(&m_sendcs);
		m_sendbuf.push_back(pBuf);
	}

	TIOBUFF *ExtractSendPacket()
	{
		CAutoLock cs(&m_sendcs);
		TIOBUFF *pBuf = m_sendbuf.front();
		m_sendbuf.pop_front();

		return pBuf;
	}
	void ClearBuf()
	{
		m_recvcs.Lock();
		TIOBUFF *pbUF = NULL;

		for (std::list<TIOBUFF*>::iterator iter1 = m_recvbuf.begin();iter1 != m_recvbuf.end();++iter1)
		{
			pbUF = *iter1;
			if (pbUF)
			{
				SAFE_DELETE(pbUF);
			}
		}

		m_recvbuf.clear();

		m_recvcs.UnLock();

		m_sendcs.Lock();
		pbUF = NULL;

		for (std::list<TIOBUFF*>::iterator iter1 = m_sendbuf.begin();iter1 != m_sendbuf.end();++iter1)
		{
			pbUF = *iter1;
			if (pbUF)
			{
				SAFE_DELETE(pbUF);
			}
		}

		m_sendbuf.clear();

		m_sendcs.UnLock();
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