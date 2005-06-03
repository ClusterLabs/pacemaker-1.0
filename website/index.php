<?php include("trick.php"); ?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
<head>
 <meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
 <meta name="author" content="http://wiki.linux-ha.org">
 <title><?php echo "$pagetitle: $sitename"; ?></title>
 <link rel="stylesheet" href="/linuxha.css" type="text/css">
</head>
<body>
<?php browser_compatibility_messages(); ?>
<div id="_site">
 <div id="_header"><a href="/" title="<? echo "$sitename Home Page."; ?>"><?php echo MoinMoin("TopLogo"); ?></a></div>
 <div id="_menu"><?php echo MoinMoin("TopMenu"); ?></div>
 <div id="_pagebody">
  <div id="_sidebar">
   <div id="_mainmenu"><?php echo MoinMoin("MainMenu"); ?></div>
   <div id="_slashboxes"><?php echo MoinMoin("SlashBoxes"); ?></div>
   <div id="_additional_actions">
     <?php echo "<a href=/print.php/$pagename>printer friendly view</a>"?>
     <?php echo "<a href=/print.php/$pagename><IMG BORDER=\"0\" src=\"/img/moin-print.png\"></a>"?>
   </div>
  </div>
  <div id="_content">
	<?php echo $content; ?>
  </div>
  <div id="_footer"><?php echo $sitename; ?> built by <a href="/SiteCredits">The Linux-HA crowd</a></div>
 </div>
</div>
</body>
</html>
