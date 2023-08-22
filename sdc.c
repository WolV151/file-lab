/********************************************************************
Copyright 2010-2017 K.C. Wang, <kwang@eecs.wsu.edu>
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
********************************************************************/

#include "sdc.h"
u32 base;
#define printf kprintf

// shared variables between SDC driver and interrupt handler
volatile char *rxbuf, *txbuf;
volatile int rxdone, txdone;
int sdc_handler()
{
  u32 status, status_err;
  int i, count = 512;
  u32 *up;
  int oldcolor;

  // read status register to find out TXempty or RxAvail
  status = *(u32 *)(base + STATUS);
  oldcolor = color;

  if (status & (1 << 21))
  { // RXavail: read data
    color = CYAN;
    printf("SDC RX interrupt: status=%x: ", status);
    up = (u32 *)rxbuf;
    // read data from FIFO
    status_err = status & (SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT | SDI_STA_RXOVERR);

    while (!status_err && count)
    {
      // printf("R%d ", count);
      *(up) = *(u32 *)(base + FIFO);
      up++;
      count -= sizeof(u32);
      status = *(u32 *)(base + STATUS);
      status_err = status & (SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT |
                             SDI_STA_RXOVERR);
    }
    color = oldcolor;
    rxdone = 1;
  }
  else if (status & (1 << 18))
  { // TXempty
    color = YELLOW;
    printf("\nSDC TX interrupt: status=%x: ", status);
    up = (u32 *)txbuf;
    status_err = status & (SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT);

    while (!status_err && count)
    {
      // printf("W%d ", count);
      *(u32 *)(base + FIFO) = *up;
      up++;
      count -= sizeof(u32);
      status = *(u32 *)(base + STATUS);
      status_err = status & (SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT);
    }
    color = oldcolor;
    txdone = 1;
  }

  // printf("write to clear register\n");
  *(u32 *)(base + STATUS_CLEAR) = 0xFFFFFFFF;
  printf("SDC interrupt handler done\n");
}

int delay()
{
  int i;
  for (i = 0; i < 1000; i++)
    ;
}

int do_command(int cmd, int arg, int resp)
{
  *(u32 *)(base + ARGUMENT) = (u32)arg;
  *(u32 *)(base + COMMAND) = 0x400 | (resp << 6) | cmd;
  // delay();
}

int sdc_init()
{
  u32 RCA = (u32)0x45670000; // QEMU's hard-coded RCA
  base = (u32)0x10005000;    // PL180 base address
  printf("sdc_init : ");
  *(u32 *)(base + POWER) = (u32)0xBF; // power on
  *(u32 *)(base + CLOCK) = (u32)0xC6; // default CLK

  // send init command sequence
  do_command(0, 0, MMC_RSP_NONE); // idle state
  do_command(55, 0, MMC_RSP_R1);  // ready state
                                  // do_command(41, 1,   MMC_RSP_R3);  // argument must not be zero
  do_command(41, 0x0000FFFF, MMC_RSP_R3);
  do_command(2, 0, MMC_RSP_R2);    // ask card CID
  do_command(3, RCA, MMC_RSP_R1);  // assign RCA
  do_command(7, RCA, MMC_RSP_R1);  // transfer state: must use RCA
  do_command(16, 512, MMC_RSP_R1); // set data block length

  // set interrupt MASK0 registers bits = RxAvail|TxEmpty
  *(u32 *)(base + MASK0) = (1 << 21) | (1 << 18); // 0x00240000;
  printf("done\n");
}

int getSector(int sector, char *buf)
{
  u32 cmd, arg;

  // printf("getSector %d %x\n", sector, buf);
  rxbuf = buf;
  rxdone = 0;
  *(u32 *)(base + DATATIMER) = 0xFFFF0000;
  // write data_len to datalength reg
  *(u32 *)(base + DATALENGTH) = 512;

  // printf("dataControl=%x\n", 0x93);
  //  0x93=|9|0011|=|9|DMA=0,0=BLOCK,1=Host<-Card,1=Enable
  *(u32 *)(base + DATACTRL) = 0x93;

  cmd = 17; // CMD17 = READ single block
  arg = (sector * 512);
  do_command(cmd, arg, MMC_RSP_R1);
  while (rxdone == 0)
    ;
  printf("getSector return\n");
}

int putSector(int sector, char *buf)
{
  u32 cmd, arg;

  // printf("putSector %d %x\n", sector, buf);
  txbuf = buf;
  txdone = 0;
  *(u32 *)(base + DATATIMER) = 0xFFFF0000;
  *(u32 *)(base + DATALENGTH) = 512;

  cmd = 24; // CMD24 = Write single block
  arg = (u32)(sector * 512);
  do_command(cmd, arg, MMC_RSP_R1);

  // printf("dataControl=%x\n", 0x91);
  //  write 0x91=|9|0001|=|9|DMA=0,BLOCK=0,0=Host->Card, Enable
  *(u32 *)(base + DATACTRL) = 0x91; // Host->card
  while (txdone == 0)
    ;
  printf("putSector return\n");
}
