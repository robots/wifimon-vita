/** @file  mlanconfig.c
  *
  * @brief Program to configure addition parameters into the mlandriver
  * 
  *  Usage: mlanconfig mlanX cmd [...] 
  *
  *  Copyright (C) 2008-2009, Marvell International Ltd.
  *  All Rights Reserved
  */
/************************************************************************
Change log:
     11/26/2008: initial version
     03/10/2009:  add setuserscan, getscantable etc. commands
     08/11/2009:  add addts, regclass, setra, scanagent etc. commands
************************************************************************/

#include    "mlanconfig.h"
#include    "mlanhostcmd.h"
#include    "mlanmisc.h"

/** mlanconfig version number */
#define MLANCONFIG_VER "M1.3"

/** Initial number of total private ioctl calls */
#define IW_INIT_PRIV_NUM    128
/** Maximum number of total private ioctl calls supported */
#define IW_MAX_PRIV_NUM     1024

/********************************************************
		Local Variables
********************************************************/

/** Private ioctl commands */
enum COMMANDS
{
    CMD_HOSTCMD,
    CMD_MEFCFG,
    CMD_ARPFILTER,
    CMD_CFG_DATA,
    CMD_CMD52RW,
    CMD_CMD53RW,
    CMD_GET_SCAN_RSP,
    CMD_SET_USER_SCAN,
    CMD_ADD_TS,
    CMD_DEL_TS,
    CMD_QCONFIG,
    CMD_QSTATS,
    CMD_TS_STATUS,
    CMD_WMM_QSTATUS,
    CMD_REGRW,
};

static t_s8 *commands[] = {
    "hostcmd",
    "mefcfg",
    "arpfilter",
    "cfgdata",
    "sdcmd52rw",
    "sdcmd53rw",
    "getscantable",
    "setuserscan",
    "addts",
    "delts",
    "qconfig",
    "qstats",
    "ts_status",
    "qstatus",
    "regrdwr",
};

static t_s8 *usage[] = {
    "Usage: ",
    "   mlanconfig -v  (version)",
    "   mlanconfig <mlanX> <cmd> [...]",
    "   where",
    "   mlanX : wireless network interface",
    "   cmd : hostcmd",
    "       : mefcfg",
    "       : arpfilter",
    "       : cfgdata",
    "       : sdcmd52rw, sdcmd53rw",
    "       : getscantable, setuserscan",
    "       : addts, delts, qconfig, qstats, ts_status, qstatus",
    "       : regrdwr",
    "       : additional parameter for hostcmd",
    "       :   <filename> <cmd>",
    " 	    : additional parameters for mefcfg are:",
    "       :   <filename>",
    "       : additional parameter for arpfilter",
    "       :   <filename>",
    "       : additional parameter for cfgdata",
    "       :   <type> <filename>",
    "       : additional parameter for sdcmd52rw",
    "       :   <function> <reg addr.> <data>",
    "       : additional parameter for sdcmd53rw",
    "       :   <func> <addr> <mode> <blksiz> <blknum> <data1> ... ...<dataN> ",
    "       : additional parameter for addts",
    "       :   <filename.conf> <section# of tspec> <timeout in ms>",
    "       : additional parameter for delts",
    "       :   <filename.conf> <section# of tspec>",
    "       : additional parameter for qconfig",
    "       :   <[set msdu <lifetime in TUs> [Queue Id: 0-3]]",
    "       :    [get [Queue Id: 0-3]] [def [Queue Id: 0-3]]>",
    "       : additional parameter for qstats",
    "       :   <[on [Queue Id: 0-3]] [off [Queue Id: 0-3]] [get [Queue Id: 0-3]]>",
    "       : additional parameter for regrdwr",
    "       :   <type> <offset> [value]",
};

static FILE *fp; /**< socket */
t_s32 sockfd;  /**< socket */
t_s8 dev_name[IFNAMSIZ + 1];    /**< device name */
static struct iw_priv_args *priv_args = NULL;      /**< private args */
static int we_version_compiled = 0;
                                  /**< version compiled */

/********************************************************
		Global Variables
********************************************************/

/********************************************************
		Local Functions
********************************************************/
/** 
 *  @brief Get private info.
 *   
 *  @param ifname   A pointer to net name
 *  @return 	    MLAN_STATUS_SUCCESS--success, otherwise --fail
 */
static int
get_private_info(const t_s8 * ifname)
{
    /* This function sends the SIOCGIWPRIV command, which is handled by the
       kernel and gets the total number of private ioctl's available in the
       host driver. */
    struct iwreq iwr;
    int s, ret = MLAN_STATUS_SUCCESS;
    struct iw_priv_args *ppriv = NULL;
    struct iw_priv_args *new_priv;
    int result = 0;
    size_t size = IW_INIT_PRIV_NUM;

    s = socket(PF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        perror("socket[PF_INET,SOCK_DGRAM]");
        return MLAN_STATUS_FAILURE;
    }

    memset(&iwr, 0, sizeof(iwr));
    strncpy(iwr.ifr_name, ifname, IFNAMSIZ);

    do {
        /* (Re)allocate the buffer */
        new_priv = realloc(ppriv, size * sizeof(ppriv[0]));
        if (new_priv == NULL) {
            printf("Error: Buffer allocation failed\n");
            ret = MLAN_STATUS_FAILURE;
            break;
        }
        ppriv = new_priv;

        iwr.u.data.pointer = (caddr_t) ppriv;
        iwr.u.data.length = size;
        iwr.u.data.flags = 0;

        if (ioctl(s, SIOCGIWPRIV, &iwr) < 0) {
            result = errno;
            ret = MLAN_STATUS_FAILURE;
            if (result == E2BIG) {
                /* We need a bigger buffer. Check if kernel gave us any hints. */
                if (iwr.u.data.length > size) {
                    /* Kernel provided required size */
                    size = iwr.u.data.length;
                } else {
                    /* No hint from kernel, double the buffer size */
                    size *= 2;
                }
            } else {
                /* ioctl error */
                perror("ioctl[SIOCGIWPRIV]");
                break;
            }
        } else {
            /* Success. Return the number of private ioctls */
            priv_args = ppriv;
            ret = iwr.u.data.length;
            break;
        }
    } while (size <= IW_MAX_PRIV_NUM);

    if ((ret == MLAN_STATUS_FAILURE) && (ppriv))
        free(ppriv);

    close(s);

    return ret;
}

/** 
 *  @brief Get Sub command ioctl number
 *   
 *  @param i        command index
 *  @param priv_cnt     Total number of private ioctls availabe in driver
 *  @param ioctl_val    A pointer to return ioctl number
 *  @param subioctl_val A pointer to return sub-ioctl number
 *  @return 	        MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
static int
marvell_get_subioctl_no(t_s32 i,
                        t_s32 priv_cnt, int *ioctl_val, int *subioctl_val)
{
    t_s32 j;

    if (priv_args[i].cmd >= SIOCDEVPRIVATE) {
        *ioctl_val = priv_args[i].cmd;
        *subioctl_val = 0;
        return MLAN_STATUS_SUCCESS;
    }

    j = -1;

    /* Find the matching *real* ioctl */

    while ((++j < priv_cnt)
           && ((priv_args[j].name[0] != '\0') ||
               (priv_args[j].set_args != priv_args[i].set_args) ||
               (priv_args[j].get_args != priv_args[i].get_args))) {
    }

    /* If not found... */
    if (j == priv_cnt) {
        printf("%s: Invalid private ioctl definition for: 0x%x\n",
               dev_name, priv_args[i].cmd);
        return MLAN_STATUS_FAILURE;
    }

    /* Save ioctl numbers */
    *ioctl_val = priv_args[j].cmd;
    *subioctl_val = priv_args[i].cmd;

    return MLAN_STATUS_SUCCESS;
}

/** 
 *  @brief Get ioctl number
 *   
 *  @param ifname   	A pointer to net name
 *  @param priv_cmd	A pointer to priv command buffer
 *  @param ioctl_val    A pointer to return ioctl number
 *  @param subioctl_val A pointer to return sub-ioctl number
 *  @return 	        MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
static int
marvell_get_ioctl_no(const t_s8 * ifname,
                     const t_s8 * priv_cmd, int *ioctl_val, int *subioctl_val)
{
    t_s32 i;
    t_s32 priv_cnt;
    int ret = MLAN_STATUS_FAILURE;

    priv_cnt = get_private_info(ifname);

    /* Are there any private ioctls? */
    if (priv_cnt <= 0 || priv_cnt > IW_MAX_PRIV_NUM) {
        /* Could skip this message ? */
        printf("%-8.8s  no private ioctls.\n", ifname);
    } else {
        for (i = 0; i < priv_cnt; i++) {
            if (priv_args[i].name[0] && !strcmp(priv_args[i].name, priv_cmd)) {
                ret = marvell_get_subioctl_no(i, priv_cnt,
                                              ioctl_val, subioctl_val);
                break;
            }
        }
    }

    if (priv_args) {
        free(priv_args);
        priv_args = NULL;
    }

    return ret;
}

/** 
 *  @brief Retrieve the ioctl and sub-ioctl numbers for the given ioctl string
 *   
 *  @param ioctl_name   Private IOCTL string name
 *  @param ioctl_val    A pointer to return ioctl number
 *  @param subioctl_val A pointer to return sub-ioctl number
 *
 *  @return             MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
int
get_priv_ioctl(char *ioctl_name, int *ioctl_val, int *subioctl_val)
{
    int retval;

    retval = marvell_get_ioctl_no(dev_name,
                                  ioctl_name, ioctl_val, subioctl_val);

    return retval;
}

/** 
 *  @brief Process host_cmd 
 *  @param argc		number of arguments
 *  @param argv		A pointer to arguments array    
 *  @return      	MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int
process_host_cmd(int argc, char *argv[])
{
    t_s8 cmdname[256];
    t_u8 *buf;
    HostCmd_DS_GEN *hostcmd;
    struct iwreq iwr;
    int ret = MLAN_STATUS_SUCCESS;
    int ioctl_val, subioctl_val;

    if (get_priv_ioctl("hostcmd",
                       &ioctl_val, &subioctl_val) == MLAN_STATUS_FAILURE) {
        return -EOPNOTSUPP;
    }

    if (argc < 5) {
        printf("Error: invalid no of arguments\n");
        printf("Syntax: ./mlanconfig mlanX hostcmd <hostcmd.conf> <cmdname>\n");
        exit(1);
    }

    if ((fp = fopen(argv[3], "r")) == NULL) {
        fprintf(stderr, "Cannot open file %s\n", argv[3]);
        exit(1);
    }

    buf = (t_u8 *) malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
    if (buf == NULL) {
        printf("Error: allocate memory for hostcmd failed\n");
        fclose(fp);
        return -ENOMEM;
    }
    sprintf(cmdname, "%s", argv[4]);
    ret = prepare_host_cmd_buffer(cmdname, buf);
    fclose(fp);

    if (ret == MLAN_STATUS_FAILURE)
        goto _exit_;

    hostcmd = (HostCmd_DS_GEN *) buf;
    memset(&iwr, 0, sizeof(iwr));
    strncpy(iwr.ifr_name, dev_name, IFNAMSIZ);
    iwr.u.data.pointer = (t_u8 *) hostcmd;
    iwr.u.data.length = le16_to_cpu(hostcmd->size);

    iwr.u.data.flags = 0;
    if (ioctl(sockfd, ioctl_val, &iwr)) {
        fprintf(stderr, "mlanconfig: MLANHOSTCMD is not supported by %s\n",
                dev_name);
        ret = MLAN_STATUS_FAILURE;
        goto _exit_;
    }
    ret = process_host_cmd_resp(buf);

  _exit_:
    if (buf)
        free(buf);

    return ret;
}

/** 
 *  @brief  get range 
 *   
 *  @return	MLAN_STATUS_SUCCESS--success, otherwise --fail
 */
static int
get_range(t_void)
{
    struct iw_range *range;
    struct iwreq iwr;
    size_t buf_len;

    buf_len = sizeof(struct iw_range) + 500;
    range = malloc(buf_len);
    if (range == NULL) {
        printf("Error: allocate memory for iw_range failed\n");
        return -ENOMEM;
    }
    memset(range, 0, buf_len);
    memset(&iwr, 0, sizeof(struct iwreq));
    iwr.u.data.pointer = (caddr_t) range;
    iwr.u.data.length = buf_len;
    strncpy(iwr.ifr_name, dev_name, IFNAMSIZ);

    if ((ioctl(sockfd, SIOCGIWRANGE, &iwr)) < 0) {
        printf("Get Range Results Failed\n");
        free(range);
        return MLAN_STATUS_FAILURE;
    }
    we_version_compiled = range->we_version_compiled;
    printf("Driver build with Wireless Extension %d\n",
           range->we_version_compiled);
    free(range);
    return MLAN_STATUS_SUCCESS;
}

/** 
 *  @brief Display usage
 *  
 *  @return       NA
 */
static t_void
display_usage(t_void)
{
    t_s32 i;
    for (i = 0; i < NELEMENTS(usage); i++)
        fprintf(stderr, "%s\n", usage[i]);
}

/** 
 *  @brief Find command
 *  
 *  @param maxcmds  max command number
 *  @param cmds     A pointer to commands buffer
 *  @param cmd      A pointer to command buffer
 *  @return         index of command or MLAN_STATUS_FAILURE
 */
static int
findcommand(t_s32 maxcmds, t_s8 * cmds[], t_s8 * cmd)
{
    t_s32 i;

    for (i = 0; i < maxcmds; i++) {
        if (!strcasecmp(cmds[i], cmd)) {
            return i;
        }
    }

    return MLAN_STATUS_FAILURE;
}

/** 
 *  @brief Process arpfilter
 *  @param argc   number of arguments
 *  @param argv   A pointer to arguments array    
 *  @return     MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int
process_arpfilter(int argc, char *argv[])
{
    t_u8 *buf;
    struct iwreq iwr;
    t_u16 length = 0;
    int ret = MLAN_STATUS_SUCCESS;
    int ioctl_val, subioctl_val;

    if (get_priv_ioctl("arpfilter",
                       &ioctl_val, &subioctl_val) == MLAN_STATUS_FAILURE) {
        return -EOPNOTSUPP;
    }

    if (argc < 4) {
        printf("Error: invalid no of arguments\n");
        printf("Syntax: ./mlanconfig mlanX arpfilter <arpfilter.conf>\n");
        exit(1);
    }

    if ((fp = fopen(argv[3], "r")) == NULL) {
        fprintf(stderr, "Cannot open file %s\n", argv[3]);
        return MLAN_STATUS_FAILURE;
    }
    buf = (t_u8 *) malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
    if (buf == NULL) {
        printf("Error: allocate memory for arpfilter failed\n");
        fclose(fp);
        return -ENOMEM;
    }
    ret = prepare_arp_filter_buffer(buf, &length);
    fclose(fp);

    if (ret == MLAN_STATUS_FAILURE)
        goto _exit_;

    memset(&iwr, 0, sizeof(iwr));
    strncpy(iwr.ifr_name, dev_name, IFNAMSIZ);
    iwr.u.data.pointer = buf;
    iwr.u.data.length = length;
    iwr.u.data.flags = 0;
    if (ioctl(sockfd, ioctl_val, &iwr)) {
        fprintf(stderr,
                "mlanconfig: arpfilter command is not supported by %s\n",
                dev_name);
        ret = MLAN_STATUS_FAILURE;
        goto _exit_;
    }

  _exit_:
    if (buf)
        free(buf);

    return ret;
}

/** 
 *  @brief Process cfgdata 
 *  @param argc     number of arguments
 *  @param argv     A pointer to arguments array    
 *  @return         MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int
process_cfg_data(int argc, char *argv[])
{
    t_u8 *buf;
    HostCmd_DS_GEN *hostcmd;
    struct iwreq iwr;
    int ret = MLAN_STATUS_SUCCESS;
    int ioctl_val, subioctl_val;

    if (get_priv_ioctl("hostcmd",
                       &ioctl_val, &subioctl_val) == MLAN_STATUS_FAILURE) {
        return -EOPNOTSUPP;
    }

    if (argc < 4 || argc > 5) {
        printf("Error: invalid no of arguments\n");
        printf
            ("Syntax: ./mlanconfig mlanX cfgdata <register type> <filename>\n");
        exit(1);
    }

    if (argc == 5) {
        if ((fp = fopen(argv[4], "r")) == NULL) {
            fprintf(stderr, "Cannot open file %s\n", argv[3]);
            exit(1);
        }
    }
    buf = (t_u8 *) malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
    if (buf == NULL) {
        printf("Error: allocate memory for hostcmd failed\n");
        fclose(fp);
        return -ENOMEM;
    }
    ret = prepare_cfg_data_buffer(argc, argv, buf);
    if (argc == 5)
        fclose(fp);

    if (ret == MLAN_STATUS_FAILURE)
        goto _exit_;

    hostcmd = (HostCmd_DS_GEN *) buf;
    memset(&iwr, 0, sizeof(iwr));
    strncpy(iwr.ifr_name, dev_name, IFNAMSIZ);
    iwr.u.data.pointer = (t_u8 *) hostcmd;
    iwr.u.data.length = le16_to_cpu(hostcmd->size);

    iwr.u.data.flags = 0;
    if (ioctl(sockfd, ioctl_val, &iwr)) {
        fprintf(stderr, "mlanconfig: MLANHOSTCMD is not supported by %s\n",
                dev_name);
        ret = MLAN_STATUS_FAILURE;
        goto _exit_;
    }
    ret = process_host_cmd_resp(buf);

  _exit_:
    if (buf)
        free(buf);

    return ret;
}

/** 
 *  @brief read current command
 *  @param ptr      A pointer to data
 *  @param curCmd   A pointer to the buf which will hold current command    
 *  @return         NULL or the pointer to the left command buf
 */
static t_s8 *
readCurCmd(t_s8 * ptr, t_s8 * curCmd)
{
    t_s32 i = 0;
#define MAX_CMD_SIZE 64 /**< Max command size */

    while (*ptr != ']' && i < (MAX_CMD_SIZE - 1))
        curCmd[i++] = *(++ptr);

    if (*ptr != ']')
        return NULL;

    curCmd[i - 1] = '\0';

    return ++ptr;
}

/** 
 *  @brief parse command and hex data
 *  @param fp       A pointer to FILE stream
 *  @param dst      A pointer to the dest buf
 *  @param cmd      A pointer to command buf for search
 *  @return         Length of hex data or MLAN_STATUS_FAILURE  		
 */
static int
fparse_for_cmd_and_hex(FILE * fp, t_u8 * dst, t_u8 * cmd)
{
    t_s8 *ptr;
    t_u8 *dptr;
    t_s8 buf[256], curCmd[64];
    t_s32 isCurCmd = 0;

    dptr = dst;
    while (fgets(buf, sizeof(buf), fp)) {
        ptr = buf;

        while (*ptr) {
            /* skip leading spaces */
            while (*ptr && isspace(*ptr))
                ptr++;

            /* skip blank lines and lines beginning with '#' */
            if (*ptr == '\0' || *ptr == '#')
                break;

            if (*ptr == '[' && *(ptr + 1) != '/') {
                if (!(ptr = readCurCmd(ptr, curCmd)))
                    return MLAN_STATUS_FAILURE;

                if (strcasecmp(curCmd, (char *) cmd))   /* Not equal */
                    isCurCmd = 0;
                else
                    isCurCmd = 1;
            }

            /* Ignore the rest if it is not correct cmd */
            if (!isCurCmd)
                break;

            if (*ptr == '[' && *(ptr + 1) == '/')
                return (dptr - dst);

            if (isxdigit(*ptr)) {
                ptr = convert2hex(ptr, dptr++);
            } else {
                /* Invalid character on data line */
                ptr++;
            }
        }
    }

    return MLAN_STATUS_FAILURE;
}

/**
 *  @brief Send an ADDTS command to the associated AP
 *
 *  Process a given conf file for a specific TSPEC data block.  Send the
 *    TSPEC along with any other IEs to the driver/firmware for transmission
 *    in an ADDTS request to the associated AP.  
 *
 *  Return the execution status of the command as well as the ADDTS response
 *    from the AP if any.
 *  
 *  mlanconfig mlanX addts <filename.conf> <section# of tspec> <timeout in ms>
 *
 *  @param argc     number of arguments
 *  @param argv     A pointer to arguments array    
 *
 *  @return         MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int
process_addts(int argc, char *argv[])
{
    int ioctl_val, subioctl_val;
    struct iwreq iwr;
    int ieBytes;
    wlan_ioctl_wmm_addts_req_t addtsReq;

    FILE *fp;
    char filename[48];
    char config_id[20];

    memset(&addtsReq, 0x00, sizeof(addtsReq));
    memset(filename, 0x00, sizeof(filename));

    if (get_priv_ioctl("addts",
                       &ioctl_val, &subioctl_val) == MLAN_STATUS_FAILURE) {
        return -EOPNOTSUPP;
    }

    if (argc != 6) {
        fprintf(stderr, "Invalid number of parameters!\n");
        return -EINVAL;
    }

    ieBytes = 0;

    strncpy(filename, argv[3], MIN(sizeof(filename) - 1, strlen(argv[3])));
    if ((fp = fopen(filename, "r")) == NULL) {
        perror("fopen");
        fprintf(stderr, "Cannot open file %s\n", argv[3]);
        exit(1);
    }

    sprintf(config_id, "tspec%d", atoi(argv[4]));

    ieBytes =
        fparse_for_cmd_and_hex(fp, addtsReq.tspecData, (t_u8 *) config_id);

    if (ieBytes > 0) {
        printf("Found %d bytes in the %s section of conf file %s\n",
               ieBytes, config_id, filename);
    } else {
        fprintf(stderr, "section %s not found in %s\n", config_id, filename);
        exit(1);
    }

    addtsReq.timeout_ms = atoi(argv[5]);

    printf("Cmd Input:\n");
    hexdump(config_id, addtsReq.tspecData, ieBytes, ' ');

    strncpy(iwr.ifr_name, dev_name, IFNAMSIZ);
    iwr.u.data.flags = subioctl_val;
    iwr.u.data.pointer = (caddr_t) & addtsReq;
    iwr.u.data.length = (sizeof(addtsReq.timeout_ms)
                         + sizeof(addtsReq.commandResult)
                         + sizeof(addtsReq.ieeeStatusCode)
                         + ieBytes);

    if (ioctl(sockfd, ioctl_val, &iwr) < 0) {
        perror("mlanconfig: addts ioctl");
        return -EFAULT;
    }

    ieBytes = iwr.u.data.length - (sizeof(addtsReq.timeout_ms)
                                   + sizeof(addtsReq.commandResult)
                                   + sizeof(addtsReq.ieeeStatusCode));
    printf("Cmd Output:\n");
    printf("ADDTS Command Result = %d\n", addtsReq.commandResult);
    printf("ADDTS IEEE Status    = %d\n", addtsReq.ieeeStatusCode);
    hexdump(config_id, addtsReq.tspecData, ieBytes, ' ');

    return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Send a DELTS command to the associated AP
 *
 *  Process a given conf file for a specific TSPEC data block.  Send the
 *    TSPEC along with any other IEs to the driver/firmware for transmission
 *    in a DELTS request to the associated AP.  
 *
 *  Return the execution status of the command.  There is no response to a
 *    DELTS from the AP.
 *  
 *  mlanconfig mlanX delts <filename.conf> <section# of tspec>
 *
 *  @param argc     number of arguments
 *  @param argv     A pointer to arguments array    
 *
 *  @return         MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int
process_delts(int argc, char *argv[])
{
    int ioctl_val, subioctl_val;
    struct iwreq iwr;
    int ieBytes;
    wlan_ioctl_wmm_delts_req_t deltsReq;

    FILE *fp;
    char filename[48];
    char config_id[20];

    memset(&deltsReq, 0x00, sizeof(deltsReq));
    memset(filename, 0x00, sizeof(filename));

    if (get_priv_ioctl("delts",
                       &ioctl_val, &subioctl_val) == MLAN_STATUS_FAILURE) {
        return -EOPNOTSUPP;
    }

    if (argc != 5) {
        fprintf(stderr, "Invalid number of parameters!\n");
        return -EINVAL;
    }

    ieBytes = 0;

    strncpy(filename, argv[3], MIN(sizeof(filename) - 1, strlen(argv[3])));
    if ((fp = fopen(filename, "r")) == NULL) {
        perror("fopen");
        fprintf(stderr, "Cannot open file %s\n", argv[3]);
        exit(1);
    }

    sprintf(config_id, "tspec%d", atoi(argv[4]));

    ieBytes =
        fparse_for_cmd_and_hex(fp, deltsReq.tspecData, (t_u8 *) config_id);

    if (ieBytes > 0) {
        printf("Found %d bytes in the %s section of conf file %s\n",
               ieBytes, config_id, filename);
    } else {
        fprintf(stderr, "section %s not found in %s\n", config_id, filename);
        exit(1);
    }

    deltsReq.ieeeReasonCode = 0x20;     /* 32, unspecified QOS reason */

    printf("Cmd Input:\n");
    hexdump(config_id, deltsReq.tspecData, ieBytes, ' ');

    strncpy(iwr.ifr_name, dev_name, IFNAMSIZ);
    iwr.u.data.flags = subioctl_val;
    iwr.u.data.pointer = (caddr_t) & deltsReq;
    iwr.u.data.length = (sizeof(deltsReq.commandResult)
                         + sizeof(deltsReq.ieeeReasonCode)
                         + ieBytes);

    if (ioctl(sockfd, ioctl_val, &iwr) < 0) {
        perror("mlanconfig: delts ioctl");
        return -EFAULT;
    }

    printf("Cmd Output:\n");
    printf("DELTS Command Result = %d\n", deltsReq.commandResult);

    return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Send a WMM AC Queue configuration command to get/set/default params
 *
 *  Configure or get the parameters of a WMM AC queue. The command takes
 *    an optional Queue Id as a last parameter.  Without the queue id, all
 *    queues will be acted upon.
 *  
 *  mlanconfig mlanX qconfig set msdu <lifetime in TUs> [Queue Id: 0-3]
 *  mlanconfig mlanX qconfig get [Queue Id: 0-3]
 *  mlanconfig mlanX qconfig def [Queue Id: 0-3]
 *
 *  @param argc     number of arguments
 *  @param argv     A pointer to arguments array    
 *
 *  @return         MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int
process_qconfig(int argc, char *argv[])
{
    int ioctl_val, subioctl_val;
    struct iwreq iwr;
    wlan_ioctl_wmm_queue_config_t queue_config_cmd;
    mlan_wmm_ac_e ac_idx;
    mlan_wmm_ac_e ac_idx_start;
    mlan_wmm_ac_e ac_idx_stop;

    const char *ac_str_tbl[] = { "BK", "BE", "VI", "VO" };

    if (argc < 4) {
        fprintf(stderr, "Invalid number of parameters!\n");
        return -EINVAL;
    }

    if (get_priv_ioctl("qconfig",
                       &ioctl_val, &subioctl_val) == MLAN_STATUS_FAILURE) {
        return -EOPNOTSUPP;
    }

    memset(&queue_config_cmd, 0x00, sizeof(queue_config_cmd));
    strncpy(iwr.ifr_name, dev_name, IFNAMSIZ);
    iwr.u.data.pointer = (caddr_t) & queue_config_cmd;
    iwr.u.data.length = sizeof(queue_config_cmd);
    iwr.u.data.flags = subioctl_val;

    if (strcmp(argv[3], "get") == 0) {
        /* 3 4 5 */
        /* qconfig get [qid] */
        if (argc == 4) {
            ac_idx_start = WMM_AC_BK;
            ac_idx_stop = WMM_AC_VO;
        } else if (argc == 5) {
            if (atoi(argv[4]) < WMM_AC_BK || atoi(argv[4]) > WMM_AC_VO) {
                fprintf(stderr, "ERROR: Invalid Queue ID!\n");
                return -EINVAL;
            }
            ac_idx_start = atoi(argv[4]);
            ac_idx_stop = ac_idx_start;
        } else {
            fprintf(stderr, "Invalid number of parameters!\n");
            return -EINVAL;
        }
        queue_config_cmd.action = WMM_QUEUE_CONFIG_ACTION_GET;

        for (ac_idx = ac_idx_start; ac_idx <= ac_idx_stop; ac_idx++) {
            queue_config_cmd.accessCategory = ac_idx;
            if (ioctl(sockfd, ioctl_val, &iwr) < 0) {
                perror("qconfig ioctl");
            } else {
                printf("qconfig %s(%d): MSDU Lifetime GET = 0x%04x (%d)\n",
                       ac_str_tbl[ac_idx],
                       ac_idx,
                       queue_config_cmd.msduLifetimeExpiry,
                       queue_config_cmd.msduLifetimeExpiry);
            }
        }
    } else if (strcmp(argv[3], "set") == 0) {
        if ((argc >= 5) && strcmp(argv[4], "msdu") == 0) {
            /* 3 4 5 6 7 */
            /* qconfig set msdu <value> [qid] */
            if (argc == 6) {
                ac_idx_start = WMM_AC_BK;
                ac_idx_stop = WMM_AC_VO;
            } else if (argc == 7) {
                if (atoi(argv[6]) < WMM_AC_BK || atoi(argv[6]) > WMM_AC_VO) {
                    fprintf(stderr, "ERROR: Invalid Queue ID!\n");
                    return -EINVAL;
                }
                ac_idx_start = atoi(argv[6]);
                ac_idx_stop = ac_idx_start;
            } else {
                fprintf(stderr, "Invalid number of parameters!\n");
                return -EINVAL;
            }
            queue_config_cmd.action = WMM_QUEUE_CONFIG_ACTION_SET;
            queue_config_cmd.msduLifetimeExpiry = atoi(argv[5]);

            for (ac_idx = ac_idx_start; ac_idx <= ac_idx_stop; ac_idx++) {
                queue_config_cmd.accessCategory = ac_idx;
                if (ioctl(sockfd, ioctl_val, &iwr) < 0) {
                    perror("qconfig ioctl");
                } else {
                    printf("qconfig %s(%d): MSDU Lifetime SET = 0x%04x (%d)\n",
                           ac_str_tbl[ac_idx],
                           ac_idx,
                           queue_config_cmd.msduLifetimeExpiry,
                           queue_config_cmd.msduLifetimeExpiry);
                }
            }
        } else {
            /* Only MSDU Lifetime provisioning accepted for now */
            fprintf(stderr, "Invalid set parameter: s/b [msdu]\n");
            return -EINVAL;
        }
    } else if (strncmp(argv[3], "def", strlen("def")) == 0) {
        /* 3 4 5 */
        /* qconfig def [qid] */
        if (argc == 4) {
            ac_idx_start = WMM_AC_BK;
            ac_idx_stop = WMM_AC_VO;
        } else if (argc == 5) {
            if (atoi(argv[4]) < WMM_AC_BK || atoi(argv[4]) > WMM_AC_VO) {
                fprintf(stderr, "ERROR: Invalid Queue ID!\n");
                return -EINVAL;
            }
            ac_idx_start = atoi(argv[4]);
            ac_idx_stop = ac_idx_start;
        } else {
            fprintf(stderr, "Invalid number of parameters!\n");
            return -EINVAL;
        }
        queue_config_cmd.action = WMM_QUEUE_CONFIG_ACTION_DEFAULT;

        for (ac_idx = ac_idx_start; ac_idx <= ac_idx_stop; ac_idx++) {
            queue_config_cmd.accessCategory = ac_idx;
            if (ioctl(sockfd, ioctl_val, &iwr) < 0) {
                perror("qconfig ioctl");
            } else {
                printf("qconfig %s(%d): MSDU Lifetime DEFAULT = 0x%04x (%d)\n",
                       ac_str_tbl[ac_idx],
                       ac_idx,
                       queue_config_cmd.msduLifetimeExpiry,
                       queue_config_cmd.msduLifetimeExpiry);
            }
        }
    } else {
        fprintf(stderr, "Invalid qconfig command; s/b [set, get, default]\n");
        return -EINVAL;
    }

    return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Turn on/off or retrieve and clear the queue statistics for an AC
 *
 *  Turn the queue statistics collection on/off for a given AC or retrieve the
 *    current accumulated stats and clear them from the firmware.  The command
 *    takes an optional Queue Id as a last parameter.  Without the queue id,
 *    all queues will be acted upon.
 *
 *  mlanconfig mlanX qstats on [Queue Id: 0-3]
 *  mlanconfig mlanX qstats off [Queue Id: 0-3]
 *  mlanconfig mlanX qstats get [Queue Id: 0-3]
 *
 *  @param argc     number of arguments
 *  @param argv     A pointer to arguments array    
 *
 *  @return         MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int
process_qstats(int argc, char *argv[])
{
    int ioctl_val, subioctl_val;
    struct iwreq iwr;
    wlan_ioctl_wmm_queue_stats_t queue_stats_cmd;
    mlan_wmm_ac_e ac_idx;
    mlan_wmm_ac_e ac_idx_start;
    mlan_wmm_ac_e ac_idx_stop;
    t_u16 usedTime[MAX_AC_QUEUES];
    t_u16 policedTime[MAX_AC_QUEUES];

    const char *ac_str_tbl[] = { "BK", "BE", "VI", "VO" };

    if (argc < 3) {
        fprintf(stderr, "Invalid number of parameters!\n");
        return -EINVAL;
    }

    if (get_priv_ioctl("qstats",
                       &ioctl_val, &subioctl_val) == MLAN_STATUS_FAILURE) {
        return -EOPNOTSUPP;
    }

    printf("\n");

    memset(usedTime, 0x00, sizeof(usedTime));
    memset(policedTime, 0x00, sizeof(policedTime));
    memset(&queue_stats_cmd, 0x00, sizeof(queue_stats_cmd));

    strncpy(iwr.ifr_name, dev_name, IFNAMSIZ);
    iwr.u.data.pointer = (caddr_t) & queue_stats_cmd;
    iwr.u.data.length = sizeof(queue_stats_cmd);
    iwr.u.data.flags = subioctl_val;

    if ((argc > 3) && strcmp(argv[3], "on") == 0) {
        if (argc == 4) {
            ac_idx_start = WMM_AC_BK;
            ac_idx_stop = WMM_AC_VO;
        } else if (argc == 5) {
            if (atoi(argv[4]) < WMM_AC_BK || atoi(argv[4]) > WMM_AC_VO) {
                fprintf(stderr, "ERROR: Invalid Queue ID!\n");
                return -EINVAL;
            }
            ac_idx_start = atoi(argv[4]);
            ac_idx_stop = ac_idx_start;
        } else {
            fprintf(stderr, "Invalid number of parameters!\n");
            return -EINVAL;
        }
        queue_stats_cmd.action = WMM_STATS_ACTION_START;
        for (ac_idx = ac_idx_start; ac_idx <= ac_idx_stop; ac_idx++) {
            queue_stats_cmd.accessCategory = ac_idx;
            if (ioctl(sockfd, ioctl_val, &iwr) < 0) {
                perror("qstats ioctl");
            } else {
                printf("qstats %s(%d) turned on\n", ac_str_tbl[ac_idx], ac_idx);
            }
        }
    } else if ((argc > 3) && strcmp(argv[3], "off") == 0) {
        if (argc == 4) {
            ac_idx_start = WMM_AC_BK;
            ac_idx_stop = WMM_AC_VO;
        } else if (argc == 5) {
            if (atoi(argv[4]) < WMM_AC_BK || atoi(argv[4]) > WMM_AC_VO) {
                fprintf(stderr, "ERROR: Invalid Queue ID!\n");
                return -EINVAL;
            }
            ac_idx_start = atoi(argv[4]);
            ac_idx_stop = ac_idx_start;
        } else {
            fprintf(stderr, "Invalid number of parameters!\n");
            return -EINVAL;
        }
        queue_stats_cmd.action = WMM_STATS_ACTION_STOP;
        for (ac_idx = ac_idx_start; ac_idx <= ac_idx_stop; ac_idx++) {
            queue_stats_cmd.accessCategory = ac_idx;
            if (ioctl(sockfd, ioctl_val, &iwr) < 0) {
                perror("qstats ioctl");
            } else {
                printf("qstats %s(%d) turned off\n",
                       ac_str_tbl[ac_idx], ac_idx);
            }
        }
    } else if ((argc >= 3) && ((argc == 3) ? 1 : (strcmp(argv[3], "get") == 0))) {
        /* If the user types: "mlanconfig mlanX qstats" without get argument.
           The mlanconfig application invokes "get" option for all queues */
        if ((argc == 4) || (argc == 3)) {
            ac_idx_start = WMM_AC_BK;
            ac_idx_stop = WMM_AC_VO;
        } else if (argc == 5) {
            if (atoi(argv[4]) < WMM_AC_BK || atoi(argv[4]) > WMM_AC_VO) {
                fprintf(stderr, "ERROR: Invalid Queue ID!\n");
                return -EINVAL;
            }
            ac_idx_start = atoi(argv[4]);
            ac_idx_stop = ac_idx_start;
        } else {
            fprintf(stderr, "Invalid number of parameters!\n");
            return -EINVAL;
        }
        printf("AC  Count   Loss  TxDly   QDly"
               "    <=5   <=10   <=20   <=30   <=40   <=50    >50\n");
        printf("----------------------------------"
               "---------------------------------------------\n");
        queue_stats_cmd.action = WMM_STATS_ACTION_GET_CLR;

        for (ac_idx = ac_idx_start; ac_idx <= ac_idx_stop; ac_idx++) {
            queue_stats_cmd.accessCategory = ac_idx;
            if (ioctl(sockfd, ioctl_val, &iwr) < 0) {
                perror("qstats ioctl");
            } else {
                printf("%s  %5u  %5u %6u %6u"
                       "  %5u  %5u  %5u  %5u  %5u  %5u  %5u\n",
                       ac_str_tbl[ac_idx],
                       queue_stats_cmd.pktCount,
                       queue_stats_cmd.pktLoss,
                       (unsigned int) queue_stats_cmd.avgTxDelay,
                       (unsigned int) queue_stats_cmd.avgQueueDelay,
                       queue_stats_cmd.delayHistogram[0],
                       queue_stats_cmd.delayHistogram[1],
                       queue_stats_cmd.delayHistogram[2],
                       queue_stats_cmd.delayHistogram[3],
                       queue_stats_cmd.delayHistogram[4],
                       queue_stats_cmd.delayHistogram[5],
                       queue_stats_cmd.delayHistogram[6]);

                usedTime[ac_idx] = queue_stats_cmd.usedTime;
                policedTime[ac_idx] = queue_stats_cmd.policedTime;
            }
        }

        printf("----------------------------------"
               "---------------------------------------------\n");
        printf("\nAC      UsedTime      PolicedTime\n");
        printf("---------------------------------\n");

        for (ac_idx = ac_idx_start; ac_idx <= ac_idx_stop; ac_idx++) {
            printf("%s        %6u           %6u\n",
                   ac_str_tbl[ac_idx],
                   (unsigned int) usedTime[ac_idx],
                   (unsigned int) policedTime[ac_idx]);
        }
    } else {
        fprintf(stderr, "Invalid qstats command;\n");
        return -EINVAL;
    }
    printf("\n");

    return MLAN_STATUS_SUCCESS;
}

/** 
 *  @brief Get the current status of the WMM Queues
 *  
 *  Command: mlanconfig mlanX qstatus
 *
 *  Retrieve the following information for each AC if wmm is enabled:
 *        - WMM IE ACM Required
 *        - Firmware Flow Required 
 *        - Firmware Flow Established
 *        - Firmware Queue Enabled
 *  
 *  @param argc     number of arguments
 *  @param argv     A pointer to arguments array    
 *
 *  @return         MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int
process_wmm_qstatus(int argc, char *argv[])
{
    int ioctl_val, subioctl_val;
    struct iwreq iwr;
    wlan_ioctl_wmm_queue_status_t qstatus;
    mlan_wmm_ac_e acVal;

    if (get_priv_ioctl("qstatus",
                       &ioctl_val, &subioctl_val) == MLAN_STATUS_FAILURE) {
        return -EOPNOTSUPP;
    }

    memset(&qstatus, 0x00, sizeof(qstatus));

    strncpy(iwr.ifr_name, dev_name, IFNAMSIZ);
    iwr.u.data.flags = subioctl_val;
    iwr.u.data.pointer = (caddr_t) & qstatus;
    iwr.u.data.length = (sizeof(qstatus));

    if (ioctl(sockfd, ioctl_val, &iwr) < 0) {
        perror("mlanconfig: qstatus ioctl");
        return -EFAULT;
    }

    for (acVal = WMM_AC_BK; acVal <= WMM_AC_VO; acVal++) {
        switch (acVal) {
        case WMM_AC_BK:
            printf("BK: ");
            break;
        case WMM_AC_BE:
            printf("BE: ");
            break;
        case WMM_AC_VI:
            printf("VI: ");
            break;
        case WMM_AC_VO:
            printf("VO: ");
            break;
        default:
            printf("??: ");
        }

        printf("ACM[%c], FlowReq[%c], FlowCreated[%c], Enabled[%c],"
               " DE[%c], TE[%c]\n",
               (qstatus.acStatus[acVal].wmmAcm ? 'X' : ' '),
               (qstatus.acStatus[acVal].flowRequired ? 'X' : ' '),
               (qstatus.acStatus[acVal].flowCreated ? 'X' : ' '),
               (qstatus.acStatus[acVal].disabled ? ' ' : 'X'),
               (qstatus.acStatus[acVal].deliveryEnabled ? 'X' : ' '),
               (qstatus.acStatus[acVal].triggerEnabled ? 'X' : ' '));
    }

    return MLAN_STATUS_SUCCESS;
}

/** 
 *  @brief Get the current status of the WMM Traffic Streams
 *  
 *  Command: mlanconfig mlanX ts_status
 *
 *  @param argc     number of arguments
 *  @param argv     A pointer to arguments array    
 *
 *  @return         MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int
process_wmm_ts_status(int argc, char *argv[])
{
    int ioctl_val, subioctl_val;
    struct iwreq iwr;
    wlan_ioctl_wmm_ts_status_t ts_status;
    int tid;

    const char *ac_str_tbl[] = { "BK", "BE", "VI", "VO" };

    if (get_priv_ioctl("ts_status",
                       &ioctl_val, &subioctl_val) == MLAN_STATUS_FAILURE) {
        return -EOPNOTSUPP;
    }

    printf("\nTID   Valid    AC   UP   PSB   FlowDir  MediumTime\n");
    printf("---------------------------------------------------\n");

    for (tid = 0; tid <= 7; tid++) {
        memset(&ts_status, 0x00, sizeof(ts_status));
        ts_status.tid = tid;

        strncpy(iwr.ifr_name, dev_name, IFNAMSIZ);
        iwr.u.data.flags = subioctl_val;
        iwr.u.data.pointer = (caddr_t) & ts_status;
        iwr.u.data.length = (sizeof(ts_status));

        if (ioctl(sockfd, ioctl_val, &iwr) < 0) {
            perror("mlanconfig: ts_status ioctl");
            return -EFAULT;
        }

        printf(" %02d     %3s    %2s    %u     %c    ",
               ts_status.tid,
               (ts_status.valid ? "Yes" : "No"),
               (ts_status.valid ? ac_str_tbl[ts_status.accessCategory] : "--"),
               ts_status.userPriority, (ts_status.psb ? 'U' : 'L'));

        if ((ts_status.flowDir & 0x03) == 0) {
            printf("%s", " ---- ");
        } else {
            printf("%2s%4s",
                   (ts_status.flowDir & 0x01 ? "Up" : ""),
                   (ts_status.flowDir & 0x02 ? "Down" : ""));
        }

        printf("%12u\n", ts_status.mediumTime);
    }

    printf("\n");

    return MLAN_STATUS_SUCCESS;
}

/** 
 *  @brief Provides interface to perform read/write operations on regsiter
 *
 *  Command: mlanconfig mlanX regrdwr <type> <offset> [value]
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *
 *  @return         MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int
process_regrdwr(int argc, char *argv[])
{
    struct iwreq iwr;
    int ioctl_val, subioctl_val;
    t_u32 type, offset, value;
    t_u8 buf[100];
    HostCmd_DS_GEN *hostcmd = (HostCmd_DS_GEN *) buf;
    int ret = MLAN_STATUS_SUCCESS;

    /* Check arguments */
    if ((argc < 5) || (argc > 6)) {
        printf("Parameters for regrdwr: <type> <offset> [value]\n");
        return -EINVAL;
    }

    if (get_priv_ioctl("hostcmd",
                       &ioctl_val, &subioctl_val) == MLAN_STATUS_FAILURE) {
        return -EOPNOTSUPP;
    }

    type = a2hex_or_atoi(argv[3]);
    offset = a2hex_or_atoi(argv[4]);
    if (argc > 5)
        value = a2hex_or_atoi(argv[5]);
    if ((ret = prepare_hostcmd_regrdwr(type, offset,
                                       (argc > 5) ? &value : NULL, buf))) {
        return ret;
    }

    memset(&iwr, 0, sizeof(iwr));
    strncpy(iwr.ifr_name, dev_name, IFNAMSIZ);
    iwr.u.data.pointer = buf;
    iwr.u.data.length = le16_to_cpu(hostcmd->size);
    iwr.u.data.flags = 0;

    if (ioctl(sockfd, ioctl_val, &iwr)) {
        fprintf(stderr, "mlanconfig: MLANHOSTCMD is not supported by %s\n",
                dev_name);
        return MLAN_STATUS_FAILURE;
    }
    ret = process_host_cmd_resp(buf);

    return ret;
}

/********************************************************
		Global Functions
********************************************************/
/** 
 *  @brief Get one line from the File
 *  
 *  @param s	    Storage location for data.
 *  @param size 	Maximum number of characters to read. 
 *  @param line		A pointer to return current line number
 *  @return         returns string or NULL 
 */
t_s8 *
mlan_config_get_line(t_s8 * s, t_s32 size, int *line)
{
    t_s8 *pos, *end, *sstart;

    while (fgets(s, size, fp)) {
        (*line)++;
        s[size - 1] = '\0';
        pos = s;

        while (*pos == ' ' || *pos == '\t')
            pos++;
        if (*pos == '#' || (*pos == '\r' && *(pos + 1) == '\n') ||
            *pos == '\n' || *pos == '\0')
            continue;

        /* Remove # comments unless they are within a double quoted * string.
           Remove trailing white space. */
        sstart = strchr(pos, '"');
        if (sstart)
            sstart = strchr(sstart + 1, '"');
        if (!sstart)
            sstart = pos;
        end = strchr(sstart, '#');
        if (end)
            *end-- = '\0';
        else
            end = pos + strlen(pos) - 1;
        while (end > pos && (*end == '\r' || *end == '\n' ||
                             *end == ' ' || *end == '\t')) {
            *end-- = '\0';
        }
        if (*pos == '\0')
            continue;
        return pos;
    }
    return NULL;
}

/** 
 *  @brief parse hex data
 *  @param dst      A pointer to receive hex data
 *  @return         length of hex data
 */
int
fparse_for_hex(t_u8 * dst)
{
    t_s8 *ptr;
    t_u8 *dptr;
    t_s8 buf[256];

    dptr = dst;
    while (fgets(buf, sizeof(buf), fp)) {
        ptr = buf;

        while (*ptr) {
            /* skip leading spaces */
            while (*ptr && (isspace(*ptr) || *ptr == '\t'))
                ptr++;

            /* skip blank lines and lines beginning with '#' */
            if (*ptr == '\0' || *ptr == '#')
                break;

            if (isxdigit(*ptr)) {
                ptr = convert2hex(ptr, dptr++);
            } else {
                /* Invalid character on data line */
                ptr++;
            }
        }
    }

    return (dptr - dst);
}

#define STACK_NBYTES            	100     /**< Number of bytes in stack */
#define MAX_BYTESEQ 		       	6       /**< Maximum byte sequence */
#define TYPE_DNUM           		1 /**< decimal number */
#define TYPE_BYTESEQ        		2 /**< byte sequence */
#define MAX_OPERAND         		0x40    /**< Maximum operands */
#define TYPE_EQ         		(MAX_OPERAND+1) /**< byte comparison:    == operator */
#define TYPE_EQ_DNUM    		(MAX_OPERAND+2) /**< decimal comparison: =d operator */
#define TYPE_EQ_BIT     		(MAX_OPERAND+3) /**< bit comparison:     =b operator */
#define TYPE_AND        		(MAX_OPERAND+4) /**< && operator */
#define TYPE_OR         		(MAX_OPERAND+5) /**< || operator */
typedef struct
{
    t_u16 sp;                         /**< Stack pointer */
    t_u8 byte[STACK_NBYTES];          /**< Stack */
} stack_t;

typedef struct
{
    t_u8 type;                    /**< Type */
    t_u8 reserve[3];       /**< so 4-byte align val array */
    /* byte sequence is the largest among all the operands and operators. */
    /* byte sequence format: 1 byte of num of bytes, then variable num bytes */
    t_u8 val[MAX_BYTESEQ + 1];    /**< Value */
} op_t;

/** 
 *  @brief push data to stack
 *  
 *  @param s			a pointer to stack_t structure
 *  
 *  @param nbytes		number of byte to push to stack  
 *  
 *  @param val			a pointer to data buffer	
 *  
 *  @return			TRUE-- sucess , FALSE -- fail
 *  			
 */
static int
push_n(stack_t * s, t_u8 nbytes, t_u8 * val)
{
    if ((s->sp + nbytes) < STACK_NBYTES) {
        memcpy((void *) (s->byte + s->sp), (const void *) val, (size_t) nbytes);
        s->sp += nbytes;
        /* printf("push: n %d sp %d\n", nbytes, s->sp); */
        return TRUE;
    } else                      /* stack full */
        return FALSE;
}

/** 
 *  @brief push data to stack
 *  
 *  @param s			a pointer to stack_t structure
 *  
 *  @param op			a pointer to op_t structure  
 *  
 *  @return			TRUE-- sucess , FALSE -- fail
 *  			
 */
static int
push(stack_t * s, op_t * op)
{
    t_u8 nbytes;
    switch (op->type) {
    case TYPE_DNUM:
        if (push_n(s, 4, op->val))
            return (push_n(s, 1, &op->type));
        return FALSE;
    case TYPE_BYTESEQ:
        nbytes = op->val[0];
        if (push_n(s, nbytes, op->val + 1) &&
            push_n(s, 1, op->val) && push_n(s, 1, &op->type))
            return TRUE;
        return FALSE;
    default:
        return (push_n(s, 1, &op->type));
    }
}

/** 
 *  @brief parse RPN string
 *  
 *  @param s			a pointer to Null-terminated string to scan. 
 *  
 *  @param first_time		a pointer to return first_time  
 *  
 *  @return			A pointer to the last token found in string.   
 *  				NULL is returned when there are no more tokens to be found. 
 *  			
 */
static char *
getop(char *s, int *first_time)
{
    const char delim[] = " \t\n";
    char *p;
    if (*first_time) {
        p = strtok(s, delim);
        *first_time = FALSE;
    } else {
        p = strtok(NULL, delim);
    }
    return (p);
}

/** 
 *  @brief Verify hex digit.
 *  
 *  @param c			input ascii char 
 *  @param h			a pointer to return integer value of the digit char. 
 *  @return			TURE -- c is hex digit, FALSE -- c is not hex digit.
 */
static int
ishexdigit(char c, t_u8 * h)
{
    if (c >= '0' && c <= '9') {
        *h = c - '0';
        return (TRUE);
    } else if (c >= 'a' && c <= 'f') {
        *h = c - 'a' + 10;
        return (TRUE);
    } else if (c >= 'A' && c <= 'F') {
        *h = c - 'A' + 10;
        return (TRUE);
    }
    return (FALSE);
}

/** 
 *  @brief convert hex string to integer.
 *  
 *  @param s			A pointer to hex string, string length up to 2 digits. 
 *  @return			integer value.
 */
static t_u8
hex_atoi(char *s)
{
    int i;
    t_u8 digit;                 /* digital value */
    t_u8 t = 0;                 /* total value */

    for (i = 0, t = 0; ishexdigit(s[i], &digit) && i < 2; i++)
        t = 16 * t + digit;
    return (t);
}

/** 
 *  @brief Parse byte sequence in hex format string to a byte sequence.
 *  
 *  @param opstr		A pointer to byte sequence in hex format string, with ':' as delimiter between two byte. 
 *  @param val			A pointer to return byte sequence string
 *  @return			NA
 */
static void
parse_hex(char *opstr, t_u8 * val)
{
    char delim = ':';
    char *p;
    char *q;
    t_u8 i;

    /* +1 is for skipping over the preceding h character. */
    p = opstr + 1;

    /* First byte */
    val[1] = hex_atoi(p++);

    /* Parse subsequent bytes. */
    /* Each byte is preceded by the : character. */
    for (i = 1; *p; i++) {
        q = strchr(p, delim);
        if (!q)
            break;
        p = q + 1;
        val[i + 1] = hex_atoi(p);
    }
    /* Set num of bytes */
    val[0] = i;
}

/** 
 *  @brief str2bin, convert RPN string to binary format
 *  
 *  @param str			A pointer to rpn string
 *  @param stack		A pointer to stack_t structure
 *  @return			MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int
str2bin(char *str, stack_t * stack)
{
    int first_time;
    char *opstr;
    op_t op;                    /* operator/operand */
    int dnum;
    int ret = MLAN_STATUS_SUCCESS;

    memset(stack, 0, sizeof(stack_t));
    first_time = TRUE;
    while ((opstr = getop(str, &first_time)) != NULL) {
        if (isdigit(*opstr)) {
            op.type = TYPE_DNUM;
            dnum = atoi(opstr);
            memcpy((t_u8 *) op.val, &dnum, sizeof(dnum));
            if (!push(stack, &op)) {
                printf("push decimal number failed\n");
                ret = MLAN_STATUS_FAILURE;
                break;
            }
        } else if (*opstr == 'h') {
            op.type = TYPE_BYTESEQ;
            parse_hex(opstr, op.val);
            if (!push(stack, &op)) {
                printf("push byte sequence failed\n");
                ret = MLAN_STATUS_FAILURE;
                break;
            }
        } else if (!strcmp(opstr, "==")) {
            op.type = TYPE_EQ;
            if (!push(stack, &op)) {
                printf("push byte cmp operator failed\n");
                ret = MLAN_STATUS_FAILURE;
                break;
            }
        } else if (!strcmp(opstr, "=d")) {
            op.type = TYPE_EQ_DNUM;
            if (!push(stack, &op)) {
                printf("push decimal cmp operator failed\n");
                ret = MLAN_STATUS_FAILURE;
                break;
            }
        } else if (!strcmp(opstr, "=b")) {
            op.type = TYPE_EQ_BIT;
            if (!push(stack, &op)) {
                printf("push bit cmp operator failed\n");
                ret = MLAN_STATUS_FAILURE;
                break;
            }
        } else if (!strcmp(opstr, "&&")) {
            op.type = TYPE_AND;
            if (!push(stack, &op)) {
                printf("push AND operator failed\n");
                ret = MLAN_STATUS_FAILURE;
                break;
            }
        } else if (!strcmp(opstr, "||")) {
            op.type = TYPE_OR;
            if (!push(stack, &op)) {
                printf("push OR operator failed\n");
                ret = MLAN_STATUS_FAILURE;
                break;
            }
        } else {
            printf("Unknown operand\n");
            ret = MLAN_STATUS_FAILURE;
            break;
        }
    }
    return ret;
}

#define FILTER_BYTESEQ 		TYPE_EQ /**< byte sequence */
#define FILTER_DNUM    		TYPE_EQ_DNUM /**< decimal number */
#define FILTER_BITSEQ		TYPE_EQ_BIT /**< bit sequence */
#define FILTER_TEST		FILTER_BITSEQ+1 /**< test */

#define NAME_TYPE		1           /**< Field name 'type' */
#define NAME_PATTERN		2       /**< Field name 'pattern' */
#define NAME_OFFSET		3           /**< Field name 'offset' */
#define NAME_NUMBYTE		4       /**< Field name 'numbyte' */
#define NAME_REPEAT		5           /**< Field name 'repeat' */
#define NAME_BYTE		6           /**< Field name 'byte' */
#define NAME_MASK		7           /**< Field name 'mask' */
#define NAME_DEST		8           /**< Field name 'dest' */

static struct mef_fields
{
    t_s8 *name;
              /**< Name */
    t_s8 nameid;/**< Name Id. */
} mef_fields[] = {
    {
    "type", NAME_TYPE}, {
    "pattern", NAME_PATTERN}, {
    "offset", NAME_OFFSET}, {
    "numbyte", NAME_NUMBYTE}, {
    "repeat", NAME_REPEAT}, {
    "byte", NAME_BYTE}, {
    "mask", NAME_MASK}, {
    "dest", NAME_DEST}
};

/** 
 *  @brief get filter data
 *  
 *  @param fp			A pointer to file stream
 *  @param ln			A pointer to line number
 *  @param buf			A pointer to hostcmd data
 *  @param size			A pointer to the return size of hostcmd buffer
 *  @return			MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int
mlan_get_filter_data(FILE * fp, int *ln, t_u8 * buf, t_u16 * size)
{
    t_s32 errors = 0, i;
    t_s8 line[256], *pos, *pos1;
    t_u16 type = 0;
    t_u32 pattern = 0;
    t_u16 repeat = 0;
    t_u16 offset = 0;
    t_s8 byte_seq[50];
    t_s8 mask_seq[50];
    t_u16 numbyte = 0;
    t_s8 type_find = 0;
    t_s8 pattern_find = 0;
    t_s8 offset_find = 0;
    t_s8 numbyte_find = 0;
    t_s8 repeat_find = 0;
    t_s8 byte_find = 0;
    t_s8 mask_find = 0;
    t_s8 dest_find = 0;
    t_s8 dest_seq[50];

    *size = 0;
    while ((pos = mlan_config_get_line(line, sizeof(line), ln))) {
        if (strcmp(pos, "}") == 0) {
            break;
        }
        pos1 = strchr(pos, '=');
        if (pos1 == NULL) {
            printf("Line %d: Invalid mef_filter line '%s'\n", *ln, pos);
            errors++;
            continue;
        }
        *pos1++ = '\0';
        for (i = 0; i < NELEMENTS(mef_fields); i++) {
            if (strncmp(pos, mef_fields[i].name, strlen(mef_fields[i].name)) ==
                0) {
                switch (mef_fields[i].nameid) {
                case NAME_TYPE:
                    type = a2hex_or_atoi(pos1);
                    if ((type != FILTER_DNUM) && (type != FILTER_BYTESEQ)
                        && (type != FILTER_BITSEQ) && (type != FILTER_TEST)) {
                        printf("Invalid filter type:%d\n", type);
                        return MLAN_STATUS_FAILURE;
                    }
                    type_find = 1;
                    break;
                case NAME_PATTERN:
                    pattern = a2hex_or_atoi(pos1);
                    pattern_find = 1;
                    break;
                case NAME_OFFSET:
                    offset = a2hex_or_atoi(pos1);
                    offset_find = 1;
                    break;
                case NAME_NUMBYTE:
                    numbyte = a2hex_or_atoi(pos1);
                    numbyte_find = 1;
                    break;
                case NAME_REPEAT:
                    repeat = a2hex_or_atoi(pos1);
                    repeat_find = 1;
                    break;
                case NAME_BYTE:
                    memset(byte_seq, 0, sizeof(byte_seq));
                    strcpy(byte_seq, pos1);
                    byte_find = 1;
                    break;
                case NAME_MASK:
                    memset(mask_seq, 0, sizeof(mask_seq));
                    strcpy(mask_seq, pos1);
                    mask_find = 1;
                    break;
                case NAME_DEST:
                    memset(dest_seq, 0, sizeof(dest_seq));
                    strcpy(dest_seq, pos1);
                    dest_find = 1;
                    break;
                }
                break;
            }
        }
        if (i == NELEMENTS(mef_fields)) {
            printf("Line %d: unknown mef field '%s'.\n", *line, pos);
            errors++;
        }
    }
    if (type_find == 0) {
        printf("Can not find filter type\n");
        return MLAN_STATUS_FAILURE;
    }
    switch (type) {
    case FILTER_DNUM:
        if (!pattern_find || !offset_find || !numbyte_find) {
            printf
                ("Missing field for FILTER_DNUM: pattern=%d,offset=%d,numbyte=%d\n",
                 pattern_find, offset_find, numbyte_find);
            return MLAN_STATUS_FAILURE;
        }
        memset(line, 0, sizeof(line));
        sprintf(line, "%ld %d %d =d ", pattern, offset, numbyte);
        break;
    case FILTER_BYTESEQ:
        if (!byte_find || !offset_find || !repeat_find) {
            printf
                ("Missing field for FILTER_BYTESEQ: byte=%d,offset=%d,repeat=%d\n",
                 byte_find, offset_find, repeat_find);
            return MLAN_STATUS_FAILURE;
        }
        memset(line, 0, sizeof(line));
        sprintf(line, "%d h%s %d == ", repeat, byte_seq, offset);
        break;
    case FILTER_BITSEQ:
        if (!byte_find || !offset_find || !mask_find) {
            printf
                ("Missing field for FILTER_BYTESEQ: byte=%d,offset=%d,repeat=%d\n",
                 byte_find, offset_find, mask_find);
            return MLAN_STATUS_FAILURE;
        }
        if (strlen(byte_seq) != strlen(mask_seq)) {
            printf("byte string's length is different with mask's length!\n");
            return MLAN_STATUS_FAILURE;
        }
        memset(line, 0, sizeof(line));
        sprintf(line, "h%s %d h%s =b ", byte_seq, offset, mask_seq);
        break;
    case FILTER_TEST:
        if (!byte_find || !offset_find || !repeat_find || !dest_find) {
            printf
                ("Missing field for FILTER_TEST: byte=%d,offset=%d,repeat=%d,dest=%d\n",
                 byte_find, offset_find, repeat_find, dest_find);
            return MLAN_STATUS_FAILURE;
        }
        memset(line, 0, sizeof(line));
        sprintf(line, "h%s %d h%s %d ", dest_seq, repeat, byte_seq, offset);
        break;
    }
    memcpy(buf, line, strlen(line));
    *size = strlen(line);
    return MLAN_STATUS_SUCCESS;
}

#define NAME_MODE	1       /**< Field name 'mode' */
#define NAME_ACTION	2       /**< Field name 'action' */
#define NAME_FILTER_NUM	3   /**< Field name 'filter_num' */
#define NAME_RPN	4       /**< Field name 'RPN' */
static struct mef_entry_fields
{
    t_s8 *name;
              /**< Name */
    t_s8 nameid;/**< Name id */
} mef_entry_fields[] = {
    {
    "mode", NAME_MODE}, {
    "action", NAME_ACTION}, {
    "filter_num", NAME_FILTER_NUM}, {
"RPN", NAME_RPN},};

typedef struct _MEF_ENTRY
{
    /** Mode */
    t_u8 Mode;
    /** Size */
    t_u8 Action;
    /** Size of expression */
    t_u16 ExprSize;
} MEF_ENTRY;

/** 
 *  @brief get mef_entry data
 *  
 *  @param fp			A pointer to file stream
 *  @param ln			A pointer to line number
 *  @param buf			A pointer to hostcmd data
 *  @param size			A pointer to the return size of hostcmd buffer
 *  @return			MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int
mlan_get_mef_entry_data(FILE * fp, int *ln, t_u8 * buf, t_u16 * size)
{
    t_s8 line[256], *pos, *pos1;
    t_u8 mode, action, filter_num = 0;
    t_s8 rpn[256];
    t_s8 mode_find = 0;
    t_s8 action_find = 0;
    t_s8 filter_num_find = 0;
    t_s8 rpn_find = 0;
    t_s8 rpn_str[256];
    int rpn_len = 0;
    t_s8 filter_name[50];
    t_s8 name_found = 0;
    t_u16 len = 0;
    int i;
    int first_time = TRUE;
    char *opstr;
    t_s8 filter_action[10];
    t_s32 errors = 0;
    MEF_ENTRY *pMefEntry = (MEF_ENTRY *) buf;
    stack_t stack;
    while ((pos = mlan_config_get_line(line, sizeof(line), ln))) {
        if (strcmp(pos, "}") == 0) {
            break;
        }
        pos1 = strchr(pos, '=');
        if (pos1 == NULL) {
            printf("Line %d: Invalid mef_entry line '%s'\n", *ln, pos);
            errors++;
            continue;
        }
        *pos1++ = '\0';
        if (!mode_find || !action_find || !filter_num_find || !rpn_find) {
            for (i = 0; i < NELEMENTS(mef_entry_fields); i++) {
                if (strncmp
                    (pos, mef_entry_fields[i].name,
                     strlen(mef_entry_fields[i].name)) == 0) {
                    switch (mef_entry_fields[i].nameid) {
                    case NAME_MODE:
                        mode = a2hex_or_atoi(pos1);
                        if (mode & ~0x7) {
                            printf("invalid mode=%d\n", mode);
                            return MLAN_STATUS_FAILURE;
                        }
                        pMefEntry->Mode = mode;
                        mode_find = 1;
                        break;
                    case NAME_ACTION:
                        action = a2hex_or_atoi(pos1);
                        if (action & ~0xff) {
                            printf("invalid action=%d\n", action);
                            return MLAN_STATUS_FAILURE;
                        }
                        pMefEntry->Action = action;
                        action_find = 1;
                        break;
                    case NAME_FILTER_NUM:
                        filter_num = a2hex_or_atoi(pos1);
                        filter_num_find = 1;
                        break;
                    case NAME_RPN:
                        memset(rpn, 0, sizeof(rpn));
                        strcpy(rpn, pos1);
                        rpn_find = 1;
                        break;
                    }
                    break;
                }
            }
            if (i == NELEMENTS(mef_fields)) {
                printf("Line %d: unknown mef_entry field '%s'.\n", *line, pos);
                return MLAN_STATUS_FAILURE;
            }
        }
        if (mode_find && action_find && filter_num_find && rpn_find) {
            for (i = 0; i < filter_num; i++) {
                opstr = getop(rpn, &first_time);
                if (opstr == NULL)
                    break;
                sprintf(filter_name, "%s={", opstr);
                name_found = 0;
                while ((pos = mlan_config_get_line(line, sizeof(line), ln))) {
                    if (strncmp(pos, filter_name, strlen(filter_name)) == 0) {
                        name_found = 1;
                        break;
                    }
                }
                if (!name_found) {
                    fprintf(stderr, "mlanconfig: %s not found in file\n",
                            filter_name);
                    return MLAN_STATUS_FAILURE;
                }
                if (MLAN_STATUS_FAILURE ==
                    mlan_get_filter_data(fp, ln, (t_u8 *) (rpn_str + rpn_len),
                                         &len))
                    break;
                rpn_len += len;
                if (i > 0) {
                    memcpy(rpn_str + rpn_len, filter_action,
                           strlen(filter_action));
                    rpn_len += strlen(filter_action);
                }
                opstr = getop(rpn, &first_time);
                if (opstr == NULL)
                    break;
                memset(filter_action, 0, sizeof(filter_action));
                sprintf(filter_action, "%s ", opstr);
            }
            /* Remove the last space */
            if (rpn_len > 0) {
                rpn_len--;
                rpn_str[rpn_len] = 0;
            }
            if (MLAN_STATUS_FAILURE == str2bin(rpn_str, &stack)) {
                printf("Fail on str2bin!\n");
                return MLAN_STATUS_FAILURE;
            }
            *size = sizeof(MEF_ENTRY);
            pMefEntry->ExprSize = stack.sp;
            memmove(buf + sizeof(MEF_ENTRY), stack.byte, stack.sp);
            *size += stack.sp;
            break;
        } else if (mode_find && action_find && filter_num_find &&
                   (filter_num == 0)) {
            pMefEntry->ExprSize = 0;
            *size = sizeof(MEF_ENTRY);
            break;
        }
    }
    return MLAN_STATUS_SUCCESS;
}

#define MEFCFG_CMDCODE	0x009a
/**
 *  @brief Process mef cfg 
 *  @param argc     number of arguments
 *  @param argv     A pointer to arguments array    
 *  @return         MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int
process_mef_cfg(int argc, char *argv[])
{
    int ioctl_val, subioctl_val;
    t_s8 line[256], cmdname[256], *pos;
    int cmdname_found = 0, name_found = 0;
    int ln = 0;
    int ret = MLAN_STATUS_SUCCESS;
    int i;
    t_u8 *buf;
    t_u16 buf_len = 0;
    t_u16 len;
    struct iwreq iwr;
    HostCmd_DS_MEF_CFG *mefcmd;
    HostCmd_DS_GEN *hostcmd;

    if (argc < 4) {
        printf("Error: invalid no of arguments\n");
        printf("Syntax: ./mlanconfig eth1 mefcfg <mef.conf>\n");
        exit(1);
    }
    if (get_priv_ioctl("hostcmd",
                       &ioctl_val, &subioctl_val) == MLAN_STATUS_FAILURE) {
        return -EOPNOTSUPP;
    }
    sprintf(cmdname, "%s={", argv[2]);
    cmdname_found = 0;
    if ((fp = fopen(argv[3], "r")) == NULL) {
        fprintf(stderr, "Cannot open file %s\n", argv[4]);
        exit(1);
    }

    buf = (t_u8 *) malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
    if (buf == NULL) {
        fclose(fp);
        fprintf(stderr, "Cannot alloc memory\n");
        exit(1);
    }
    memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);

    hostcmd = (HostCmd_DS_GEN *) (buf);
    hostcmd->command = MEFCFG_CMDCODE;
    mefcmd = (HostCmd_DS_MEF_CFG *) (buf + S_DS_GEN);
    buf_len = sizeof(HostCmd_DS_MEF_CFG) + S_DS_GEN;

    while ((pos = mlan_config_get_line(line, sizeof(line), &ln))) {
        if (strcmp(pos, cmdname) == 0) {
            cmdname_found = 1;
            sprintf(cmdname, "Criteria=");
            name_found = 0;
            while ((pos = mlan_config_get_line(line, sizeof(line), &ln))) {
                if (strncmp(pos, cmdname, strlen(cmdname)) == 0) {
                    name_found = 1;
                    mefcmd->Criteria = a2hex_or_atoi(pos + strlen(cmdname));
                    break;
                }
            }
            if (!name_found) {
                fprintf(stderr, "mlanconfig: criteria not found in file '%s'\n",
                        argv[3]);
                break;
            }
            sprintf(cmdname, "NumEntries=");
            name_found = 0;
            while ((pos = mlan_config_get_line(line, sizeof(line), &ln))) {
                if (strncmp(pos, cmdname, strlen(cmdname)) == 0) {
                    name_found = 1;
                    mefcmd->NumEntries = a2hex_or_atoi(pos + strlen(cmdname));
                    break;
                }
            }
            if (!name_found) {
                fprintf(stderr,
                        "mlanconfig: NumEntries not found in file '%s'\n",
                        argv[3]);
                break;
            }
            for (i = 0; i < mefcmd->NumEntries; i++) {
                sprintf(cmdname, "mef_entry_%d={", i);
                name_found = 0;
                while ((pos = mlan_config_get_line(line, sizeof(line), &ln))) {
                    if (strncmp(pos, cmdname, strlen(cmdname)) == 0) {
                        name_found = 1;
                        break;
                    }
                }
                if (!name_found) {
                    fprintf(stderr, "mlanconfig: %s not found in file '%s'\n",
                            cmdname, argv[3]);
                    break;
                }
                if (MLAN_STATUS_FAILURE ==
                    mlan_get_mef_entry_data(fp, &ln, buf + buf_len, &len)) {
                    ret = MLAN_STATUS_FAILURE;
                    break;
                }
                buf_len += len;
            }
            break;
        }
    }
    fclose(fp);
    /* hexdump("mef_cfg",buf,buf_len, ' '); */
    if (!cmdname_found)
        fprintf(stderr, "mlanconfig: cmdname '%s' not found in file '%s'\n",
                argv[4], argv[3]);

    if (!cmdname_found || !name_found) {
        ret = MLAN_STATUS_FAILURE;
        goto mef_exit;
    }
    hostcmd->size = buf_len;

    strncpy(iwr.ifr_name, dev_name, IFNAMSIZ);
    iwr.u.data.pointer = buf;
    iwr.u.data.length = buf_len;
    iwr.u.data.flags = 0;
    if (ioctl(sockfd, ioctl_val, &iwr)) {
        fprintf(stderr, "mlanconfig: MEFCFG is not supported by %s\n",
                dev_name);
        ret = MLAN_STATUS_FAILURE;
        goto mef_exit;
    }
    ret = process_host_cmd_resp(buf);

  mef_exit:
    if (buf)
        free(buf);
    return ret;

}

/** 
 *  @brief Entry function for mlanconfig
 *  @param argc		number of arguments
 *  @param argv     A pointer to arguments array    
 *  @return      	MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
int
main(int argc, char *argv[])
{
    t_s32 cmd;

    if ((argc == 2) && (strcmp(argv[1], "-v") == 0)) {
        fprintf(stdout, "Marvell mlanconfig version %s\n", MLANCONFIG_VER);
        exit(0);
    }
    if (argc < 3) {
        fprintf(stderr, "Invalid number of parameters!\n");
        display_usage();
        exit(1);
    }

    strncpy(dev_name, argv[1], IFNAMSIZ);

    /* 
     * create a socket 
     */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "mlanconfig: Cannot open socket.\n");
        exit(1);
    }
    if (get_range() < 0) {
        fprintf(stderr, "mlanconfig: Cannot get range.\n");
        exit(1);
    }
    switch ((cmd = findcommand(NELEMENTS(commands), commands, argv[2]))) {
    case CMD_HOSTCMD:
        process_host_cmd(argc, argv);
        break;
    case CMD_MEFCFG:
        process_mef_cfg(argc, argv);
        break;
    case CMD_ARPFILTER:
        process_arpfilter(argc, argv);
        break;
    case CMD_CFG_DATA:
        process_cfg_data(argc, argv);
        break;
    case CMD_CMD52RW:
        process_sdcmd52rw(argc, argv);
        break;
    case CMD_CMD53RW:
        process_sdcmd53rw(argc, argv);
        break;
    case CMD_GET_SCAN_RSP:
        process_getscantable(argc, argv);
        break;
    case CMD_SET_USER_SCAN:
        process_setuserscan(argc, argv);
        break;
    case CMD_ADD_TS:
        process_addts(argc, argv);
        break;
    case CMD_DEL_TS:
        process_delts(argc, argv);
        break;
    case CMD_QCONFIG:
        process_qconfig(argc, argv);
        break;
    case CMD_QSTATS:
        process_qstats(argc, argv);
        break;
    case CMD_TS_STATUS:
        process_wmm_ts_status(argc, argv);
        break;
    case CMD_WMM_QSTATUS:
        process_wmm_qstatus(argc, argv);
        break;
    case CMD_REGRW:
        process_regrdwr(argc, argv);
        break;
    default:
        fprintf(stderr, "Invalid command specified!\n");
        display_usage();
        exit(1);
    }

    return MLAN_STATUS_SUCCESS;
}
