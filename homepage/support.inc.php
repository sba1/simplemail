<?php

/* Returns the size string of a file */
function get_filesize($fn)
{
  $size = filesize($fn);
  if ($size >= 1024*10) return sprintf("%d.%d kilo bytes",$size / 1024,1000*($size%1024)/1024/100);
  else return sprintf("%d bytes",$size);
}

/* returns a string for a <img> element */
function get_img($fn)
{
  $size = GetImageSize($fn);
  echo '<img src="'.$fn.'" '.$size[3].' ALT="'.basename($fn).'('.get_filesize($fn).')">';
}

?>

