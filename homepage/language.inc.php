<?php

/* Function returns the correct translated string of a string array */
function get_string($text_array)
{
  global $HTTP_ACCEPT_LANGUAGE;
  if (isset($HTTP_ACCEPT_LANGUAGE)) $accepted_langs = explode(" ",$HTTP_ACCEPT_LANGUAGE);
  else $accepted_langs = array("en");

  $supported_langs = array_keys($text_array);
  $used_lang = "en";
  foreach($accepted_langs as $lang)
  {
    if (in_array($lang,$supported_langs))
    {
      $used_lang = $lang;
      break;
    }
  }
  return $text_array[$used_lang];
}

$DateText["de"]="Datum";
$DateText["en"]="Date";
$NewsText["de"]="News";
$NewsText["en"]="News";
$MenuText["de"]="Menü";
$MenuText["en"]="Menu";
$LastModificationText["de"]="Letzte Änderung:";
$LastModificationText["en"]="Last modification:";
$VisitorsText["de"]="Besucher:";
$VisitorsText["en"]="Visitors:";
$WelcomeText["de"]="Willkommen";
$WelcomeText["en"]="Welcome";
$ScreenshotsText["en"]="Screenshots";
$DownloadsText["en"]="Downloads";
$FAQText["en"]="FAQ";
$LinksText["en"]="Links";
$ContactText["en"]="Contact";
$ContactText["de"]="Kontakt";
$GalleryText["en"]="Gallery";
$GalleryText["de"]="Galery";

?>








