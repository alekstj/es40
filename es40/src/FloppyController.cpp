/** ES40 emulator.
  * Copyright (C) 2007 by Camiel Vanderhoeven
  *
  * Website: www.camicom.com
  * E-mail : camiel@camicom.com
  * 
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the GNU General Public License
  * as published by the Free Software Foundation; either version 2
  * of the License, or (at your option) any later version.
  * 
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  * 
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
  * 
  * Although this is not required, the author would appreciate being notified of, 
  * and receiving any modifications you may make to the source code that might serve
  * the general public.
  * 
  * FLOPPYCONTROLLER.CPP contains the code for the emulated Floppy Controller devices.
  *
  **/

#include "StdAfx.h"
#include "FloppyController.h"
#include "System.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFloppyController::CFloppyController(CSystem * c, int id) : CSystemComponent(c)
{
	c->RegisterMemory(this, 0, X64(00000801fc0003f0) - (0x80 * id), 8);
	iMode = 0;
	iID = id;
	iActiveRegister = 0;

	iRegisters[0x00] = 0x28;
	iRegisters[0x01] = 0x9c;
	iRegisters[0x02] = 0x88;
	iRegisters[0x03] = 0x78;
	iRegisters[0x04] = 0x00;
	iRegisters[0x05] = 0x00;
	iRegisters[0x06] = 0xff;
	iRegisters[0x07] = 0x00;
	iRegisters[0x08] = 0x00;
	iRegisters[0x09] = 0x00;
	iRegisters[0x0a] = 0x00;
	iRegisters[0x0b] = 0x00;
	iRegisters[0x0c] = 0x00;
	iRegisters[0x0d] = 0x03;
	iRegisters[0x0e] = 0x02;
	iRegisters[0x0f] = 0x00;
	iRegisters[0x10] = 0x00;
	iRegisters[0x11] = 0x00;
	iRegisters[0x12] = 0x00;
	iRegisters[0x13] = 0x00;
	iRegisters[0x14] = 0x00;
	iRegisters[0x15] = 0x00;
	iRegisters[0x16] = 0x00;
	iRegisters[0x17] = 0x00;
	iRegisters[0x18] = 0x00;
	iRegisters[0x19] = 0x00;
	iRegisters[0x1a] = 0x00;
	iRegisters[0x1b] = 0x00;
	iRegisters[0x1c] = 0x00;
	iRegisters[0x1d] = 0x00;
	iRegisters[0x1e] = 0x3c;
	iRegisters[0x1f] = 0x00;
	iRegisters[0x20] = 0x3c;
	iRegisters[0x21] = 0x3c;
	iRegisters[0x22] = 0x3d;
	iRegisters[0x23] = 0x00;
	iRegisters[0x24] = 0x00;
	iRegisters[0x25] = 0x00;
	iRegisters[0x26] = 0x00;
	iRegisters[0x27] = 0x00;
	iRegisters[0x28] = 0x00;
	iRegisters[0x29] = 0x00;

    printf("%%FDC-I-INIT: Floppy Drive Controller emulator initialized.\n");

}

CFloppyController::~CFloppyController()
{

}

void CFloppyController::WriteMem(int index, u64 address, int dsize, u64 data)
{
    dsize;
    index;

	switch (address)
	{
	case 0:
		switch (data&0xff)
		{
		case 0x55:
//			if (iMode==1)
//				printf("Entering configuration mode for floppy controller %d\n", iID);
			if (iMode==0 || iMode==1) 
				iMode++;
			break;
		case 0xaa:
//			if (iMode==2)
//				printf("Leaving configuration mode for floppy controller %d\n", iID);
			iMode=0;
			break;
		default:
			if (iMode==2)
				iActiveRegister = (int)(data&0xff);
//			else
//				printf("Unknown command: %02x on floppy port %d\n",(u32)(data&0xff),(u32)address);
		}
		break;
	case 1:
		if (iMode==2)
		{
			if (iActiveRegister < 0x2a)
				iRegisters[iActiveRegister] = (u8)(data&0xff);
//			else
//				printf("Unknown floppy register: %02x\n",iActiveRegister);
		}
//		else
//			printf("Unknown command: %02x on floppy port %d\n",(u32)(data&0xff),(u32)address);
		break;
//	default:
//		printf("Unknown command: %02x on floppy port %d\n",(u32)(data&0xff),(u32)address);
	}
}

u64 CFloppyController::ReadMem(int index, u64 address, int dsize)
{
    dsize;
    index;

	switch (address)
	{
	case 1:
		if (iMode==2)
		{
			if (iActiveRegister < 0x2a)
				return (u64)(iRegisters[iActiveRegister]);
//			else
//				printf("Unknown floppy register: %02x\n",iActiveRegister);
		}
//		else
//			printf("Unknown read access on floppy port %d\n",(u32)address);
		break;
//	default:
//		printf("Unknown read access on floppy port %d\n",(u32)address);
	}

	return 0;
}