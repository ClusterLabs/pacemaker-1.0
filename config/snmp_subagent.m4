AC_DEFUN([LIB_RPM], [
    AC_MSG_CHECKING("for rpm libraries")
    dnl back up the old value
    LIBS_BEFORE_RPMCHECK="$LIBS"
    CFLAGS_BEFORE_RPMCHECK="$CFLAGS"
    
    LIBS=""
    CFLAGS="" 

    dnl # BEGIN include from UCD-SNMP config scripts.
    dnl #
    dnl # The test below are taken directly from configure.in from UCD-SNMP 4.2.5.
    dnl # Apparently, UCD-SNMP introduced some very nasty library dependencies since 
    dnl # 4.2.4. 

    # ARG.  RPM is a real pain.

    # FWIW librpm.la, librpmio.la, and libpopt.la have correct dependencies.

    _rpmlibs=""

    # zlib is required for newer versions of rpm
    _cppflags="${CPPFLAGS}"
    _ldflags="${LDFLAGS}"

    # dunno if this is needed for rpm-4.0.x, earlier probably needs.
    AC_CHECK_HEADER(zlib.h,
	AC_CHECK_LIB(z, gzread, , CPPFLAGS=${_cppflags} LDFLAGS=${_ldflags}),
	CPPFLAGS=${_cppflags} LDFLAGS=${_ldflags})

    # two variants of bzip2 need checking.
    AC_CHECK_LIB(bz2, bzread, [_rpmlibs="$_rpmlibs -lbz2"],
	AC_CHECK_LIB(bz2, BZ2_bzread, [_rpmlibs="$_rpmlibs -lbz2"],))

    # two variants of db1 need checking.
    AC_CHECK_LIB(db1, dbopen, [_rpmlibs="-ldb1 $_rpmlibs"],
	AC_CHECK_LIB(db, dbopen, [_rpmlibs="-ldb $_rpmlibs"])
    )

    # two variants of db3 need checking.
    AC_CHECK_LIB(db-3.1, db_create, [_rpmlibs="-ldb-3.1 $_rpmlibs"],
	AC_CHECK_LIB(db-3.0, db_create, [_rpmlibs="-ldb-3.0 $_rpmlibs"])
    )

    # rpm-3.0.5 and later needs popt.
    AC_CHECK_LIB(popt, poptParseArgvString, [_rpmlibs="-lpopt $_rpmlibs"])

    # rpm-4.0.x needs rpmio.
    AC_CHECK_LIB(rpmio, Fopen, [_rpmlibs="-lrpmio $_rpmlibs"],,$_rpmlibs)

    # now check for rpm using the appropriate libraries.
    AC_CHECK_LIB(rpm, rpmGetFilesystemList,[
	AC_DEFINE(HAVE_LIBRPM, 1, [have librpm])
	RPM_LIBS="-lrpm $_rpmlibs $LIBS"
	RPM_CFLAGS="$CFLAGS -I/usr/include/rpm"
    ],[
      # rpm-4.0.3 librpmdb actually contains what we need.
	AC_CHECK_LIB(rpmdb, rpmdbOpen,[
	AC_DEFINE(HAVE_LIBRPM, 1, [have librpm])
	RPM_LIBS="$LIBS -lrpmdb -lrpm $_rpmlibs "
	RPM_CFLAGS="$CFLAGS -I/usr/include/rpm"
      ],,-lrpm $_rpmlibs)
    ])

    dnl END include from UCD-SNMP config scripts.
    
    dnl restore the old value
    AC_MSG_WARN(rpm libs are: $RPM_LIBS, $RPM_CFLAGS)
    LIBS=$LIBS_BEFORE_RPMCHECK
    CFLAGS=$CFLAGS_BEFORE_RPMCHECK
])

AC_DEFUN([LIB_SNMP], 
    [AC_MSG_CHECKING([for the SNMP libraries...])
    dnl back up old value
    LIBS_BEFORE_SNMPCHECK=$LIBS
    CFLAGS_BEFORE_SNMPCHECK=$CFLAGS

    NET_SNMP_HEADER=no
    dnl check net-snmp headers
    AC_CHECK_HEADERS([net-snmp/version.h], [NET_SNMP_HEADER=yes], 
        [AC_MSG_WARN([net-snmp header not found])], [])

    UCD_SNMP_HEADER=no
    dnl then check ucd-snmp headers 
    AC_CHECK_HEADERS([ucd-snmp/version.h], [UCD_SNMP_HEADER=yes],
        [AC_MSG_WARN([ucd-snmp header not found])], [#define UCD_COMPATIBLE])
        dnl make sure net-snmp is compiled with "--enable-ucd-snmp-compatibility"

    dnl if
    dnl   test x"$NET_SNMP_HEADER" = x"yes" && test x"$UCD_SNMP_HEADER" = x"no" ;
    dnl then
    dnl   AC_MSG_WARN([Your NET-SNMP is not compiled with --enable-ucd-snmp-compatibility turned on.])
    dnl   AC_MSG_ERROR([Please recompile the NET-SNMP with that option.])
    dnl fi

    dnl exit if no SNMP headers found.
    if
      test x$NET_SNMP_HEADER = x"no"  && test x"$UCD_SNMP_HEADER" = x"no" ;
    then
      AC_MSG_WARN([No NET-SNMP or UCD-SNMP headers found])
      AC_MSG_ERROR([Please download the SNMP package from http://www.net-snmp.org.])
    fi

    dnl some libs and header might be needed later
    AC_CHECK_HEADERS([tcpd.h], [], [])
    AC_CHECK_LIB([wrap], [eval_client], [have_libwrap=yes], 
        [have_libwrap=no], [])
    AC_CHECK_LIB([nsl], [nis_perror], [have_libnsl=yes], 
        [have_libnsl=no], [])
    AC_CHECK_LIB([crypto], [main], [have_libcrypto=yes], 
        [have_libcrypto=no], [])

    dnl find out the mibs_dir
    AC_MSG_CHECKING(which MIB directory to use)
    for mibs_dir in /usr/share/snmp/mibs /usr/local/share/snmp/mibs 
    do 
    	if test -d $mibs_dir; then
	    MIBS_DIR=$mibs_dir
	    AC_MSG_RESULT($MIBS_DIR);
	fi
    done
    AC_ARG_WITH(mibsdir, 
    		[  --with-mibsdir=DIR      directory for mib files. ],
		[ MIBS_DIR="$withval" ])
    if 
        test "X$MIBS_DIR" = X
    then
    	AC_MSG_ERROR(Could not locate mib directory)
    fi
    AC_SUBST(MIBS_DIR)

    dnl now start the snmp library dependency checking.
    SNMP_LIBS=""
    SNMP_LIBS_FOUND=no
    dnl check ucd-snmp libraries
    if test x"$NET_SNMP_HEADER" = x"no"; then

	dnl try the simpliest case first
        UCD_SNMP_LIBS="-lsnmp -lucdagent -lucdmibs"
        LIBS=$UCD_SNMP_LIBS
        AC_TRY_LINK([], [], 
	    [SNMP_LIBS="$LIBS"
	    SNMP_LIBS_FOUND=yes], 
	    [AC_MSG_WARN([UCD-SNMP: "$LIBS" dependency failed, try adding crypto...])])

 	dnl try adding libcrypto
 	LIB_CRYPTO=""
 	if test x"$SNMP_LIBS_FOUND" = x"no"; then
 	    if test x"$have_libcrypto" = x"yes"; then
 		LIB_CRYPTO="-lcrypto"
 		LIBS="$UCD_SNMP_LIBS $LIB_CRYPTO"
 		AC_TRY_LINK([], [], 
 		    [SNMP_LIBS="$LIBS"
 		    SNMP_LIBS_FOUND=yes], 
 		    [AC_MSG_WARN([UCD-SNMP: "$LIBS" dependency failed, try adding tcpwrapper...])])
 	    else
 		AC_MSG_WARN([UCD-SNMP: no libcrypto available, try adding tcpwrapper...])
 	    fi
 	fi
 
 	dnl try adding tcp_wrapper
 	LIB_WRAP=""
 	if test x"$SNMP_LIBS_FOUND" = x"no"; then
 	    if test x"$have_libwrap" = x"yes"; then
 		if test x"$have_libnsl" = x"yes"; then
 		    LIB_WRAP="-lwrap -lnsl"
 		else 
 		    LIB_WRAP="-lwrap"
 		fi
 		LIBS="$UCD_SNMP_LIBS $LIB_CRYPTO $LIB_WRAP"
 		AC_TRY_LINK([#include <tcpd.h>
 		    int allow_severity = 1;
 		    int deny_severity = 2;], [],
 		    [SNMP_LIBS="$LIBS"
 		    SNMP_LIBS_FOUND=yes
		    AC_DEFINE([SNMP_NEED_TCPWRAPPER], 1, [Need to include tcpd.h headers]) ], 
 		    [AC_MSG_WARN([UCD-SNMP: "$LIBS" dependency failed, try adding rpms, it will be a mess...])])
 	    else
 		AC_MSG_WARN([UCD-SNMP: no libwrap available, try adding rpms, it will be a mess...])
 	    fi
 	fi
 
 	dnl try adding rpm libraries
 	if test x"$SNMP_LIBS_FOUND" = x"no"; then
 	    dnl getting rpm libraries
 	    LIB_RPM
	    LIBS="$UCD_SNMP_LIBS $LIB_CRYPTO $LIB_WRAP $RPM_LIBS"
	    AC_TRY_LINK([#include <tcpd.h>
		int allow_severity = 1;
		int deny_severity = 2;], [],
		[SNMP_LIBS="$LIBS"
		SNMP_LIBS_FOUND=yes
		AC_DEFINE([SNMP_NEED_TCPWRAPPER], 1, [Need to include tcpd.h headers]) ], 
		[AC_MSG_WARN([UCD-SNMP: "$LIBS" dependency failed!!!])])
 	fi

    dnl check for net-snmp libraries
    else
	AC_DEFINE([HAVE_NET_SNMP], 1, [have net-snmp headers])
	AC_CHECK_PROG([have_net_snmp_config], [net-snmp-config], 
	    [yes], [no])
	AC_CHECK_HEADERS([net-snmp/util_funcs.h], [NET_SNMP_UTIL_FUNCS_HEADER=yes], 
        [AC_MSG_WARN([net-snmp util_funcs header not found])], [])
	dnl no net-snmp-config, quit
	if test x"have_net_snmp_config" = x"no"; then
	    AC_MSG_ERROR([NET-SNMP: program "net-snmp-config" not found in default path.])
	else
	    NET_SNMP_LIBS=`net-snmp-config --agent-libs`

 	    LIBS=$NET_SNMP_LIBS
            AC_TRY_LINK([], [], 
	    		[SNMP_LIBS="$LIBS"
	    		SNMP_LIBS_FOUND=yes], 
	    		[AC_MSG_WARN([NET-SNMP: "$LIBS" dependency failed, try adding libwrap...])])

    	    dnl try adding tcp_wrapper
 	    LIB_WRAP=""
 	    if test x"$SNMP_LIBS_FOUND" = x"no"; then
 	        if test x"$have_libwrap" = x"yes"; then
 	    	    if test x"$have_libnsl" = x"yes"; then
 	    	        LIB_WRAP="-lwrap -lnsl"
 	    	    else 
 	    	        LIB_WRAP="-lwrap"
 	    	    fi
 	    	    LIBS="$NET_SNMP_LIBS $LIB_WRAP"
 	    	    AC_TRY_LINK([#include <tcpd.h>
 	    	                 int allow_severity = 1;
 	    	                 int deny_severity = 2;], [],
 	    	                 [ SNMP_LIBS="$LIBS"
 	    	                   SNMP_LIBS_FOUND=yes 
				   AC_DEFINE([SNMP_NEED_TCPWRAPPER], 1, [Need to include tcpd.h headers]) ], 
 	    	                 [AC_MSG_WARN([NET-SNMP: "$LIBS" dependency failed!!!])])
 	        else
 	    	    AC_MSG_WARN([NET-SNMP: no libwrap available. "$LIBS" dependency failed!!!])
 	        fi
 	    fi

	    dnl try adding rpm libraries
	    RPM_LIBS=""
	    if test x"$SNMP_LIBS_FOUND" = x"no"; then
	    	dnl get the rpm libraries
		LIB_RPM
		LIBS="$NET_SNMP_LIBS $LIB_CRYPTO $LIB_WRAP $RPM_LIBS"
		AC_TRY_LINK([], [],
		    [SNMP_LIBS="$LIBS"
		    SNMP_LIBS_FOUND=yes], 
		    [AC_MSG_WARN([NET-SNMP: "$LIBS" dependency failed!!!])])
	    fi
	fi
    fi

    dnl restore the old value
    LIBS=$LIBS_BEFORE_SNMPCHECK
    CFLAGS=$CFLAGS_BEFORE_SNMPCHECK

    if test x"$SNMP_LIBS_FOUND" = x"no"; then
	AC_MSG_ERROR([
	Despite my best effort I still cannot figure out the library 
	dependencies for your snmp libraries.  
	
	Your best bet will be compile the net-snmp package from the source and 
	try again. 

	Note for RedHat/Fedora users:

	    If you installed the NET-SNMP RPM from a RedHat/Fedora CD, make sure 
	    both the symbolic links for libbz2.so -> libbz2.so.x and 
	    libelf.so -> libelf.so.x exists. Or install the libelf-devel and 
	    libbz2-devl rpms.
	    
	For SUSE LINUX users: 

	    If you installed the NET-SNMP RPM from a Suse release, please
	    double check and make sure that you have the 'tcpd-devel' 
	    rpm installed on your system as well. 
	    Many problems come from the lack of one of these packages
		openssl, openssl-devel
		rpm, rpm-devel
		and tcpd-devel

	])
    else
	AC_MSG_WARN([SNMP: snmp library dependency resolved. List of libraries needed to compile the subagent:])
	AC_MSG_WARN([	$SNMP_LIBS.])
	AC_SUBST(SNMP_LIBS)
    fi
])
	



