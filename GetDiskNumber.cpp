// GetDiskNumber.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#define  SENDIDLENGTH  sizeof (SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE
#define  DFP_GET_VERSION          0x00074080
#define  DFP_RECEIVE_DRIVE_DATA   0x0007c088
//  Valid values for the bCommandReg member of IDEREGS.
#define  IDE_ATAPI_IDENTIFY  0xA1  //  Returns ID sector for ATAPI.
#define  IDE_ATA_IDENTIFY    0xEC  //  Returns ID sector for ATA.
#define  IDENTIFY_BUFFER_SIZE  512

// 테스트1
// 테스트2
// 테스트3
// 테스트4
// 테스트5

// Define global buffers.
BYTE IdOutCmd [sizeof (SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE - 1];
// DoIDENTIFY
// FUNCTION: Send an IDENTIFY command to the drive
// bDriveNum = 0-3
// bIDCmd = IDE_ATA_IDENTIFY or IDE_ATAPI_IDENTIFY
BOOL DoIDENTIFY (HANDLE hPhysicalDriveIOCTL, PSENDCMDINPARAMS pSCIP,
	PSENDCMDOUTPARAMS pSCOP, BYTE bIDCmd, BYTE bDriveNum,
	PDWORD lpcbBytesReturned)
{
	// Set up data structures for IDENTIFY command.
	pSCIP -> cBufferSize = IDENTIFY_BUFFER_SIZE;
	pSCIP -> irDriveRegs.bFeaturesReg = 0;
	pSCIP -> irDriveRegs.bSectorCountReg = 1;
	pSCIP -> irDriveRegs.bSectorNumberReg = 1;
	pSCIP -> irDriveRegs.bCylLowReg = 0;
	pSCIP -> irDriveRegs.bCylHighReg = 0;
	// Compute the drive number.
	pSCIP -> irDriveRegs.bDriveHeadReg = 0xA0 | ((bDriveNum & 1) << 4);
	// The command can either be IDE identify or ATAPI identify.
	pSCIP -> irDriveRegs.bCommandReg = bIDCmd;
	pSCIP -> bDriveNumber = bDriveNum;
	pSCIP -> cBufferSize = IDENTIFY_BUFFER_SIZE;
	return ( DeviceIoControl (hPhysicalDriveIOCTL, DFP_RECEIVE_DRIVE_DATA,
		(LPVOID) pSCIP,
		sizeof(SENDCMDINPARAMS) - 1,
		(LPVOID) pSCOP,
		sizeof(SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE - 1,
		lpcbBytesReturned, NULL) );
}
char *ConvertToString (DWORD diskdata [256], int firstIndex, int lastIndex)
{
	static char string [1024];
	int index = 0;
	int position = 0;

	//  each integer has two characters stored in it backwards
	for (index = firstIndex; index <= lastIndex; index++)
	{
		//  get high byte for 1st character
		string [position] = (char) (diskdata [index] / 256);
		position++;

		//  get low byte for 2nd character
		string [position] = (char) (diskdata [index] % 256);
		position++;
	}

	//  end the string 
	string [position] = '\0';

	//  cut off the trailing blanks
	for (index = position - 1; index > 0 && ' ' == string [index]; index--)
		string [index] = '\0';

	return string;
}
typedef struct _GETVERSIONOUTPARAMS
{
	BYTE bVersion;      // Binary driver version.
	BYTE bRevision;     // Binary driver revision.
	BYTE bReserved;     // Not used.
	BYTE bIDEDeviceMap; // Bit map of IDE devices.
	DWORD fCapabilities; // Bit mask of driver capabilities.
	DWORD dwReserved[4]; // For future use.
} GETVERSIONOUTPARAMS, *PGETVERSIONOUTPARAMS, *LPGETVERSIONOUTPARAMS;

char* GetDiskSerial(int drive) 
{ 
	char serial[1024]={0};

	HANDLE hPhysicalDriveIOCTL = 0;

	//  Try to get a handle to PhysicalDrive IOCTL, report failure
	//  and exit if can't.
	char driveName [256];

	sprintf (driveName, "\\\\.\\PhysicalDrive%d", drive);

	//  Windows NT, Windows 2000, must have admin rights
	hPhysicalDriveIOCTL = CreateFileA (driveName,
		GENERIC_READ | GENERIC_WRITE, 
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, 0, NULL);

	if (hPhysicalDriveIOCTL != INVALID_HANDLE_VALUE)
	{
		GETVERSIONOUTPARAMS VersionParams;
		DWORD               cbBytesReturned = 0;

		// Get the version, etc of PhysicalDrive IOCTL
		memset ((void*) &VersionParams, 0, sizeof(VersionParams));

		if ( ! DeviceIoControl (hPhysicalDriveIOCTL, DFP_GET_VERSION,
			NULL, 
			0,
			&VersionParams,
			sizeof(VersionParams),
			&cbBytesReturned, NULL) )
		{         
			// printf ("DFP_GET_VERSION failed for drive %d\n", i);
			// continue;
		}

		// If there is a IDE device at number "i" issue commands
		// to the device
		if (VersionParams.bIDEDeviceMap > 0)
		{
			BYTE             bIDCmd = 0;   // IDE or ATAPI IDENTIFY cmd
			SENDCMDINPARAMS  scip;
			//SENDCMDOUTPARAMS OutCmd;

			// Now, get the ID sector for all IDE devices in the system.
			// If the device is ATAPI use the IDE_ATAPI_IDENTIFY command,
			// otherwise use the IDE_ATA_IDENTIFY command
			bIDCmd = (VersionParams.bIDEDeviceMap >> drive & 0x10) ? \
IDE_ATAPI_IDENTIFY : IDE_ATA_IDENTIFY;

			memset (&scip, 0, sizeof(scip));
			memset (IdOutCmd, 0, sizeof(IdOutCmd));

			if ( DoIDENTIFY (hPhysicalDriveIOCTL, 
				&scip, 
				(PSENDCMDOUTPARAMS)&IdOutCmd, 
				(BYTE) bIDCmd,
				(BYTE) drive,
				&cbBytesReturned))
			{
				DWORD diskdata [256];
				int ijk = 0;
				USHORT *pIdSector = (USHORT *)
					((PSENDCMDOUTPARAMS) IdOutCmd) -> bBuffer;

				for (ijk = 0; ijk < 256; ijk++)
					diskdata [ijk] = pIdSector [ijk];
				strcpy(serial, ConvertToString(diskdata,10,19));    
			}
		}

		CloseHandle (hPhysicalDriveIOCTL);
	}

	return serial; 
}

int _tmain(int argc, _TCHAR* argv[])
{
	//0:첫번째 마스터 1:첫번째 슬레이브 2:두번째 마스터 3:두번째 슬레이브 
	printf("%s\n", GetDiskSerial(0));
	system("pause");

	return 0;
}

