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

  phpinfo();

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
      echo "jkjkaadadasdsadsddadasdsaadas";
    }
  }
    
?>