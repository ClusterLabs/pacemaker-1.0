#ifndef __LOCK_H
#	define __LOCK_H
int	ttylock(const char *serial_device);
int	ttyunlock(const char *serial_device);
int	DoLock(const char * prefix, const char *lockname);
int	DoUnlock(const char * prefix, const char *lockname);
#endif	/* __LOCK_H */
