<?php

/* Function returns correct filename (localized) */
function get_filename($fn)
{
  global $HTTP_ACCEPT_LANGUAGE;
  if (isset($HTTP_ACCEPT_LANGUAGE)) $accepted_langs = explode(" ",$HTTP_ACCEPT_LANGUAGE);
  else $accepted_langs = array("en");

  foreach($accepted_langs as $lang)
  {
    $new_fn = $fn . ".".$lang;
    if (file_exists($new_fn)) return $new_fn;
  }
  return $fn;
}

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
$MenuText["de"]="Men�";
$MenuText["en"]="Menu";
$LastModificationText["de"]="Letzte �nderung:";
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
$DownloadsIntroText["en"]="The following downloads are currently available";
$DownloadsIntroText["de"]="Follgende Dateien sind momentan verf�gbar";
$BannersText["en"]="Here are some banners you might want to put on your homepage";
$BannersText["de"]="Hier sind noch einige Banner, die auf der eigenen Homepage gezeigt werden k�nnen";
$BannersCreatorText["en"]="They are made by Richard \"zeg\" Wagenf�hrer, thank you very much for it!";
$BannersCreatorText["de"]="Sie wurden von Richard \"zeg\" Wagenf�hrer erstellt.";

?>










