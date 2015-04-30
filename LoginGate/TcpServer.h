ifndef _TCPSERVER_H_
#define _TCPSERVER_H_

#include "GlobalDefine.h"


class cIocpServer
{
public:
	cIocpServer();
	virtual ~cIocpServer();


	// ����������
	bool Start();

	//	ֹͣ������
	void Stop();

	// ����Socket��
	bool LoadSocketLib();

	// ж��Socket�⣬��������
	void UnloadSocketLib() { WSACleanup(); }

	// ��ñ�����IP��ַ
	string GetLocalIP();

	// ���ü����˿�
	void SetPort( const int& nPort ) { m_nPort=nPort; }


	//���ڷ��� cid:����socketcid   
	void SendPacket(DWORD cid,BYTE bCmdGroup,BYTE bCmd,char * buffer, int length);	

	//��Ҫ�����������������󣬴���ʱ�����߳��������淢��һ������Ϣ����رո�socket,���Է�ֹ���ⷢ�Ͳ���
	virtual bool BatchComplete(DWORD cid,char *pPacket);
protected:

	// ��ʼ��IOCP
	bool _InitializeIOCP();

	// ��ʼ��Socket
	bool _InitializeListenSocket();

	//��ʼ����̨�����߳�
	bool _InitializeBackendThread();

	// ����ͷ���Դ
	void _DeInitialize();

	// Ͷ��Accept����
	bool _PostAccept(); 

	// ���пͻ��������ʱ�򣬽��д���
	bool _DoAccpet(PerIoContext* pIoContext,DWORD dwBytes);


	// Ͷ�ݽ�����������
	bool _PostRecv(PerIoContext *pIoContext);
	
	// ���н��յ����ݵ����ʱ�򣬽��д���
	bool _DoRecv( PerSocketSession* pSocketContext, PerIoContext* pIoContext ,DWORD dwBytes);

	//���͸���̨
	void _RecvPost(PerSocketSession* pSocketContext,PerIoContext *pIoContext,DWORD dwBytes);

	// Ͷ�ݽӷ�������
	bool _PostSend(PerIoContext *pIoContext);

	//����������
	bool _DoSend(PerSocketSession * pSocketContext, PerIoContext * pIoContext);

	////������Ϣ����̨
	void _SendPost(PerIoContext *pIoContext);

	//�������󣬷��͹����̹߳رոø�socket
	void _RequestCloseSocket(PerSocketSession *pSession);

	bool _DoClose(PerSocketSession *pSocketContext,PerIoContext *pIoContext);


	//���������������������Ϣ
	void _IoSend();

	//��Ϣ����
	void _IoRecv();

	// ������󶨵���ɶ˿���
	bool _AssociateWithIOCP( PerSocketSession *pContext);

	// ����socket�������
	void _SetSocketProp(SOCKET socket);

	// ������ɶ˿��ϵĴ���
	bool HandleError( PerSocketSession *pContext,const DWORD& dwErr );

	// �̺߳�����ΪIOCP�������Ĺ������߳�
	static DWORD WINAPI _WorkerThread(LPVOID lpParam);

	// �̺߳�����Ϊsession ���������߳�
	static DWORD WINAPI _BackEndThread(LPVOID lpParam);

	// ��ñ����Ĵ���������
	int _GetNoOfProcessors();

	// �жϿͻ���Socket�Ƿ��Ѿ��Ͽ�
	bool _IsSocketAlive(SOCKET s);

	// ������������ʾ��Ϣ
	void _ShowMessage( char*szFormat,...);


	//�ر�socket
	void _CloseSocket(SOCKET s);

private:

	HANDLE                       m_hShutdownEvent;              // ����֪ͨ�߳�ϵͳ�˳����¼���Ϊ���ܹ����õ��˳��߳�

	HANDLE						 m_hBackendThread;				//��̨�����߳�

	HANDLE                       m_hIOCompletionPort;           // ��ɶ˿ڵľ��

	HANDLE*                      m_phWorkerThreads;             // �������̵߳ľ��ָ��

	int		                     m_nThreads;                    // ���ɵ��߳�����

	string                       m_strIP;                       // �������˵�IP��ַ
	int                          m_nPort;                       // �������˵ļ����˿�


	PerSocketSession*            m_pListenContext;              // ���ڼ�����Socket��Context��Ϣ

	LPFN_ACCEPTEX                m_lpfnAcceptEx;                // AcceptEx �� GetAcceptExSockaddrs �ĺ���ָ�룬���ڵ�����������չ����
	LPFN_GETACCEPTEXSOCKADDRS    m_lpfnGetAcceptExSockAddrs; 



	CLock						 m_csSendContext;
	//CRITICAL_SECTION			 m_csSendContext;
	list<PerIoContext*>			 m_IoSendFontList;
	list<PerIoContext*>			 m_IoSendBackList;	

	CLock						m_csRecvContext;			
	//CRITICAL_SECTION			 m_csRecvContext;
	list<PerIoContext*>			m_IoRecvFontList;
	list<PerIoContext*>			m_IoRecvBackList;	
};
#endif