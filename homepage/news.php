<?php
  $file='xml/news.xml';
  $maxnews=5;
  $ammount=0;
  $tableended=true;

  function startElement($parser, $name, $attrs)
  {
    global $ammount;
    global $maxnews;

    if($ammount > $maxnews)
    {
      if($tableended == false)
      {
        echo('</table>');
        $tableended=true;
      }
      return;
    }

    switch($name)
    {
      case 'NEWS':
        include('newsheader.html');
        $tableended = false;
        break;

      case 'ENTRY':
        echo('<tr bgcolor="#CACACA">');
        $ammount++;
        break;

      case 'DATE':
        echo('<td valign="top">');
        break;

      case 'TEXT':
        echo('<td>');
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
    global $ammount;
    global $maxnews;

    if($ammount > $maxnews)
    {
      return;
    }

    switch($name)
    {
      case 'NEWS':
        echo('</table>');
        break;

      case 'ENTRY':
        echo('</tr>');
        break;

      case 'DATE':
        echo('</td>');
        break;

      case 'TEXT':
        echo('</td>');
        break;

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

?>
