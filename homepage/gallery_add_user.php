<?php

  function gallery_picture($filename,$name,$email)
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

  if ($name != "" && $email != "" && $userfile != "")
  {
    if (file_exists("gallery/users.csv")) $fh = fopen("gallery/users.csv","r+");
    else $fh = 0;

    $add_pic = 1;

    if ($fh)
    {
      while ($data = fgetcsv($fh, 1000, ",")) 
      {
        if (!strcasecmp($data[0],$email))
        {
          echo("<center>".get_string($GalleryAlreadyText)."</center><br>");
          gallery_picture($data[2],$data[1],$data[0]);
          $add_pic = 0;
          break;
        }
      }
      fclose(fh);
    }

    if ($add_pic == 1)
    {
      $uh = fopen($userfile,"rb");
      if ($uh)
      {
	$binary = fread($uh,$userfile_size);
        $base64 = wordwrap(base64_encode($binary),64,"\n",!);
        $boundary = "--=gzdsghkdgsdfjkdsjfk";

	mail("sebauer@t-online.de","SimpleMail User Gallery",
	     /* The mail's body */
	     "--".$boundary."\n".
             "Content-Type: text/plain\n".
             "Content-transfer-encoding: base64\n".
             "\n".wordwrap(base64_encode("Name: ".$name."\n".
						"EMail: ".$email."\n"),64,"\n",1)."\n".
             "--".$boundary."\n".
	     "Content-Disposition: attachment; filename=image\n".
             "Content-Type: application/octet-stream\n".
             "Content-transfer-encoding: base64\n".
	     "\n".$base64."\n".
	     "--".$boundary."--\n",
             /* Additional headers */
	     "MIME-Version: 1.0\n",
	     "Content-Type: multipart/mixed; boundary=\"".$boundary."\"");

	fclose($uh);
      }
    }
  }
    
?>