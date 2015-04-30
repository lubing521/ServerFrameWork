#ifndef _MMTIME_CONTROL_H_ 
#define _MMTIME_CONTROL_H_
/********************************************************************

	purpose:	1.CMTimeSpan ʱ����
					CMTimeSpan=CMTimeSpan+CMTimeSpan;
					SetTime(xx,xx,xx,xx);
					SetTick();
					AdjustTime();
				2.CMTime
				static:
					GetCurTime();
					GetCurTickCount();
				normal:
					SetTime(xx);
					GetTime();GetTickCount();GetSystime();
					Format("char format");
					>,<,==,<=,>=
					CMTime=CMTime-CMTimeSpan;
					CMTimeSpan=CMTime-CMTime;
					CMTime=CMTime+CMTimeSpan;
				3.CMTimeControl
					StartTime();
//  struct tm {
// 	int tm_sec;		/* [0,59] */
// 	int tm_min;		/* [0,59] */
// 	int tm_hour;	/* [0,23] */
// 	int tm_mday;	/* [1,31] */
// 	int tm_mon;		/* [0,11] */
// 	int tm_year;	/* ʵ����ݼ�ȥ1900 */
// 	int tm_wday;	/* [0,6],0���������죬1��������һ */
// 	int tm_yday;	/* [0,365],0����1��1�գ�1����1��2�� */
// 	int tm_isdst;	/* ����ʱ��ʶ����ʵ������ʱ��ʱ��tm_isdstΪ������ʵ������ʱ��ʱ��tm_isdstΪ0�����˽����ʱ��tm_isdst()Ϊ����*/
// };

//UTC(�����׼ʱ��):Э������ʱ���ֳ�Ϊ�����׼ʱ�䣬Ҳ���Ǵ������֪�ĸ������α�׼ʱ��(Greenwich Mean Time��GMT)��
//Calendar Time(����ʱ��):����ʱ�䣬���á���һ����׼ʱ��㵽��ʱ��ʱ�侭��������������ʾ��ʱ�䡣
//epoch(ʱ���):ʱ��㡣ʱ����ڱ�׼C/C++����һ�����������ô�ʱ��ʱ��ͱ�׼ʱ�����������(������ʱ��)����ʾ��
//clock tick(ʱ�Ӽ�ʱ��Ԫ):ʱ�Ӽ�ʱ��Ԫ��һ��ʱ�Ӽ�ʱ��Ԫ��ʱ�䳤������CPU���Ƶġ�һ��clock tick����CPU��һ��ʱ�����ڣ�����C/C++��һ��������ʱ��λ
/*********************************************************************/
#include <stdio.h>
#include <time.h>
#include <memory.h>
#include <Windows.h>
#include <sys/timeb.h>

#pragma comment(lib,"Winmm.lib")
enum TimeType//��,�ֶ�ʱ,Сʱ��ʱ
{
	timer_type_nul,  //adjust
	timer_type_sec,
	timer_type_min,
	timer_type_hour,
};

enum TimePartName
{
	time_sec,
	time_min,
	time_hour,
	time_day,
	time_month,	  //[0,11]
	time_year,	  //ʵ����ݼ�ȥ1900
	time_weekday, //[0,6],0����������,1��������һ
	time_yeardays,//[0,365],0����1��1��,1����1��2��	  
};
class CMTime;
class CMTimeControl;
class CMTimeSpan
{
	friend CMTime;
public:
	CMTimeSpan();
	CMTimeSpan(int sec,int min=0,int hour=0,int day=0);
	~CMTimeSpan();

	void				SetTime(int sec,int min=0,int hour=0,int day=0);
	void				SetTick(long lTick = 0);            //transition timespan in tick
	void				AdjustTime();                       //adjust time in normal

	DWORD				GetSenconds()	{ return m_lTick;}
	int					GetDays()		{ return m_nDay;}
	CMTimeSpan			operator+(CMTimeSpan& mTime);
	CMTimeSpan			operator-(CMTimeSpan& mTime);
private:
	int		m_nDay;
	int		m_nHour;
	int		m_nMin;
	int		m_nSec;

	long	m_lTick;
};

class CMTime
{
public:
	CMTime();
	CMTime(time_t nTime);
	CMTime(SYSTEMTIME tmSystem);
	CMTime(char* szTime);
	CMTime(tm tmTime);
	~CMTime();

	static tm		GetCurTime();
	static time_t	GetCurTickCount();		//��ȡ1970�꿪ʼ�ĵ�ǰ����

	tm				SetTime(time_t nTime);
	tm				SetTime(SYSTEMTIME tmSystem);
	tm				SetTime(char* szTime);
	tm				SetTime(tm tmTime);

	tm				GetTime();
	time_t			GetTickCount();
	SYSTEMTIME		GetSystime();

	char*			Format(char* szFormat = "%y-%m-%d %H:%M:%S");
	//----Get Part of Time----
	bool			NextWeek(int nDay=0,int nHour=0);//next week day nHour:00:00 [0,6],0����������,1��������һ
	bool			NextDay(int nDay=1,int nHour=0); //next nDay     00:00:00
	//************************************
	// Method:    operator[]
	// FullName:  CMTime::operator[]
	// Access:    public 
	// Returns:   int
	// Qualifier:
	// Parameter: BYTE bTimePartName
	// 				time_sec,
	// 				time_min,
	// 				time_hour,
	// 				time_day,
	// 				time_month,
	// 				time_year,
	// 				time_weekday, //[0,6],0���������죬1��������һ
	// 				time_yeardays,//[0,365],0����1��1�գ�1����1��2��
	//************************************
	int				operator[](BYTE bTimePartName);
	bool			operator>(CMTime& mTime);
	bool			operator<(CMTime& mTime);
	bool			operator>=(CMTime& mTime);
 	bool			operator<=(CMTime& mTime);
	bool			operator==(CMTime& mTime);
	CMTime			operator-(CMTimeSpan& mTimeSpan);
	CMTimeSpan		operator-(CMTime& mTime);
	CMTime			operator+(CMTimeSpan& mTimeSpan);
private:
	tm				m_tmTime;
	time_t			m_tmTimeTick;
	char			m_szTime[1024];
};

//��ʱ��
typedef  DWORD (*ONTIME)(CMTimeControl* pTime,void *pParam);
class CMTimeControl
{
public:
	CMTimeControl();
	~CMTimeControl();

	static double UseTime(bool isEnd = false); 
	//-------------------------part of timer----------------
	void	SetEvent(ONTIME  pfunTime,void *pParam);

	int		StartTime(BYTE bType,DWORD dwTimePeriod );
	void	StopTime();
	int		GetTimeCount();
	void	AdjustTime(BYTE bType,DWORD dwTimePeriod );		//����ʱ��

	inline	bool IsUse()		{ return m_dwTimerID != 0;}			//��ʱ���Ƿ��Ѿ���������
private:
	static void CALLBACK _TimeCallBack(UINT wTimerID, UINT msg, DWORD dwUser, DWORD dw1, DWORD dw2);	
private:
	ONTIME  m_pfunTime;
	//data for timer
	DWORD	m_dwTimerID;
	DWORD	m_dwTimeCount;	
	bool	m_bAdjustTimer;//false adjust  true timer
	CMTime	m_tmStartTime;
	BYTE	m_bType;//Timer Time Type
	BYTE	m_bCurType;//Current Time Type

	DWORD	m_dwTimePeriod;		//��ʱ��,Ĭ��ʱ��

	void*	m_pParam;		//
};
#endif