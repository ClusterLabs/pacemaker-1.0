#ifndef __SNMP_CONFIG_RESOLVE_H
#define __SNMP_CONFIG_RESOLVE_H

/* the following undef is needed because these values
   are defined in both the config.h for linux-ha and 
   net-snmp.  So they created a conflict.  To resolve
   this, we include the net-snmp-config.h first, and 
   include this file before we include other heartbeat
   header files. */

#ifdef PACKAGE_BUGREPORT
#undef PACKAGE_BUGREPORT
#endif

#ifdef PACKAGE_NAME
#undef PACKAGE_NAME
#endif

#ifdef PACKAGE_STRING
#undef PACKAGE_STRING
#endif

#ifdef PACKAGE_TARNAME
#undef PACKAGE_TARNAME
#endif

#ifdef PACKAGE_VERSION
#undef PACKAGE_VERSION
#endif

#endif // __SNMP_CONFIG_RESOLVE_H
