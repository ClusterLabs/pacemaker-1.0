/* $Id: lock.h,v 1.2 2004/02/17 22:11:57 lars Exp $ */
#ifndef __LOCK_H
#	define __LOCK_H
int	ttylock(const char *serial_device);
int	ttyunlock(const char *serial_device);
int	DoLock(const char * prefix, const char *lockname);
int	DoUnlock(const char * prefix, const char *lockname);
#endif	/* __LOCK_H */
