<?php
  $fn = "hits.txt";

  #echo("<B>Visitors:</B> ");

  $fp = fopen($fn,"r");
  if($fp==0) die("Error while reading hits.txt.");
  $hits=intval(fread($fp,filesize($fn)),8);
  fclose($fp);
  if($newguest == "true")
  {
    $hits++;
    $fp = fopen($fn, "w");
    fwrite($fp, strval($hits));
    fclose($fp);
  }

  #echo($hits);
?>

