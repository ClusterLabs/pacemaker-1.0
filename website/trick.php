<?php
# WikiWare - Standard Semantic Content Switcher
# Dmytri Kleiner -- dmytrik@trick.ca
# Lars Ellenberg -- l.g.e@web.de
#   some adjustments in 2005 for the moved linux ha wiki
# Copyleft 2003-2004 Idiosyntactix. All rights detourned.	

# Usage Examples:
#   include "trick.php";
#   print $content; 

# General Configuration
$sitename = "Linux HA";

# for my local test wiki
# $MOINMOINalias = "ha"; # do NOT include leading or trailing slash
# $MOINMOINserver = "http://localhost";

# for this one:
$MOINMOINserver = "http://wiki.linux-ha.org";
$MOINMOINalias = ""; # do NOT include leading or trailing slash
$MOINMOINurl = "$MOINMOINserver/$MOINMOINalias";
$MOINMOINcachedir = "_cache";
$MOINMOINfilemod = 0664;
#
#	Images to delete when we see them...
#
global $MOINMOINCacheLimit;
global $MOINMOINExtraneousImages;
global $MOINMOINSitesToIndex;
global $MOINMOINpagename;

$MOINMOINExtraneousImages = array(
	'/wiki/classic/img/moin-diff.png',
	'/wiki/classic/img/moin-ftp.png',
	'/wiki/classic/img/moin-rss.png',
	'/wiki/classic/img/moin-inter.png',
	'/wiki/classic/img/moin-www.png',
	'/wiki/classic/img/moin-top.png',
	'/wiki/classic/img/moin-bottom.png',
);
$MOINMOINCacheLimit = array("RecentChanges" => 300);

/* Only these sites will have this tag:
 *	<meta name="robots" content="index,follow">
 * When viewed via any other site name, the will say:
 *	<meta name="robots" content="noindex,nofollow">
 */

$MOINMOINSitesToIndex = array("www.linux-ha.org" => 1, "linux-ha.org" => 1);



# for http://some.server/WikiPageName
$local_cache_url_prefix = "/";
# for http://some.vhost.dedicated.to.transclusion/WikiPageName
# $local_cache_url_prefix = "/"

#############################################################
#############################################################
#############################################################


include "moinmoin.php";

$path_info = $HTTP_SERVER_VARS["PATH_INFO"];

if (strlen($path_info) < 3) { 
	
	# Default Home Page
 $real_path_info = $path_info;	
	$path_info = "/HomePage";
}



# Wiki Page
$MOINMOINpagename = substr($path_info,1);

# in case there is some wiki style escaped special char in the pagename derived from the url
# $pagetitle is only used in the "title" tag, and should be readable
# (typically displayed as window/tab title by your browser)
$pagetitle = str_replace(array("_20","_21","_2c", "_2d","_2e","_2f","_27", "_28", "_29", "_3a"), array(" ","!",",","-",".","/","'","(",")",":"), $MOINMOINpagename);

# in case there are some special chars in the page name, that are not yet escaped ...
# $MOINMOINpagename is used to request that page from the wiki, or to read in the cache file
$MOINMOINpagename = str_replace(array(" ","!",",","-",".","/","'","(",")",":"), array("_20","_21","_2c", "_2d","_2e","_2f","_27", "_28", "_29", "_3a"), $MOINMOINpagename);
	
$content = MoinMoin($MOINMOINpagename);

?>
