<?php
    if($what != "simplemail.lzx") die("Unknown file requested!");

    $fp = fopen("downloads.txt","r");
    if($fp==0) die("Error while reading downloads.txt.");
    $downs=intval(fread($fp,filesize("downloads.txt")),10);
    fclose($fp);
    $downs++;
    $fp = fopen("downloads.txt", "w");
    fwrite($fp, strval($downs));
    fclose($fp);
    readfile("files/simplemail.lzx");
?>
