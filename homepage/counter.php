<?php

    if($newguest == "")
    {
        setcookie("newguest","true");
        $newguest="true";
    }
    else
    {
        setcookie("newguest", "false");
        $newguest="false";
    }

    $fn = "hits.txt";

    $fp = fopen($fn,"r");
    if($fp==0) die("Error while reading hits.txt.");
    $hits=intval(fread($fp,filesize($fn)),10);
    fclose($fp);
    if($newguest == "true")
    {
        $hits++;
        $fp = fopen($fn, "w");
        fwrite($fp, strval($hits));
        fclose($fp);
    }
?>

