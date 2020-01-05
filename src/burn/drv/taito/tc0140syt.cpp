#include "burnint.h"
#include "z80_intf.h"
#include "taito_ic.h"

#define TC0140SYT_PORT01_FULL         (0x01)
#define TC0140SYT_PORT23_FULL         (0x02)
#define TC0140SYT_PORT01_FULL_MASTER  (0x04)
#define TC0140SYT_PORT23_FULL_MASTER  (0x08)

typedef struct TC0140SYT
{
	UINT8 SlaveData[4];
	UINT8 MasterData[4];
	UINT8 MainMode;
	UINT8 SubMode;
	UINT8 Status;
	UINT8 NmiEnabled;
	UINT8 NmiReq;
} TC0140SYT;

static INT32 TC0140SYT_Z80_SELECT = 0;

static struct TC0140SYT tc0140syt;

static void InterruptController(void)
{
	tc0140syt.NmiReq = tc0140syt.Status & (TC0140SYT_PORT23_FULL | TC0140SYT_PORT01_FULL);

	ZetSetIRQLine(0x20, (tc0140syt.NmiReq && tc0140syt.NmiEnabled) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

void TC0140SYTPortWrite(UINT8 Data)
{
	Data &= 0x0f;
	
	tc0140syt.MainMode = Data;
}

UINT8 TC0140SYTCommRead()
{
	switch (tc0140syt.MainMode) {
		case 0x00: {
			return tc0140syt.MasterData[tc0140syt.MainMode++];
		}
		
		case 0x01: {
			tc0140syt.Status &= ~TC0140SYT_PORT01_FULL_MASTER;
			return tc0140syt.MasterData[tc0140syt.MainMode++];
		}
		
		case 0x02: {
			return tc0140syt.MasterData[tc0140syt.MainMode++];
		}
		
		case 0x03: {
			tc0140syt.Status &= ~TC0140SYT_PORT23_FULL_MASTER;
			return tc0140syt.MasterData[tc0140syt.MainMode++];
		}
		
		case 0x04: {
			return tc0140syt.Status;
		}
	}
	
	return 0;
}

void TC0140SYTCommWrite(UINT8 Data)
{
	Data &= 0x0f;
	
	switch (tc0140syt.MainMode) {
		case 0x00: {
			tc0140syt.SlaveData[tc0140syt.MainMode++] = Data;
			return;
		}
		
		case 0x01: {
			tc0140syt.SlaveData[tc0140syt.MainMode++] = Data;
			tc0140syt.Status |= TC0140SYT_PORT01_FULL;
			ZetOpen(TC0140SYT_Z80_SELECT);
			InterruptController();
			ZetClose();
			return;
		}
		
		case 0x02: {
			tc0140syt.SlaveData[tc0140syt.MainMode++] = Data;
			return;
		}
		
		case 0x03: {
			tc0140syt.SlaveData[tc0140syt.MainMode++] = Data;
			tc0140syt.Status |= TC0140SYT_PORT23_FULL;
			ZetOpen(TC0140SYT_Z80_SELECT);
			InterruptController();
			ZetClose();
			return;
		}
		
		case 0x04: {
			if (Data) {
				ZetOpen(TC0140SYT_Z80_SELECT);
				ZetReset();
				ZetClose();
			}
			return;
		}
	}
}

void TC0140SYTSlavePortWrite(UINT8 Data)
{
	Data &= 0x0f;
	tc0140syt.SubMode = Data;
}

UINT8 TC0140SYTSlaveCommRead()
{
	UINT8 nRet = 0;
	
	switch (tc0140syt.SubMode) {
		case 0x00: {
			nRet = tc0140syt.SlaveData[tc0140syt.SubMode++];
			break;
		}
		
		case 0x01: {
			tc0140syt.Status &= ~TC0140SYT_PORT01_FULL;
			nRet = tc0140syt.SlaveData[tc0140syt.SubMode++];
			InterruptController();
			break;
		}
		
		case 0x02: {
			nRet = tc0140syt.SlaveData[tc0140syt.SubMode++];
			break;
		}
		
		case 0x03: {
			tc0140syt.Status &= ~TC0140SYT_PORT23_FULL;
			nRet = tc0140syt.SlaveData[tc0140syt.SubMode++];
			InterruptController();
			break;
		}
				
		case 0x04: {
			nRet = tc0140syt.Status;
			break;
		}
	}
	
	return nRet;
}

void TC0140SYTSlaveCommWrite(UINT8 Data)
{
	Data &= 0x0f;
	
	switch (tc0140syt.SubMode) {
		case 0x00: {
			tc0140syt.MasterData[tc0140syt.SubMode++]= Data;
			break;
		}
		
		case 0x01: {
			tc0140syt.MasterData[tc0140syt.SubMode++]= Data;
			tc0140syt.Status |= TC0140SYT_PORT01_FULL_MASTER;
			break;
		}
		
		case 0x02: {
			tc0140syt.MasterData[tc0140syt.SubMode++]= Data;
			break;
		}
		
		case 0x03: {
			tc0140syt.MasterData[tc0140syt.SubMode++]= Data;
			tc0140syt.Status |= TC0140SYT_PORT23_FULL_MASTER;
			break;
		}
				
		case 0x05: {
			tc0140syt.NmiEnabled = 0;
			InterruptController();
			break;
		}
		
		case 0x06: {
			tc0140syt.NmiEnabled = 1;
			InterruptController();
			break;
		}
	}
}

void TC0140SYTReset()
{	
	for (INT32 i = 0; i < 4; i++) {
		tc0140syt.SlaveData[i] = 0;
		tc0140syt.MasterData[i] = 0;
	}

	tc0140syt.MainMode = 0;
	tc0140syt.SubMode = 0;
	tc0140syt.Status = 0;
	tc0140syt.NmiEnabled = 0;
	tc0140syt.NmiReq = 0;
}

void TC0140SYTInit(INT32 nCpu)
{
	TaitoIC_TC0140SYTInUse = 1;
	TC0140SYT_Z80_SELECT = nCpu;
}

void TC0140SYTExit()
{
	INT32 i;
	
	for (i = 0; i < 4; i++) {
		tc0140syt.SlaveData[i] = 0;
		tc0140syt.MasterData[i] = 0;
	}

	tc0140syt.MainMode = 0;
	tc0140syt.SubMode = 0;
	tc0140syt.Status = 0;
	tc0140syt.NmiEnabled = 0;
	tc0140syt.NmiReq = 0;
	TC0140SYT_Z80_SELECT = 0;
}

void TC0140SYTScan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(tc0140syt);
	}
}

#undef TC0140SYT_PORT01_FULL
#undef TC0140SYT_PORT23_FULL
#undef TC0140SYT_PORT01_FULL_MASTER
#undef TC0140SYT_PORT23_FULL_MASTER
