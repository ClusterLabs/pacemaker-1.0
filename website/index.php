<!--
Author:       Dmytri Kleiner <dk@trick.ca>
Support:      linux-ha@lists.linux-ha.org
License:      GNU General Public License (GPL)
Copyright:    (C) 2005 Dmytri Kleiner <dk@trick.ca>
-->
<?php include("trick.php"); ?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html lang="en">
<head>
 <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
 <meta http-equiv="keywords"
       content="high-availability, open source software, free software, POSIX, UNIX, FreeBSD, Solaris, cluster, reliability, availability, serviceability">
<script type="text/javascript">
  if ((navigator.appName).indexOf("Microsoft") != -1) {
    document.write('<link rel="stylesheet" href="/linuxhaIE6.css" type="text/css">');
  }else{
    document.write('<link rel="stylesheet" href="/linuxha.css" type="text/css">');
  }
</script>
<noscript>
  <link rel="stylesheet" href="/linuxha.css" type="text/css">
</noscript>
 <meta name="author" content="wiki dot linux dash ha dot org">
 <?php robots_metadata(); ?>
 <title><?php echo "$pagetitle: $sitename"; ?></title>
</head>
<body>
<?php browser_compatibility_messages(); ?>
<div id="i_site">
 <div id="i_header"><?php echo MoinMoin("TopLogo"); ?></div>
 <div id="i_menu"><?php echo MoinMoin("TopMenu"); ?></div>
 <div id="i_pagebody">
  <div id="i_sidebar">
   <div id="i_mainmenu"><?php echo MoinMoin("MainMenu"); ?></div>
   <div id="i_search"><?php search_box_html(20); ?></div>
   <div id="i_slashboxes"><?php echo MoinMoin("SlashBoxes"); ?></div>
   <div id="i_additional_actions">
     <?php echo "<a href=\"/print.php/$MOINMOINpagename\">printer friendly view";
      echo "<IMG src=\"/img/moin-print.png\" alt=\"printer\" WIDTH=16 HEIGHT=14></A>"; ?>
   </div>
  </div>
  <div id="i_content">
	<?php echo $content; ?>
  </div>
  <div id="i_footer"><?php echo MoinMoin("PageFooter"); ?></div>
 </div>
</div>
</body>
</html>
