<?php
  require_once("language.inc.php");

  $file='xml/news.xml';
  $maxnews=5;

  $news = array();  // the array of all the news
  $date = 0;        // the date of the current news
  $text = array();  // the text array of the current news
  $lang = 0;        // current language
  $inside_text = 1;

  function add_text($str)
  {
    global $text;
    global $lang;
    global $inside_text;

    if ($inside_text) $text[$lang] .= $str;
  }

  function startElement($parser, $name, $attrs)
  {
    global $date;
    global $text;
    global $lang;
    global $inside_text;

    switch($name)
    {
      case 'ENTRY':
        $date = $attrs[DATE];
        $text = array();
        break;

      case 'TEXT':
        if (isset($attrs[LANG])) $lang = $attrs[LANG];
        else $lang = "en";
        unset($text[$lang]);
        $inside_text = 1;
        break;

      case 'A': 
        add_text("<a href=$attrs[HREF]>");
        break;

      case 'LIST':
        add_text("<ul>");
        break;

      case 'ITEM':
        add_text("<li>");
        break;
    }
  }

  function endElement($parser, $name)
  {
    global $date;
    global $text;
    global $lang;
    global $news;
    global $inside_text;

    switch($name)
    {
      case 'ENTRY':
        $news[] = array(DATE => $date, TEXT => $text);
        break;

      case 'TEXT':
        $inside_text = 0;
        break;

      case 'A':
        add_text("</a>");
        break;

      case 'LIST':
        add_text("</ul>");
        break;

      case 'ITEM':
        add_text("</li>");
        break;
    }
  }

  function characterData($parser, $data)
  {
    global $text;
    global $lang;

    add_text($data);
  }

  $xml_parser = xml_parser_create();
  xml_set_character_data_handler($xml_parser, 'characterData');
  xml_set_element_handler($xml_parser, startElement, 'endElement');

  if(!($fp = fopen($file, 'r')))
  {
    die('could not open XML input');
  }

  while ($data = fread($fp, 4096))
  {
    if(!xml_parse($xml_parser, $data, feof($fp)))
    {
      die(sprintf('XML error: %s at line %d',
        xml_error_string(xml_get_error_code($xml_parser)),
        xml_get_current_line_number($xml_parser)));
    }
  }

  xml_parser_free($xml_parser);

  echo "<table><tr bgcolor=\"black\">";
  echo "<th><font color=\"white\">".get_string($DateText)."</font></th>";
  echo "<th><font color=\"white\">".get_string($NewsText)."</font></th>";
  echo "</tr>";

  $news_count = 0;

  foreach ($news as $val)
  {
    echo "<tr bgcolor=\"#CACACA\"><td valign=\"top\">$val[DATE]</td>";

    echo "<td>".get_string($val[TEXT])."</td>";

    echo "</tr>";

    $news_count++;
    if ($news_count == $maxnews) break;
  }  

  echo "</table>"; 


?>


