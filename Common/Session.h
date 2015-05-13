//////////////////////////////////////////////////////////////////////////
///Session ��Ϣ�� User��Ϣ��Gate��Ϣ,ģ�´�������������socket
//////////////////////////////////////////////////////////////////////////
#ifndef _SESSION_H_
#define _SESSION_H_
#include "GlobalDefine.h"
#include "List.h"
#include <list>

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
	SOCKET	 sock;
	xe_uint8 bOperator;			//����

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