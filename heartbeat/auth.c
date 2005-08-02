/* $Id: auth.c,v 1.19 2005/08/02 15:27:52 gshi Exp $ */
/*
 * auth.c: Authentication code for heartbeat
 *
 * Copyright (C) 1999,2000 Mitja Sarp <mitja@lysator.liu.se>
 *	Somewhat mangled by Alan Robertson <alanr@unix.sh>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <portability.h>
#define time FOOtime
#include <glib.h>
#undef time
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pils/plugin.h>
#include <heartbeat.h>

struct HBAuthOps * findauth(const char * type, char ** tptr);

unsigned char result[MAXLINE];


extern GHashTable*	AuthFunctions;
extern PILPluginUniv*	PluginLoadingSystem;


struct HBAuthOps *
findauth(const char * type, char ** tptr)
{
	struct HBAuthOps*	ret;

	/* Look and see if we already have the module loaded in memory */
	if (!g_hash_table_lookup_extended(AuthFunctions, type
	,	(gpointer*) tptr, (gpointer) &ret)) {

		PIL_rc	rc;

		/* Nope.  Load it now. */

		rc = PILLoadPlugin(PluginLoadingSystem, HB_AUTH_TYPE_S
		,	 type, NULL);

		if (rc != PIL_OK) {
			ha_log(LOG_ERR, "LoadPlugin on %s returned %d: %s"
			,	type, rc, PIL_strerror(rc));
		}
		
		if (!g_hash_table_lookup_extended(AuthFunctions, type
		,       (gpointer) tptr, (gpointer)&ret)) {
			ha_log(LOG_ERR, "Lookup extended#2 returned FALSE for %s"
			,	type);
			ha_log(LOG_ERR, "Table size: %d"
			,	g_hash_table_size(AuthFunctions));
			ret = NULL;
		}
	}
	return ret;
}

/*
 *  Set authentication method and key.
 *  Open and parse the keyfile.
 */

int
parse_authfile(void)
{
	FILE *		f;
	char		buf[MAXLINE];
	char		method[MAXLINE];
	char		key[MAXLINE];
	int		i;
	int		src;
	int		rc = HA_OK;
	int		authnum = -1;
	struct stat	keyfilestat;
	static int	ParsedYet = 0;
	int		j;

	if (ANYDEBUG) {
		ha_log(LOG_DEBUG
		,	"Beginning authentication parsing");
	}
	if (ANYDEBUG) {
		ha_log(LOG_DEBUG
		,	"%d max authentication methods", MAXAUTH);
	}
	if ((f = fopen(KEYFILE, "r")) == NULL) {
		ha_log(LOG_ERR, "Cannot open keyfile [%s].  Stop."
		,	KEYFILE);
		return(HA_FAIL);
	}
	if (ANYDEBUG) {
		ha_log(LOG_DEBUG, "Keyfile opened");
	}

	if (fstat(fileno(f), &keyfilestat) < 0
	||	keyfilestat.st_mode & (S_IROTH | S_IRGRP)) {
		ha_log(LOG_ERR, "Bad permissions on keyfile"
		" [%s], 600 recommended.", KEYFILE);
		fclose(f);
		return(HA_FAIL);
	}
	if (ANYDEBUG) {
		ha_log(LOG_DEBUG, "Keyfile perms OK");
	}
	config->auth_time = keyfilestat.st_mtime;
	config->rereadauth = 0;

	/* Allow for us to reread the file without restarting... */
	config->authmethod = NULL;
	config->authnum = -1;
	if (ANYDEBUG) {
		ha_log(LOG_DEBUG
		,	"%d max authentication methods", MAXAUTH);
	}
	/*
	 * We reload modules more than necessary.
	 *
	 * In an ideal world, we wouldn't unload something unless
	 * it became unreferenced.  But this is kind of a pain.
	 * We could make a list of the current modules
	 * cross compare it against the new set, but it's kind
	 * of a pain.
	 *
	 * At least we don't load every auth module, then unload those
	 * we find out we don't need - like the old code did ;-)
	 *
	 * (clean) patches are being accepted ;-)
	 */
	for (j=0; j < MAXAUTH; ++j) {
		if (ParsedYet) {
			if (config->auth_config[j].auth) {
				/* Unload this auth module */
				PILIncrIFRefCount(PluginLoadingSystem
				,	HB_AUTH_TYPE_S
				,	config->auth_config[j].authname, -1);

			}
			if (config->auth_config[j].key) {
				ha_free(config->auth_config[j].key);
			}
		}
		config->auth_config[j].auth = NULL;
		config->auth_config[j].authname = NULL;
		config->auth_config[j].key=NULL;
	}
	ParsedYet=1;

	while(fgets(buf, MAXLINE, f) != NULL) {
		char *	bp = buf;
		struct HBAuthOps *	at;
		
		bp += strspn(bp, WHITESPACE);

		if (*bp == COMMENTCHAR || *bp == EOS) {
			continue;
		}
		if (*bp == 'a') {
			if ((src=sscanf(bp, "auth %d", &authnum)) != 1) {
				ha_log(LOG_ERR
				,	"Invalid auth line [%s] in " KEYFILE
				,	 buf);
				rc = HA_FAIL;
			}
			/* Parsing of this line now complete */
			continue;
		}


		key[0] = EOS;
		if ((src=sscanf(bp, "%d%s%s", &i, method, key)) >= 2) {

			char *	cpkey;
			char *	permname;

			if (ANYDEBUG) {
				ha_log(LOG_DEBUG
				,	"Found authentication method [%s]"
				,	 method);
			}

			if ((i < 0) || (i >= MAXAUTH)) {
				ha_log(LOG_ERR, "Invalid authnum [%d] in "
				KEYFILE, i);
				PILIncrIFRefCount(PluginLoadingSystem
				,	HB_AUTH_TYPE_S, method, -1);
				rc = HA_FAIL;
				continue;
			}

			if ((at = findauth(method, &permname)) == NULL) {
				ha_log(LOG_ERR, "Invalid authtype [%s]"
				,	method);
				PILIncrIFRefCount(PluginLoadingSystem
				,	HB_AUTH_TYPE_S, method, -1);
				rc = HA_FAIL;
				continue;
			}

			if (strlen(key) > 0 && !at->needskey()) {
				ha_log(LOG_INFO
				,	"Auth method [%s] doesn't use a key"
				,	method);
				PILIncrIFRefCount(PluginLoadingSystem
				,	HB_AUTH_TYPE_S, method, -1);
				rc = HA_FAIL;
				continue;
			}
			if (strlen(key) == 0 && at->needskey()) {
				ha_log(LOG_ERR
				,	"Auth method [%s] requires a key"
				,	method);
				PILIncrIFRefCount(PluginLoadingSystem
				,	HB_AUTH_TYPE_S, method, -1);
				rc = HA_FAIL;
				continue;
			}

			cpkey =	ha_strdup(key);
			if (cpkey == NULL) {
				ha_log(LOG_ERR, "Out of memory for authkey");
				fclose(f);
				PILIncrIFRefCount(PluginLoadingSystem
				,	HB_AUTH_TYPE_S, method, -1);
				return(HA_FAIL);
			}
			config->auth_config[i].key = cpkey;
			config->auth_config[i].auth = at;
			config->auth_config[i].authname = permname;

			if (ANYDEBUG) {
				ha_log(LOG_INFO
				,	"AUTH: i=%d: key = 0x%0lx"
				", auth=0x%0lx, authname=%s", i
				,	(unsigned long)cpkey
				,	(unsigned long)at
				,	permname);
			}

			if (i == authnum) {
				config->authnum = i;
				config->authmethod = config->auth_config+i;
				if (ANYDEBUG) {
					ha_log(LOG_DEBUG
					,	"Outbound signing method is %d"
					,	i);
				}
			}
		}else if (*bp != EOS) {
			ha_log(LOG_ERR, "Auth line [%s] is invalid."
			,	buf);
			rc = HA_FAIL;
		}
	}

	fclose(f);
	if (!config->authmethod) {
		if (authnum < 0) {
			ha_log(LOG_ERR
			,	"Missing auth directive in keyfile [%s]"
			,	KEYFILE);
		}else{
			ha_log(LOG_ERR
			,	"Auth Key [%d] not found in keyfile [%s]"
			,	authnum, KEYFILE);
		}
		rc = HA_FAIL;
	}
	if (ANYDEBUG) {
		ha_log(LOG_DEBUG
		,	"Authentication parsing complete [%d]",  rc);
	}
	return(rc);
}
/*
 * $Log: auth.c,v $
 * Revision 1.19  2005/08/02 15:27:52  gshi
 * it should be (gpointer*) instead of gpointer
 *
 * Revision 1.18  2004/09/18 23:10:50  alan
 * Brought forward changes from 1.2 stable version.
 *
 * Revision 1.17  2004/09/03 18:12:48  gshi
 * fixed a bug in PILS for unloading a plugin
 * enabled unloading in HBauth and stonith
 *
 * Revision 1.16  2004/08/28 00:23:44  alan
 * Fixed a bug pointed out by Lars Ellenberg.
 *
 * Revision 1.15  2004/02/17 22:11:57  lars
 * Pet peeve removal: _Id et al now gone, replaced with consistent Id header.
 *
 * Revision 1.14  2004/02/10 05:32:43  alan
 * Suppress unloading authentication modules for now.
 *
 * Revision 1.13  2004/01/21 11:34:14  horms
 * - Replaced numerous malloc + strcpy/strncpy invocations with strdup
 *   * This usually makes the code a bit cleaner
 *   * Also is easier not to make code with potential buffer over-runs
 * - Added STRDUP to pils modules
 * - Removed some spurious MALLOC and FREE redefinitions
 *   _that could never be used_
 * - Make sure the return value of strdup is honoured in error conditions
 *
 * Revision 1.12  2003/12/21 11:18:37  horms
 * Don't used type-pruned pointers. Fixed some uninitialised variables
 *
 * Revision 1.11  2003/02/07 08:37:16  horms
 * Removed inclusion of portability.h from .h files
 * so that it does not need to be installed.
 *
 * Revision 1.10  2003/02/05 09:06:33  horms
 * Lars put a lot of work into making sure that portability.h
 * is included first, everywhere. However this broke a few
 * things when building against heartbeat headers that
 * have been installed (usually somewhere under /usr/include or
 * /usr/local/include).
 *
 * This patch should resolve this problem without undoing all of
 * Lars's hard work.
 *
 * As an asside: I think that portability.h is a virus that has
 * infected all of heartbeat's code and now must also infect all
 * code that builds against heartbeat. I wish that it didn't need
 * to be included all over the place. Especially in headers to
 * be installed on the system. However, I respect Lars's opinion
 * that this is the best way to resolve some weird build problems
 * in the current tree.
 *
 * Revision 1.9  2002/10/21 10:17:17  horms
 * hb api clients may now be built outside of the heartbeat tree
 *
 * Revision 1.8  2002/04/20 05:36:22  alan
 * Added a little debug for debugging Matt Soffen's authentication
 * problems.
 *
 * Revision 1.7  2001/07/18 20:44:55  alan
 * Removed extra #endif somehow added during last delta...
 *
 * Revision 1.6  2001/07/18 15:36:33  alan
 * Put in Emily Ratliff's fix of adding a missing parameter to an ha_log() call.
 *
 */
