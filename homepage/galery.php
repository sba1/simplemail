<?php

function galery_picture($filename,$name,$email)
{
  echo("<center>");
  $size = GetImageSize("galery/" . $filename);
  printf("<img src=\"galery/%s\" %s>",$filename,$size[3]);
  echo("<font size=-1>");
  if ($name != "") echo("<br>$name");
  if ($email != "") echo("<br>$email");
  echo("</font>");
  echo("</center>");
}

?>
