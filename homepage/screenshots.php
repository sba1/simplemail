<?php
  $file='xml/screenshots.xml';

  class Screenshot {
    var $filename;     // The filename of the screenshot
    var $description;  // The screenshot's describtion
    var $version;      // The version number of SimpleMail from where the screenshot is

    function Screenshot ($aa) {
        foreach ($aa as $k=>$v)
            $this->$k = trim($aa[$k]);
        $this->filename = "screenshots/".$this->filename;
    }
  }

  function readDatabase($filename) {
    // read the xml database of screenshots
    $data = implode("",file($filename));
    $parser = xml_parser_create();
    xml_parser_set_option($parser,XML_OPTION_CASE_FOLDING,0);
    xml_parser_set_option($parser,XML_OPTION_SKIP_WHITE,1);
    xml_parse_into_struct($parser,$data,$values,$tags);
    xml_parser_free($parser);

    // loop through the structures
    foreach ($tags as $key=>$val) {
        if ($key == "shot") {
            $shotranges = $val;
            // each contiguous pair of array entries are the 
            // lower and upper range for each shot definition
            for ($i=0; $i < count($shotranges); $i+=2) {
                    $offset = $shotranges[$i] + 1;
                $len = $shotranges[$i + 1] - $offset;
                $tdb[] = parseShot(array_slice($values, $offset, $len));
            }
        } else {
            continue;
        }
    }
    return $tdb;
  }

  function parseShot($svalues) {
    for ($i=0; $i < count($svalues); $i++)
        $shot[$svalues[$i]["tag"]] = $svalues[$i]["value"];
    return new Screenshot($shot);
  }

  $db = readDatabase($file);
  echo("<table summary=\"SimpleMail Screenshots\">");

  $ver = "init";
  $col = 0;

  foreach ($db as $k=>$v) {
    if ($ver != $v->version || $col >= 2)
    {
      if ($ver != "init") {
        echo("</tr>");
      }
      if ($ver != $v->version)
      {
        echo("<tr><th colspan=2>Version " . $v->version."</th></tr>");
      } else
      {
        echo("<tr>");
      }
      $col = 0;
    }

    echo("<td valign=\"top\" align=\"center\">");
    $ver = $v->version;
    $preview_name = str_replace(".png","_preview.png", $v->filename);
    $preview_size = GetImageSize ($preview_name);
    $filesize = filesize($v->filename);
    printf("<A HREF=\"%s\"><IMG SRC=\"%s\" %s ALT=\"(%ld bytes)\"></A>",$v->filename,$preview_name,$preview_size[3],$filesize);
    echo("<br><font size=\"-1\">". $v->description."</font></td>");
    $col++;
  }

  echo("</tr></table>");
?>


