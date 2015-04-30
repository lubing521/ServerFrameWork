ifndef _TCPSERVER_H_
#define _TCPSERVER_H_

#include "GlobalDefine.h"


class cIocpServer
{
public:
	cIocpServer();
	virtual ~cIocpServer();


	// 启动服务器
	bool Start();

	//	停止服务器
	void Stop();

	// 加载Socket库
	bool LoadSocketLib();

	// 卸载Socket库，彻底完事
	void UnloadSocketLib() { WSACleanup(); }

	// 获得本机的IP地址
	string GetLocalIP();

	// 设置监听端口
	void SetPort( const int& nPort ) { m_nPort=nPort; }


	//用于发包 cid:连接socketcid   
	void SendPacket(DWORD cid,BYTE bCmdGroup,BYTE bCmd,char * buffer, int length);	

	//重要函数，碰到操作错误，错误时向工作线程里面里面发生一条空消息请求关闭该socket,可以防止恶意发送操作
	virtual bool BatchComplete(DWORD cid,char *pPacket);
protected:

	// 初始化IOCP
	bool _InitializeIOCP();

	// 初始化Socket
	bool _InitializeListenSocket();

	//初始化后台处理线程
	bool _InitializeBackendThread();

	// 最后释放资源
	void _DeInitialize();

	// 投递Accept请求
	bool _PostAccept(); 

	// 在有客户端连入的时候，进行处理
	bool _DoAccpet(PerIoContext* pIoContext,DWORD dwBytes);


	// 投递接收数据请求
	bool _PostRecv(PerIoContext *pIoContext);
	
	// 在有接收的数据到达的时候，进行处理
	bool _DoRecv( PerSocketSession* pSocketContext, PerIoContext* pIoContext ,DWORD dwBytes);

	//发送给后台
	void _RecvPost(PerSocketSession* pSocketContext,PerIoContext *pIoContext,DWORD dwBytes);

	// 投递接发送数据
	bool _PostSend(PerIoContext *pIoContext);

	//处理发送数据
	bool _DoSend(PerSocketSession * pSocketContext, PerIoContext * pIoContext);

	////发送消息至后台
	void _SendPost(PerIoContext *pIoContext);

	//恶意请求，发送工作线程关闭该该socket
	void _RequestCloseSocket(PerSocketSession *pSession);

	bool _DoClose(PerSocketSession *pSocketContext,PerIoContext *pIoContext);


	//这个才是真正发生数据消息
	void _IoSend();

	//消息处理
	void _IoRecv();

	// 将句柄绑定到完成端口中
	bool _AssociateWithIOCP( PerSocketSession *pContext);

	// 设置socket相关属性
	void _SetSocketProp(SOCKET socket);

	// 处理完成端口上的错误
	bool HandleError( PerSocketSession *pContext,const DWORD& dwErr );

	// 线程函数，为IOCP请求服务的工作者线程
	static DWORD WINAPI _WorkerThread(LPVOID lpParam);

	// 线程函数，为session 发送数据线程
	static DWORD WINAPI _BackEndThread(LPVOID lpParam);

	// 获得本机的处理器数量
	int _GetNoOfProcessors();

	// 判断客户端Socket是否已经断开
	bool _IsSocketAlive(SOCKET s);

	// 在主界面中显示信息
	void _ShowMessage( char*szFormat,...);


	//关闭socket
	void _CloseSocket(SOCKET s);

private:

	HANDLE                       m_hShutdownEvent;              // 用来通知线程系统退出的事件，为了能够更好的退出线程

	HANDLE						 m_hBackendThread;				//后台处理线程

	HANDLE                       m_hIOCompletionPort;           // 完成端口的句柄

	HANDLE*                      m_phWorkerThreads;             // 工作者线程的句柄指针

	int		                     m_nThreads;                    // 生成的线程数量

	string                       m_strIP;                       // 服务器端的IP地址
	int                          m_nPort;                       // 服务器端的监听端口


	PerSocketSession*            m_pListenContext;              // 用于监听的Socket的Context信息

	LPFN_ACCEPTEX                m_lpfnAcceptEx;                // AcceptEx 和 GetAcceptExSockaddrs 的函数指针，用于调用这两个扩展函数
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