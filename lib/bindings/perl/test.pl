#!/usr/bin/perl
#
# Silly-minded test script.
#
# Copyright (C) 2004 Lars Marowsky-Brée
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

use heartbeat::clplumbing::log;
use heartbeat::clplumbing::ipc;

cl_log_set_entity("test.pl");
cl_log_enable_stderr(1);
cl_log('notice', "Starting!");

my $ch = heartbeat::clplumbing::ipc::channel->new("/var/lib/heartbeat/echo");

cl_log('notice', "Connecting... (hopefully with $IPC_OK)");

if ($rc = $ch->initiate_connection != $IPC_OK) {
	cl_log('err', "Connection failed with $rc");
	exit(1);
}

my $f = 0;
my $n = 0;

cl_log('notice', "Entering message loop.");

while ($n++ < 10 && $ch->isrconn() && $ch->iswconn()) {
	my $s = "Message number: $n";
	cl_log('debug', "Constructing message.");
	my $m1 = heartbeat::clplumbing::ipc::message->new($ch, $s);
	cl_log('debug', "Sending message.");
	$ch->send($m1);
	cl_log('debug', "Waiting for reply.");
	$ch->waitin();
	cl_log('debug', "Receiving answer.");
	my ($rc, $m2) = $ch->recv();
	# my $data = $m2->body();
	#if ($data ne $s) {
	#	cl_log('warn', "Error in iteration $n: $s ne $data");
	#	$f++;
	#}
}

cl_log('notice', "Messages: $n - errors: $f");

