<?php require("counter.php"); ?>
<html>
  <head>
    <title>SimpleMail</title>
  </head>

  <body bgcolor="white">

    <center><A href="index.php"><IMG border="0" src="pictures/sm_logo.png" alt="sm_logo.png (1171 bytes)" width=274 height=45></A></center>

    <table width="100%" summary="Main page.">
      <tr bgcolor="black">
        <th>
          <font color="white">
            <tt>
              Menu
            </tt>
          </font>
        </th>
        <th>
          <font color="white">
            <tt>
              <?php
                switch($body)
                {

                  case 'screenshots':
                    echo('Screenshots');
                    break;

                  case 'downloads':
                    echo('Downloads');
                    break;

                  case 'faq':
                    echo('FAQ');
                    break;

                  case 'links':
                    echo('Links');
                    break;

                  case 'contact':
                    echo('Contact');
                    break;

                  case 'gallery':
                    echo('Gallery');
                    break;

                  default:
                    echo('Welcome');
                    break;
                }
              ?>
            </tt>
          </font>
        </th>
      </tr>

      <tr>
        <td valign="top">
          <?php require('menu.php'); ?>
        </td>

        <td>
          <?php
            switch($body)
            {

              case 'screenshots':
                require('screenshots.php');
                break;

              case 'downloads':
                require('downloads.php');
                break;

              case 'faq':
                require('faq.php');
                break;

              case 'links':
                require('links.php');
                break;

              case 'contact':
                require('contact.php');
                break;

              case 'gallery':
                require('gallery.php');
                break;

              default:
                require('main.php');
                break;
            }

          ?>
        </td>
      </tr>
    </table>

    <table align="center" width="100%" summary="Footer containing infos and banners.">
      <tr bgcolor="black">
        <td>
          <center>
          <font color="white"
            <tt>
                <?php

                  switch(body)
                  {

                    case 'screenshots':
                    $file = 'xml/screenshots.xml';
                      break;

                    case 'downloads':
                      $file = 'xml/downloads.xml';
                      break;

                    case 'faq':
                      $file = 'xml/faq.xml';
                      break;

                    case 'links':
                      $file = 'xml/links.xml';
                      break;

                    case 'contact':
                      $file = 'contact.php';
                      break;

                    default:
                      $file = 'xml/news.xml';
                      break;
                  }

                  $unixTime = filemtime($file);

                  echo("Last modification: ".date("Y/m/d", $unixTime));
                  echo(' · ');
                  echo("Visitors: $hits"); ?>
              </tt>
            </font>
          </center>
        </td>
      </tr>
    </table>

    <table>
      <tr>
        <td>

        </td>
      </tr>
    </table>

  </body>
</html>
