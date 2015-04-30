#pragma once

#include "defstl/queue.h"
#include "lock.h"


class CWHQueue : public CQueue< BYTE >
{
public:
	virtual ~CWHQueue()
	{
		ClearAll();
	}

	bool PushQ( BYTE *lpbtQ )
	{
		CAutoLock cs(&m_cs);
		bool bRet = Enqueue( lpbtQ );

		return bRet;
	}

	BYTE * PopQ()
	{
		CAutoLock cs(&m_cs);
		BYTE *pData = Dequeue();

		return pData;
	}

	CLock m_cs;
};
