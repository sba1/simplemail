<?php

  $email = $HTTP_SERVER_VARS["QUERY_STRING"];
  $fh = fopen("gallery/users.csv","r");

  while ($data = fgetcsv($fh, 1000, ",")) 
  {
    if (!strcasecmp($data[0],$email))
    {
       $size = GetImageSize("galery/" . $data[2]);
       switch($size[2])
       {
	  case 1: header("Content-Type: image/gif");break;
	  case 2: header("Content-Type: image/jpeg");break;
	  case 3: header("Content-Type: image/png");break;
	  default: header("Content-Type: image/x-unidentified");break;
       }
       fclose($fh);
       readfile("gallery/" . $data[2]);
       exit;
    }
  }
  fclose($fh);
?>


?>