package heartbeat::clplumbing::ipc;

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

our @EXPORT_OK = ( );

our @EXPORT = qw(
	$IPC_OK $IPC_FAIL $IPC_BROKEN $IPC_INTR
	$IPC_CONNECT $IPC_WAIT $IPC_DISCONNECT $IPC_DISC_PENDING
);

our $VERSION = '0.01';

our $IPC_CONNECT = $heartbeat::cl_raw::IPC_CONNECT;
our $IPC_WAIT = $heartbeat::cl_raw::IPC_WAIT;
our $IPC_DISCONNECT = $heartbeat::cl_raw::IPC_DISCONNECT;
our $IPC_DISC_PENDING = $heartbeat::cl_raw::IPC_DISC_PENDING;
our $IPC_OK = $heartbeat::cl_raw::IPC_OK;
our $IPC_FAIL = $heartbeat::cl_raw::IPC_FAIL;
our $IPC_BROKEN = $heartbeat::cl_raw::IPC_BROKEN;
our $IPC_INTR = $heartbeat::cl_raw::IPC_INTR;

=head1 NAME

heartbeat::clplumbing::ipc - Perl extension for heartbeat IPC code

=head1 SYNOPSIS

=over

  use heartbeat::clplumbing::ipc;
  
  if ($rc == $IPC_CONNECT) ...
  
=back

=head1 DESCRIPTION

This module provides a wrapper for the heartbeat IPC code. It exports
the C<IPC_CONNECT, IPC_WAIT, IPC_DISCONNECT, IPC_DISC_PENDING> and
C<IPC_OK, IPC_FAIL, IPC_BROKEN, IPC_INTR> codes by default and provides
some more classes for easily dealing with the non-blocking IPC layer,
which are explained further below.

=cut

package heartbeat::clplumbing::ipc::message;

=head2 heartbeat::clplumbing::ipc::message

IPC_Message abstraction providing the basic functions of heartbeat
messages. Note that this cleans up after itself, ie messages are
automatically freed when they go out of scope.

=over

=item my $msg = heartbeat::clplumbing::ipc::message-E<gt>new($ch, $s);

Simply create a message with the scalar data provided; the length
attribute is automatically filled into the message.

You need to provide the message constructor with a
C<heartbeat::clplumbing::ipc::channel> reference to fully fill in the
lower level structures.

=item my $body = $msg-E<gt>body();

Returns the message body with a maximum size of the message length.

=item my $len = $msg-E<gt>len();

Returns the length of the message data. Probably not needed all that
often in Perl, as all data structures are dynamic anyway.

=back

=cut

use Carp qw(croak);

sub new {
	my ($class, $ch, $data) = @_;
	ref ($class) and croak "class name needed";
	ref($ch) or croak "Instance variable needed";
	UNIVERSAL::isa($ch, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type for argument, not an IPC channel";

	# By default, we assume we need to clean up after ourselves.
	my $self = { cleanup => 1, };
	
	$self->{raw} = heartbeat::cl_raw::ipc_msg_constructor(
		$ch->{raw},
		length($data), $data);
	
	bless $self, $class;

	return $self;
}

sub new_from_raw {
	my ($class, $msg) = @_;
	ref ($class) and croak "class name needed";
	
	$msg =~ /^_p_IPC_Message/o or 
		croak("Need to supply a raw IPC_Message reference!");
	
	# By default, we assume we need to clean up after ourselves.
	my $self = { cleanup => 1, };
	
	$self->{raw} = $msg;

	bless $self, $class;
	return $self;
}

sub body {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::message")
		or croak "Wrong object type, not an IPC message";
	
	my $len = $self->len();
	my $data = heartbeat::cl_raw::ipc_msg_get_body($self->{raw});
	# Additional sanity checking for the message length:
	if (length($data) > $len) {
		$data = substr($data,0,$len);
	}
	return $data;
}

sub len {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::message")
		or croak "Wrong object type, not an IPC message";
	
	return
	heartbeat::cl_rawc::IPC_MESSAGE_msg_len_get($self->{raw}); 
}

sub DESTROY {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::message")
		or croak "Wrong object type, not an IPC message";

	if ($self->{'cleanup'} == 1) {
		heartbeat::cl_raw::ipc_msg_done($self->{raw});
	}
}

package heartbeat::clplumbing::ipc::auth;
use Carp qw(croak);

=head2 heartbeat::clplumbing::ipc::auth

Wrap the IPC_Auth functionality.

=over 

=item my $auth = heartbeat::clplumbing::ipc::auth-E<gt>new();

Construct a new IPC_Auth object, with an initially empty set of uid /
gid lists.

=cut

sub new {
	my ($class) = @_;
	ref ($class) and croak "class name needed";
	
	my $self = { };
	
	$self->{raw} = heartbeat::cl_raw::helper_create_auth();
	
	bless $self, $class;

	return $self;
}

=item $auth-E<gt>add_uid($uid);

Add the specified uid to the object.

The UID must be numeric.

=cut

sub add_uid {
	my ($self, $uid) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::auth")
		or croak "Wrong object type, not an IPC auth";
	
	if ($uid !~ /^\d+$/) {
		croak "add_uid called with non-integer uid!";
	}

	heartbeat::cl_raw::helper_add_auth_uid($self->{raw}, $uid);
}

=item $auth-E<gt>add_gid($uid);

Add the specified gid to the object.

The GID must be numeric.

=cut

sub add_gid {
	my ($self, $gid) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::auth")
		or croak "Wrong object type, not an IPC auth";
	
	if ($gid !~ /^\d+$/) {
		croak "add_gid called with non-integer uid!";
	}

	heartbeat::cl_raw::helper_add_auth_uid($self->{raw}, $gid);
}

sub DESTROY {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::auth")
		or croak "Wrong object type, not an IPC auth";
	
	heartbeat::cl_raw::ipc_destroy_auth($self->{raw});

}

=back

=cut

package heartbeat::clplumbing::ipc::server;
use Carp qw(croak);

=head2 heartbeat::clplumbing::ipc::server

Wrap the IPC_WaitConnection functionality.

=over 

=item my $wc = heartbeat::clplumbing::ipc::server-E<gt>new($path);

Construct a new WaitConnection object of type Unix Domain Socket
listening on the given path.

=cut

sub new {
	my ($class, $path) = @_;
	ref ($class) and croak "class name needed";
	
	my $self = { };
	
	my $h = heartbeat::cl_raw::simple_hash_new();
	
	heartbeat::cl_raw::simple_hash_insert($h,
		$heartbeat::cl_raw::IPC_PATH_ATTR,
		$path);
	$self->{raw} = heartbeat::cl_raw::ipc_channel_constructor(
                $heartbeat::cl_raw::IPC_DOMAIN_SOCKET,$h);
	heartbeat::cl_raw::simple_hash_destroy($h);
	
	bless $self, $class;

	return $self;
}

=item my $fd = $wc-E<gt>get_select_fd();

Get the filehandle for the wait connection which can be used with
C<select()> to wait for incoming connections.

=cut
sub get_select_fd {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::server")
		or croak "Wrong object type, not an IPC WaitConnection";

	return heartbeat::cl_raw::ipc_wc_get_select_fd($self->{raw});
}

=item my $ch = $wc-E<gt>accept_connection($auth);

Returns the C<heartbeat::clplumbing::ipc::channel> object for the
accepted connection, if the connection passed the authentication test.

C<$auth> must be a C<heartbeat::clplumbing::ipc::auth> object.

=cut

sub accept_connection {
	my ($self, $auth) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::server")
		or croak "Wrong object type, not an IPC WaitConnection";
	UNIVERSAL::isa($auth, "heartbeat::clplumbing::ipc::auth")
		or croak "Wrong object type, not an IPC Auth";

	my $raw_ch = heartbeat::cl_raw::ipc_wc_accept_connection($self->{raw}, $auth);
	
	if ($raw_ch) {
		return heartbeat::clplumbing::ipc::channel->new_from_raw($raw_ch);
	} else {
		return -1;
	}
}

sub DESTROY {
	my ($self, $auth) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::server")
		or croak "Wrong object type, not an IPC WaitConnection";

	heartbeat::clplumbing::ipc::ipc_wc_destroy($self->{raw});
}

=back

=cut

package heartbeat::clplumbing::ipc::channel;
use Carp qw(croak);

=head2 heartbeat::clplumbing::ipc::channel

IPC_Channel abstraction providing the basic functions of heartbeat
messages. Note that this cleans up after itself, ie messages are
automatically freed when they go out of scope.

=over

=cut

=item my $ch = heartbeat::clplumbing::ipc::channel-E<gt>new($path);

Creates a Unix Domain Socket channel object using the provided path.

=cut

sub new {
	my ($class, $path) = @_;
	ref ($class) and croak "class name needed";

	my $self = { };
	
	my $h = heartbeat::cl_raw::simple_hash_new();
	
	heartbeat::cl_raw::simple_hash_insert($h,
		$heartbeat::cl_raw::IPC_PATH_ATTR,
		$path);
	$self->{raw} = heartbeat::cl_raw::ipc_channel_constructor(
                $heartbeat::cl_raw::IPC_DOMAIN_SOCKET,$h);
	heartbeat::cl_raw::simple_hash_destroy($h);
	
	bless $self, $class;

	return $self;	
}

# Undocumented on purpose, should be called externally, only from the
# WaitConnection wrapper!

sub new_from_raw {
	my ($class, $ch) = @_;
	ref ($class) and croak "class name needed";
	
	$ch =~ /^_p_IPC_Channel/o or 
		croak("Need to supply a raw IPC_Channel reference!");
	
	# By default, we assume we need to clean up after ourselves.
	my $self = { };
	
	$self->{raw} = $ch;

	bless $self, $class;
	return $self;
}

=item $ch-E<gt>initiate_connection();

Initiates the connection to the path given at creation time, and will
return the corresponding IPC error/success code.

=cut

sub initiate_connection {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";

	return heartbeat::cl_raw::ipc_ch_initiate_connection($self->{raw});
}

=item $ch-E<gt>verify_auth($auth);

Verifies the authentication of the connection vs the supplied IPC_Auth
object.

Not implemented yet. TODO.

=cut

sub verify_auth {
	my ($self, $auth) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";
	UNIVERSAL::isa($auth, "heartbeat::clplumbing::ipc::auth")
		or croak "Wrong object type, not an IPC Auth";

	croak("Not yet implemented.");
}

=item my ($rc, $msg) = $ch-E<gt>recv();

Tries to receive an IPC message from the channel, which is automatically
returned as a C<heartbeat::clplumbing::ipc::message> object.

Returns a list with the return code from the lower-level recv operation
and the message.

=cut

sub recv {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";

	my $msg_with_rc = 
		heartbeat::cl_raw::ipc_ch_recv($self->{raw});
	
	my $msg = heartbeat::clplumbing::ipc::message->new_from_raw(
		heartbeat::cl_rawc::IPC_MESSAGE_WITH_RC_msg_get($msg_with_rc)); 

	my $rc =
	heartbeat::cl_rawc::IPC_MESSAGE_WITH_RC_rc_get($msg_with_rc);
	
	heartbeat::cl_raw::ipc_ch_msg_with_rc_destroy($msg_with_rc);
	
	return ($rc, $msg);
}

=item my $rc = $ch-E<gt>send($msg);

Queues an IPC message for sending on the channel; you need to provide it
an C<heartbeat::clplumbing::ipc::message> object.

Returns the lower-level status code.

=cut
sub send {
	my ($self, $msg) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";
	UNIVERSAL::isa($msg, "heartbeat::clplumbing::ipc::message")
		or croak "Wrong object type, not an IPC message";

	# After the message has been sent, it will be cleaned up. We
	# MUST NOT do this ourselves - flip the switch inside the IPC
	# Message object for this.
	$msg->{cleanup} = 0;
	
	return heartbeat::cl_raw::ipc_ch_send($self->{raw},
		$msg->{raw});
}

=item $ch-E<gt>waitin();

Wait until input is available, also returns the lower-level status
code.

=cut
sub waitin {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";
	return heartbeat::cl_raw::ipc_ch_waitin($self->{raw});
}

=item $ch-E<gt>waitout();

Wait until all queued messages have been send or the channel is
disconnected. Also returns the lower-level status code.

=cut
sub waitout {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";
	return heartbeat::cl_raw::ipc_ch_waitout($self->{raw});
}

=item $ch-E<gt>iswconn();

Returns true if the channel is connected for writing.

=cut
sub iswconn {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";
	return heartbeat::cl_raw::ipc_ch_iswconn($self->{raw});
}

=item $ch-E<gt>isrconn();

Returns true if the channel is ready to be read from, ie either in
IPC_OK or IPC_DISC_PENDING.

=cut
sub isrconn {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";
	return heartbeat::cl_raw::ipc_ch_isrconn($self->{raw});
}

=item $ch-E<gt>is_message_pending();

Returns TRUE if a message can be read right now.

=cut
sub is_message_pending {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";
	return heartbeat::cl_raw::ipc_ch_is_message_pending($self->{raw});
}

=item $ch-E<gt>is_sending_blocked();

Returns true if the channel is connected and there is still a message in
the send queue.

=cut
sub is_sending_blocked {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";
	return heartbeat::cl_raw::ipc_ch_is_sending_blocked($self->{raw});
}

=item $ch-E<gt>resume_io();

Resume IO and return the status code.

=cut

sub resume_io {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";
	return heartbeat::cl_raw::ipc_ch_resume_io($self->{raw});
}

=item $ch-E<gt>get_send_select_fd();

Return the fd which can be used in the poll() function for the sending
side of the connection.

=cut
sub get_send_select_fd {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";
	return heartbeat::cl_raw::ipc_ch_get_send_select_fd($self->{raw});
}

=item $ch-E<gt>get_recv_select_fd();

Return the fd which can be used in the poll() function for the receiving
side of the connection.

=cut
sub get_recv_select_fd {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";
	return heartbeat::cl_raw::ipc_ch_get_recv_select_fd($self->{raw});
}

=item $ch-E<gt>set_send_qlen($i);

Set the sending queue len to C<$i>, which needs to be a positive
integer.

=cut
sub set_send_qlen {
	my ($self, $qlen) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";
	($qlen <= 0) and croak "send_qlen needs to be greater than 0";

	return heartbeat::cl_raw::ipc_ch_set_send_qlen($self->{raw},
		$qlen);
}

=item $ch-E<gt>set_recv_qlen($i);

Set the receiving queue len to C<$i>, which needs to be a positive
integer.

=cut
sub set_recv_qlen {
	my ($self, $qlen) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";
	($qlen <= 0) and croak "recv_qlen needs to be greater than 0";

	return heartbeat::cl_raw::ipc_ch_set_recv_qlen($self->{raw},
		$qlen);
}

sub DESTROY {
	my ($self) = @_;
	ref($self) or croak "Instance variable needed";
	UNIVERSAL::isa($self, "heartbeat::clplumbing::ipc::channel")
		or croak "Wrong object type, not an IPC channel";
	heartbeat::cl_raw::ipc_ch_destroy($self->{raw});
}

=back

=cut

1;
__END__

=head1 SEE ALSO

The IPC header files in heartbeat.

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
