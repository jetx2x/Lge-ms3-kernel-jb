/* arch/arm/mach-msm/lge/lge_ats_eta.c
 *
 * Copyright (C) 2010 LGE, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <mach/msm_rpcrouter.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <mach/board_lge.h>
#include <mach/lge_at.h>
#include <linux/slab.h>
#include <mach/lge_ats.h>
#include <mach/lge_diag_test.h>
#include <linux/delay.h>

#include <mach/lge_backup_items.h>  // LS696_me : CAL BACKUP merge
#define JIFFIES_TO_MS(t) ((t) * 1000 / HZ)

/* LGE_CHANGE
 * Support MTC using diag port 
 * 2010-07-11 taehung.kim@lge.com
 */
#ifdef CONFIG_LGE_DIAG_MTC
extern unsigned char g_diag_mtc_check;
#endif

unsigned int ats_mtc_log_mask = 0x00000000;

int base64_decode(char *, unsigned char *, int);
int base64_encode(char *, int, char *);

extern int event_log_start(void);
extern int event_log_end(void);

#define ETA_CMD_STR "/system/bin/eta"
#define ETA_SHELL_STR "/system/bin/sh"

int eta_execute_n(char *string, size_t size, int wait)
{
	int ret;
	static char cmdstr[LIMIT_MAX_SEND_SIZE_BUFFER];

	char *envp[] = {
		"HOME=/",
		"TERM=linux",
		NULL,
	};

	char *argv[] = {
		ETA_CMD_STR,
		NULL,
		NULL,
	};

	size += 1;

//	if (!(cmdstr = kmalloc(size, GFP_KERNEL)))
//	{
//		return ENOMEM;
//	}

	argv[1] = cmdstr;
	memset(cmdstr, 0, sizeof(cmdstr));

	snprintf(cmdstr, size, "%s", string);
	printk(KERN_INFO "[ETA]execute eta : data - %s\n", string);

	if (wait == 1)
		ret = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	else
		ret = call_usermodehelper(argv[0], argv, envp, UMH_NO_WAIT);
	if (ret != 0) {
		printk(KERN_ERR "[ETA]Eta failed to run \": %i\n", ret);
	}
	else
	{
		printk(KERN_INFO "[ETA]execute ok, ret = %d\n", ret);
	}

	if (!wait)
		msleep(100);

//	kfree(cmdstr);
	return ret;
}

/*------ Base64 Encoding Table ------*/
const char MimeBase64[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	 '4', '5', '6', '7', '8', '9', '+', '/'
};

/*------ Base64 Decoding Table ------*/
static int DecodeMimeBase64[256] = {
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* 00-0F */
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* 10-1F */
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63, /* 20-2F */ 
	52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1, /* 30-3F */ 
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14, /* 40-4F */ 
	15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1, /* 50-5F */ 
	-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40, /* 60-6F */ 
	41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1, /* 70-7F */ 
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* 80-8F */ 
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* 90-9F */ 
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* A0-AF */ 
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* B0-BF */ 
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* C0-CF */ 
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* D0-DF */ 
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* E0-EF */ 
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  /* F0-FF */ 
};

int base64_decode(char *text, unsigned char *dst, int numBytes)
{
	const char* cp; 
	int space_idx = 0, phase; 
	int d, prev_d = 0; 
	unsigned char c;

	printk(KERN_INFO "[ETA] text: 0x%X, dst: 0x%X, size: %d\n",  (unsigned int)text, (unsigned int)dst, numBytes);
		
	space_idx = 0; 
	phase = 0;
	
	for ( cp = text; *cp != '\0'; ++cp ) { 
		d = DecodeMimeBase64[(int) *cp]; 
	    if ( d != -1 ) { 
	    	switch ( phase ) { 
	        	case 0: 
		        	++phase; 
	        		break; 
	        	case 1: 
	          		c = ( ( prev_d << 2 ) | ( ( d & 0x30 ) >> 4 ) );
				printk(KERN_INFO "[ETA] space_idx: 0x%X, char: 0x%X\n",  space_idx, c);
	          		if ( space_idx < numBytes )
		            dst[space_idx++] = c; 
			        ++phase; 
	    		    break; 
		        case 2: 
		        	c = ( ( ( prev_d & 0xf ) << 4 ) | ( ( d & 0x3c ) >> 2 ) );
				printk(KERN_INFO "[ETA] space_idx: 0x%X, char: 0x%X\n",  space_idx, c);
			        if ( space_idx < numBytes ) 
	         		dst[space_idx++] = c; 
	          		++phase; 
	          		break; 
	        	case 3: 
	          		c = ( ( ( prev_d & 0x03 ) << 6 ) | d ); 
				printk(KERN_INFO "[ETA] space_idx: 0x%X, char: 0x%X\n",  space_idx, c);
	          		if ( space_idx < numBytes ) 
	            	dst[space_idx++] = c; 
	          		phase = 0; 
	          		break; 
			} 	
	      	prev_d = d; 
		}
	}
	printk(KERN_INFO "[ETA] Complete..\n");
	return space_idx;
}

int base64_encode(char *text, int numBytes, char *encodedText)
{
	unsigned char input[3] = {0,0,0}; 
	unsigned char output[4] = {0,0,0,0}; 
	int  index, i, j; 
	char *p, *plen; 

	plen = text + numBytes - 1; 

	j = 0;

	for (i = 0, p = text;p <= plen; i++, p++) { 
	    index = i % 3; 
	    input[index] = *p;

		if (index == 2 || p == plen) { 
	    	output[0] = ((input[0] & 0xFC) >> 2); 
			output[1] = ((input[0] & 0x3) << 4) | ((input[1] & 0xF0) >> 4); 
			output[2] = ((input[1] & 0xF) << 2) | ((input[2] & 0xC0) >> 6); 
			output[3] = (input[2] & 0x3F);

			encodedText[j++] = MimeBase64[output[0]]; 
			encodedText[j++] = MimeBase64[output[1]]; 
			encodedText[j++] = index == 0? '=' : MimeBase64[output[2]]; 
			encodedText[j++] = index < 2? '=' : MimeBase64[output[3]];

			input[0] = input[1] = input[2] = 0; 
		} 
	}
	encodedText[j] = '\0';

	return strlen(encodedText); 	
}

void ats_mtc_send_key_log_to_eta(struct ats_mtc_key_log_type* p_ats_mtc_key_log)
{
	unsigned char *eta_cmd_buf;
	unsigned char *eta_cmd_buf_encoded;
	int index =0;
	int lenb64 = 0;
	int exec_result = 0;
	unsigned long long eta_time_val = 0;
	
	eta_cmd_buf = kmalloc(sizeof(unsigned char)*50, GFP_KERNEL);
	if(!eta_cmd_buf) {
		printk(KERN_ERR "%s: Error in alloc memory!!\n", __func__);
		return;
	}
	eta_cmd_buf_encoded = kmalloc(sizeof(unsigned char)*50, GFP_KERNEL);
	if(!eta_cmd_buf_encoded) {
		printk(KERN_ERR "%s: Error in alloc memory!!\n", __func__);
		kfree(eta_cmd_buf);
		return;
	}
	memset(eta_cmd_buf,0x00, 50);
	memset(eta_cmd_buf_encoded,0x00, 50);
				
	index = 0;
	eta_cmd_buf[index++] = (unsigned char)0xF0; //MTC_CMD_CODE
	eta_cmd_buf[index++] = (unsigned char)0x08; //MTC_LOG_REQ_CMD

	eta_cmd_buf[index++] = (unsigned char)p_ats_mtc_key_log->log_id; //LOG_ID, 1 key, 2 touch
	eta_cmd_buf[index++] = (unsigned char)p_ats_mtc_key_log->log_len; //LOG_LEN
	eta_cmd_buf[index++] = (unsigned char)0; //LOG_LEN

	eta_time_val = (unsigned long long)JIFFIES_TO_MS(jiffies);
	eta_cmd_buf[index++] = (unsigned char)(eta_time_val & 0xff); //LSB
	eta_cmd_buf[index++] = (unsigned char)( (eta_time_val >> 8) & 0xff );
	eta_cmd_buf[index++] = (unsigned char)( (eta_time_val >> 16) & 0xff );
	eta_cmd_buf[index++] = (unsigned char)( (eta_time_val >> 24) & 0xff );
	eta_cmd_buf[index++] = (unsigned char)( (eta_time_val >> 32) & 0xff );
	eta_cmd_buf[index++] = (unsigned char)( (eta_time_val >> 40) & 0xff );
	eta_cmd_buf[index++] = (unsigned char)( (eta_time_val >> 48) & 0xff );
	eta_cmd_buf[index++] = (unsigned char)( (eta_time_val >> 56) & 0xff ); // MSB

	index = 13;
	if(p_ats_mtc_key_log->log_id == ATS_MTC_KEY_LOG_ID_KEY)
	{
		eta_cmd_buf[index++] = (unsigned char)((p_ats_mtc_key_log->x_hold)&0xFF);// hold
		eta_cmd_buf[index++] = (unsigned char)((p_ats_mtc_key_log->y_code)&0xFF);//key code

		for(index = 15; index<23; index++) // ACTIVE_UIID 8
		{
			eta_cmd_buf[index] = 0;
		}
	}
	else if(p_ats_mtc_key_log->log_id == ATS_MTC_KEY_LOG_ID_TOUCH)
	{
		eta_cmd_buf[index++] = (unsigned char)1; // MAIN LCD
		eta_cmd_buf[index++] = (unsigned char)p_ats_mtc_key_log->action;
		eta_cmd_buf[index++] = (unsigned char)((p_ats_mtc_key_log->x_hold)&0xFF);// index = 15
		eta_cmd_buf[index++] = (unsigned char)(((p_ats_mtc_key_log->x_hold)>>8)&0xFF);// index = 16
		eta_cmd_buf[index++] = (unsigned char)((p_ats_mtc_key_log->y_code)&0xFF);// index = 17
		eta_cmd_buf[index++] = (unsigned char)(((p_ats_mtc_key_log->y_code)>>8)&0xFF);// index = 18

		for(index = 19; index<27; index++) // ACTIVE_UIID 8
		{
			eta_cmd_buf[index] = 0;
		}
	}

	lenb64 = base64_encode((char *)eta_cmd_buf, index, (char *)eta_cmd_buf_encoded);
			
	exec_result = eta_execute(eta_cmd_buf_encoded);
	printk(KERN_INFO "[ETA]AT+MTC exec_result %d\n",exec_result);

	kfree(eta_cmd_buf);
	kfree(eta_cmd_buf_encoded);

}
EXPORT_SYMBOL(ats_mtc_send_key_log_to_eta);

// LS696_me : CAL BACKUP merge
#define MMC_XCALBACKUP_DATA_SIZE	40276 // therm add //5332 //add xo //5256 //hdet v2 //5220

typedef struct MmcPartition MmcPartition;

struct MmcPartition {
    char *device_index;
    char *filesystem;
    char *name;
    unsigned dstatus;
    unsigned dtype ;
    unsigned dfirstsec;
    unsigned dsize;
};

extern int lge_erase_block(int secnum, size_t size);
extern int lge_write_block(int secnum, unsigned char *buf, size_t size);
extern int lge_read_block(int secnum, unsigned char *buf, size_t size);
extern int lge_mmc_scan_partitions(void);
extern const MmcPartition *lge_mmc_find_partition_by_name(const char *name);
extern void lge_mmc_print_partition_status(void);
// END: 0010090 sehyuny.kim@lge.com 2010-10-21

int lge_ats_handle_atcmd_eta(struct msm_rpc_server *server,
								 struct rpc_request_hdr *req, unsigned len)
{
	int result = HANDLE_OK;
	int loop = 0;
	char ret_string[MAX_STRING_RET];
	uint32_t ret_value1 =0;
	uint32_t ret_value2 = 0;
	static AT_SEND_BUFFER_t totalBuffer[LIMIT_MAX_SEND_SIZE_BUFFER];
	static uint32_t totalBufferSize = 0;
	uint32_t at_cmd,at_act;
	int len_b64;
	char *decoded_params;
	unsigned char b0;
	unsigned char b1;
	unsigned char b2;
	unsigned char b3;
	unsigned long logmask = 0x00;
	struct rpc_ats_atcmd_eta_args *args = (struct rpc_ats_atcmd_eta_args *)(req + 1);

	memset(server->retvalue.ret_string, 0, sizeof(server->retvalue.ret_string));

	memset (ret_string, 0, sizeof(ret_string));

	/* init for LARGE Buffer */
	if(args->sendNum == 0)
	{
		// init when first send
		memset(totalBuffer, 0, sizeof(totalBuffer));
		totalBufferSize = 0;
	}
	
	args->at_cmd = be32_to_cpu(args->at_cmd);
	args->at_act = be32_to_cpu(args->at_act);
	args->sendNum = be32_to_cpu(args->sendNum);
	args->endofBuffer = be32_to_cpu(args->endofBuffer);
	args->buffersize = be32_to_cpu(args->buffersize);
		
	printk(KERN_INFO "[ETA]handle_misc_rpc_call at_cmd = 0x%X, at_act=%d, sendNum=%d:\n",
	      args->at_cmd, args->at_act,args->sendNum);
	printk(KERN_INFO "[ETA]handle_misc_rpc_call endofBuffer = %d, buffersize=%d:\n",
	      args->endofBuffer, args->buffersize);
	printk(KERN_INFO "[ETA]input buff[0] = 0x%X,buff[1]=0x%X,buff[2]=0x%X:\n",args->buffer[0],args->buffer[1],args->buffer[2]);
	if(args->sendNum < MAX_SEND_LOOP_NUM)
	{
		for(loop = 0; loop < args->buffersize; loop++)
		{
			// totalBuffer[MAX_SEND_SIZE_BUFFER*args->sendNum + loop] =  be32_to_cpu(args->buffer[loop]);
			totalBuffer[MAX_SEND_SIZE_BUFFER*args->sendNum + loop] =  (args->buffer[loop]);
		}
		
		// memcpy(totalBuffer + MAX_SEND_SIZE_BUFFER*args->sendNum, args->buffer, args->buffersize);
		totalBufferSize += args->buffersize;
			
	}
	printk(KERN_INFO "[ETA]handle_misc_rpc_call buff[0] = 0x%X, buff[1]=0x%X, buff[2]=0x%X\n",
	      totalBuffer[0 + args->sendNum*MAX_SEND_SIZE_BUFFER], totalBuffer[1 + args->sendNum*MAX_SEND_SIZE_BUFFER], totalBuffer[2+args->sendNum*MAX_SEND_SIZE_BUFFER]);

	if(!args->endofBuffer )
		return HANDLE_OK_MIDDLE;

	at_cmd = args->at_cmd;
	at_act = args->at_act;

///////////////////////////////////////////////////
/* please use
static uint8_t totalBuffer[LIMIT_MAX_SEND_SIZE_BUFFER];
static uint32_t totalBufferSize = 0;
uint32_t at_cmd,at_act;
*/
///////////////////////////////////////////////////
	switch (at_cmd)
	{
// LS696_me : CAL BACKUP merge
// BEGIN: 0010090 sehyuny.kim@lge.com 2010-10-21
// MOD 0010090: [FactoryReset] Enable Recovery mode FactoryReset
		case MP2AP_CALBACKUP:
		{
			switch(at_act)
			{
				case CALBACKUP_ERASE:
				{
					uint erase_bytes = 0;
					const MmcPartition *pXcal_part = NULL; 
					unsigned long xcalbackup_bytes_pos_in_emmc = 0;
#ifdef CONFIG_LGE_EMMC_SUPPORT
					lge_mmc_scan_partitions();
					pXcal_part = lge_mmc_find_partition_by_name("misc");
#endif
					if ( pXcal_part == NULL )
					{
						printk(KERN_INFO"NO XCAL\n");
						return 0;
					}

					xcalbackup_bytes_pos_in_emmc = (pXcal_part->dfirstsec*512) + PTN_XCAL_POSITION_IN_MISC_PARTITION;
					/* Erase 10 Sectors */
#ifdef CONFIG_LGE_EMMC_SUPPORT
					erase_bytes = lge_erase_block(xcalbackup_bytes_pos_in_emmc,PTN_XCAL_POSITION_IN_MISC_PARTITION);
#endif
					printk(KERN_INFO "[CAL BACKUP]handle_misc_rpc_call offset : 0x%X, erase_bytes[%d]\n", xcalbackup_bytes_pos_in_emmc, erase_bytes);
					break;
				}
				case CALBACKUP_CAL_READ:
				{
					unsigned short *read_seek_pos = (unsigned short *) &totalBuffer[0];
					int  read_bytes = 0;
					long lenb64 = 0;
					unsigned int remain_bytes = 0;
					const MmcPartition *pXcal_part = NULL; 
					unsigned long xcalbackup_bytes_pos_in_emmc = 0;	
					unsigned char *eta_cmd_buf_encoded;					
					
					printk(KERN_INFO "[ETA]CALBACKUP_CAL_READ *read_seek_pos[%d]\n",*read_seek_pos);

#ifdef CONFIG_LGE_EMMC_SUPPORT
					lge_mmc_scan_partitions();
					pXcal_part = lge_mmc_find_partition_by_name("misc");
#endif
					// pXcal_part = lge_mmc_find_partition_by_name("xcalbackup");
					if ( pXcal_part == NULL )
					{
						printk(KERN_INFO"NO XCAL\n");
						return 0;
					}

                    // data encode for transmitting
					eta_cmd_buf_encoded = kmalloc(sizeof(unsigned char)*MAX_EMMC_STRING_RET, GFP_KERNEL);
					if(!eta_cmd_buf_encoded) {
						printk(KERN_ERR "%s: Error in alloc memory!!\n", __func__);
						kfree(eta_cmd_buf_encoded);
						return 0;
					}
					memset(eta_cmd_buf_encoded,0x00, MAX_EMMC_STRING_RET);

					xcalbackup_bytes_pos_in_emmc = (pXcal_part->dfirstsec*512) + (PTN_XCAL_POSITION_IN_MISC_PARTITION + (*read_seek_pos * MAX_EMMC_STRING_RET));
					printk(KERN_INFO "[ETA]CALBACKUP_CAL_READ xcalbackup_bytes_pos_in_emmc[%d]\n",xcalbackup_bytes_pos_in_emmc);
#ifdef CONFIG_LGE_EMMC_SUPPORT
					read_bytes = lge_read_block(xcalbackup_bytes_pos_in_emmc,(unsigned char *) eta_cmd_buf_encoded, MAX_EMMC_STRING_RET);
#endif
					//read_bytes = lge_read_block(xcalbackup_bytes_pos_in_emmc,(unsigned char *) ret_string, MAX_STRING_RET);
					printk(KERN_INFO "[ETA]CALBACKUP_CAL_READ read_bytes[%d]\n",read_bytes);

					lenb64 = base64_encode((char *)eta_cmd_buf_encoded, read_bytes, (char *)ret_string);

					printk(KERN_INFO "[ETA]CALBACKUP_CAL_READ lenb64[%d]\n", lenb64);

					//if ( (MMC_XCALBACKUP_DATA_SIZE-(*read_seek_pos * MAX_EMMC_STRING_RET)) < (MAX_EMMC_STRING_RET))
					//	remain_bytes = 0;
					//else
						remain_bytes = (MMC_XCALBACKUP_DATA_SIZE-(*read_seek_pos * MAX_EMMC_STRING_RET));

					if ( remain_bytes > 0 )
					{
						ret_value1 = *read_seek_pos + 1;
					} else {
						ret_value1 = 0;
					}
			
					ret_value2 = lenb64;
					//ret_value2 = read_bytes;
					kfree(eta_cmd_buf_encoded);
					printk(KERN_INFO "[ETA]CALBACKUP_CAL_READ mem free ok[%d]\n", ret_value1);
					break;
				}

				case NVCRC_BACKUP_WRITE:
				{
					const MmcPartition *pMisc_part = NULL; 
					unsigned long misc_bytes_pos_in_emmc = 0;

#ifdef CONFIG_LGE_EMMC_SUPPORT
					lge_mmc_scan_partitions();
					pMisc_part = lge_mmc_find_partition_by_name("misc");
#endif
					if ( pMisc_part == NULL )
					{
					
						printk(KERN_INFO"NO MISC\n");
						return 0;
					}
					
					misc_bytes_pos_in_emmc = (pMisc_part->dfirstsec*512) + PTN_NVCRC_PERSIST_POSITION_IN_MISC_PARTITION;
#ifdef CONFIG_LGE_EMMC_SUPPORT
					lge_write_block(misc_bytes_pos_in_emmc,totalBuffer,totalBufferSize);					
#endif
					break;
				}
				case NVCRC_BACKUP_READ:
				{
					const MmcPartition *pMisc_part = NULL; 
					unsigned long misc_bytes_pos_in_emmc = 0;
#ifdef CONFIG_LGE_EMMC_SUPPORT
					lge_mmc_scan_partitions();
					pMisc_part = lge_mmc_find_partition_by_name("misc");
#endif
					if ( pMisc_part == NULL )
					{
					
						printk(KERN_INFO"NO MISC\n");
						return 0;
					}
					
					misc_bytes_pos_in_emmc = (pMisc_part->dfirstsec*512) + PTN_NVCRC_PERSIST_POSITION_IN_MISC_PARTITION;
#ifdef CONFIG_LGE_EMMC_SUPPORT
					lge_read_block(misc_bytes_pos_in_emmc,(unsigned char *) ret_string, 512);
#endif
					break;
				}
				
				case CALBACKUP_MEID_WRITE:
				case PID_BACKUP_WRITE:
				case WLAN_MAC_ADDRESS_BACKUP_WRITE:
				case PF_NIC_MAC_BACKUP_WRITE:
				case CALBACKUP_ESN_WRITE:
				{
					const MmcPartition *pMisc_part = NULL; 
					unsigned long misc_bytes_pos_in_emmc = 0;

#ifdef CONFIG_LGE_EMMC_SUPPORT					
					lge_mmc_scan_partitions();
					pMisc_part = lge_mmc_find_partition_by_name("misc");
#endif
					if ( pMisc_part == NULL )
					{
					
						printk(KERN_INFO"NO MISC\n");
						return 0;
					}

					if(at_act == CALBACKUP_MEID_WRITE)
						misc_bytes_pos_in_emmc = (pMisc_part->dfirstsec*512) + PTN_MEID_PERSIST_POSITION_IN_MISC_PARTITION;
					else if(at_act == CALBACKUP_ESN_WRITE)
						misc_bytes_pos_in_emmc = (pMisc_part->dfirstsec*512) + PTN_ESN_PERSIST_POSITION_IN_MISC_PARTITION;
					else if(at_act == PID_BACKUP_WRITE)
						misc_bytes_pos_in_emmc = (pMisc_part->dfirstsec*512) + PTN_PID_PERSIST_POSITION_IN_MISC_PARTITION;
					else if(at_act == WLAN_MAC_ADDRESS_BACKUP_WRITE)
						misc_bytes_pos_in_emmc = (pMisc_part->dfirstsec*512) + PTN_WLAN_MAC_PERSIST_POSITION_IN_MISC_PARTITION;
					else if(at_act == PF_NIC_MAC_BACKUP_WRITE)
						misc_bytes_pos_in_emmc = (pMisc_part->dfirstsec*512) + PTN_PF_NIC_MAC_PERSIST_POSITION_IN_MISC_PARTITION;

					printk("%s, 0x%lX\n", __func__, misc_bytes_pos_in_emmc);
					printk("%s, %s\n", __func__, totalBuffer);
#ifdef CONFIG_LGE_EMMC_SUPPORT
					lge_write_block(misc_bytes_pos_in_emmc,totalBuffer,totalBufferSize);					
#endif
					break;
				}
				
				case CALBACKUP_MEID_READ:
				case PID_BACKUP_READ:
				case WLAN_MAC_ADDRESS_BACKUP_READ:
				case PF_NIC_MAC_BACKUP_READ:
				case CALBACKUP_ESN_READ:
				{
					const MmcPartition *pMisc_part = NULL; 
					unsigned long misc_bytes_pos_in_emmc = 0;
#ifdef CONFIG_LGE_EMMC_SUPPORT
					lge_mmc_scan_partitions();
					pMisc_part = lge_mmc_find_partition_by_name("misc");
#endif
					if ( pMisc_part == NULL )
					{
					
						printk(KERN_INFO"NO MISC\n");
						return 0;
					}

					if(at_act == CALBACKUP_MEID_READ)
						misc_bytes_pos_in_emmc = (pMisc_part->dfirstsec*512) + PTN_MEID_PERSIST_POSITION_IN_MISC_PARTITION;
					else if(at_act == CALBACKUP_ESN_READ)
						misc_bytes_pos_in_emmc = (pMisc_part->dfirstsec*512) + PTN_ESN_PERSIST_POSITION_IN_MISC_PARTITION;
					else if(at_act == PID_BACKUP_READ)
						misc_bytes_pos_in_emmc = (pMisc_part->dfirstsec*512) + PTN_PID_PERSIST_POSITION_IN_MISC_PARTITION;
					else if(at_act == WLAN_MAC_ADDRESS_BACKUP_READ)
						misc_bytes_pos_in_emmc = (pMisc_part->dfirstsec*512) + PTN_WLAN_MAC_PERSIST_POSITION_IN_MISC_PARTITION;
					else if(at_act == PF_NIC_MAC_BACKUP_READ)
						misc_bytes_pos_in_emmc = (pMisc_part->dfirstsec*512) + PTN_PF_NIC_MAC_PERSIST_POSITION_IN_MISC_PARTITION;

#ifdef CONFIG_LGE_EMMC_SUPPORT
					lge_read_block(misc_bytes_pos_in_emmc,(unsigned char *) ret_string, 512);
#endif
					break;
				}
				/* TBD
                case USBID_REMOTE_WRITE:
				{
					unsigned short pid;
					unsigned long long magic = 0;					
					const MmcPartition *pMisc_part = NULL; 
					char send_data[100];
					unsigned long misc_bytes_pos_in_emmc = 0;
					extern int android_get_product_id(void);

					printk("%s, USBID REMOTE WRITE\n", __func__);
#ifdef CONFIG_LGE_EMMC_SUPPORT
					lge_mmc_scan_partitions();
					pMisc_part = lge_mmc_find_partition_by_name("misc");
#endif
					if ( pMisc_part == NULL )
					{
					
						printk(KERN_INFO"NO MISC\n");
						return 0;
					}

					pid = android_get_product_id();
					magic = USBID_MAGIC_CODE;
                    *(unsigned long long *)send_data = magic;
					memcpy(ret_string, send_data,USBID_MAGIC_CODE_SIZE);
					memcpy(ret_string+USBID_MAGIC_CODE_SIZE,&pid, 4);
					misc_bytes_pos_in_emmc = (pMisc_part->dfirstsec*512) + PTN_USBID_PERSIST_POSITION_IN_MISC_PARTITION;
#ifdef CONFIG_LGE_EMMC_SUPPORT
					lge_write_block(misc_bytes_pos_in_emmc,(unsigned char *) ret_string, 8);
#endif
                    break;
				}
				*/
				case WEB_SID_REMARK_READ:
				{
					unsigned short *read_seek_pos = (unsigned short *) &totalBuffer[0];
					int  read_bytes;
					long lenb64 = 0;
					unsigned int remain_bytes = 0;
					const MmcPartition *pXcal_part; 
					unsigned long xcalbackup_bytes_pos_in_emmc = 0; 
					unsigned char *eta_cmd_buf_encoded; 				
					
					printk(KERN_INFO "[ETA]WEB_SID_REMARK_READ *read_seek_pos[%d]\n",*read_seek_pos);

					lge_mmc_scan_partitions();
					pXcal_part = lge_mmc_find_partition_by_name("misc");
					if ( pXcal_part == NULL )
					{
						printk(KERN_INFO"NO WEB_SID\n");
						return 0;
					}

					// data encode for transmitting
					eta_cmd_buf_encoded = kmalloc(sizeof(unsigned char)*MAX_EMMC_STRING_RET, GFP_KERNEL);
					if(!eta_cmd_buf_encoded) {
						printk(KERN_ERR "%s: Error in alloc memory!!\n", __func__);
						kfree(eta_cmd_buf_encoded);
						return 0;
					}
					memset(eta_cmd_buf_encoded,0x00, MAX_EMMC_STRING_RET);

					xcalbackup_bytes_pos_in_emmc = (pXcal_part->dfirstsec*512) + (PTN_WEB_SID_POSITION_IN_MISC_PARTITION + (*read_seek_pos * MAX_EMMC_STRING_RET));
					printk(KERN_INFO "[ETA]WEB_SID_REMARK_READ xcalbackup_bytes_pos_in_emmc[%d]\n",xcalbackup_bytes_pos_in_emmc);
					read_bytes = lge_read_block(xcalbackup_bytes_pos_in_emmc,(unsigned char *) eta_cmd_buf_encoded, MAX_EMMC_STRING_RET);
					printk(KERN_INFO "[ETA]WEB_SID_REMARK_READ read_bytes[%d]\n",read_bytes);

					lenb64 = base64_encode((char *)eta_cmd_buf_encoded, read_bytes, (char *)ret_string);

					printk(KERN_INFO "[ETA]WEB_SID_REMARK_READ lenb64[%d]\n", lenb64);

					ret_value1 = 0;			
					ret_value2 = lenb64;
					kfree(eta_cmd_buf_encoded);
					printk(KERN_INFO "[ETA]WEB_SID_REMARK_READ mem free ok[%d]\n", ret_value1);
					break;
				}

				case WEB_SID_REMARK_WRITE:
				{
					const MmcPartition *pMisc_part; 
					unsigned long misc_bytes_pos_in_emmc = 0;

					lge_mmc_scan_partitions();
					pMisc_part = lge_mmc_find_partition_by_name("misc");
					if ( pMisc_part == NULL )
					{
						printk(KERN_INFO"NO MISC\n");
						return 0;
					}
					misc_bytes_pos_in_emmc = (pMisc_part->dfirstsec*512) + PTN_WEB_SID_POSITION_IN_MISC_PARTITION;
					lge_write_block(misc_bytes_pos_in_emmc,totalBuffer,totalBufferSize);
					break;
				}

				default:
					if ( at_act >= CALBACKUP_CAL_WRITE )
					{
						uint written_bytes = 0;
						const MmcPartition *pXcal_part = NULL; 
						unsigned long xcalbackup_bytes_pos_in_emmc = 0;
#ifdef CONFIG_LGE_EMMC_SUPPORT						
						lge_mmc_scan_partitions();
						pXcal_part = lge_mmc_find_partition_by_name("misc");
#endif
						// pXcal_part = lge_mmc_find_partition_by_name("xcalbackup");
						if ( pXcal_part == NULL )
						{
						
							printk(KERN_INFO"NO XCAL\n");
							return 0;
						}

						xcalbackup_bytes_pos_in_emmc = (pXcal_part->dfirstsec*512) + PTN_XCAL_POSITION_IN_MISC_PARTITION + (at_act - CALBACKUP_CAL_WRITE);
#ifdef CONFIG_LGE_EMMC_SUPPORT
						written_bytes = lge_write_block(xcalbackup_bytes_pos_in_emmc,totalBuffer,totalBufferSize);
#endif
						printk(KERN_INFO "[ETA]handle_misc_rpc_call, offset : 0x%X, written_bytes[%d]\n", xcalbackup_bytes_pos_in_emmc, written_bytes);
						ret_value1 = written_bytes;

						break;
					}
			}
			break;
		}
// END: 0010090 sehyuny.kim@lge.com 2010-10-21

		case ATCMD_MTC:
		{
			int exec_result =0;

			printk(KERN_INFO "\n[ETA]ATCMD_MTC\n ");
			/* LGE_CHANGE
			 * Support MTC using diag port 
			 * 2010-07-11 taehung.kim@lge.com
			 */
#ifdef CONFIG_LGE_DIAG_MTC
			g_diag_mtc_check = 0;
#endif
			if(at_act != ATCMD_ACTION)
				result = HANDLE_FAIL;

			printk(KERN_INFO "[ETA]totalBuffer : [%s] size: %d\n", totalBuffer, totalBufferSize);

			decoded_params = kmalloc(sizeof(char)*totalBufferSize, GFP_KERNEL);
			if(!decoded_params) {
				printk(KERN_ERR "%s: Insufficent memory!!!\n", __func__);
				result = HANDLE_ERROR;
				break;
			}
			printk(KERN_INFO "[ETA] encoded_addr: 0x%X, decoded_addr: 0x%X, size: %d\n",  (unsigned int)totalBuffer, (unsigned int)decoded_params, sizeof(char)*totalBufferSize);
			
			len_b64 = base64_decode((char *)totalBuffer, (unsigned char *)decoded_params, totalBufferSize);
			printk(KERN_INFO "[ETA] sub cmd: 0x%X, param1: 0x%X, param2: 0x%X (length = %d)\n",  
			decoded_params[1], decoded_params[2], decoded_params[3], strlen(decoded_params));

			if (decoded_params[1] == 0x33)
				exec_result = eta_execute_n(totalBuffer, totalBufferSize +1, 0 ); //LGE_UPDATE [irene.park@lge.com] totalBuffesize increase + 1 ( Null size )
			else
				exec_result = eta_execute_n(totalBuffer, totalBufferSize +1, 1 ); //LGE_UPDATE [irene.park@lge.com] totalBuffesize increase + 1 ( Null size )
			printk(KERN_INFO "[ETA]AT+MTC exec_result %d\n",exec_result);
			
/*
			if((temp = (char *)strchr((const char *)totalBuffer, (int)'=') + 1) == NULL) {
				printk(KERN_INFO "[ETA]Invalid Parameter\n");
				result = HANDLE_FAIL;
			}
			encoded_params = temp;
*/		

			switch(decoded_params[1]) 
			{
				case 0x07://MTC_LOGGING_MASK_REQ_CMD:
					printk(KERN_INFO "[ETA] logging mask request cmd : %d\n", decoded_params[1]);

					b0 = decoded_params[2];
					b1 = decoded_params[3];
					b2 = decoded_params[4];
					b3 = decoded_params[5];

					logmask = b3<<24 | b2<<16 | b1<<8 | b0;

					switch(logmask)
					{
						case 0x00000000://ETA_LOGMASK_DISABLE_ALL:
						case 0xFFFFFFFF://ETA_LOGMASK_ENABLE_ALL:
						case 0x00000001://ETA_LOGITEM_KEY:
						case 0x00000002://ETA_LOGITEM_TOUCHPAD:
						case 0x00000003://ETA_LOGITME_KEYTOUCH:
							ats_mtc_log_mask = logmask;
							break;
						default:
							ats_mtc_log_mask = 0x00000000;//ETA_LOGMASK_DISABLE_ALL;
							break;
					}
/* add key log by younchan.kim*/
					if(logmask & 0xFFFFFFFF)
						event_log_start();
					else
						event_log_end();
					break;
					
				default:
					break;
			}
			
			kfree(decoded_params);

			sprintf(ret_string, "edcb");
			ret_value1 = 10;
			ret_value2 = 20;

		}
		break;

		default :
			result = HANDLE_ERROR;
			break;
	}

	/* give to RPC server result */
	strncpy(server->retvalue.ret_string, ret_string, MAX_STRING_RET);
	server->retvalue.ret_string[MAX_STRING_RET-1] = 0;
	server->retvalue.ret_value1 = ret_value1;
	server->retvalue.ret_value2 = ret_value2;
	if(args->endofBuffer )
	{
		/* init when first send */
		memset(totalBuffer, 0, sizeof(totalBuffer));
		totalBufferSize = 0;
	}

	if(result == HANDLE_OK)
		result = RPC_RETURN_RESULT_OK;
	else if(result == HANDLE_OK_MIDDLE)
		result = RPC_RETURN_RESULT_MIDDLE_OK;
	else
		result= RPC_RETURN_RESULT_ERROR;

	printk(KERN_INFO"%s: resulte = %d\n",
		   __func__, result);

	return result;
}

