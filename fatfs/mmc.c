/*-----------------------------------------------------------------------*/
/* MMC/SDC (in SPI mode) control module  (C)ChaN, 2006                   */
/*-----------------------------------------------------------------------*/
/* Only rcvr_spi(), xmit_spi(), disk_timerproc(), disk_initialize () and */
/* some macros are platform dependent.                                   */
/*-----------------------------------------------------------------------*/


#include "AT91SAM7S64.h"
#include "diskio.h"

#define USE_DMA

/* MMC/SD command (in SPI) */
#define CMD0	(0x40+0)	/* GO_IDLE_STATE */
#define CMD1	(0x40+1)	/* SEND_OP_COND */
#define CMD9	(0x40+9)	/* SEND_CSD */
#define CMD10	(0x40+10)	/* SEND_CID */
#define CMD12	(0x40+12)	/* STOP_TRANSMISSION */
#define CMD17	(0x40+17)	/* READ_SINGLE_BLOCK */
#define CMD18	(0x40+18)	/* READ_MULTIPLE_BLOCK */
#define CMD24	(0x40+24)	/* WRITE_BLOCK */
#define CMD25	(0x40+25)	/* WRITE_MULTIPLE_BLOCK */
#define CMD58	(0x40+58)	/* READ_OCR */

/* Control signals */
#define SELECT()	//PORTB &= ~1		/* MMC CS = L */
#define	DESELECT()	//PORTB |= 1		/* MMC CS = H */

//#define SOCKPORT	PINB			/* Socket contact port */
//#define SOCKWP		0x20			/* Write protect switch (PB5) */
//#define SOCKINS		0x10			/* Card detect switch (PB4) */

#define POWER_ON()	//PORTE &= ~0x80	/* Socke power (PE7) */
#define POWER_OFF()	//PORTE |=  0x80

/*
	AT91SAM7S64 SPI Pins:   Function (A/B)
	PA12 - MISO - AT91C_PA12_MISO  (1/0)
	PA13 - MOSI - AT91C_PA13_MOSI  (1/0)
	PA14 - SCK  - AT91C_PA14_SPCK  (1/0)
	
	Chip-Selects (available on different pins)
	PA11 - NPCS0 - AT91C_PA11_NPCS0 (1/0)
	PA31 - NPCS1 - AT91C_PA31_NPCS1 (1/0)
	PA09 - NPCS1 - AT91C_PA9_NPCS1  (1/0)
	...AT91C_PA3_NPCS3	(0/1)
	...AT91C_PA5_NPCS3	(0/1)
	...AT91C_PA10_NPCS2	(0/1)
	...AT91C_PA22_NPCS3	(0/1)
	...AT91C_PA30_NPCS2 (0/1)
*/

/* here: use NCPS0 @ PA11: */
#define NCPS_PDR_BIT     AT91C_PA11_NPCS0
#define NCPS_ASR_BIT     AT91C_PA11_NPCS0
#define NPCS_BSR_BIT     0
#define SPI_CSR_NUM      0          

#define SPI_SCBR_MIN     2


/* PCS_0 for NPCS0, PCS_1 for NPCS1 ... */
#define PCS_0 ((0<<0)|(1<<1)|(1<<2)|(1<<3))
#define PCS_1 ((1<<1)|(0<<1)|(1<<2)|(1<<3))
#define PCS_2 ((1<<1)|(1<<1)|(0<<2)|(1<<3))
#define PCS_3 ((1<<1)|(1<<1)|(1<<2)|(0<<3))
/* TODO: ## */
#if (SPI_CSR_NUM == 0)
#define SPI_MR_PCS       PCS_0
#elif (SPI_CSR_NUM == 1)
#define SPI_MR_PCS       PCS_1
#elif (SPI_CSR_NUM == 2)
#define SPI_MR_PCS       PCS_2
#elif (SPI_CSR_NUM == 3)
#define SPI_MR_PCS       PCS_3
#else
#error "SPI_CSR_NUM invalid"
// not realy - when using an external address decoder...
// but this code takes over the complete SPI-interace anyway
#endif

static volatile
DSTATUS Stat = STA_NOINIT;	/* Disk status */

static volatile
BYTE Timer;			/* 100Hz decrement timer */



/*-----------------------------------------------------------------------*/
/* Module Private Functions                                              */


/*--------------------------------*/
/* Transmit a byte to MMC via SPI */

//#define xmit_spi(dat) 	SPDR=(dat); loop_until_bit_is_set(SPSR,SPIF)

static
BYTE xmit_spi(BYTE dat)
{
	AT91PS_SPI pSPI      = AT91C_BASE_SPI;
	
	while( !( pSPI->SPI_SR & AT91C_SPI_TDRE ) ); // transfer compl. wait
	pSPI->SPI_TDR = dat;

	while( !( pSPI->SPI_SR & AT91C_SPI_RDRF ) ); // wait for char
	return (BYTE)( pSPI->SPI_RDR ); // it's important to read RDR here!
}

static
BYTE rcvr_spi()
{
	BYTE dat;
	AT91PS_SPI pSPI      = AT91C_BASE_SPI;
	
	while( !( pSPI->SPI_SR & AT91C_SPI_TDRE ) ); // transfer compl. wait
	pSPI->SPI_TDR = 0xFF;

	while( !( pSPI->SPI_SR & AT91C_SPI_RDRF ) ); // wait for char
	dat = (BYTE)( pSPI->SPI_RDR );

	//iprintf("r: %x\n", dat);

	return dat;
}

static
void rcvr_spi_m(BYTE *dest)
{
	BYTE dat;
	AT91PS_SPI pSPI      = AT91C_BASE_SPI;
	
	while( !( pSPI->SPI_SR & AT91C_SPI_TDRE ) ); // transfer compl. wait
	pSPI->SPI_TDR = 0xFF;

	while( !( pSPI->SPI_SR & AT91C_SPI_RDRF ) ); // wait for char
	dat = (BYTE)( pSPI->SPI_RDR );

	*dest = dat;
}

/*---------------------*/
/* Wait for card ready */

static
BYTE wait_ready ()
{
	BYTE res;


	Timer = 50;			/* Wait for ready in timeout of 500ms */
	rcvr_spi();
	do
		res = rcvr_spi();
	while ((res != 0xFF) && Timer);
	return res;
}



/*--------------------------------*/
/* Receive a data packet from MMC */

static
BOOL rcvr_datablock (
	BYTE *buff,			/* Data buffer to store received data */
	BYTE wc				/* Word count (0 means 256 words) */
)
{
	BYTE token;

#ifdef USE_DMA
	/* TODO: deuglyfy */
	static const BYTE dummy_ff_block[512] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#endif

	Timer = 10;
	do {							/* Wait for data packet in timeout of 100ms */
		token = rcvr_spi();
	} while ((token == 0xFF) && Timer);
	//iprintf("token: %x\n", token);
	if(token != 0xFE) return FALSE;	/* If not valid data token, retutn with error */

#ifdef USE_DMA
	// enable DMA transfer
	*AT91C_SPI_RPR = buff;
	*AT91C_SPI_RCR = 2 * (wc == 0 ? 256 : wc);
	*AT91C_SPI_TPR = dummy_ff_block;
	*AT91C_SPI_TCR = 2 * (wc == 0 ? 256 : wc);
	*AT91C_SPI_PTCR = AT91C_PDC_RXTEN;
	*AT91C_SPI_PTCR = AT91C_PDC_TXTEN;
	/*while(i++ < 512)
	*AT91C_SPI_TDR = 0xFF; // start transfer*/
	
	while(! (*AT91C_SPI_SR & AT91C_SPI_ENDRX));	
	*AT91C_SPI_PTCR = AT91C_PDC_RXTDIS;
	*AT91C_SPI_PTCR = AT91C_PDC_TXTDIS;
#else
	do {							/* Receive the data block into buffer */
		rcvr_spi_m(buff++);
		rcvr_spi_m(buff++);
	} while (--wc);
#endif
	
	rcvr_spi();						/* Discard CRC */
	rcvr_spi();

	//puts("rcvr_datablock success");
	return TRUE;					/* Return with success */
}



/*---------------------------*/
/* Send a data packet to MMC */

#ifndef _READONLY
static
BOOL xmit_datablock (
	const BYTE *buff,	/* 512 byte data block to be transmitted */
	BYTE token			/* Data/Stop token */
)
{
	BYTE resp, wc = 0;


	if (wait_ready() != 0xFF) return FALSE;

	xmit_spi(token);					/* Xmit data token */
	if (token != 0xFD) {	/* Is data token */
		do {							/* Xmit the 512 byte data block to MMC */
			xmit_spi(*buff++);
			xmit_spi(*buff++);
		} while (--wc);
		xmit_spi(0xFF);					/* CRC (Dummy) */
		xmit_spi(0xFF);
		resp = rcvr_spi();				/* Reveive data response */
		if ((resp & 0x1F) != 0x05)		/* If not accepted, return with error */
			return FALSE;
	}

	return TRUE;
}
#endif


/*------------------------------*/
/* Send a command packet to MMC */

static
BYTE send_cmd (
	BYTE cmd,		/* Command byte */
	DWORD arg		/* Argument */
)
{
	BYTE n, res;


	if (wait_ready() != 0xFF) return 0xFF;

	/* Send command packet */
	xmit_spi(cmd);						/* Command */
	xmit_spi((BYTE)(arg >> 24));		/* Argument[31..24] */
	xmit_spi((BYTE)(arg >> 16));		/* Argument[23..16] */
	xmit_spi((BYTE)(arg >> 8));			/* Argument[15..8] */
	xmit_spi((BYTE)arg);				/* Argument[7..0] */
	xmit_spi(0x95);						/* CRC (valid for only CMD0) */

	/* Receive command response */
	if (cmd == CMD12) rcvr_spi();		/* Skip a stuff byte when stop reading */
	n = 10;								/* Wait for a valid response in timeout of 10 attempts */
	do
		res = rcvr_spi();
	while ((res & 0x80) && --n);

	//iprintf("res: %i\n", res);

	return res;			/* Return with the response value */
}

static void if_spiSetSpeed(unsigned char speed)
{
	unsigned long reg;
	AT91PS_SPI pSPI      = AT91C_BASE_SPI;

	if ( speed < SPI_SCBR_MIN ) speed = SPI_SCBR_MIN;
	if ( speed > 1 ) speed &= 0xFE;

	reg = pSPI->SPI_CSR[SPI_CSR_NUM];
	reg = ( reg & ~(AT91C_SPI_SCBR) ) | ( (unsigned long)speed << 8 );
	pSPI->SPI_CSR[SPI_CSR_NUM] = reg;
}

static void init_spi()
{
	BYTE i;
	AT91PS_SPI pSPI      = AT91C_BASE_SPI;
	AT91PS_PIO pPIOA     = AT91C_BASE_PIOA;
	AT91PS_PMC pPMC      = AT91C_BASE_PMC;
	// AT91PS_PDC pPDC_SPI  = AT91C_BASE_PDC_SPI;

	// disable PIO from controlling MOSI, MISO, SCK (=hand over to SPI)
	// keep CS untouched - used as GPIO pin during init
	pPIOA->PIO_PDR = AT91C_PA12_MISO | AT91C_PA13_MOSI | AT91C_PA14_SPCK; //  | NCPS_PDR_BIT;
	// set pin-functions in PIO Controller
	pPIOA->PIO_ASR = AT91C_PA12_MISO | AT91C_PA13_MOSI | AT91C_PA14_SPCK; /// not here: | NCPS_ASR_BIT;
	/// not here: pPIOA->PIO_BSR = NPCS_BSR_BIT;

	// set chip-select as output high (unselect card)
	pPIOA->PIO_PER  = NPCS_BSR_BIT; // enable PIO of CS-pin
	pPIOA->PIO_SODR = NPCS_BSR_BIT; // set
	pPIOA->PIO_OER  = NPCS_BSR_BIT; // output

	// enable peripheral clock for SPI ( PID Bit 5 )
	pPMC->PMC_PCER = ( (WORD) 1 << AT91C_ID_SPI ); // n.b. IDs are just bit-numbers

	// SPI enable and reset
	pSPI->SPI_CR = AT91C_SPI_SPIEN | AT91C_SPI_SWRST;

	// SPI mode: master, fixed periph. sel., FDIV=0, fault detection disabled
	pSPI->SPI_MR  = AT91C_SPI_MSTR | AT91C_SPI_PS_FIXED | AT91C_SPI_MODFDIS;

	// set PCS for fixed select
	pSPI->SPI_MR &= 0xFFF0FFFF; // clear old PCS - redundant (AT91lib)
	pSPI->SPI_MR |= ( (SPI_MR_PCS<<16) & AT91C_SPI_PCS ); // set PCS

	// set chip-select-register
	// 8 bits per transfer, CPOL=1, ClockPhase=0, DLYBCT = 0
	// TODO: Why has CPOL to be active here and non-active on LPC2000?
	//       Take closer look on timing diagrams in datasheets.
	/*
	MMC reads data on the rising edge => CPOL=CPHA can't work
	=> possible settings: CPOL=0, CPHA=1; CPOL=1, CPHA=0

	*/
	//pSPI->SPI_CSR[SPI_CSR_NUM] = AT91C_SPI_CPOL | AT91C_SPI_BITS_8 | AT91C_SPI_NCPHA;
	//pSPI->SPI_CSR[SPI_CSR_NUM] = AT91C_SPI_BITS_8 | AT91C_SPI_NCPHA; 
	pSPI->SPI_CSR[SPI_CSR_NUM] = AT91C_SPI_CPOL | AT91C_SPI_BITS_8; // ok
	//pSPI->SPI_CSR[SPI_CSR_NUM] = AT91C_SPI_BITS_8;

	// slow during init
	if_spiSetSpeed(0xFE); 

	// enable
	pSPI->SPI_CR = AT91C_SPI_SPIEN;

	/* Send 20 spi commands with card not selected */
	for(i=0;i<21;i++)
		xmit_spi(0xFF);

	/* enable automatic chip-select */
	pPIOA->PIO_ODR  = NPCS_BSR_BIT; // input
	pPIOA->PIO_CODR = NPCS_BSR_BIT; // clear
	// disable PIO from controlling the CS pin (=hand over to SPI)
	pPIOA->PIO_PDR = NCPS_PDR_BIT;
	// set pin-functions in PIO Controller
	pPIOA->PIO_ASR = NCPS_ASR_BIT;
	pPIOA->PIO_BSR = NPCS_BSR_BIT;

}

/*-----------------------------------------------------------------------*/
/* Public Functions                                                      */


/*-----------------------*/
/* Initialize Disk Drive */

DSTATUS disk_initialize ()
{
	BYTE n;
	
	init_spi();

	Stat |= STA_NOINIT;
	if (!(Stat & STA_NODISK)) {
		#if 0
		n = 10;						/* Dummy clock */
		do
			rcvr_spi();
		while (--n);
		#endif
		SELECT();			/* CS = L */
		if (send_cmd(CMD0, 0) == 1) {			/* Enter Idle state */
			Timer = 100;						/* Wait for card ready in timeout of 1 sec */
			while (Timer && send_cmd(CMD1, 0));
			if (Timer) Stat &= ~STA_NOINIT;		/* When device goes ready, clear STA_NOINIT */
		}
		DESELECT();			/* CS = H */
		rcvr_spi();			/* Idle (Release DO) */
	}

	// max speed
	if_spiSetSpeed(0x00);

	return Stat;
}



/*----------------*/
/* Shutdown       */


DSTATUS disk_shutdown ()
{
	return 0;
}



/*--------------------*/
/* Return Disk Status */

DSTATUS disk_status ()
{
	return Stat;
}



/*----------------*/
/* Read Sector(s) */

DRESULT disk_read (
	BYTE *buff,			/* Data buffer to store read data */
	DWORD sector,		/* Sector number (LBA) */
	BYTE count			/* Sector count (1..255) */
)
{
	if (Stat & STA_NOINIT) return RES_NOTRDY;
	if (!count) return RES_PARERR;

	sector *= 512;		/* LBA --> byte address */

	SELECT();			/* CS = L */

	if (count == 1) {	/* Single block read */
		//puts("doing single block read");
		if ((send_cmd(CMD17, sector) == 0)	/* READ_SINGLE_BLOCK */
			&& rcvr_datablock(buff, (BYTE)(512/2)))
			count = 0;
	}
	else {				/* Multiple block read */
		//puts("doing multi-block read");
		if (send_cmd(CMD18, sector) == 0) {	/* READ_MULTIPLE_BLOCK */
			do {
				if (!rcvr_datablock(buff, (BYTE)(512/2))) break;
				buff += 512;
			} while (--count);
			send_cmd(CMD12, 0);				/* STOP_TRANSMISSION */
		}
	}

	DESELECT();			/* CS = H */
	rcvr_spi();			/* Idle (Release DO) */

	return count ? RES_ERROR : RES_OK;
}



/*-----------------*/
/* Write Sector(s) */

#ifndef _READONLY
DRESULT disk_write (
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector number (LBA) */
	BYTE count			/* Sector count (1..255) */
)
{
	if (Stat & STA_NOINIT) return RES_NOTRDY;
	if (Stat & STA_PROTECT) return RES_WRPRT;
	if (!count) return RES_PARERR;
	sector *= 512;		/* LBA --> byte address */

	SELECT();			/* CS = L */

	if (count == 1) {	/* Single block write */
		if ((send_cmd(CMD24, sector) == 0)	/* WRITE_BLOCK */
			&& xmit_datablock(buff, 0xFE))
			count = 0;
	}
	else {				/* Multiple block write */
		if (send_cmd(CMD25, sector) == 0) {	/* WRITE_MULTIPLE_BLOCK */
			do {
				if (!xmit_datablock(buff, 0xFC)) break;
				buff += 512;
			} while (--count);
			if (!xmit_datablock(0, 0xFD))	/* STOP_TRAN token */
				count = 1;
		}
	}

	DESELECT();			/* CS = H */
	rcvr_spi();			/* Idle (Release DO) */

	return count ? RES_ERROR : RES_OK;
}
#endif



/*--------------------------*/
/* Miscellaneous Functions  */

DRESULT disk_ioctl (
	BYTE ctrl,		/* Control code */
	void *buff		/* Buffer to send/receive data block */
)
{
	DRESULT res;
	BYTE n, csd[16], *ptr = buff;
	WORD csm, csize;


	if (Stat & STA_NOINIT) return RES_NOTRDY;

	SELECT();		/* CS = L */

	res = RES_ERROR;
	switch (ctrl) {
		case GET_SECTORS :	/* Get number of sectors on the disk (unsigned long) */
			if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16/2)) {
				/* Calculate disk size */
				csm = 1 << (((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2);
				csize = ((WORD)(csd[8] & 3) >> 6) + (WORD)(csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
				*(DWORD*)ptr = (DWORD)csize * csm;
				res = RES_OK;
			}
			break;

		case MMC_GET_CSD :	/* Receive CSD as a data block (16 bytes) */
			if ((send_cmd(CMD9, 0) == 0)	/* READ_CSD */
				&& rcvr_datablock(ptr, 16/2))
				res = RES_OK;
			break;

		case MMC_GET_CID :	/* Receive CID as a data block (16 bytes) */
			if ((send_cmd(CMD10, 0) == 0)	/* READ_CID */
				&& rcvr_datablock(ptr, 16/2))
				res = RES_OK;
			break;

		case MMC_GET_OCR :	/* Receive OCR as an R3 resp (4 bytes) */
			if (send_cmd(CMD58, 0) == 0) {	/* READ_OCR */
				for (n = 0; n < 4; n++)
					*ptr++ = rcvr_spi();
				res = RES_OK;
			}
			break;

		default:
			res = RES_PARERR;
	}

	DESELECT();			/* CS = H */
	rcvr_spi();			/* Idle (Release DO) */

	return res;
}



/*---------------------------------------*/
/* Device timer interrupt procedure      */
/* This must be called in period of 10ms */

void disk_timerproc ()
{
	BYTE n, s;


	n = Timer;						/* 100Hz decrement timer */
	if (n) Timer = --n;

}


/*---------------------------------------------------------*/
/* User Provided Timer Function for FatFs module           */
/*---------------------------------------------------------*/
/* This is a real time clock service to be called from     */
/* FatFs module. Any valid time must be returned even if   */
/* the system does not support a real time clock.          */


DWORD get_fattime ()
{

	return	((2006UL-1980) << 25)	// Year = 2006
			| (2UL << 21)			// Month = Feb
			| (9UL << 16)			// Day = 9
			| (22U << 11)			// Hour = 22
			| (30U << 5)			// Min = 30
			| (0U >> 1)				// Sec = 0
			;

}