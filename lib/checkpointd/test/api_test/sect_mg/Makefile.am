# Copyright (C) 2002 Alan Robertson
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
MAINTAINERCLEANFILES    = Makefile.in

INCLUDES                = -I$(top_builddir)/include \
			-I$(top_srcdir)/include \
			-I$(top_builddir)/libltdl \
			-I$(top_srcdir)/libltdl \
			-I$(top_builddir)/linux-ha \
			-I$(top_srcdir)/linux-ha \
			-I$(top_builddir)/heartbeat \
			-I$(top_srcdir)/heartbeat \
			-I$(top_builddir)/include/clplumbing \
			-I$(top_srcdir)/include/clplumbing \
			-I$(top_builddir)/lib/checkpointd/test \
			-I$(top_srcdir)/lib/checkpointd/test 

hadir			= $(sysconfdir)/ha.d
halibdir		= $(libdir)/@HB_PKG@
commmoddir		= $(halibdir)/modules/comm
havarlibdir		= $(localstatedir)/lib/@HB_PKG@
ckptvarlibdir		= $(havarlibdir)/ckpt
havarrundir		= $(localstatedir)/run/

test_LDADD		= $(GLIBLIB) \
	 		$(top_builddir)/lib/checkpointd/libckpt.la \
			$(top_builddir)/replace/libreplace.la \
			$(top_builddir)/lib/clplumbing/libplumb.la \
			$(top_builddir)/lib/hbclient/libhbclient.la 
## binary progs
noinst_PROGRAMS		= create delete iterator_finalize iterator_init iterator_next expire_time_set
halib_PROGRAMS		= create delete iterator_finalize iterator_init iterator_next expire_time_set

#ckpttest
create_SOURCES		= create.c
create_LDADD		= $(test_LDADD)
create_LDFLAGS	= 

delete_SOURCES		= delete.c
delete_LDADD		= $(test_LDADD) 
delete_LDFLAGS	= 

iterator_finalize_SOURCES	= iterator_finalize.c
iterator_finalize_LDADD		= $(test_LDADD) 
iterator_finalize_LDFLAGS	= 

iterator_next_SOURCES	= iterator_next.c
iterator_next_LDADD	= $(test_LDADD) 
iterator_next_LDFLAGS	= 

iterator_init_SOURCES	= iterator_init.c
iterator_init_LDADD	= $(test_LDADD)
iterator_init_LDFLAGS	= 

expire_time_set_SOURCES	= expire_time_set.c
expire_time_set_LDADD	= $(test_LDADD) 
expire_time_set_LDFLAGS	= 


