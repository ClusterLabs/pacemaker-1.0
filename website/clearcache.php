<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
  <title>Clear Cache</title>
  <meta http-equiv="Content-Type" content="text/html; charset=ISO-8859-1">
  <!-- style sheet ? -->
</head>
<body>
<h1>Clear Cache</h1>
<hr>
<blockquote>
<?php
	clearstatcache();
	$cachedir = "_cache";

	$errorcount = 0;
	$trycount = 0;

	# lge suggests:
	$delete_prefix = substr($HTTP_SERVER_VARS["PATH_INFO"],1);
	# maybe:
	$delete_prefix = str_replace(array(" ","!",",","-",".","/","'","(",")",":"), array("_20","_21","_2c", "_2d","_2e","_2f","_27", "_28", "_29", "_3a"), $delete_prefix);

	$dir_fd = opendir($cachedir);
	while($entryname = readdir($dir_fd))
	{
	     	$cachefile = "$cachedir/$entryname" ;
		if (!is_dir($cachefile) and (!$delete_prefix or preg_match("/^$delete_prefix/",$entryname)))
	     	{
	     		if (unlink($cachefile))
	     		{ 
	     			$try = "deleted";
	     			$trycount++;
	     		} else {
	     			$try = "failed";
	     			$errorcount++;
	     			$trycount++;
	     		}
	     		echo "$entryname ... <tt>$try</tt><br>\n";
	     	}
	}
	closedir($dir_fd);
?>

<br><br>

<?php	
	
	if ($trycount == 0)
	{
		echo $delete_prefix
		     ?  "No pages matching ^$delete_prefix.* in the Transclusion Cache.<br><br>"
		     :  "Your Transclusion Cache is empty.<br><br>";

	} else {
	
		if ($errorcount == 0)
		{
?>  

Your Transclusion Cache <?php echo ($delete_prefix ? "for $delete_prefix" : "" )?> has been cleared.<br>
Latest updates will now be transcluded.<br><br>

<?php
			echo "$trycount cache files deleted.<br>";		
	
		} else {
?>

Unable to fully clear cache.<br><br>

<?php
			echo "$errorcount out of $trycount failed.<br>";
		}
	}
?>
</blockquote>
<a href="/">Return to Linux-HA Web Site</a>
</body>
</html>
