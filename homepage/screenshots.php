<?php
  $file='xml/screenshots.xml';

  function startElement($parser, $name, $attrs)
  {
    switch($name)
    {
      case 'SCREENSHOTS':
        include('screenshotsheader.html');
        break;

      case 'SHOT':
        echo('<tr bgcolor="#CACACA">');
        break;

      case 'DESCRIPTION':
        echo('<td>');
        break;

      case 'VERSION':
        echo('<td>');
        break;

      case 'SIZE':
        echo('<td>');
        break;

      case 'FILENAME':
        echo('<td><a href="screenshots/');
        break;

      case 'A':
        echo("<a href=$attrs[HREF]>");
        break;

      case 'LIST':
        echo("<ul>");
        break;

      case 'ITEM':
        echo('<li>');
        break;
    }
  }

  function endElement($parser, $name)
  {
    switch($name)
    {
      case 'SCREENSHOTS':
        echo('</table>');
        break;

      case 'SHOT':
        echo('</tr>');
        break;

      case 'DESCRIPTION':
        echo('</td>');
        break;

      case 'VERSION':
        echo('</td>');
        break;

      case 'SIZE':
        echo('bytes </td>');
        break;

      case 'FILENAME':
        echo('">View</a>');

      case 'A':
        echo('</a>');
        break;

      case 'LIST':
        echo('</ul>');
        break;

      case 'ITEM':
        echo('</li>');
    }
  }

  function characterData($parser, $data)
  {
    global $ammount;
    global $maxnews;

    if($ammount > $maxnews)
    {
      return;
    }

    echo($data);
  }


  $xml_parser = xml_parser_create();
  xml_set_character_data_handler($xml_parser, 'characterData');
  xml_set_element_handler($xml_parser, startElement, 'endElement');

  if(!($fp = fopen($file, 'r')))
  {
    die('could not open XML input');
  }

  echo("<center>");

  while ($data = fread($fp, 4096))
  {
    if(!xml_parse($xml_parser, $data, feof($fp)))
    {
      die(sprintf('XML error: %s at line %d',
        xml_error_string(xml_get_error_code($xml_parser)),
        xml_get_current_line_number($xml_parser)));
    }
  }

  echo("</center>");

  xml_parser_free($xml_parser);

?>

