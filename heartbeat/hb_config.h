const static char * _hb_config_h_Id = "$Id: hb_config.h,v 1.2 2003/07/01 02:36:22 alan Exp $";
/*
 * Parse various heartbeat configuration files...
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
 *	portions (c) 1999,2000 Mitja Sarp
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _HB_CONFIG_H
#define _HB_CONFIG_H

int		parse_ha_resources(const char * cfgfile);
void		dump_config(void);
int		add_node(const char * value, int nodetype);
int   		parse_authfile(void);
int		init_config(const char * cfgfile);
int		StringToBaud(const char * baudstr);
const char *	GetParameterValue(const char * name);

#endif /* _HB_CONFIG_H */
