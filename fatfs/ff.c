/*--------------------------------------------------------------------------/
/  FatFs - FAT file system module  R0.01                     (C)ChaN, 2006
/---------------------------------------------------------------------------/
/ FatFs module is an experimenal project to implement FAT file system to
/ cheap microcontrollers. This is opened for education, reserch and
/ development. You can use, modify and republish it for non-profit or profit
/ use without any limitation under your responsibility.
/---------------------------------------------------------------------------/
/  Feb 26, 2006  R0.00  Prototype
/  Apr 29, 2006  R0.01  First stable version
/---------------------------------------------------------------------------*/

#include <string.h>
#include "ff.h"			/* FatFs declarations */
#include "diskio.h"		/* Include file for user provided functions */


FATFS *FatFs;			/* Pointer to the file system object */


/*-------------------------------------------------------------------------
  Module Private Functions
-------------------------------------------------------------------------*/

/*----------------------*/
/* Change Window Offset */

static
BOOL move_window (
	DWORD sector		/* Sector number to make apperance in the FatFs->win */
)						/* Move to zero only writes back dirty window */
{
	DWORD wsect;
	FATFS *fs = FatFs;


	wsect = fs->winsect;
	if (wsect != sector) {	/* Changed current window */
#ifndef _FS_READONLY
		BYTE n;
		if (fs->winflag) {	/* Write back dirty window if needed */
			if (disk_write(fs->win, wsect, 1) != RES_OK) return FALSE;
			fs->winflag = 0;
			if (wsect < (fs->fatbase + fs->sects_fat)) {	/* In FAT area */
				for (n = fs->n_fats; n >= 2; n--) {	/* Refrect the change to all FAT copies */
					wsect += fs->sects_fat;
					if (disk_write(fs->win, wsect, 1) != RES_OK) break;
				}
			}
		}
#endif
		if (sector) {
			if (disk_read(fs->win, sector, 1) != RES_OK) return FALSE;
			fs->winsect = sector;
		}
	}
	return TRUE;
}



/*---------------------*/
/* Get Cluster State   */

static
DWORD get_cluster (
	DWORD clust			/* Cluster# to get the link information */
)
{
	FATFS *fs = FatFs;


	if ((clust >= 2) && (clust < fs->max_clust)) {		/* Valid cluster# */
		switch (fs->fs_type) {
		case FS_FAT16 :
			if (!move_window(clust / 256 + fs->fatbase)) break;
			return LD_WORD(&(fs->win[((WORD)clust * 2) & 511]));
		case FS_FAT32 :
			if (!move_window(clust / 128 + fs->fatbase)) break;
			return LD_DWORD(&(fs->win[((WORD)clust * 4) & 511]));
		}
	}
	return 1;	/* Return with 1 means failed */
}



/*--------------------------*/
/* Change a Cluster State   */

#ifndef _FS_READONLY
static
BOOL put_cluster (
	DWORD clust,		/* Cluster# to change */
	DWORD val			/* New value to mark the cluster */
)
{
	FATFS *fs = FatFs;


	switch (fs->fs_type) {
	case FS_FAT16 :
		if (!move_window(clust / 256 + fs->fatbase)) return FALSE;
		ST_WORD(&(fs->win[((WORD)clust * 2) & 511]), (WORD)val);
		break;
	case FS_FAT32 :
		if (!move_window(clust / 128 + fs->fatbase)) return FALSE;
		ST_DWORD(&(fs->win[((WORD)clust * 4) & 511]), val);
		break;
	default :
		return FALSE;
	}
	fs->winflag = 1;
	return TRUE;
}
#endif



/*------------------------*/
/* Remove a Cluster Chain */

#ifndef _FS_READONLY
static
BOOL remove_chain (
	DWORD clust			/* Cluster# to remove chain from */
)
{
	DWORD nxt;


	while ((nxt = get_cluster(clust)) >= 2) {
		if (!put_cluster(clust, 0)) return FALSE;
		clust = nxt;
	}
	return TRUE;
}
#endif



/*-----------------------------------*/
/* Stretch or Create a Cluster Chain */

#ifndef _FS_READONLY
static
DWORD create_chain (
	DWORD clust			 /* Cluster# to stretch, 0 means create new */
)
{
	DWORD ncl, ccl, mcl = FatFs->max_clust;


	if (clust == 0) {	/* Create new chain */
		ncl = 1;
		do {
			ncl++;						/* Check next cluster */
			if (ncl >= mcl) return 0;	/* No free custer was found */
			ccl = get_cluster(ncl);		/* Get the cluster status */
			if (ccl == 1) return 0;		/* Any error occured */
		} while (ccl);				/* Repeat until find a free cluster */
	}
	else {				/* Stretch existing chain */
		ncl = get_cluster(clust);	/* Check the cluster status */
		if (ncl < 2) return 0;		/* It is an invalid cluster */
		if (ncl < mcl) return ncl;	/* It is already followed by next cluster */
		ncl = clust;				/* Search free cluster */
		do {
			ncl++;						/* Check next cluster */
			if (ncl >= mcl) ncl = 2;	/* Wrap around */
			if (ncl == clust) return 0;	/* No free custer was found */
			ccl = get_cluster(ncl);		/* Get the cluster status */
			if (ccl == 1) return 0;		/* Any error occured */
		} while (ccl);				/* Repeat until find a free cluster */
	}

	if (!put_cluster(ncl, 0xFFFFFFFF)) return 0;		/* Mark the new cluster "in use" */
	if (clust && !put_cluster(clust, ncl)) return 0;	/* Link it to previous one if needed */

	return ncl;		/* Return new cluster number */
}
#endif



/*----------------------------*/
/* Get Sector# from Cluster#  */

static
DWORD clust2sect (
	DWORD clust		/* Cluster# to be converted */
)
{
	FATFS *fs = FatFs;


	clust -= 2;
	if (clust >= fs->max_clust) return 0;		/* Invalid cluster# */
	return clust * fs->sects_clust + fs->database;
}



/*------------------------*/
/* Check File System Type */

static
BYTE check_fs (
	DWORD sect		/* Sector# to check if it is a FAT boot record or not */
)
{
	static const char fatsign[] = "FAT16FAT32";
	FATFS *fs = FatFs;


	memset(fs->win, 0, 512);
	if (disk_read(fs->win, sect, 1) == RES_OK) {	/* Load boot record */
		if (LD_WORD(&(fs->win[510])) == 0xAA55) {		/* Is it valid? */
			if (!memcmp(&(fs->win[0x36]), &fatsign[0], 5))
				return FS_FAT16;
			if (!memcmp(&(fs->win[0x52]), &fatsign[5], 5) && (fs->win[0x28] == 0))
				return FS_FAT32;
		}
	}
	return 0;
}



/*--------------------------------*/
/* Move Directory Pointer to Next */

static
BOOL next_dir_entry (
	DIR *scan			/* Pointer to directory object */
)
{
	DWORD clust;
	WORD idx;
	FATFS *fs = FatFs;


	idx = scan->index + 1;
	if ((idx & 15) == 0) {		/* Table sector changed? */
		scan->sect++;			/* Next sector */
		if (!scan->clust) {		/* In static table */
			if (idx >= fs->n_rootdir) return FALSE;	/* Reached to end of table */
		} else {				/* In dynamic table */
			if (((idx / 16) & (fs->sects_clust - 1)) == 0) {	/* Cluster changed? */
				clust = get_cluster(scan->clust);		/* Get next cluster */
				if ((clust >= fs->max_clust) || (clust < 2))	/* Reached to end of table */
					return FALSE;
				scan->clust = clust;				/* Initialize for new cluster */
				scan->sect = clust2sect(clust);
			}
		}
	}
	scan->index = idx;	/* Lower 4 bit of scan->index indicates offset in scan->sect */
	return TRUE;
}



/*--------------------------------------*/
/* Get File Status from Directory Entry */

static
void get_fileinfo (
	FILINFO *finfo, 	/* Ptr to Store the File Information */
	const BYTE *dir		/* Ptr to the Directory Entry */
)
{
	BYTE n, c, a;
	char *p;


	p = &(finfo->fname[0]);
	a = *(dir+12);	/* NT flag */
	for (n = 0; n < 8; n++) {	/* Convert file name (body) */
		c = *(dir+n);
		if (c == ' ') break;
		if (c == 0x05) c = 0xE5;
		if ((a & 0x08) && (c >= 'A') && (c <= 'Z')) c += 0x20;
		*p++ = c;
	}
	if (*(dir+8) != ' ') {		/* Convert file name (extension) */
		*p++ = '.';
		for (n = 8; n < 11; n++) {
			c = *(dir+n);
			if (c == ' ') break;
			if ((a & 0x10) && (c >= 'A') && (c <= 'Z')) c += 0x20;
			*p++ = c;
		}
	}
	*p = '\0';

	finfo->fattrib = *(dir+11);			/* Attribute */
	finfo->fsize = LD_DWORD(dir+28);	/* Size */
	finfo->fdate = LD_WORD(dir+24);		/* Date */
	finfo->ftime = LD_WORD(dir+22);		/* Time */
}



/*-------------------------------------------------------------------*/
/* Pick a Paragraph and Create the Name in Format of Directory Entry */

static
char make_dirfile (
	const char **path,		/* Pointer to the file path pointer */
	char *dirname			/* Pointer to directory name buffer {Name(8), Ext(3), NT flag(1)} */
)
{
	BYTE n, t, c, a, b;


	memset(dirname, ' ', 8+3);	/* Fill buffer with spaces */
	a = 0; b = 0x18;	/* NT flag */
	n = 0; t = 8;
	for (;;) {
		c = *(*path)++;
		if (c <= ' ') c = 0;
		if ((c == 0) || (c == '/')) {			/* Reached to end of str or directory separator */
			if (n == 0) break;
			dirname[11] = a & b; return c;
		}
		if (c == '.') {
			if(!(a & 1) && (n >= 1) && (n <= 8)) {	/* Enter extension part */
				n = 8; t = 11; continue;
			}
			break;
		}
#ifdef _USE_SJIS
		if (((c >= 0x81) && (c <= 0x9F)) ||		/* Accept S-JIS code */
		    ((c >= 0xE0) && (c <= 0xFC))) {
			if ((n == 0) && (c == 0xE5))		/* Change heading \xE5 to \x05 */
				c = 0x05;
			a ^= 1; goto md_l2;
		}
		if ((c >= 0x7F) && (c <= 0x80)) break;	/* Reject \x7F \x80 */
#else
		if (c >= 0x7F) goto md_l1;				/* Accept \x7F-0xFF */
#endif
		if (c == '"') break;					/* Reject " */
		if (c <= ')') goto md_l1;				/* Accept ! # $ % & ' ( ) */
		if (c <= ',') break;					/* Reject * + , */
		if (c <= '9') goto md_l1;				/* Accept - 0-9 */
		if (c <= '?') break;					/* Reject : ; < = > ? */
		if (!(a & 1)) {	/* These checks are not applied to S-JIS 2nd byte */
			if (c == '|') break;				/* Reject | */
			if ((c >= '[') && (c <= ']')) break;/* Reject [ \ ] */
			if ((c >= 'A') && (c <= 'Z'))
				(t == 8) ? (b &= ~0x08) : (b &= ~0x10);
			if ((c >= 'a') && (c <= 'z')) {		/* Convert to upper case */
				c -= 0x20;
				(t == 8) ? (a |= 0x08) : (a |= 0x10);
			}
		}
	md_l1:
		a &= ~1;
	md_l2:
		if (n >= t) break;
		dirname[n++] = c;
	}
	return 1;
}



/*-------------------*/
/* Trace a File Path */

static
FRESULT trace_path (
	DIR *scan,			/* Pointer to directory object to return last directory */
	char *fn,			/* Pointer to last segment name to return */
	const char *path,	/* Full-path string to trace a file or directory */
	BYTE **dir			/* Directory pointer in Win[] to retutn */
)
{
	DWORD clust;
	char ds;
	BYTE *dptr = NULL;
	FATFS *fs = FatFs;

	/* Initialize directory object */
	clust = fs->dirbase;
	if (fs->fs_type == FS_FAT32) {
		scan->clust = scan->sclust = clust;
		scan->sect = clust2sect(clust);
	} else {
		scan->clust = scan->sclust = 0;
		scan->sect = clust;
	}
	scan->index = 0;

	while ((*path == ' ') || (*path == '/')) path++;	/* Skip leading spaces */
	if ((BYTE)*path < ' ') {							/* Null path means the root directory */
		*dir = NULL; return FR_OK;
	}

	for (;;) {
		ds = make_dirfile(&path, fn);			/* Get a paragraph into fn[] */
		if (ds == 1) return FR_INVALID_NAME;
		for (;;) {
			if (!move_window(scan->sect)) return FR_RW_ERROR;
			dptr = &(fs->win[(scan->index & 15) * 32]);	/* Pointer to the directory entry */
			if (*dptr == 0)								/* Has it reached to end of dir? */
				return !ds ? FR_NO_FILE : FR_NO_PATH;
			if (    (*dptr != 0xE5)						/* Matched? */
				&& !(*(dptr+11) & AM_VOL)
				&& !memcmp(dptr, fn, 8+3) ) break;
			if (!next_dir_entry(scan))					/* Next directory pointer */
				return !ds ? FR_NO_FILE : FR_NO_PATH;
		}
		if (!ds) { *dir = dptr; return FR_OK; }			/* Matched with end of path */
		if (!(*(dptr+11) & AM_DIR)) return FR_NO_PATH;	/* Cannot trace because it is a file */
		clust = ((DWORD)LD_WORD(dptr+20) << 16) | LD_WORD(dptr+26); /* Get cluster# of the directory */
		scan->clust = scan->sclust = clust;				/* Restart scan with the new directory */
		scan->sect = clust2sect(clust);
		scan->index = 0;
	}
}



/*---------------------------*/
/* Reserve a Directory Entry */

#ifndef _FS_READONLY
static
BYTE* reserve_direntry (
	DIR *scan			/* Target directory to create new entry */
)
{
	DWORD clust, sector;
	BYTE c, n, *dptr;
	FATFS *fs = FatFs;


	/* Re-initialize directory object */
	clust = scan->sclust;
	if (clust) {	/* Dyanmic directory table */
		scan->clust = clust;
		scan->sect = clust2sect(clust);
	} else {		/* Static directory table */
		scan->sect = fs->dirbase;
	}
	scan->index = 0;

	do {
		if (!move_window(scan->sect)) return NULL;
		dptr = &(fs->win[(scan->index & 15) * 32]);		/* Pointer to the directory entry */
		c = *dptr;
		if ((c == 0) || (c == 0xE5)) return dptr;		/* Found an empty entry! */
	} while (next_dir_entry(scan));						/* Next directory pointer */
	/* Reached to end of the directory table */

	/* Abort when static table or could not stretch dynamic table */
	if ((!clust) || !(clust = create_chain(scan->clust))) return NULL;
	if (!move_window(0)) return 0;

	fs->winsect = sector = clust2sect(clust);			/* Cleanup the expanded table */
	memset(fs->win, 0, 512);
	for (n = fs->sects_clust; n; n--) {
		if (disk_write(fs->win, sector, 1) != RES_OK) return NULL;
		sector++;
	}
	fs->winflag = 1;
	return fs->win;
}
#endif



/*-----------------------------------------*/
/* Make Sure that the File System is Valid */

static
FRESULT check_mounted ()
{
	FATFS *fs = FatFs;


	if (!fs) return FR_NOT_ENABLED;		/* Has the FatFs been enabled? */

	if (disk_status() & STA_NOINIT) {	/* The drive has not been initialized */
		if (fs->files)					/* Drive was uninitialized with any file left opend */
			return FR_INCORRECT_DISK_CHANGE;
		else
			return f_mountdrv();;		/* Initialize file system and return resulut */
	} else {							/* The drive has been initialized */
		if (!fs->fs_type)				/* But the file system has not been initialized */
			return f_mountdrv();		/* Initialize file system and return resulut */
	}
	return FR_OK;						/* File system is valid */
}





/*--------------------------------------------------------------------------*/
/* Public Funciotns                                                         */
/*--------------------------------------------------------------------------*/


/*----------------------------------------------------------*/
/* Load File System Information and Initialize FatFs Module */

FRESULT f_mountdrv ()
{
	BYTE fat;
	DWORD sect, fatend;
	FATFS *fs = FatFs;

	if (!fs) return FR_NOT_ENABLED;

	/* Initialize file system object */
	memset(fs, 0, sizeof(FATFS));

	/* Initialize disk drive */
	if (disk_initialize() & STA_NOINIT)	return FR_NOT_READY;

	/* Search FAT partition */
	fat = check_fs(sect = 0);		/* Check sector 0 as an SFD format */

	if (!fat) {						/* Not a FAT boot record, it will be an FDISK format */
		/* Check a pri-partition listed in top of the partition table */
		if (fs->win[0x1C2]) {					/* Is the partition existing? */
			sect = LD_DWORD(&(fs->win[0x1C6]));	/* Partition offset in LBA */
			fat = check_fs(sect);				/* Check the partition */
		}
	}
	if (!fat) return FR_NO_FILESYSTEM;	/* No FAT patition */

	/* Initialize file system object */
	fs->fs_type = fat;								/* FAT type */
	fs->sects_fat = 								/* Sectors per FAT */
		(fat == FS_FAT32) ? LD_DWORD(&(fs->win[0x24])) : LD_WORD(&(fs->win[0x16]));
	fs->sects_clust = fs->win[0x0D];				/* Sectors per cluster */
	fs->n_fats = fs->win[0x10];						/* Number of FAT copies */
	fs->fatbase = sect + LD_WORD(&(fs->win[0x0E]));	/* FAT start sector (physical) */
	fs->n_rootdir = LD_WORD(&(fs->win[0x11]));		/* Nmuber of root directory entries */

	fatend = fs->sects_fat * fs->n_fats + fs->fatbase;
	if (fat == FS_FAT32) {
		fs->dirbase = LD_DWORD(&(fs->win[0x2C]));	/* Directory start cluster */
		fs->database = fatend;	 					/* Data start sector (physical) */
	} else {
		fs->dirbase = fatend;						/* Directory start sector (physical) */
		fs->database = fs->n_rootdir / 16 + fatend;	/* Data start sector (physical) */
	}
	fs->max_clust = 								/* Maximum cluster number */
		(LD_DWORD(&(fs->win[0x20])) - fs->database + sect) / fs->sects_clust + 2;

	return FR_OK;
}



/*-----------------------------*/
/* Get Number of Free Clusters */

FRESULT f_getfree (
	DWORD *nclust		/* Pointer to the double word to return number of free clusters */
)
{
	DWORD n, clust, sect;
	BYTE m, *ptr, fat;
	FRESULT res;
	FATFS *fs = FatFs;


	if ((res = check_mounted()) != FR_OK) return res;
	fat = fs->fs_type;

	/* Count number of free clusters */
	n = m = clust = 0;
	ptr = NULL;
	sect = fs->fatbase;
	do {
		if (m == 0) {
			if (!move_window(sect++)) return FR_RW_ERROR;
			ptr = fs->win;
		}
		if (fat == FS_FAT32) {
			if (LD_DWORD(ptr) == 0) n++;
			ptr += 4; m += 2;
		} else {
			if (LD_WORD(ptr) == 0) n++;
			ptr += 2; m++;
		}
		clust++;
	} while (clust < fs->max_clust);

	*nclust = n;
	return FR_OK;
}



/*-----------------------*/
/* Open or Create a File */

FRESULT f_open (
	FIL *fp,			/* Pointer to the buffer of new file object to create */
	const char *path,	/* Pointer to the file path */
	BYTE mode			/* Access mode and file open mode flags */
)
{
	FRESULT res;
	BYTE *dir;
	DIR dirscan;
	char fn[8+3+1];
	FATFS *fs = FatFs;


	if ((res = check_mounted()) != FR_OK) return res;
#ifndef _FS_READONLY
	if ((mode & (FA_WRITE|FA_CREATE_ALWAYS)) && (disk_status() & STA_PROTECT))
		return FR_WRITE_PROTECTED;
#endif

	res = trace_path(&dirscan, fn, path, &dir);	/* Trace the file path */

#ifndef _FS_READONLY
	/* Create or Open a File */
	if (mode & (FA_CREATE_ALWAYS|FA_OPEN_ALWAYS)) {
		DWORD dw;
		if (res != FR_OK) {		/* No file, create new */
			mode |= FA_CREATE_ALWAYS;
			if (res != FR_NO_FILE) return res;
			dir = reserve_direntry(&dirscan);	/* Reserve a directory entry */
			if (dir == NULL) return FR_DENIED;
			memcpy(dir, fn, 8+3);		/* Initialize the new entry */
			*(dir+12) = fn[11];
			memset(dir+13, 0, 32-13);
		} else {				/* File already exists */
			if ((dir == NULL) || (*(dir+11) & (AM_RDO|AM_DIR)))	/* Could not overwrite (R/O or DIR) */
				return FR_DENIED;
			if (mode & FA_CREATE_ALWAYS) {	/* Resize it to zero */
				dw = fs->winsect;			/* Remove the cluster chain */
				if (!remove_chain(((DWORD)LD_WORD(dir+20) << 16) | LD_WORD(dir+26))
					|| !move_window(dw) )
					return FR_RW_ERROR;
				ST_WORD(dir+20, 0); ST_WORD(dir+26, 0);	/* cluster = 0 */
				ST_DWORD(dir+28, 0);					/* size = 0 */
			}
		}
		if (mode & FA_CREATE_ALWAYS) {
			*(dir+11) = AM_ARC;
			dw = get_fattime();
			ST_DWORD(dir+14, dw);	/* Created time */
			ST_DWORD(dir+22, dw);	/* Updated time */
			fs->winflag = 1;
		}
	}
	/* Open a File */
	else {
#endif
		if (res != FR_OK) return res;		/* Trace failed */
		if ((dir == NULL) || (*(dir+11) & AM_DIR))	/* It is a directory */
			return FR_NO_FILE;
#ifndef _FS_READONLY
		if ((mode & FA_WRITE) && (*(dir+11) & AM_RDO)) /* R/O violation */
			return FR_DENIED;
	}
#endif

#ifdef _FS_READONLY
	fp->flag = mode & (FA_UNBUFFERED|FA_READ);
#else
	fp->flag = mode & (FA_UNBUFFERED|FA_WRITE|FA_READ);
	fp->dir_sect = fs->winsect;				/* Pointer to the directory entry */
	fp->dir_ptr = dir;
#endif
	fp->org_clust =	((DWORD)LD_WORD(dir+20) << 16) | LD_WORD(dir+26);	/* File start cluster */
	fp->fsize = LD_DWORD(dir+28);		/* File size */
	fp->fptr = 0;						/* File ptr */
	fp->sect_clust = 1;					/* Sector counter */
	fs->files++;
	return FR_OK;
}



/*-----------*/
/* Read File */

FRESULT f_read (
	FIL *fp, 		/* Pointer to the file object */
	BYTE *buff,		/* Pointer to data buffer */
	WORD btr,		/* Number of bytes to read */
	WORD *br		/* Pointer to number of bytes read */
)
{
	DWORD clust, sect, ln;
	WORD rcnt;
	BYTE cc;
	FATFS *fs = FatFs;


	*br = 0;
	if (!fs) return FR_NOT_ENABLED;
	if ((disk_status() & STA_NOINIT) || !fs->fs_type) return FR_NOT_READY;	/* Check disk ready */
	if (fp->flag & FA__ERROR) {puts("error line 707"); return FR_RW_ERROR;}	/* Check error flag */
	if (!(fp->flag & FA_READ)) return FR_DENIED;	/* Check access mode */
	ln = fp->fsize - fp->fptr;
	if (btr > ln) btr = ln;							/* Truncate read count by number of bytes left */

	for ( ;  btr;									/* Repeat until all data transferred */
		buff += rcnt, fp->fptr += rcnt, *br += rcnt, btr -= rcnt) {
		if ((fp->fptr & 511) == 0) {				/* On the sector boundary */
			if (--(fp->sect_clust)) {				/* Decrement sector counter */
				sect = fp->curr_sect + 1;			/* Next sector */
			} else {								/* Next cluster */
				clust = (fp->fptr == 0) ? fp->org_clust : get_cluster(fp->curr_clust);
				if ((clust < 2) || (clust >= fs->max_clust)) {iprintf("error line 719: clust=%lu\n", clust); goto fr_error;}
				fp->curr_clust = clust;				/* Current cluster */
				sect = clust2sect(clust);			/* Current sector */
				fp->sect_clust = fs->sects_clust;	/* Re-initialize the sector counter */
			}
#ifndef _FS_READONLY
			if (fp->flag & FA__DIRTY) {				/* Flush file I/O buffer if needed */
				if (disk_write(fp->buffer, fp->curr_sect, 1) != RES_OK) {puts("error line 726"); goto fr_error;}
				fp->flag &= ~FA__DIRTY;
			}
#endif
			fp->curr_sect = sect;					/* Update current sector */
			cc = btr / 512;							/* When left bytes >= 512 */
			if (cc) {								/* Read maximum contiguous sectors */
				if (cc > fp->sect_clust) cc = fp->sect_clust;
				if (disk_read(buff, sect, cc) != RES_OK) {puts("error line 734"); goto fr_error;}
				fp->sect_clust -= cc - 1;
				fp->curr_sect += cc - 1;
				rcnt = cc * 512; continue;
			}
			if (fp->flag & FA_UNBUFFERED)			/* Reject unaligned access when unbuffered mode */
				return FR_ALIGN_ERROR;
			if (disk_read(fp->buffer, sect, 1) != RES_OK)	/* Load the sector into file I/O buffer */
				{puts("error line 742"); goto fr_error;}
		}
		rcnt = 512 - (fp->fptr & 511);				/* Copy fractional bytes from file I/O buffer */
		if (rcnt > btr) rcnt = btr;
		memcpy(buff, &fp->buffer[fp->fptr & 511], rcnt);
	}

	return FR_OK;

fr_error:	/* Abort this file due to an unrecoverable error */
	fp->flag |= FA__ERROR;
	return FR_RW_ERROR;
}



/*------------*/
/* Write File */

#ifndef _FS_READONLY
FRESULT f_write (
	FIL *fp,			/* Pointer to the file object */
	const BYTE *buff,	/* Pointer to the data to be written */
	WORD btw,			/* Number of bytes to write */
	WORD *bw			/* Pointer to number of bytes written */
)
{
	DWORD clust, sect;
	WORD wcnt;
	BYTE cc;
	FATFS *fs = FatFs;


	*bw = 0;
	if (!fs) return FR_NOT_ENABLED;
	if ((disk_status() & STA_NOINIT) || !fs->fs_type) return FR_NOT_READY;
	if (fp->flag & FA__ERROR) return FR_RW_ERROR;	/* Check error flag */
	if (!(fp->flag & FA_WRITE)) return FR_DENIED;	/* Check access mode */
	if (fp->fsize + btw < fp->fsize) btw = 0;		/* File size cannot reach 4GB */

	for ( ;  btw;									/* Repeat until all data transferred */
		buff += wcnt, fp->fptr += wcnt, *bw += wcnt, btw -= wcnt) {
		if ((fp->fptr & 511) == 0) {				/* On the sector boundary */
			if (--(fp->sect_clust)) {				/* Decrement sector counter */
				sect = fp->curr_sect + 1;			/* Next sector */
			} else {								/* Next cluster */
				if (fp->fptr == 0) {				/* Top of the file */
					clust = fp->org_clust;
					if (clust == 0)					/* No cluster is created */
						fp->org_clust = clust = create_chain(0);	/* Create a new cluster chain */
				} else {							/* Middle or end of file */
					clust = create_chain(fp->curr_clust);			/* Trace or streach cluster chain */
				}
				if ((clust < 2) || (clust >= fs->max_clust)) break;
				fp->curr_clust = clust;				/* Current cluster */
				sect = clust2sect(clust);			/* Current sector */
				fp->sect_clust = fs->sects_clust;	/* Re-initialize the sector counter */
			}
			if (fp->flag & FA__DIRTY) {				/* Flush file I/O buffer if needed */
				if (disk_write(fp->buffer, fp->curr_sect, 1) != RES_OK) goto fw_error;
				fp->flag &= ~FA__DIRTY;
			}
			fp->curr_sect = sect;					/* Update current sector */
			cc = btw / 512;							/* When left bytes >= 512 */
			if (cc) {								/* Write maximum contiguous sectors */
				if (cc > fp->sect_clust) cc = fp->sect_clust;
				if (disk_write(buff, sect, cc) != RES_OK) goto fw_error;
				fp->sect_clust -= cc - 1;
				fp->curr_sect += cc - 1;
				wcnt = cc * 512; continue;
			}
			if (fp->flag & FA_UNBUFFERED)			/* Reject unalighend access when unbuffered mode */
				return FR_ALIGN_ERROR;
			if ((fp->fptr < fp->fsize) &&  			/* Fill sector buffer with file data if needed */
				(disk_read(fp->buffer, sect, 1) != RES_OK))
					goto fw_error;
		}
		wcnt = 512 - (fp->fptr & 511);				/* Copy fractional bytes to file I/O buffer */
		if (wcnt > btw) wcnt = btw;
		memcpy(&fp->buffer[fp->fptr & 511], buff, wcnt);
		fp->flag |= FA__DIRTY;
	}

	if (fp->fptr > fp->fsize) fp->fsize = fp->fptr;	/* Update file size if needed */
	fp->flag |= FA__WRITTEN;						/* Set file changed flag */
	return FR_OK;

fw_error:	/* Abort this file due to an unrecoverable error */
	fp->flag |= FA__ERROR;
	return FR_RW_ERROR;
}
#endif



/*-------------------*/
/* Seek File Pointer */

FRESULT f_lseek (
	FIL *fp,		/* Pointer to the file object */
	DWORD ofs		/* File pointer from top of file */
)
{
	DWORD clust;
	BYTE sc;
	FATFS *fs = FatFs;


	if (!fs) return FR_NOT_ENABLED;
	if ((disk_status() & STA_NOINIT) || !fs->fs_type) return FR_NOT_READY;
	if (fp->flag & FA__ERROR) return FR_RW_ERROR;
#ifndef _FS_READONLY
	if (fp->flag & FA__DIRTY) {			/* Write-back dirty buffer if needed */
		if (disk_write(fp->buffer, fp->curr_sect, 1) != RES_OK) goto fk_error;
		fp->flag &= ~FA__DIRTY;
	}
#endif
	if (ofs > fp->fsize) ofs = fp->fsize;	/* Clip offset by file size */
	if ((ofs & 511) && (fp->flag & FA_UNBUFFERED)) return FR_ALIGN_ERROR;
	fp->fptr = ofs; fp->sect_clust = 1; 	/* Re-initialize file pointer */

	/* Seek file pinter if needed */
	if (ofs) {
		ofs = (ofs - 1) / 512;				/* Calcurate current sector */
		sc = fs->sects_clust;				/* Number of sectors in a cluster */
		fp->sect_clust = sc - (ofs % sc);	/* Calcurate sector counter */
		ofs /= sc;							/* Number of clusters to skip */
		clust = fp->org_clust;				/* Seek to current cluster */
		while (ofs--)
			clust = get_cluster(clust);
		if ((clust < 2) || (clust >= fs->max_clust)) goto fk_error;
		fp->curr_clust = clust;
		fp->curr_sect = clust2sect(clust) + sc - fp->sect_clust;	/* Current sector */
		if (fp->fptr & 511) {										/* Load currnet sector if needed */
			if (disk_read(fp->buffer, fp->curr_sect, 1) != RES_OK)
				goto fk_error;
		}
	}

	return FR_OK;

fk_error:	/* Abort this file due to an unrecoverable error */
	fp->flag |= FA__ERROR;
	return FR_RW_ERROR;
}



/*-------------------------------------------------*/
/* Synchronize between File and Disk without Close */

#ifndef _FS_READONLY
FRESULT f_sync (
	FIL *fp		/* Pointer to the file object */
)
{
	BYTE *ptr;
	FATFS *fs = FatFs;


	if (!fs) return FR_NOT_ENABLED;
	if ((disk_status() & STA_NOINIT) || !fs->fs_type)
		return FR_INCORRECT_DISK_CHANGE;

	/* Has the file been written? */
	if (fp->flag & FA__WRITTEN) {
		/* Write back data buffer if needed */
		if (fp->flag & FA__DIRTY) {
			if (disk_write(fp->buffer, fp->curr_sect, 1) != RES_OK) return FR_RW_ERROR;
			fp->flag &= ~FA__DIRTY;
		}
		/* Update the directory entry */
		if (!move_window(fp->dir_sect)) return FR_RW_ERROR;
		ptr = fp->dir_ptr;
		*(ptr+11) |= AM_ARC;					/* Set archive bit */
		ST_DWORD(ptr+28, fp->fsize);			/* Update file size */
		ST_WORD(ptr+26, fp->org_clust);			/* Update start cluster */
		ST_WORD(ptr+20, fp->org_clust >> 16);
		ST_DWORD(ptr+22, get_fattime());		/* Updated time */
		fs->winflag = 1;
		fp->flag &= ~FA__WRITTEN;
	}
	if (!move_window(0)) return FR_RW_ERROR;

	return FR_OK;
}
#endif



/*------------*/
/* Close File */

FRESULT f_close (
	FIL *fp		/* Pointer to the file object to be closed */
)
{
	FRESULT res;


#ifndef _FS_READONLY
	res = f_sync(fp);
#else
	res = FR_OK;
#endif
	if (res == FR_OK) {
		fp->flag = 0;
		FatFs->files--;
	}
	return res;
}



/*------------------------------*/
/* Delete a File or a Directory */

#ifndef _FS_READONLY
FRESULT f_unlink (
	const char *path			/* Pointer to the file or directory path */
)
{
	FRESULT res;
	BYTE *dir, *sdir;
	DWORD dclust, dsect;
	DIR dirscan;
	char fn[8+3+1];
	FATFS *fs = FatFs;


	if ((res = check_mounted()) != FR_OK) return res;
	if (disk_status() & STA_PROTECT) return FR_WRITE_PROTECTED;

	res = trace_path(&dirscan, fn, path, &dir);	/* Trace the file path */

	if (res != FR_OK) return res;				/* Trace failed */
	if (dir == NULL) return FR_NO_FILE;			/* It is a root directory */
	if (*(dir+11) & AM_RDO) return FR_DENIED;	/* It is a R/O item */
	dsect = fs->winsect;
	dclust = ((DWORD)LD_WORD(dir+20) << 16) | LD_WORD(dir+26);

	if (*(dir+11) & AM_DIR) {					/* It is a sub-directory */
		dirscan.clust = dclust;					/* Check if the sub-dir is empty or not */
		dirscan.sect = clust2sect(dclust);
		dirscan.index = 0;
		do {
			if (!move_window(dirscan.sect)) return FR_RW_ERROR;
			sdir = &(fs->win[(dirscan.index & 15) * 32]);
			if (*sdir == 0) break;
			if (!((*sdir == 0xE5) || (*sdir == '.')) && !(*(sdir+11) & AM_VOL))
				return FR_DENIED;	/* The directory is not empty */
		} while (next_dir_entry(&dirscan));
	}

	if (!remove_chain(dclust)) return FR_RW_ERROR;	/* Remove the cluster chain */
	if (!move_window(dsect)) return FR_RW_ERROR;	/* Mark the directory entry deleted */
	*dir = 0xE5; fs->winflag = 1;
	if (!move_window(0)) return FR_RW_ERROR;

	return FR_OK;
}
#endif



/*--------------------*/
/* Create a Directory */

#ifndef _FS_READONLY
FRESULT f_mkdir (
	const char *path		/* Pointer to the directory path */
)
{
	FRESULT res;
	BYTE *dir, *w, n;
	DWORD sect, dsect, dclust, tim;
	DIR dirscan;
	char fn[8+3+1];
	FATFS *fs = FatFs;


	if ((res = check_mounted()) != FR_OK) return res;
	if (disk_status() & STA_PROTECT) return FR_WRITE_PROTECTED;

	res = trace_path(&dirscan, fn, path, &dir);	/* Trace the file path */

	if (res == FR_OK) return FR_DENIED;		/* Any file or directory is already existing */
	if (res != FR_NO_FILE) return res;

	dir = reserve_direntry(&dirscan);		/* Reserve a directory entry */
	if (dir == NULL) return FR_DENIED;
	sect = fs->winsect;
	dsect = clust2sect(dclust = create_chain(0));	/* Get a new cluster for new directory */
	if (!dsect) return FR_DENIED;
	if (!move_window(0)) return 0;

	w = fs->win;
	memset(w, 0, 512);						/* Initialize the directory table */
	for (n = fs->sects_clust - 1; n; n--) {
		if (disk_write(w, dsect+n, 1) != RES_OK) return FR_RW_ERROR;
	}

	fs->winsect = dsect;					/* Create dot directories */
	memset(w, ' ', 8+3);
	*w = '.';
	*(w+11) = AM_DIR;
	tim = get_fattime();
	ST_DWORD(w+22, tim);
	memcpy(w+32, w, 32); *(w+33) = '.';
	ST_WORD(w+26, dclust);
	ST_WORD(w+20, dclust >> 16);
	ST_WORD(w+32+26, dirscan.sclust);
	ST_WORD(w+32+20, dirscan.sclust >> 16);
	fs->winflag = 1;

	if (!move_window(sect)) return FR_RW_ERROR;
	memcpy(dir, fn, 8+3);			/* Initialize the new entry */
	*(dir+11) = AM_DIR;
	*(dir+12) = fn[11];
	memset(dir+13, 0, 32-13);
	ST_DWORD(dir+22, tim);			/* Crated time */
	ST_WORD(dir+26, dclust);		/* Table start cluster */
	ST_WORD(dir+20, dclust >> 16);
	fs->winflag = 1;

	if (!move_window(0)) return FR_RW_ERROR;

	return FR_OK;
}
#endif



/*---------------------------*/
/* Initialize directroy scan */

FRESULT f_opendir (
	DIR *scan,			/* Pointer to directory object to initialize */
	const char *path	/* Pointer to the directory path, null str means the root */
)
{
	FRESULT res;
	BYTE *dir;
	char fn[8+3+1];


	if ((res = check_mounted()) != FR_OK) return res;

	res = trace_path(scan, fn, path, &dir);	/* Trace the directory path */

	if (res == FR_OK) {						/* Trace completed */
		if (dir != NULL) {					/* It is not a root dir */
			if (*(dir+11) & AM_DIR) {		/* The entry is a directory */
				scan->clust = ((DWORD)LD_WORD(dir+20) << 16) | LD_WORD(dir+26);
				scan->sect = clust2sect(scan->clust);
				scan->index = 0;
			} else {						/* The entry is a file */
				res = FR_NO_PATH;
			}
		}
	}
	return res;
}



/*----------------------------------*/
/* Read Directory Entry in Sequense */

FRESULT f_readdir (
	DIR *scan,			/* Pointer to the directory object */
	FILINFO *finfo		/* Pointer to file information to return */
)
{
	BYTE *dir, c;
	FATFS *fs = FatFs;


	if (!fs) return FR_NOT_ENABLED;
	finfo->fname[0] = 0;
	if ((disk_status() & STA_NOINIT) || !fs->fs_type) return FR_NOT_READY;

	while (scan->sect) {
		if (!move_window(scan->sect)) return FR_RW_ERROR;
		dir = &(fs->win[(scan->index & 15) * 32]);		/* pointer to the directory entry */
		c = *dir;
		if (c == 0) break;								/* Has it reached to end of dir? */
		if ((c != 0xE5) && (c != '.') && !(*(dir+11) & AM_VOL))	/* Is it a valid entry? */
			get_fileinfo(finfo, dir);
		if (!next_dir_entry(scan)) scan->sect = 0;		/* Next entry */
		if (finfo->fname[0]) break;						/* Found valid entry */
	}

	return FR_OK;
}



/*-----------------*/
/* Get File Status */

FRESULT f_stat (
	const char *path,	/* Pointer to the file path */
	FILINFO *finfo		/* Pointer to file information to return */
)
{
	FRESULT res;
	BYTE *dir;
	DIR dirscan;
	char fn[8+3+1];


	if ((res = check_mounted()) != FR_OK) return res;

	res = trace_path(&dirscan, fn, path, &dir);		/* Trace the file path */

	if (res == FR_OK)								/* Trace completed */
		get_fileinfo(finfo, dir);

	return res;
}



/*-----------------------*/
/* Change File Attribute */

#ifndef _FS_READONLY
FRESULT f_chmod (
	const char *path,	/* Pointer to the file path */
	BYTE value,			/* Attribute bits */
	BYTE mask			/* Attribute mask to change */
)
{
	FRESULT res;
	BYTE *dir;
	DIR dirscan;
	char fn[8+3+1];
	FATFS *fs = FatFs;


	if ((res = check_mounted()) != FR_OK) return res;
	if (disk_status() & STA_PROTECT) return FR_WRITE_PROTECTED;

	res = trace_path(&dirscan, fn, path, &dir);	/* Trace the file path */

	if (res == FR_OK) {						/* Trace completed */
		if (dir == NULL) {
			res = FR_NO_FILE;
		} else {
			mask &= AM_RDO|AM_HID|AM_SYS|AM_ARC;	/* Valid attribute mask */
			*(dir+11) = (value & mask) | (*(dir+11) & ~mask);	/* Apply attribute change */
			fs->winflag = 1;
			if(!move_window(0)) res = FR_RW_ERROR;
		}
	}
	return res;
}
#endif


