
/* 
 * ccmmem.c: membership routine
 *
 * Copyright (c) International Business Machines  Corp., 2002
 * Author: Ram Pai (linuxram@us.ibm.com)
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <lha_internal.h>
#include "ccm.h"
#include "ccmmisc.h"

void
ccm_mem_reset(ccm_info_t* info)
{
	info->memcount = 0;	
	return;
}

int
ccm_mem_add(ccm_info_t* info, int index)
{

	int count;

	if (info == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return HA_FAIL;
	}
	if (index < 0 || index > llm_get_nodecount(&info->llm)){
		ccm_log(LOG_ERR, "%s: index(%d) out of range",
		       __FUNCTION__, index);
		return HA_FAIL;
	}

	count = info->memcount;	
	info->ccm_member[count] = index;
	
	info->memcount++;
	
	if (info->memcount >= MAXNODE){
		ccm_log(LOG_ERR, "%s: membership count(%d) out of range",
		       __FUNCTION__, info->memcount);
		return HA_FAIL;
	}
	
	return HA_OK;
}

int
ccm_get_memcount(ccm_info_t* info){

	if (info == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return -1;
	}
	
	return info->memcount;
	
}

int 
ccm_mem_bitmapfill(ccm_info_t *info, 
	const char *bitmap)
{
	llm_info_t *llm;
	uint i;
	
	llm = &info->llm;
	ccm_mem_reset(info);
	for ( i = 0 ; i < llm_get_nodecount(llm); i++ ) {
		if(bitmap_test(i, bitmap, MAXNODE)){
			if (ccm_mem_add(info, i) != HA_OK){
				ccm_log(LOG_ERR, "%s: adding node(%s)"
				       "to member failed",
				       __FUNCTION__, 
				       llm_get_nodename(llm, i));
				return HA_FAIL;
			}
		}
	}
	
	return HA_OK;
}

int 
ccm_mem_strfill(ccm_info_t *info, 
		const char *memlist)
{
	char *bitmap = NULL;
	int ret;
	
	bitmap_create(&bitmap, MAXNODE);
	if (bitmap == NULL){
		ccm_log(LOG_ERR, "%s:bitmap creation failure", 
		       __FUNCTION__);
		return HA_FAIL;
	}
	if (ccm_str2bitmap((const  char *) memlist, strlen(memlist), 
			   bitmap) < 0){
		ccm_log(LOG_ERR, "%s: string(%s) to bitmap conversion failed",
		       __FUNCTION__, memlist);
		return HA_FAIL;
	}
	
	ret = ccm_mem_bitmapfill(info, bitmap);
	
	bitmap_delete(bitmap);
	
	return ret;
}

gboolean
node_is_member(ccm_info_t* info, const char* node)
{
	int i,indx;
	llm_info_t *llm = &info->llm;

	if (info == NULL || node == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return HA_FAIL;
	}
	
	for ( i = 0 ; i < ccm_get_memcount(info) ; i++ ) {
		indx =  info->ccm_member[i];
		if(strncmp(llm_get_nodename(llm, indx), node, 
			   NODEIDSIZE) == 0){
			return TRUE;
		}
	}	
	
	return FALSE;
}

gboolean
i_am_member(ccm_info_t* info)
{
	llm_info_t* llm;
	int i;

	if (info == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return FALSE;
	}	
	
	llm = &info->llm;
	if (llm->myindex <0){
		ccm_log(LOG_ERR, "%s: myindex in llm is not set",
		       __FUNCTION__);
		return FALSE;
	}
	
	for (i = 0; i < info->memcount; i++){
		if (info->ccm_member[i] == llm->myindex){
			return TRUE;
		}
	}
	return FALSE;
}

int 
ccm_mem_delete(ccm_info_t* info, int index)
{
	int  i;
	int memcount;
	
	if (info == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return HA_FAIL;
	}
	if (index < 0 || index > llm_get_nodecount(&info->llm)){
		ccm_log(LOG_ERR, "%s: index(%d) out of range",
		       __FUNCTION__, index);
		return HA_FAIL;
	}

	memcount = info->memcount;
	for (i =0; i < info->memcount; i++){
		if (info->ccm_member[i] == index){
			info->ccm_member[i] = info->ccm_member[memcount-1];
			info->memcount--;
			return HA_OK;
		}
	}       
	
	ccm_log(LOG_ERR, "%s: node index(%d) not found in membership",
	       __FUNCTION__, index);
	return HA_FAIL;

}

int
ccm_mem_update(ccm_info_t *info, const char *node, 
		enum change_event_type change_type)
{
	llm_info_t *llm = &info->llm;
	
	if (change_type == NODE_LEAVE){
		return ccm_mem_delete(info, llm_get_index(llm ,node));
	}else{
		return ccm_mem_add(info, llm_get_index(llm, node));
	}
}

int
ccm_mem_filluptime(ccm_info_t* info, int* uptime_list, int uptime_size)
{
	int i;

	if (uptime_size != info->memcount){
		ccm_log(LOG_ERR, "%s: uptime_list size (%d) != memcount(%d)",
		       __FUNCTION__, uptime_size, info->memcount);
		return HA_FAIL;		
	}
	
	for (i = 0; i < info->memcount; i++){
		llm_set_uptime(&info->llm, info->ccm_member[i], uptime_list[i]);
	}

	return HA_OK;
}

int 
am_i_member_in_memlist(ccm_info_t *info, const char *memlist)
{
	char *bitmap = NULL;
	int numBytes, myindex;
	llm_info_t *llm;
	
	if (info == NULL || memlist == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return FALSE;
	}
	
	llm = &info->llm;
	bitmap_create(&bitmap, MAXNODE);
	if (bitmap == NULL){
		ccm_log(LOG_ERR ,"%s: bitmap creatation failed",
		       __FUNCTION__);
		return FALSE;
	}
	
	numBytes = ccm_str2bitmap(memlist, strlen(memlist), bitmap);
	
	myindex = llm_get_myindex(llm);
	
	if (bitmap_test(myindex, bitmap, numBytes*BitsInByte)){
		bitmap_delete(bitmap);
		return TRUE;
	}

	bitmap_delete(bitmap);
	return FALSE;
}
