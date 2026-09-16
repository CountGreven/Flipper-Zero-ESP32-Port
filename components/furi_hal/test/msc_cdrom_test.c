#include "msc_cdrom.h"
#include <stdio.h>
#include <string.h>
static int fails=0,checks=0;
static void eq(const char*n,long g,long w){checks++;if(g!=w){printf("FAIL %-22s got %ld want %ld\n",n,g,w);fails++;}}
#define CD 100000u
int main(void){
    uint8_t o[64];
    /* INQUIRY */
    { uint8_t r[36]; memset(r,0xAA,36); msc_cdrom_inquiry(r,"Flipper ","Virtual CD-ROM  ");
      eq("inq pdt",r[0],0x05); eq("inq rem",r[1],0x80);
      checks++; if(memcmp(r+16,"Virtual CD-ROM  ",16)){printf("FAIL inq product\n");fails++;} }
    /* READ TOC fmt0 LBA */
    { uint8_t c[16]={0x43,0,0,0,0,0,0,0x00,0x14,0}; int32_t n=msc_cdrom_scsi(c,o,sizeof o,CD);
      eq("toc0 len",n,20); eq("toc0 dl",(o[0]<<8)|o[1],18); eq("toc0 adr",o[5],0x16);
      eq("toc0 lead",o[14],0xAA); eq("toc0 lo",(o[16]<<24)|(o[17]<<16)|(o[18]<<8)|o[19],CD); }
    /* READ TOC fmt0 MSF */
    { uint8_t c[16]={0x43,0x02,0,0,0,0,0,0x00,0x14,0}; msc_cdrom_scsi(c,o,sizeof o,CD);
      eq("tocM M",o[17],22); eq("tocM S",o[18],15); eq("tocM F",o[19],25); }
    /* READ TOC fmt2 raw */
    { uint8_t c[16]={0x43,0,0x02,0,0,0,0,0x00,0x30,0}; int32_t n=msc_cdrom_scsi(c,o,sizeof o,CD);
      eq("toc2 len",n,48); eq("toc2 dl",(o[0]<<8)|o[1],46);
      eq("toc2 A0",o[4+3],0xA0); eq("toc2 A1",o[4+11+3],0xA1); eq("toc2 A2",o[4+22+3],0xA2); eq("toc2 t1",o[4+33+3],1); }
    /* GET CONFIGURATION */
    { uint8_t c[16]={0x46,0,0,0,0,0,0,0x00,0x20,0}; int32_t n=msc_cdrom_scsi(c,o,sizeof o,CD);
      eq("cfg len",n,16); eq("cfg dl",(o[0]<<24)|(o[1]<<16)|(o[2]<<8)|o[3],12);
      eq("cfg cur",(o[6]<<8)|o[7],0x0008); eq("cfg prof",(o[12]<<8)|o[13],0x0008); eq("cfg curbit",o[14],1); }
    /* GET EVENT STATUS */
    { uint8_t c[16]={0x4A,1,0,0,0x10,0,0,0,8,0}; int32_t n=msc_cdrom_scsi(c,o,sizeof o,CD);
      eq("evt len",n,4); eq("evt nea",o[2],0x80); }
    /* READ HEADER */
    { uint8_t c[16]={0x44,0,0,0,0,0,0,0,8,0}; int32_t n=msc_cdrom_scsi(c,o,sizeof o,CD);
      eq("hdr len",n,8); eq("hdr mode",o[0],0x01); }
    /* unemulated -> -1 */
    { uint8_t c[16]={0xBD,0,0,0,0,0,0,0,0,0}; eq("unhandled",msc_cdrom_scsi(c,o,sizeof o,CD),-1); }
    printf("\n%d checks, %d failures\n",checks,fails);
    return fails?1:0;
}
