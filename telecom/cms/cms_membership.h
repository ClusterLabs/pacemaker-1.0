/*
 * cms_membership.h: cms daemon membership event handler header
 *
 * Copyright (c) 2004 Intel Corp.
 *
 * Author: Zou Yixiong (yixiong.zou@intel.com)
 * Author: Zhu Yi (yi.zhu@intel.com)
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */
#ifndef __CMS_MEMBERSHIP_H__
#define __CMS_MEMBERSHIP_H__

int set_cms_status(const char * node, const char * status, void * private);
int cms_membership_init(cms_data_t * cms_data);
int cms_membership_dispatch(SaClmHandleT * handle, SaDispatchFlagsT flags);
int cms_membership_get_input_fd(SaClmHandleT * handle);
void cms_membership_finalize(SaClmHandleT * handle);

#endif /* __CMS_MEMBERSHIP_H__ */
