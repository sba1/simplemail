<?php

$filename = "simplemail-0.18.lha";
$version = "0.18";

  require_once("language.inc.php");
  require_once("support.inc.php");


?>

<p>
<?php echo get_string($DownloadsIntroText);?>
</p>

<center>
<table>
<tr bgcolor="black">
<th><font color="white">Archive</font></th>
<th><font color="white">Version</font></th>
<th><font color="white">Date</font></th>
<?php //<th><font color="white">Size</font></th>
?>
</tr>

<tr bgcolor="gray">
<?php
//  echo '<td align="center"><A HREF="download.php?filename='.$filename.'">'.$filename.'</A></td>';
  echo '<td align="center"><A HREF="http://prdownloads.sourceforge.net/simplemail/simplemail-0.18.lha?download">'.$filename.'</A></td>';
  echo '<td align="center">'.$version.'</td>';
  echo '<td align="center">2002/12/24</td>';

//  echo '<td align="center">'.date("Y/m/d", filemtime("files/".$filename)).'</td>';
//  echo '<td align="center">'.get_filesize("files/".$filename).'</td>';
?>
</tr>

</table>
</center>

<p>
<?php echo get_string($BannersText); ?>
</p>

<table border=0 cellspacing=3 cellpadding=3 align="center" summary="Banner">
<tr>
<td><?php get_img("banner/sm_now1.png");?></td>
<td><?php get_img("banner/sm_now2.png");?></td>
</tr>
<td><?php get_img("banner/sm_now3.png");?></td>
<td><?php get_img("banner/sm_now4.png");?></td>
</table>

<p>
<?php echo get_string($BannersCreatorText); ?>
</p>

<p>
<?php echo get_string($BannersCreator2Text); get_img("banner/sm_now5.png");?>

