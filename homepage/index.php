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
  $LinkMenu["downloads"]["datename"]="xml/downloads.xml";

  $LinkMenu["faq"]["filename"]="faq.php";
  $LinkMenu["faq"]["title"]=get_string($FAQText);
  $LinkMenu["faq"]["datename"]="xml/faq.xml";

  $LinkMenu["links"]["filename"]="links.php";
  $LinkMenu["links"]["title"]=get_string($LinksText);
  $LinkMenu["links"]["datename"]="links.php";

  $LinkMenu["contact"]["filename"]="contact.php";
  $LinkMenu["contact"]["title"]=get_string($ContactText);
  $LinkMenu["contact"]["datename"]="contact.php";

  $LinkMenu["gallery"]["filename"]="gallery.php";
  $LinkMenu["gallery"]["title"]=get_string($GalleryText);
  $LinkMenu["gallery"]["datename"]="gallery.php";

  if (!isset($body)) $body="default";
?>
 
<html>
  <head>
    <title>SimpleMail</title>
    <STYLE TYPE="text/css"><!--
      A.menu_active { text-decoration: none; color: red; }
      A.menu_inactive { text-decoration: none; color: gray; }
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
	      if ($key == $body) $class = "menu_active";
	      else $class = "menu_inactive";

	      printf("<A CLASS=\"%s\" HREF=\"index.php%s\">%s</A><br>",$class,$key!="default"?"?body=".$key:"",$value["title"]);
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
        <td>
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
    </table>

    <table>
      <tr>
        <td>

        </td>
      </tr>
    </table>

  </body>
</html>



