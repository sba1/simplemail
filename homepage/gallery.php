<?php
  require_once("language.inc.php");
  require_once("support.inc.php");

  $num_users = 0;

  if (file_exists("gallery/users.csv"))
  {
    $fh = fopen("gallery/users.csv","r");
    if ($fh)
    {
      while ($line = fgets($fh, 1000))
         $num_users++;
      fclose($fh);
    }
    printf(get_string($GalleryUsersText)."<br>",$num_users);

    $fh = fopen("galery/users.csv","r");
    if ($fh)
    {
      while ($data = fgetcsv($fh, 1000, ",")) 
        printf("<A HREF=\"gallery_get_image.php?%s\">%s</A><br>",$data[0],$data[1]);
      fclose($fh);
    }
  } else {
    echo(get_string($GalleryNoUsersText)."<br>");
  }

  echo "<HR>\n";
  echo get_string($GalleryIntroText)."<br>";

  echo '<FORM ENCTYPE="multipart/form-data" ACTION="index.php?body=gallery_add_user" METHOD=POST><P>';
  echo get_string($GalleryNameText).'<INPUT TYPE="text" name="name">'.'<BR>';
  echo get_string($GalleryEMailText).'<INPUT TYPE="text" name="email">'.'<BR>';
  echo get_string($GalleryPhotoText).'<INPUT TYPE="file" NAME="userfile">'.'<BR>';
?>

<INPUT TYPE="hidden" name="MAX_FILE_SIZE" value="75000">
<center>
  <INPUT TYPE="submit" VALUE="Send the picture">
  <INPUT TYPE="reset">
</center>
</P>
</FORM>






