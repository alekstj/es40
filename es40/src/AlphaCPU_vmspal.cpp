/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * WWW    : http://sourceforge.net/projects/es40
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
 */

/**
 * \file 
 * Contains routines that replace parts of the VMS PALcode for the emulated
 * DecChip 21264CB EV68 Alpha processor. Based on disassembly of original VMS
 * PALcode, HRM, and OpenVMS AXP Internals and Data Structures.
 *
 * $Id$
 *
 * X-1.9        Camiel Vanderhoeven                             30-JAN-2008
 *      Remember number of instructions left in current memory page, so
 *      that the translation-buffer doens't need to be consulted on every
 *      instruction fetch when the Icache is disabled.
 *
 * X-1.8        Camiel Vanderhoeven                             27-JAN-2008
 *      Comments.
 *
 * X-1.7        Camiel Vanderhoeven                             21-JAN-2008
 *      Fixed typo.
 *
 * X-1.6        Camiel Vanderhoeven                             18-JAN-2008
 *      Replaced sext_64 inlines with sext_u64_<bits> inlines for
 *      performance reasons (thanks to David Hittner for spotting this!);
 *
 * X-1.5        Camiel Vanderhoeven                             08-JAN-2008
 *      Removed last references to IDE disk read SRM replacement.
 *
 * X-1.4        Camiel Vanderhoeven                             28-DEC-2007
 *      Keep the compiler happy.
 *
 * X-1.3        Camiel Vanderhoeven                             12-DEC-2007
 *      Use disk base class for direct IDE access.
 *
 * X-1.2        Camiel Vanderhoeven                             10-DEC-2007
 *      Use configurator.
 *
 * X-1.1        Camiel Vanderhoeven                             2-DEC-2007
 *      Initial version in CVS.
 **/

#include "StdAfx.h"
#include "AlphaCPU.h"

#include "Serial.h"
#include "AliM1543C_ide.h"
#include "Disk.h"

/***********************************************************
 *                                                         *
 *                      VMS PALcode                        *
 *                                                         *
 ***********************************************************/

#define p4 state.r[32+4]
#define p5 state.r[32+5]
#define p6 state.r[32+6]
#define p7 state.r[32+7]

#define p20 state.r[32+20]
#define p21 state.r[32+21]
#define p22 state.r[32+22]
#define p23 state.r[32+23]

#define r0 state.r[0]
#define r1 state.r[1]
#define r2 state.r[2]
#define r3 state.r[3]
//#define r4 state.r[4]
//#define r5 state.r[5]
//#define r6 state.r[6]
//#define r7 state.r[7]
#define r8 state.r[8]
#define r9 state.r[9]
#define r10 state.r[10]
#define r11 state.r[11]
#define r12 state.r[12]
#define r13 state.r[13]
#define r14 state.r[14]
#define r15 state.r[15]
#define r16 state.r[16]
#define r17 state.r[17]
#define r18 state.r[18]
#define r19 state.r[19]
//#define r20 state.r[20]
//#define r21 state.r[21]
//#define r22 state.r[22]
//#define r23 state.r[23]
#define r24 state.r[24]
//#define r25 state.r[25]
//#define r26 state.r[26]
#define r27 state.r[27]
#define r28 state.r[28]
#define r29 state.r[29]
#define r30 state.r[30]
#define r31 state.r[31]

#define hw_stq(a,b) cSystem->WriteMem(a&~X64(7),64,b)
#define hw_stl(a,b) cSystem->WriteMem(a&~X64(3),32,b)
#define stq(a,b)                                              \
      if (virt2phys(a,&phys_address,ACCESS_WRITE,NULL,0)) \
        return -1;                                             \
      cSystem->WriteMem(phys_address, 64, b);
#define ldq(a,b)                                              \
      if (virt2phys(a,&phys_address,ACCESS_READ,NULL,0))  \
        return -1;                                             \
      b = cSystem->ReadMem(phys_address, 64);
#define stl(a,b)                                              \
      if (virt2phys(a,&phys_address,ACCESS_WRITE,NULL,0)) \
        return -1;                                             \
      cSystem->WriteMem(phys_address, 32, b);
#define ldl(a,b)                                              \
      if (virt2phys(a,&phys_address,ACCESS_READ,NULL,0))  \
        return -1;                                             \
      b = sext_u64_32(cSystem->ReadMem(phys_address, 32));
#define ldb(a,b)                                              \
      if (virt2phys(a,&phys_address,ACCESS_READ,NULL,0))  \
        return -1;                                             \
      b = (char)(cSystem->ReadMem(phys_address, 8));
#define hw_ldq(a,b) b = cSystem->ReadMem(a&~X64(7),64)
#define hw_ldl(a,b) b = sext_u64_32(cSystem->ReadMem(a&~X64(3),32));
#define hw_ldbu(a,b) b = cSystem->ReadMem(a,8)

/**
 * Mask for interrupt enabling at IPL's.
 *
 * For each of the 32 IPL's gives the values for eien, slen, cren, pcen, 
 * sien and asten.
 *
 * Source: table of IER masks in PALcode at offset 0d00H. 
 **/

static int ipl_ier_mask[32][6] = {
  /* ei, sl, cr, pc,     si, ast */
  {0x3f,  0,  1,  3, 0xfffe,   1},
  {0x3f,  0,  1,  3, 0xfffc,   1},
  {0x3f,  0,  1,  3, 0xfff8,   0},
  {0x3f,  0,  1,  3, 0xfff0,   0},
  {0x3f,  0,  1,  3, 0xffe0,   0},
  {0x3f,  0,  1,  3, 0xffc0,   0},
  {0x3f,  0,  1,  3, 0xff80,   0},
  {0x3f,  0,  1,  3, 0xff00,   0},
  {0x3f,  0,  1,  3, 0xfe00,   0},
  {0x3f,  0,  1,  3, 0xfc00,   0},
  {0x3f,  0,  1,  3, 0xf800,   0},
  {0x3f,  0,  1,  3, 0xf000,   0},
  {0x3f,  0,  1,  3, 0xe000,   0},
  {0x3f,  0,  1,  3, 0xc000,   0},
  {0x3f,  0,  1,  3, 0x8000,   0},
  {0x3f,  0,  1,  3,      0,   0},
  {0x3f,  0,  1,  3,      0,   0},
  {0x3f,  0,  1,  3,      0,   0},
  {0x3f,  0,  1,  3,      0,   0},
  {0x3f,  0,  1,  3,      0,   0},
  {0x3f,  0,  1,  3,      0,   0},
  {0x3d,  0,  1,  3,      0,   0},
  {0x31,  0,  1,  3,      0,   0},
  {0x31,  0,  1,  3,      0,   0},
  {0x31,  0,  1,  3,      0,   0},
  {0x31,  0,  1,  3,      0,   0},
  {0x31,  0,  1,  3,      0,   0},
  {0x31,  0,  1,  3,      0,   0},
  {0x31,  0,  1,  3,      0,   0},
  {0x31,  0,  1,  0,      0,   0},
  {0x31,  0,  1,  3,      0,   0},
  {0x10,  0,  1,  3,      0,   0}
};

/***************************************************************************//**
 * \name VMS_pal_call
 * VMS PALcode CALL replacement routines.
 ******************************************************************************/
//\{

/**
 * Implementation of CALL_PAL CFLUSH opcode.
 **/

void CAlphaCPU::vmspal_call_cflush()
{
  // don't do anything...
}

/**
 * Implementation of CALL_PAL DRAINA opcode.
 **/

void CAlphaCPU::vmspal_call_draina()
{
  // don't do anything...
}

/**
 * Implementation of CALL_PAL LDQP opcode.
 **/

void CAlphaCPU::vmspal_call_ldqp()
{
  hw_ldq(r16, r0);
}

/**
 * Implementation of CALL_PAL STQP opcode.
 **/

void CAlphaCPU::vmspal_call_stqp()
{
  hw_stq(r16, r17);
}

/**
 * Implementation of CALL_PAL SWPCTX opcode.
 **/

void CAlphaCPU::vmspal_call_swpctx()
{
  p23 = state.pc;

  if (r16 & 0x7f)
  {
    //context pointer not properly aligned...
    p4 = 0x430;
    hw_stq(p21+0x150,p23);
    hw_stq(p21+0x158,p4);
    vmspal_int_initiate_exception();
    return;
  }

  hw_ldq(r16+0x30,p4);
  hw_ldq(r16+0x38,p5);
  hw_ldq(r16+0x28,p6);

  p20 = state.aster | (state.astrr<<4);

  p6 &= 0xff;
  state.asn0 = (int)p6;
  state.asn1 = (int)p6;
  state.asn = (int)p6;
  state.aster = (int)p4 & 0xf;
  state.astrr = (int)(p4>>4) & 0xf;
  state.fpen = (int)p5 & 1;
  state.ppcen = (int)(p5>>0x3e) & 1;
  state.check_int = true;

  hw_ldq(r16+0x40,p7);
  hw_ldq(r16+0x20,p6);

  p4 = state.cc + state.cc_offset;
  state.cc_offset = ((u32)p7 & 0xffffffff) - state.cc;

  p6 <<= 0x0d;
  hw_stq(p21+8,p6);
  hw_ldq(p21+0x10,p5);
  hw_ldq(r16,p6);
  hw_stl(p5+0x40,p4);
  hw_stq(p5+0x30,p20);
  hw_stq(p5,r30);

  r30 = p6;
  hw_stq(p21+0x10,r16);
}

/**
 * Implementation of CALL_PAL MFPR_ASN opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_asn()
{
  r0 = state.asn;
}

/**
 * Implementation of CALL_PAL MTPR_ASTEN opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_asten()
{
  r0 = state.aster;                     
  state.aster &= r16;
  state.aster |= (r16>>4) & 0xf;       
  state.check_int = true;
}

/**
 * Implementation of CALL_PAL MTPR_ASTSR opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_astsr()
{
  r0 = state.astrr;                     
  state.astrr &= r16;
  state.astrr |= (r16>>4) & 0xf;      
  state.check_int = true;
}

/**
 * Implementation of CALL_PAL CSERVE opcode.
 **/

void CAlphaCPU::vmspal_call_cserve()
{
  p23 = state.pc;

  switch(r16)
  {
  case 0x10:
    hw_ldl(r17,r0);
    break;
  case 0x11:
    hw_stl(r17,r18);
    break;
  case 0x12:
    state.pc = 0x12e21;
    state.rem_ins_in_page = 0;
    break;
  case 0x13:
    state.pc = 0x12f95;
    state.rem_ins_in_page = 0;
    break;
  case 0x14:
    state.pc = 0x13115;
    state.rem_ins_in_page = 0;
    break;
  case 0x15:
    state.pc = 0x131c1;
    state.rem_ins_in_page = 0;
    break;
  case 0x40:
    state.pc = 0x13249;
    state.rem_ins_in_page = 0;
    break;
  case 0x41:
    hw_ldq(p21+0x98,r0);
    break;
  case 0x42:
    state.pc = 0x13781;
    state.rem_ins_in_page = 0;
    break;
  case 0x43:
    state.pc = 0x13261;
    state.rem_ins_in_page = 0;
    break;
  case 0x44:
    state.pc = r17;
    state.rem_ins_in_page = 0;
    break;
  case 0x45:
    state.pc = 0x13289;
    state.rem_ins_in_page = 0;
    break;
  case 0x65:
    state.pc = 0x132bd;
    state.rem_ins_in_page = 0;
    break;
  case 0x3e:
    state.pc = 0x1344d;
    state.rem_ins_in_page = 0;
    break;
  case 0x66:
    state.pc = 0x133e9;
    state.rem_ins_in_page = 0;
    break;
  }
}

/**
 * Implementation of CALL_PAL MFPR_FEN opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_fen()
{
  r0 = state.fpen?1:0;
}

/**
 * Implementation of CALL_PAL MTPR_FEN opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_fen()
{
  hw_ldq(p21+0x10,p4);
  state.fpen=(r16&1);
  hw_stl(p4+0x38,r16);
}

/**
 * Implementation of CALL_PAL MFPR_IPL opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_ipl()
{
  r0 = (p22>>8) & 0x1f;
}

/**
 * Implementation of CALL_PAL MTPR_IPL opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_ipl()
{
  r0 = (p22>>8) & 0xff;
  p22 &= ~X64(ff00);
  p22 |= (r16<<8);
  state.eien = ipl_ier_mask[r16][0];
  state.slen = ipl_ier_mask[r16][1];
  state.cren = ipl_ier_mask[r16][2];
  state.pcen = ipl_ier_mask[r16][3];
  state.sien = ipl_ier_mask[r16][4];
  state.asten = ipl_ier_mask[r16][5];
  state.check_int = true;
}

/**
 * Implementation of CALL_PAL MFPR_MCES opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_mces()
{
  r0 = (p22>>16) & 0xff;  
}

/**
 * Implementation of CALL_PAL MTPR_MCES opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_mces()
{
  p22 =        (p22 & X64(ffffffffff00ffff))       
      |        (p22 & X64(0000000000070000) & ~(r16 << 16))                      
      | ((r16 <<16) & X64(0000000000180000));
}

/**
 * Implementation of CALL_PAL MFPR_PCBB opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_pcbb()
{
  hw_ldq(p21+0x10,r0);
}

/**
 * Implementation of CALL_PAL MFPR_PRBR opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_prbr()
{
  hw_ldq(p21+0xa8,r0);
}

/**
 * Implementation of CALL_PAL MTPR_PRBR opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_prbr()
{
  hw_stq(p21+0xa8,r16);
}

/**
 * Implementation of CALL_PAL MFPR_PTBR opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_ptbr()
{
  hw_ldq(p21+8,r0);
  r0 >>= 0x0d;
}

/**
 * Implementation of CALL_PAL MFPR_SCBB opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_scbb()
{
  hw_ldq(p21+0x170,r0);
  r0 >>= 0x0d;
}

/**
 * Implementation of CALL_PAL MTPR_SCBB opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_scbb()
{
  hw_stq(p21+0x170,(r16&X64(ffffffff))<<0xd);
}

/**
 * Implementation of CALL_PAL MTPR_SIRR opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_sirr()
{
  if (r16>0 && r16<16)  
  {
    state.sir |= 1 << r16;
    state.check_int = true;
  }
}

/**
 * Implementation of CALL_PAL MFPR_SISR opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_sisr()
{
  r0 = state.sir;
}

/**
 * Implementation of CALL_PAL MFPR_TBCHK opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_tbchk()
{
  r0 = X64(8000000000000000);
}


/**
 * Implementation of CALL_PAL MTPR_TBIA opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_tbia()
{
  tbia(ACCESS_READ);                 
  tbia(ACCESS_EXEC);                 
  flush_icache();                       
}

/**
 * Implementation of CALL_PAL MTPR_TBIAP opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_tbiap()
{
  tbiap(ACCESS_READ);          
  tbiap(ACCESS_EXEC);          
  flush_icache_asm();                   
}

/**
 * Implementation of CALL_PAL MTPR_TBIS opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_tbis()
{
  tbis(r16,ACCESS_READ);   
  tbis(r16,ACCESS_EXEC);   
}

/**
 * Implementation of CALL_PAL MFPR_ESP opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_esp()
{
  u64 t;
  hw_ldq(p21+0x10,t);
  hw_ldq(t+8,r0);
}

/**
 * Implementation of CALL_PAL MTPR_ESP opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_esp()
{
  u64 t;
  hw_ldq(p21+0x10,t);
  hw_stq(t+8,r16);
}

/**
 * Implementation of CALL_PAL MFPR_SSP opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_ssp()
{
  u64 t;
  hw_ldq(p21+0x10,t);
  hw_ldq(t+0x10,r0);
}

/**
 * Implementation of CALL_PAL MTPR_SSP opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_ssp()
{
  u64 t;
  hw_ldq(p21+0x10,t);
  hw_stq(t+0x10,r16);
}

/**
 * Implementation of CALL_PAL MFPR_USP opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_usp()
{
  u64 t;
  hw_ldq(p21+0x10,t);
  hw_ldq(t+0x18,r0);
}

/**
 * Implementation of CALL_PAL MTPR_USP opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_usp()
{
  u64 t;
  hw_ldq(p21+0x10,t);
  hw_stq(t+0x18,r16);
}

/**
 * Implementation of CALL_PAL MTPR_TBISD opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_tbisd()
{
  tbis(r16,ACCESS_READ);
}

/**
 * Implementation of CALL_PAL MTPR_TBISI opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_tbisi()
{
  tbis(r16,ACCESS_EXEC);
}

/**
 * Implementation of CALL_PAL MFPR_ASTEN opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_asten()
{
  r0 = state.aster;
}

/**
 * Implementation of CALL_PAL MFPR_ASTSR opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_astsr()
{
  r0 = state.astrr;
}

/**
 * Implementation of CALL_PAL MFPR_VPTB opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_vptb()
{
  hw_ldq(p21,r0);
}

/**
 * Implementation of CALL_PAL MTPR_DATFX opcode.
 **/

void CAlphaCPU::vmspal_call_mtpr_datfx()
{
  u64 t,u;
  hw_ldq(p21+0x10,t);
  hw_ldq(t,u);
  u |= X64(1)<<0x3f;
  u &= ~(r16<<0x3f);
  hw_stq(t,u);
}

/**
 * Implementation of CALL_PAL MFPR_WHAMI opcode.
 **/

void CAlphaCPU::vmspal_call_mfpr_whami()
{
  hw_ldq(p21+0x98,r0);
}

/**
 * Implementation of CALL_PAL IMB opcode.
 **/

void CAlphaCPU::vmspal_call_imb()
{
  if (p22&0x18)
  {
    hw_ldq(p21+0x10,p20);
    hw_ldl(p20+0x3c,p5);
    p5 |= 1;
    hw_stl(p20+0x3c,p5);
  }
  flush_icache();
}

/**
 * Implementation of CALL_PAL PROBER opcode.
 **/

void CAlphaCPU::vmspal_call_prober()
{
  u64 pa;

  p23 = state.pc;
  p4 = r18 & 3;                               // alt mode
  p5 = p22 & 0x18;
  p5 >>= 3;                                   // current mode
  p6 = p5-p4;
  if (p5>p4)
    p4 = p5;
  state.alt_cm = (int)p4;
  hw_stq(p21+0x140, state.pc);
  hw_stq(p21+0x148, p4);

  if (virt2phys(r16, &pa, ACCESS_READ | ALT | PROBE, NULL, 0)<0)
    return;

  if (virt2phys(r16+r17, &pa, ACCESS_READ | ALT | PROBE, NULL, 0)<0)
    return;

  r0 = 1;
  return;
}

/**
 * Implementation of CALL_PAL PROBEW opcode.
 **/

void CAlphaCPU::vmspal_call_probew()
{
  u64 pa;

  p23 = state.pc;
  p4 = r18 & 3;                               // alt mode
  p5 = p22 & 0x18;
  p5 >>= 3;                                   // current mode
  p6 = p5-p4;
  if (p5>p4)
    p4 = p5;
  state.alt_cm = (int)p4;
  hw_stq(p21+0x140, state.pc);
  hw_stq(p21+0x148, p4);
  if (virt2phys(r16, &pa, ACCESS_WRITE | ALT | PROBE | PROBEW, NULL, 0)<0)
    return;

  if (virt2phys(r16+r17, &pa, ACCESS_WRITE | ALT | PROBE | PROBEW, NULL, 0)<0)
    return;

  r0 = 1;
  return;
}

/**
 * Implementation of CALL_PAL RD_PS opcode.
 **/

void CAlphaCPU::vmspal_call_rd_ps()
{
  r0 = p22 & X64(ffff);
}

/**
 * Implementation of CALL_PAL REI opcode.
 **/

int CAlphaCPU::vmspal_call_rei()
{
  u64 phys_address;

  p23 = state.pc;

  p7 = p22 & 0x18;  //old cm
  hw_stq(p21+0x150,p23);
  if (r30 & 0x3f)
  {
    // stack not aligned
    p4 = 0x430;
    hw_stq(p21+0x158,p4);
    return vmspal_int_initiate_exception();
  }

  if (p7)
  {
  // what to do if the next instruction results in a TNV exception?

    ldq(r30+0x38,p20);
    state.bIntrFlag = false;
    ldq(r30+0x30,p23);
    p4 = p20 & 0x18; // new cm
    if ((p4<p7) || (p20 & ~X64(3f0000000000001b)))
    {
      p4 = 0x430;
      hw_stq(p21+0x158,p4);
      vmspal_int_initiate_exception();
      return 0;
    }
    ldq(r30+0x10,state.r[4]);
    ldq(r30+0x18,state.r[5]);
    ldq(r30+0x20,state.r[6]);
    ldq(r30+0x28,state.r[7]);
    ldq(r30,r2);
    ldq(r30+8,r3);
    hw_ldq(p21+0x10,p6);
    p5 = (p20>>56)&0xff;
    p7 += p6;
    p6 += p4;
    p20 &= 0xffff;
    p22 &= ~X64(ffff);
    state.cm = (int)(p4>>3)&3;
    p22 |= p20;
    p23 &= ~X64(3);
    p20 = r30 + 0x40;
    p20 |= p5;
    hw_stq(p7,p20);
    hw_ldq(p6,r30);
    state.pc = p23;
    state.rem_ins_in_page = 0;
    return 0;
  }

  ldq(r30+0x38,p20);
  state.bIntrFlag = false;
  p7 = (p20>>8) & 0xff;
  ldq(r30+0x10,state.r[4]);
  ldq(r30+0x18,state.r[5]);
  ldq(r30+0x20,state.r[6]);
  ldq(r30+0x28,state.r[7]);
  ldq(r30,r2);
  ldq(r30+8,r3);
  p4 = p20 & 0x18; // new cm
  ldq(r30+0x30,p23);

  if (p4)
  {
    hw_ldq(p21+0x10,p7);
    p7 += p4;
    state.cm = (int)(p4>>3)&3;
    p5 = (p20>>56)&0xff;
    p20 &= 0xff;
    p22 &= ~X64(ffff);
    p22 |= p20;
    p23 &= ~X64(3);
    p20 = r30 + 0x40;
    p20 |= p5;
    hw_ldq(p7,r30);
    hw_stq(p21+0x18,p20);
    state.eien = ipl_ier_mask[0][0];
    state.slen = ipl_ier_mask[0][1];
    state.cren = ipl_ier_mask[0][2];
    state.pcen = ipl_ier_mask[0][3];
    state.sien = ipl_ier_mask[0][4];
    state.asten = ipl_ier_mask[0][5];
    state.check_int = true;
    state.pc = p23;
    state.rem_ins_in_page = 0;
    return 0;
  }
  p5 = (p20>>56) & 0xff;
  p20 &= 0xffff;
  p22 &= ~X64(ffff);
  p7 = (p20>>8) & 0xff;
  p22 |= p20;
  p23 &= ~X64(3);
  p20 = r30 + 0x40;
  r30 = p20 | p5;
  state.eien = ipl_ier_mask[p7][0];
  state.slen = ipl_ier_mask[p7][1];
  state.cren = ipl_ier_mask[p7][2];
  state.pcen = ipl_ier_mask[p7][3];
  state.sien = ipl_ier_mask[p7][4];
  state.asten = ipl_ier_mask[p7][5];
  state.check_int = true;
  state.pc = p23;
  state.rem_ins_in_page = 0;
  return 0;
}

/**
 * Implementation of CALL_PAL SWASTEN opcode.
 **/

void CAlphaCPU::vmspal_call_swasten()
{
  r0 = (state.aster & (1<<((p22>>3)&3)))?1:0;    
  if (r16&1)                                                
  {                                                                 
    state.aster |= (1<<((p22>>3)&3));                    
    state.check_int = true;                                         
  }                                                                 
  else                                                              
    state.aster &= ~(1<<((p22>>3)&3));                   
}

/**
 * Implementation of CALL_PAL WR_PS_SW opcode.
 **/

void CAlphaCPU::vmspal_call_wr_ps_sw()
{
  p22 &= ~X64(3);                            
  p22 |= r16 & X64(3);               
}

/**
 * Implementation of CALL_PAL RSCC opcode.
 **/

void CAlphaCPU::vmspal_call_rscc()
{
  hw_ldq(p21+0xa0,r0);
  if (state.cc<(r0 & X64(00000000ffffffff)))    
    r0 += X64(1)<<0x20;                         
  r0 &= X64(ffffffff00000000);                  
  r0 |= state.cc;                               
  hw_stq(p21+0xa0,r0);
}

/**
 * Implementation of CALL_PAL READ_UNQ opcode.
 **/

void CAlphaCPU::vmspal_call_read_unq()
{
  u64 t;
  hw_ldq(p21+0x10,t);
  hw_ldq(t+0x48,r0);
}

/**
 * Implementation of CALL_PAL WRITE_UNQ opcode.
 **/

void CAlphaCPU::vmspal_call_write_unq()
{
  u64 t;
  hw_ldq(p21+0x10,t);
  hw_stq(t+0x48,r16);
}
//\}

/***************************************************************************//**
 * \name VMS_pal_int
 * Internal routines used by VMS PALcode replacement.
 ******************************************************************************/
//\{

/**
 * Pass control to the OS for handling the exception.
 **/
int CAlphaCPU::vmspal_int_initiate_exception()
{
  u64 phys_address;

  p4 = p22 & X64(18);
  hw_ldq(p21 + X64(10),   p20);
  if (p4)
  {
    // change mode to kernel
    state.cm = 0;
    // switch to kernel stack
    p20 += p4;
    hw_stq(p20,      r30);
    hw_ldq(p21 + X64(18), r30);
  }
  p20 = r30 & X64(3f);
  r30 &= ~X64(3f);
  stq(r30 - X64(40),    r2);
  stq(r30 - X64(38),    r3);
  hw_stq(p21 + X64(f0), r1);
  r3 = p21;
  stq(r30 - X64(30),    state.r[4]);
  stq(r30 - X64(28),    state.r[5]);
  stq(r30 - X64(20),    state.r[6]);
  stq(r30 - X64(18),    state.r[7]);
  hw_ldq(state.r[3] + X64(160), state.r[4]);
  hw_ldq(state.r[3] + X64(168), state.r[5]);
  hw_ldq(p21 + X64(f0), r1);
  hw_ldq(p21 + X64(170), p4);
  hw_ldq(p21 + X64(158), p5);
  p7 = p22 & X64(ffff);
  p20 <<= 0x38;
  p20 |= p7;
  p4 += p5;
  hw_ldq(p4 + X64(00), r2);
  hw_ldq(p4 + X64(08), r3);
  p22 &= ~X64(1b);
  r30 -= X64(40);
  hw_ldq(p21 + X64(150), p6);
  r2 &= ~X64(3);
  stq(r30 + X64(38), p20);
  stq(r30 + X64(30), p6);

  state.pc = r2;
  state.rem_ins_in_page = 0;
  return -1;
}

/**
 * Pass control to the OS for handling the interrupt.
 **/

int CAlphaCPU::vmspal_int_initiate_interrupt()
{
  u64 phys_address;
  
  p4 = p22 & X64(18);
  hw_ldq(p21 + X64(10),   p20);
  if (p4)
  {
    // change mode to kernel
    state.cm = 0;
    // switch to kernel stack
    p20 += p4;
    hw_stq(p20,      r30);
    hw_ldq(p21 + X64(18), r30);
  }
  p20 = r30 & X64(3f);
  r30 &= ~X64(3f);
  stq(r30 - X64(40),    r2);
  stq(r30 - X64(38),    r3);
  hw_stq(p21 + X64(f0), r1);
  r3 = p21;
  stq(r30 - X64(30),    state.r[4]);
  stq(r30 - X64(28),    state.r[5]);
  stq(r30 - X64(20),    state.r[6]);
  stq(r30 - X64(18),    state.r[7]);
  hw_ldq(state.r[3] + X64(160), state.r[4]);
  hw_ldq(state.r[3] + X64(168), state.r[5]);
  hw_ldq(p21 + X64(f0), r1);
  hw_ldq(p21 + X64(170), p4);
  hw_ldq(p21 + X64(158), p5);
  p7 = p22 & X64(ffff);
  p20 <<= 0x38;
  p20 |= p7;
  p4 += p5;
  hw_ldq(p4, r2);
  hw_ldq(p4 + X64(08), r3);
  hw_ldq(p21 + X64(128), p4);
  p22 &= ~X64(ffff);
  p22 |= p4;
  r30 -= X64(40);
  hw_ldq(p21 + X64(150), p6);
  r2 &= ~X64(3);
  stq(r30 + X64(38), p20);
  stq(r30 + X64(30), p6);

  state.pc = r2;
  state.rem_ins_in_page = 0;
  return -1;
}
//\}

/***************************************************************************//**
 * \name VMS_pal_ent
 * VMS PALcode replacement trap/exception entry-points.
 ******************************************************************************/
//\{

/**
 * Interrupt entry point for Software Interrupts.
 **/

int CAlphaCPU::vmspal_ent_sw_int(int si)
{
  int x;

  state.exc_addr = state.current_pc;
  p23 = state.current_pc;
  for (x=15;x>=0;x--)
  {
    if (si & (1<<x))
      break;
  }
  if (x<=0)
    return 0;

  p7 = x;
  p4 = (u64)x << 4;
  p5 = p4 + 0x500;
  hw_stq(p21+0x158,p5);
  hw_stq(p21+0x150,state.current_pc);
  state.sir &= ~(1<<x);
  state.eien = ipl_ier_mask[x][0];
  state.slen = ipl_ier_mask[x][1];
  state.cren = ipl_ier_mask[x][2];
  state.pcen = ipl_ier_mask[x][3];
  state.sien = ipl_ier_mask[x][4];
  state.asten = ipl_ier_mask[x][5];
  state.check_int = true;
  p20 = (u64)x << 8;
  p20 |= 4;
  hw_stq(p21+0x128, p20);
  return vmspal_int_initiate_interrupt();
}

/**
 * Interrupt entry point for External Interrupts.
 **/

int CAlphaCPU::vmspal_ent_ext_int(int ei)
{
  bool do_11670 = false;

  state.exc_addr = state.current_pc;
  p23 = state.current_pc;
  p7 = ei;
  if (ei & 0x04)
  {
    // TIMER interrupt
    cSystem->clear_clock_int(state.iProcNum);
    p6 = cSystem->get_c_misc();

    p22 += X64(0000010000000000);
    p22 &= X64(ffff0fffffffffff);

    hw_ldq(p21+0xa0,p20);
    p6 = X64(1)<<0x20;
    p4 = state.cc;
    p5 = p20 & X64(ffffffff);
    p20 &= X64(ffffffff00000000);
    p7 = p4-p5;
    if (((s64)p7)<0)
      p20 += p6;
    p20 |= state.cc;
    hw_stq(p21+0xa0,p20);
    hw_stq(p21+0x1c8,0);
    p20 = 0x600;
    p7 = 0x16;

    do_11670 = true;
  }
  else if (ei & 0x02)
  {
    p5 = cSystem->get_c_dir(state.iProcNum);
    if (test_bit_64(p5,0x32))
      FAILURE("Can't handle IRQ 50");

    p4 = 0x100;
    p20 = 8;
    while (p20<0x30)
    {
      if (p4&p5)
        break;
        p4 *= 2;
        p20++;
    }
    if (p4&p5)
    {
      p20 <<= 4;
      p20 += 0x900;
      p7 = 0x15;

      do_11670 = true;
    }
    else if (test_bit_64(p5,0x37))
    {
        // irq 55 - PIC - isa
      p5 = X64(00000801f8000000);
        // pic_read_vector
      hw_ldl(p5,p5);
      p4 = p5 & 0xff;
      if (p4 == 0x07)
        FAILURE ("Can't handle PIC interrupt 7");

      if (p4 >= 0x10)
        return 0;

      p4 <<= 4;
      p20 = p4 + 0x800;
      hw_ldq(0x148,p5);
      if (!p5 || p20!=X64(830))
      {
        p7 = 0x15;
        do_11670 = true;
      }
      else
      {
        hw_stq(0x150,p20);
        p4 = X64(00000801a0000000);
        p5 = X64(2000);
        hw_stq(p4+0x80,p5);
        hw_ldq(p4+0x80,p5);
        return 0;
      }
    }
    else 
    {
      p6 = p5 & X64(0060000000000000);
      if (p6)
        cSystem->set_c_dim(state.iProcNum,cSystem->get_c_dim(state.iProcNum) & ~p6);
      return 0;
    }
  }

  if (do_11670)
  {
    hw_stq(p21+0x150,state.current_pc);
    hw_stq(p21+0x158,p20);
    hw_stq(p21+0x1d8,p20);
    state.eien = ipl_ier_mask[p7][0];
    state.slen = ipl_ier_mask[p7][1];
    state.cren = ipl_ier_mask[p7][2];
    state.pcen = ipl_ier_mask[p7][3];
    state.sien = ipl_ier_mask[p7][4];
    state.asten = ipl_ier_mask[p7][5];
    state.check_int = true;
    p20 = p7 << 8;
    p20 |= 4;
    hw_stq(p21+0x128,p20);

    return vmspal_int_initiate_interrupt();
  }
  return 0;
}

/**
 * Interrupt entry point for Asynchronous System Traps.
 **/

int CAlphaCPU::vmspal_ent_ast_int(int ast)
{
  int x;

  state.exc_addr = state.current_pc;
  p23 = state.current_pc;

  for (x=0;x<4;x++)
  {
    if (ast & (1<<x))
      break;
  }
  state.asten = 0;
  state.sien &= ~7;
  p4 = (u64)x << 4;
  p5 = 0x240 + p4;
  hw_stq(p21+0x158,p5);
  hw_stq(p21+0x150,p23);
  state.astrr &= ~(1<<x);
  p20 = (p22 & 4) + 0x200;
  hw_stq(p21+0x128,p20);
  return vmspal_int_initiate_interrupt();
}

/**
 * Entry point for Single Data Translation Buffer Miss.
 **/

int CAlphaCPU::vmspal_ent_dtbm_single(int flags)
{
  u64 pte_phys;
  u64 t25;
  u64 t26;

  p23 = state.exc_addr;
  p4 = va_form(state.fault_va,false);
  p5 = state.mm_stat;
  p7 = state.exc_sum;
  p6 = state.fault_va;
  p7 &= ~X64(1);
  if (test_bit_64(p22,63))
  {
    // 1-ON-1 translations
    p5 = 0xff01;
    p4 = p6>>0x0d;
    if (!test_bit_64(p6,43))
    {
      p4 &= ~X64(ffffffffbfc00000);
      if (    (p4 >= 0x400a0000 && p4 <= 0x400bffff)
           || (p4 >= 0x400c8000 && p4 <= 0x400cffff)
           || (p4 >= 0x400e0000 && p4 <= 0x400fbfff)
           || (p4 >= 0x400ff800 && p4 <= 0x400fffff)
           || (p4 >= 0x40180000 && p4 <= 0x401bffff)
           || (p4 >= 0x401c8000 && p4 <= 0x401fbfff)
           || (p4 >= 0x401fb000 && p4 <= 0x401fffff)
           || (p4 >= 0x40200000 && p4 <= 0x403fffff) )
        p5 = X64(1);
    }

    p4 <<= 0x20;
    p4 |= p5;
    add_tb(p6,p4,ACCESS_READ);
    return 0;
  }

  p4 &=~X64(7);
  if (virt2phys(p4,&pte_phys,ACCESS_READ | NO_CHECK | VPTE | (flags & (PROBE | PROBEW)), NULL, 0))
    return -1;
  p4 = cSystem->ReadMem(pte_phys,64);

  if (!test_bit_64(p4,0))
  {
    if (flags & PROBE)
    {
      t25 = X64(30000);
      p4 |= t25;
      t26 = p5 & 1;                            // write or read?
      t26 *= 4;
      t25 = ((p22>>3) & 3) | 8;
      t26 += t25;
      t25 = p4 >> 0x10;
      if (!(t25 & 1))
        t26 = 8;
      t26 = (t25 << t26) & p4;
      p7 = t26?0x90:0x80;                           //page fault or acv
      hw_stq(p21+0x158, p7);
      t26 = p23 & ~X64(3);
      hw_ldq(p21+0x148, p7);
      if (test_bit_64(p4,(int)(8+p7+((flags & PROBEW)?4:0))))
        return 1;
      r0 = 0;
      return -1;
    }

    if (state.current_pc & 1)
    {
      t25 = X64(30000);
      p4 |= t25;
      t26 = p5 & 1;                            // write or read?
      t26 *= 4;
      t25 = ((p22>>3) & 3) | 8;
      t26 += t25;
      t25 = p4 >> 0x10;
      if (!(t25 & 1))
        t26 = 8;
      t26 = (t25 << t26) & p4;
      p7 = t26?0x90:0x80;                           //page fault or acv
      hw_stq(p21+0x158, p7);
      t26 = p23 & ~X64(3);

      hw_stq(p21+0x118,state.r[25]);
      hw_stq(p21+0x120,state.r[26]);
      state.r[25] = t25;
      state.r[26] = t26;
      state.pc = 0xd981;
      state.rem_ins_in_page = 0;
      return -1;
      //return vmspal_int_dfault_in_palmode();
    }
    hw_stq(p21+X64(150),p23);
    hw_stq(p21+X64(160),p6);
    p20 = ((p22 >> 3) & 3) + 8;
    if (    (test_bit_64(p5,0) && ((p5>>4)&0x3f)==0x18)
        || (!test_bit_64(p5,0) && ((p7>>8)&0x1f)==0x1f) )
    {
      // write "MISC" or read to R31
      state.pc = state.current_pc + 4;
      state.rem_ins_in_page = 0;
      return -1;
    }
    p5 &= X64(1);
    p7 = p5 << 0x3f;
    p6 = 4 * p5;
    hw_stq(p21+X64(168),p7);
    p6 += p20;
    p6 = X64(1) << p6;
    p6 &= p4;
    p20 = (p6)?0x90:0x80;
    hw_stq(p21+X64(158),p20);
    return vmspal_int_initiate_exception();
  }
  add_tb(p6,p4,ACCESS_READ);
  return 0;
}

/**
 * Entry point for Instruction Translation Buffer Miss.
 **/

int CAlphaCPU::vmspal_ent_itbm(int flags)
{
  u64 pte_phys;

  p4 = va_form(state.exc_addr,true);
  p23 = state.exc_addr;
  p6 = p23;
  if (test_bit_64(p22,63))
  {
    // 1-ON-1 translations enabled
    p6 = p23>> 0x0d;
    p5 = 0xf01;
    p6 <<= 0x0d;
    p6 |= p5;
    add_tb_i(p23,p6);
    return 0;
  }

  p4 &=~X64(7);
  if (virt2phys(p4,&pte_phys,ACCESS_READ | NO_CHECK | VPTE, NULL, 0))
    return -1;
  p4 = cSystem->ReadMem(pte_phys,64);

  p6 = 0xfff;
  p5 = p4 & p6;
  p6 = p4 >> 0x20;
  p6 <<= 0x0d;
  if (!(p4&1) || (p4&8))
  {
    p20 = (p4&1)?0xc0:0x90;

    p6 = p22 >> 3;
    hw_stq(p21+0x160,p23);
    hw_stq(p21+0x150,p23);
    p6 &= X64(3);
    p5 = 1;
    p6 += 8;
    hw_stq(p21+0x168,p5);
    p6 = p5<<p6;
    p6 &= p4;
    // Access Violation
    if (!p6)
      p20 = 0x80;
    hw_stq(p21+0x158,p20);
    return vmspal_int_initiate_exception();
  }
  p6 |= p5;
  add_tb_i(p23,p6);
  return 0;
}

/**
 * Entry point for Double Data Translation Buffer Miss.
 **/

int CAlphaCPU::vmspal_ent_dtbm_double_3(int flags)
{
  u64 t25;
  u64 t26;

  p7 &= 0xffff;
  p5 <<= 16;
  p7 |= p5;
  p5 = state.exc_addr;
  p7 |= 1;
  hw_ldq(p21+8, t25);                                      //PTBR

  t26 = (p4 << 0x1f) >> 0x33;                                   //L1 index
  t25 += t26;
  hw_ldq(t25, t25);
  t26 = (p4 << 0x29) >> 0x33;
  if (t25 & 1)
  {
    t25 >>= 0x20;
    t25 <<= 0x0d;
    t25 += t26;
    hw_ldq(t25, t25);
    if (t25 & 1)
    {
      add_tb(p4,t25,ACCESS_READ);
      return 0;
    }
  }
  // PAGE FAULT...
  p4 = p5;          // return address...
  p5 = p7 >> 0x10;
  t26 = state.pal_base;

  if(flags & PROBE)                        // in PALmode!!
  {
    p4 = t25 & ~X64(30000);
    t26 = p5 & 1;                            // write or read?
    t26 *= 4;
    t25 = ((p22>>3) & 3) | 8;
    t26 += t25;
    t25 = p4 >> 16;
    if (!(t25 & 1))
      t26 = 8;
    t26 = (X64(1) << t26) & p4;
    p7 = t26?0x90:0x80;                           //page fault or acv
    hw_stq(p21+0x158, p7);

    if (p7 != 0x80)
    {
      p5 = (flags & PROBEW) ? X64(8000000000000000) : 0;
      hw_stq(p21+0x160,p6);
      hw_stq(p21+0x168,p5);
      hw_stq(p21+0x150,state.current_pc);
      return vmspal_int_initiate_exception();
    }
    r0 = 0;
    return -1;
  }

  if(p23 & 1)                        // in PALmode!!
  {
    p4 = t25 & ~X64(30000);
    t26 = p5 & 1;                            // write or read?
    t26 *= 4;
    t25 = ((p22>>3) & 3) | 8;
    t26 += t25;
    t25 = p4 >> 16;
    if (!(t25 & 1))
      t26 = 8;
    t26 = (X64(1) << t26) & p4;
    p7 = t26?0x90:0x80;                           //page fault or acv
    hw_stq(p21+0x158, p7);
    t26 = p23 & ~X64(3);

    hw_stq(p21+0x118,state.r[25]);
    hw_stq(p21+0x120,state.r[26]);
    state.r[25] = t25;
    state.r[26] = t26;
    state.pc = 0xd981;
    state.rem_ins_in_page = 0;
    return -1;
  }

  p20 = p4 & ~X64(3);
  p4 = t25 & 0x100;
  hw_stq(p21+0x150, p23);
  p20 = t26 - p20;
  t26 = p20 + 0x590;
  if (!t26)
  {
    p5 = 1;
    hw_stq(p21+0x160, p23);
    p20 = p4?0x90:0x80;
    hw_stq(p21+0x168,p5);
    hw_stq(p21+0x158,p20);
    return vmspal_int_initiate_exception();
  }
  if (    (test_bit_64(p5,0) && ((p5>>4)&0x3f)==0x18)
      || (!test_bit_64(p5,0) && ((p7>>8)&0x1f)==0x1f) )
  {
    state.pc = p23 + 4;
    state.rem_ins_in_page = 0;
    return -1;
  }
  p5 <<= 0x3f;
  p20 = p4?0x90:0x80;
  hw_stq(p21+0x168,p5);
  hw_stq(p21+0x160,p6);
  hw_stq(p21+0x158,p20);
  return vmspal_int_initiate_exception();
}

/**
 * Entry point for IStream Access Violation
 **/

int CAlphaCPU::vmspal_ent_iacv(int flags)
{
  p6 = state.current_pc;
  p4 = 1;
  p5 = 0x80;
  hw_stq(p21+0x168,p4);
  hw_stq(p21+0x158,p5);
  if (state.current_pc & 1)
  {
    p7 = state.current_pc;
    p20 = 5;
    hw_stq(p21+0xc8,p20);
    p23 = state.current_pc;
    state.pc = 0xde01;
    state.rem_ins_in_page = 0;
    return -1;
  }
  hw_stq(p21+0x160,state.current_pc);
  hw_stq(p21+0x150,state.current_pc);
  return vmspal_int_initiate_exception();
}

/**
 * Entry point for DStream Fault.
 **/

int CAlphaCPU::vmspal_ent_dfault(int flags)
{
  u64 t25, t26;
  p6 = state.current_pc;
  p7 = state.exc_sum;
  p5 = state.mm_stat;
  if (flags & PROBE)
  {
    hw_stq(p21+0xd0,p23);
    p23 = p6;
    t26 = p6 & ~X64(3);
    p6 = state.fault_va;
    t25 = p5 & 2;
    p7 = 0x80;
    if (!t25)
    {
      t25 = p5 & 8;
      p7 = t25?0xb0:0xa0;
      tbis(p6,ACCESS_READ);
    }
    hw_stq(p21+0x158,p7);
    p4 = 1;
    hw_ldq(p21+0x158,p7);
    if (p7 == 0xa0 || p7 == 0xb0)
    {
      return 1;
    }
    r0 = 0;
    return -1;
  }

  if (state.current_pc & 1)
  {
    hw_stq(p21+0xd0,p23);
    p23 = p6;
    t26 = p6 & ~X64(3);
    p6 = state.fault_va;
    t25 = p5 & 2;
    p7 = 0x80;
    if (!t25)
    {
      t25 = p5 & 8;
      p7 = t25?0xb0:0xa0;
      tbis(p6,ACCESS_READ);
    }
    hw_stq(p21+0x158,p7);
    p4 = 1;
    hw_stq(p21+0x118,state.r[25]);
    hw_stq(p21+0x120,state.r[26]);
    state.r[25] = t25;
    state.r[26] = t26;
    state.pc = 0xd981;
    state.rem_ins_in_page = 0;
    return -1;
  }

  p20 = state.fault_va;
  if (   ( (state.mm_stat & 1) && (((state.mm_stat>>4)&0x3f)==0x18))
      || (!(state.mm_stat & 1) && (((state.exc_sum>>8)&0x1f)==0x1f))  )
  {
    state.pc = state.current_pc + 4;
    state.rem_ins_in_page = 0;
    return -1;
  }
  p7 = 0x80;
  if (!(state.mm_stat & 2))
  {
    p7 = (state.mm_stat & 4)?0xa0:0xb0;
    tbis(p20,ACCESS_READ);
  }
  hw_stq(p21+0x158,p7);
  hw_stq(p21+0x160,p20);
  p5 <<= 0x3f;
  hw_stq(p21+0x168, p5);
  hw_stq(p21+0x150,p6);
  return vmspal_int_initiate_exception();
}
//\}
