<?php
  $file='xml/faq.xml';

  function startElement($parser, $name, $attrs)
  {

    switch($name)
    {
      case 'FAQ':
        echo('<ul>');
        break;

      case 'ENTRY':
        echo('<li>');
        break;

      case 'QUESTION':
        echo('<font color="red"><b>Q</b>:');
        break;

      case 'ANSWER':
        echo('<b>A</b>:');
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
      case 'FAQ':
        echo('</ul>');
        break;

      case 'ENTRY':
        echo('</li>');
        break;

      case 'QUESTION':
        echo('</font><br><br>');
        break;

      case 'ANSWER':
        echo('<br><br></li>');
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

