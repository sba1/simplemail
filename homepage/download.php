<?php

  $path = "files/".$filename;

  if ($filename != "simplemail.lha") die("Unknown file requested!");
 
  $fp = fopen("downloads.txt","r");
  if ($fp==0) $downs=0;
  else {
    $downs=intval(fread($fp,filesize("downloads.txt")),10);
    fclose($fp);
  }

  $downs++;
  $fp = fopen("downloads.txt", "w");
  fwrite($fp, strval($downs));
  fclose($fp);
  header("Content-Type: application/x-lha");
  header("Content-Length: " . filesize($path));
  header("Content-Disposition: attachment; filename=\"".$filename."\"");

  readfile($path);

?>

