<?php-track-vars?>
<?php $what=$HTTP_SERVER_VARS["QUERY_STRING"];?>
<?php error_reporting(1);?>
<?php
    if($what != "simplemail.lzx") die("Unknown file requested!");

    $fp = fopen("downloads.txt","r");
    if($fp==0)
    {
      $downs=0;
    } else
    {
      $downs=intval(fread($fp,filesize("downloads.txt")),10);
      fclose($fp);
    }
    $downs++;
    $fp = fopen("downloads.txt", "w");
    fwrite($fp, strval($downs));
    fclose($fp);
    header("Content-Type: application/x-lzx");
    readfile("files/simplemail.lzx");
?>

