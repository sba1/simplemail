<?php
  require("counter.php");
  require_once("language.inc.php");

  $LinkMenu["default"]["filename"]="main.php";
  $LinkMenu["default"]["title"]=get_string($WelcomeText);
  $LinkMenu["default"]["datename"]="xml/news.xml";

  $LinkMenu["screenshots"]["filename"]="screenshots.php";
  $LinkMenu["screenshots"]["title"]=get_string($ScreenshotsText);
  $LinkMenu["screenshots"]["datename"]="xml/screenshots.xml";

  $LinkMenu["downloads"]["filename"]="downloads.php";
  $LinkMenu["downloads"]["title"]=get_string($DownloadsText);
  $LinkMenu["downloads"]["datename"]="downloads.php";

  $LinkMenu["faq"]["filename"]="faq.php";
  $LinkMenu["faq"]["title"]=get_string($FAQText);
  $LinkMenu["faq"]["datename"]=get_filename("xml/faq.xml");

  $LinkMenu["links"]["filename"]="links.php";
  $LinkMenu["links"]["title"]=get_string($LinksText);
  $LinkMenu["links"]["datename"]="links.php";

  $LinkMenu["contact"]["filename"]="contact.php";
  $LinkMenu["contact"]["title"]=get_string($ContactText);
  $LinkMenu["contact"]["datename"]="contact.php";

  $LinkMenu["gallery"]["filename"]="gallery.php";
  $LinkMenu["gallery"]["title"]=get_string($GalleryText);
  $LinkMenu["gallery"]["datename"]="gallery.php";

  $LinkMenu["gallery_add_user"]["filename"]="gallery_add_user.php";
  $LinkMenu["gallery_add_user"]["title"]=get_string($GalleryText);
  $LinkMenu["gallery_add_user"]["datename"]="gallery.php";

  if (!isset($body)) $body="default";
?>
 
<html>
  <head>
	<title>SimpleMail</title>
	<STYLE TYPE="text/css"><!--
	  A { text-decoration: none }
	  A:hover{ background: #ffa }
--></STYLE>

  </head>

  <body bgcolor="white">

	<center><A href="index.php"><IMG border="0" src="pictures/sm_logo.png" alt="sm_logo.png (1171 bytes)" width=274 height=45></A></center>

	<table width="100%" summary="Main page.">
	  <tr bgcolor="black">
		<th width=0>
		  <font color="white">
			<tt>
			  <?php echo get_string($MenuText);?>
			</tt>
		  </font>
		</th>
		<th>
		  <font color="white">
			<tt>
			  <?php echo $LinkMenu[$body]["title"];?>
			</tt>
		  </font>
		</th>
	  </tr>

	  <tr>
		<td valign="top" width=0>
	  <?php
	    foreach($LinkMenu as $key => $value)
	    {
	      if ($key != "gallery_add_user")
	      {
	        if ($key != $body)
	          printf("<A CLASS=\"menu\" HREF=\"index.php%s\">",$key!="default"?"?body=".$key:"");
	        echo $value["title"];
	        if ($key != $body)
	          echo "</A>";
	        echo "<br>";
			  }
	    }
		  ?>
		</td>

		<td>
		  <?php
			include($LinkMenu[$body]["filename"]);
		  ?>
		</td>
	  </tr>
	</table>

	<table align="center" width="100%" summary="Footer containing infos and banners.">
	  <tr bgcolor="black">
		<td colspan="2">
		  <center>
		  <font color="white"
			<tt>
				<?php

				  $unixTime = filemtime($LinkMenu[$body]["datename"]);

		  printf("%s %s · %s %s",get_string($LastModificationText),date("Y/m/d", $unixTime),
					 get_string($VisitorsText),$hits);
		?>

			  </tt>
			</font>
		  </center>
		</td>
	  </tr>
	  <tr>
		<td>
			Hosted by: <A href="http://sourceforge.net"> <IMG src="http://sourceforge.net/sflogo.php?group_id=15322" width="88" height="31" border="0" alt="SourceForge Logo"></A>
		</td>
		<td align="right">
			<A href="http://sourceforge.net/project/project_donations.php?group_id=15322"><img src="pictures/paypal.gif" width="62" height="31" border="0" alt="Donate to SimpleMail!">
			</A>
		</td>
	  </tr>
	</table>

  </body>
</html>





