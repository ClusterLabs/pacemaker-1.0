
/*
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
 */

#ifndef _CCM_MISC_H_
#define _CCM_MISC_H_

#define MAX_MEMLIST_STRING  64

int		ccm_bitmap2str(const char *bitmap, 
			       char* memlist, int size);
int		ccm_str2bitmap(const char *_memlist, 
			       int size, char *bitmap);

void		leave_init(void);
void		leave_reset(void);
void		leave_cache(int i);
int		leave_get_next(void);
int		leave_any(void);


int	ccm_memlist_changed(ccm_info_t *info,char *bitmap);
int	ccm_fill_memlist(ccm_info_t *info, const char *bitmap);
int	ccm_fill_memlist_from_str(ccm_info_t *info, const char *memlist);
int	ccm_fill_memlist_from_bitmap(ccm_info_t *info, 	const char *bitmap);
int	ccm_get_membership_index(ccm_info_t *info, const char *node);
gboolean node_is_member(ccm_info_t* info, const char* node);

gboolean part_of_cluster(int state);
int	ccm_string2type(const char *type);
char *	ccm_type2string(enum ccm_type type);
void   ccm_log(int priority, const char * fmt, ...) G_GNUC_PRINTF(2,3);

#endif



