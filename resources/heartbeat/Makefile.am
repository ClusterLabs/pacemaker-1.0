#
# heartbeat: Linux-HA heartbeat code
#
# Copyright (C) 2001 Michael Moerz
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
AM_CFLAGS                       = @CFLAGS@  @LIBNETDEFINES@


MAINTAINERCLEANFILES    = Makefile.in

INCLUDES		= -I$(top_srcdir)/include -I$(top_srcdir)/linux-ha

gliblib			= @GLIBLIB@

havarlibdir             = $(localstatedir)/lib/@HB_PKG@

harddir			= $(sysconfdir)/ha.d/resource.d


if USE_IPV6ADDR
hard_PROGRAMS		= IPv6addr
else
hard_PROGRAMS		= 
endif

IPv6addr_SOURCES	= IPv6addr.c

IPv6addr_LDADD		= $(top_builddir)/lib/clplumbing/libplumb.la	\
			$(gliblib) @LIBNETLIBS@  			\
			$(top_builddir)/lib/pils/libpils.la



hard_SCRIPTS	=			\
			apache		\
			AudibleAlarm	\
			db2		\
			Delay		\
			Filesystem	\
			hto-mapfuncs	\
			ICP		\
			IPaddr		\
			IPaddr2		\
			IPsrcaddr	\
			LinuxSCSI	\
			LVM		\
			LVSSyncDaemonSwap \
			MailTo		\
			OCF		\
			portblock	\
			ServeRAID	\
			SendArp		\
			Raid1		\
			WAS		\
			WinPopup	\
			Xinetd
