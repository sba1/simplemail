<?php-track-vars?>
<?php $what="screenshots/".$HTTP_SERVER_VARS["QUERY_STRING"];?>
<?php
  include("screenshots.php");
  $dims=screenshot_dims($what);
  header("Content-Type: image/png");

  $tmp=tempnam("/tmp","simplemail");
  system(sprintf("pngtopnm %s | pnmscale -xysize %ld %ld | ppmquant 256 | pnmtopng >%s",$what,$dims[0],$dims[1],$tmp));
  readfile($tmp);
  unlink($tmp);

?>
