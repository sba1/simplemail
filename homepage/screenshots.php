<?php

function screenshot_dims($filename)
{
  $size = GetImageSize($filename);
  $width = 200;
  $height = 200*$size[1]/$size[0];
  return array($width,$height);
}

function screenshot_preview($filename,$desc)
{
  $size = GetImageSize($filename);
  $dims = screenshot_dims($filename);
  printf("<A HREF=\"%s\">",$filename);
  printf("<IMG SRC=\"%s\" WIDTH=%d HEIGHT=%d ALT=\"%ld x %ld\">","screenshots_preview.php?".basename($filename),$dims[0],$dims[1],$size[0],$size[1]);
  printf("</A>");
  echo("<br>$desc<br>");
}

?>
