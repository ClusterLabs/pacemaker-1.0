package heartbeat::clplumbing::log;

use 5.008001;
use strict;
use warnings;
use Carp qw(croak);
use heartbeat::cl_raw;

require Exporter;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use heartbeat::clplumbing::log ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	cl_log_enable_stderr
	cl_log_set_entity
	cl_log
	cl_log_set_logfile
	cl_log_set_debugfile
	cl_log_set_facility
);

our $VERSION = '0.01';

our %LOG_PRIO_MAP = (
	'emerg' => 0, 'alert' => 1, 'crit' => 2, 'err' => 3,
	'warn' => 4, 'notice' => 5, 'info' => 6, 'debug' => 7,
);

our %LOG_FACILITY_MAP = (
	'kern' => 0, 'user' => 1, 'mail' => 2, 'daemon' => 3,
	'auth' => 4, 'syslog' => 5, 'lpr' => 6, 'news' => 7,
	'uucp' => 8, 'cron' => 9, 'authpriv' => 10, 'ftp' => 11,
	'local0' => 16, 'local1' => 17, 'local2' => 18,
	'local3' => 19, 'local4' => 20, 'local5' => 21,
	'local6' => 22, 'local7' => 23,
);

sub cl_log_enable_stderr {
	my ($enable) = @_;

	heartbeat::cl_raw::cl_log_enable_stderr($enable);
}

sub cl_log_set_entity {
	my ($entity) = @_;

	heartbeat::cl_raw::cl_log_set_entity($entity);
}

sub cl_log_set_logfile {
	my ($logfile) = @_;

	heartbeat::cl_raw::cl_log_set_logfile($logfile);
}

sub cl_log_set_debugfile {
	my ($debugfile) = @_;

	heartbeat::cl_raw::cl_log_set_debugfile($debugfile);
}

sub cl_log_set_facility {
	my ($facility) = @_;
	
	# Fix the logging facility.
	if (defined($LOG_FACILITY_MAP{$facility})) {
		$facility = $LOG_FACILITY_MAP{$facility};
	} elsif (($facility !~ /^\d+$/) 
		&& ($facility >= 0) 
		&& ($facility <= 23)) {
		$facility = 6;
	}

	heartbeat::cl_raw::cl_log_set_facility($facility);
}

sub cl_log {
	my ($prio, $message) = @_;
	
	# Fix the logging priority.
	if (defined($LOG_PRIO_MAP{$prio})) {
		$prio = $LOG_PRIO_MAP{$prio};
	} elsif ($prio !~ /^[0-7]$/) {
		$prio = 6;
	}
	
	heartbeat::cl_raw::cl_log_helper($prio, $message);
}

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

heartbeat::clplumbing::log - Perl wrapper for the heartbeat cl_log.

=head1 SYNOPSIS

=over

  use heartbeat::clplumbing::log;

  cl_log_set_entity("myprogram");
  cl_log_enable_stderr(1);
  cl_log('warn', "Wow, that was easy.");

=back

=head1 DESCRIPTION

This module provides a wrapper for the cl_log.[ch] libraries as provided
by heartbeat.

It decidedly does not provide these as an object, as all cl_log
functions are global to a given process, and thus this abstraction is
more natural.

=head1 FUNCTIONS PROVIDED

=over

=item C<cl_log_set_entity($entity)>

This function sets the entity used in the logged messages; entity should
be a string.

=item C<cl_log_enable_stderr($yes)>

This function enables logging to stderr or disables it.

=item C<cl_log($priority, $logmessage)>

Logs the string in $logmessage with the given priority. Priority can
either be a numerical value from 0-7, or the corresponding syslog
priority, one of: C<emerg alert crit err warn notice info debug>.

=item C<cl_log_set_logfile($path)>

Enable logging to the given logfile.

=item C<cl_log_set_debugfile($path)>

Enable debug logging to the given logfile.

=item C<cl_log_set_facility($facility)>

Enable logging to the given syslog facility, which you can either
specify as a numerical value from C<0-23> or in the preferred from of a
string.

=back

=head2 EXPORT

All by default.

=head1 SEE ALSO

The cl_log.h header file.

=head1 AUTHOR

Lars Marowsky-Bree, E<lt>lmb@suse.deE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2004 by Lars Marowsky-Bree

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

=cut
