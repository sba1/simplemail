<?php

  $path = "files/".$filename;

  if ($filename != "simplemail.lzx") die("Unknown file requested!");
 
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
  header("Content-Type: application/x-lzx");
  readfile($path);

?>
