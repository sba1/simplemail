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
?>
