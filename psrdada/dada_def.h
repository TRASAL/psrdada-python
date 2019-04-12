#ifndef __DADA_DEF_H
#define __DADA_DEF_H

#include <unistd.h>
#include <inttypes.h>

/* ************************************************************************

   dada default definitions

   ************************************************************************ */

/* base key number used to identify DATA header and data block */
#define DADA_DEFAULT_BLOCK_KEY 0x0000dada

/* default number of blocks in Data Block */
#define DADA_DEFAULT_BLOCK_NUM  ((uint64_t) 4)

/* default size of blocks in Data Block */
#define DADA_DEFAULT_BLOCK_SIZE  ((uint64_t) sysconf (_SC_PAGE_SIZE) * 128)

/* default size of block in Header Block */
#define DADA_DEFAULT_HEADER_SIZE ((uint64_t) sysconf (_SC_PAGE_SIZE))

/* default port to connect to pwc command interface */
#define DADA_DEFAULT_PWC_PORT   56026

/* default port to connect to pwcc command interface */
#define DADA_DEFAULT_PWCC_PORT   56030

/* default port to connect to primary write client logging interface */
#define DADA_DEFAULT_PWC_LOG    56027

/* default port to connect to dada_pwc_command combined logging interface */
#define DADA_DEFAULT_PWCC_LOGPORT 56028

/* default port to connect to dbdisk logging interface */
#define DADA_DEFAULT_DBDISK_LOG 56037

/* default port to connect to diskdb logging interface */
#define DADA_DEFAULT_DISKDB_LOG 56039

/* default port to connect to dbnic logging interface */
#define DADA_DEFAULT_DBNIC_LOG  56047

/* default port to connect to primary write client command interface */
#define DADA_DEFAULT_NICDB_PORT 56056

/* default port to connect to primary write client logging interface */
#define DADA_DEFAULT_NICDB_LOG  56057

/* default port to connect to dbull client logging interface */
#define DADA_DEFAULT_DBNULL_LOG  56061

/* default port to connect to dada_dbmonitor logging interface */
#define DADA_DEFAULT_DBMONITOR_LOG  56063

/* default port to connect to dbnic logging interface */
#define DADA_DEFAULT_DBNDB_LOG  56071

/* default port to connect to for IB comm management */
#define DADA_DEFAULT_IBDB_PORT 56072

/* default port to connect to dbib logging interface */
#define DADA_DEFAULT_DBIB_LOG  56073

/* default port to connect to ibdb logging interface */
#define DADA_DEFAULT_IBDB_LOG  56074

/* default file size of 640 Million bytes */
#define DADA_DEFAULT_FILESIZE 640000000

#define DADA_DEFAULT_XFERSIZE DADA_DEFAULT_FILESIZE

/* maximum length of observation id string */
#define DADA_OBS_ID_MAXLEN 64

/* the format of the UTC_START string used in call to strftime */
#define DADA_TIMESTR "%Y-%m-%d-%H:%M:%S"

/* Length of the DADA_TIMESTR */
#define DADA_TIMESTR_LENGTH 21

#define DADA_ERROR_SOFT  -1
#define DADA_ERROR_HARD  -2
#define DADA_ERROR_FATAL -3
#endif
