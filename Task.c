
#include "Type.h"
#include "Mcu.h"
#include "Protocol.h"
#include "RecvBuffer.h"
#include "Task.h"

#include "System.h"
#include "Uart.h"
#include "Timer.h"
#include "Usb.h"

#include "Gpio.h"

#include "Packet.h"

#define OUT_BUFFER_SIZE  8
static UINT8X s_sendBuffer[OUT_BUFFER_SIZE];

static BOOL s_isSwitchedPort = FALSE;

//keyboard break code
static UINT8C s_keyboardBreakCode[KEYBOARD_LEN] = 
{
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

//mouse break code
static UINT8C s_mouseBreakCode[MOUSE_LEN] = 
{
	0x00, 0x00, 0x00, 0x00,
};

static void SendKeyboardToUsb(UINT8 *pData, UINT8 len)
{
	Enp1IntIn(pData, len);
}

static void SendMouseToUsb(UINT8 *pData, UINT8 len)
{
	Enp2IntIn(pData, len);
}

static void InitTimer2(UINT8 ms)
{
	UINT16 val = FREQ_SYS / 1000ul * ms;
	
    mTimer2ClkFsys();	           //T2��ʱ��ʱ������
    mTimer_x_ModInit(2, 0);         //T2 ��ʱ��ģʽ����
    mTimer_x_SetData(2, val);	   //T2��ʱ����ֵ
    mTimer2RunCTL(1);              //T2��ʱ������			

    ET2 = 1;
}

void InitSystem(void)
{
    CfgFsys();           //CH551ʱ��ѡ������
    mDelaymS(5);         //�޸���Ƶ�ȴ��ڲ������ȶ�,�ؼ�	

	InitRecvBuffer();
	
    InitUART0();         //����0��ʼ��
	
	InitTimer2(5);       //5ms �ж�

#ifdef DEBUG
	Port1Cfg(6, 1);
	Port1Cfg(7, 1);
#endif

    USBDeviceInit();     //USB�豸ģʽ��ʼ��

#ifndef DEBUG
	CH554WDTModeSelect(1);
#endif

    HAL_ENABLE_INTERRUPTS();    //����Ƭ���ж�
}

void ProcessUartData(void)
{
	if (CheckEnumerationStatus())
	{
		P1_6 = 1;
	}
	else
	{
		P1_6 = 0;
	}
	
	if (CheckRecvBuffer())
	{
		UINT8 *packet = GetOutputBuffer();
		
		UINT8 id = packet[0];
		UINT8 *pData = &packet[1];

		switch (id)
		{
		case ID_KEYBOARD:
			if (s_isSwitchedPort)
			{
				SendKeyboardToUsb(pData, KEYBOARD_LEN);
			}
			
			break;

		case ID_MOUSE:
			if (s_isSwitchedPort)
			{
				SendMouseToUsb(pData, MOUSE_LEN);
			}
			
			break;

		case ID_QUERY_ONLINE:
			{
				UINT8 online;
				UINT8 len;
#ifdef DEBUG
				P1_6 = !P1_6;
#endif
				if (CheckEnumerationStatus())
				{
					online = STATUS_ONLINE;
				}
				else
				{
					online = STATUS_OFFLINE;
				}

				if (BuildOnlineStatusPacket(s_sendBuffer, sizeof(s_sendBuffer), &len, online))
				{
					CH554UART0SendData(s_sendBuffer, len);
				}	
			}

			break;

		case ID_SWITCH:
			if (pData[0] == SWITCH_IN)
			{
				s_isSwitchedPort = TRUE;
			}
			else
			{
				s_isSwitchedPort = FALSE;

				//send break code
				SendKeyboardToUsb(s_keyboardBreakCode, KEYBOARD_LEN);

				SendMouseToUsb(s_mouseBreakCode, MOUSE_LEN);
			}
			
			break;

		default:
			break;
		}
	}
}

void ProcessKeyboardLed()
{
	static UINT8 ledSave = 0x00;
	
	UINT8 led = GetKeyboardLedStatus();
	if (led != ledSave)
	{
		if (s_isSwitchedPort)
		{
			UINT8 len;
			if (BuildKeyboardLedPacket(s_sendBuffer, sizeof(s_sendBuffer), &len, led))
			{
				CH554UART0SendData(s_sendBuffer, len);
			}
		}

		ledSave = led;

#ifdef DEBUG 

		if (led & 0x02)
		{
			P1_7 = 1;
		}		
		else
		{
			P1_7 = 0;
		}
#endif
	}
}

#ifndef DEBUG
void FeedWdt(void)
{
	CH554WDTFeed(0);
}

#else
void FeedWdt(void)
{
}

#endif

