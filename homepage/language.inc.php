<?php

/* Function returns correct filename (localized) */
function get_filename($fn)
{
  $HTTP_ACCEPT_LANGUAGE=getenv("HTTP_ACCEPT_LANGUAGE");
  if (isset($HTTP_ACCEPT_LANGUAGE)) $accepted_langs = explode(",",$HTTP_ACCEPT_LANGUAGE);
  else $accepted_langs = array("en");

  foreach($accepted_langs as $lang)
  {
    $lang = trim($lang);
    $pos = strpos($lang,";");
    if ($pos !== FALSE && $pos > 0)
    	$lang = substr($lang,0,$pos);

    $new_fn = $fn . "." . $lang;
    if (file_exists($new_fn)) return $new_fn;
  }
  return $fn;
}

/* Function returns the correct translated string of a string array */
function get_string($text_array)
{
  $HTTP_ACCEPT_LANGUAGE=getenv("HTTP_ACCEPT_LANGUAGE");
  if (isset($HTTP_ACCEPT_LANGUAGE)) $accepted_langs = explode(",",$HTTP_ACCEPT_LANGUAGE);
  else $accepted_langs = array("en");

  $supported_langs = array_keys($text_array);

  $used_lang = "en";
  foreach($accepted_langs as $lang)
  {
    $lang = trim($lang);
    $pos = strpos($lang,";");
    if ($pos !== FALSE && $pos > 0)
    	$lang = substr($lang,0,$pos);

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
$GalleryText["de"]="Galerie";
$DownloadsIntroText["en"]="The following downloads are currently available";
$DownloadsIntroText["de"]="Folgende Dateien sind momentan verfügbar";
$BannersText["en"]="Here are some banners you might want to put on your homepage";
$BannersText["de"]="Hier sind noch einige Banner, die auf der eigenen Homepage gezeigt werden können";
$BannersCreatorText["en"]="They are made by Richard \"zeg\" Wagenführer, thank you very much for it!";
$BannersCreatorText["de"]="Sie wurden von Richard \"zeg\" Wagenführer erstellt.";
$BannersCreator2Text["en"]="The following banner was created by Richard Kapp, also thanks to him!";
$BannersCreator2Text["de"]="Das folgende Bannner wurde von Richard Kapp erstellt:";
$GalleryUsersText["en"]="There are %d users in this database";
$GalleryUsersText["de"]="Es befinden sich %d Benutzer in der Datenbank.";
$GalleryNoUsersText["en"]="No users are in the database.";
$GalleryNoUsersText["de"]="Es ist niemand in der Datenbank eingetragen.";
$GalleryIntroText["en"]="Please fill in the formular to be added into the database.";
$GalleryIntroText["de"]="Um in die Datenbank eingetragen zu werden, füllen Sie bitte das Formular aus.";
$GalleryNameText["en"]="Your Name:";
$GalleryNameText["de"]="Ihr Name:";
$GalleryEMailText["en"]="Your E-Mail Address:";
$GalleryEMailText["de"]="Ihre E-Mail Adresse:";
$GalleryPhotoText["en"]="Your Photo:";
$GalleryPhotoText["de"]="Ihr Foto:";
$GalleryAlreadyText["en"]="Your photo is already inside SimpleMail's gallery";
$GalleryAlreadyText["de"]="Ihr Foto ist bereits in SimpleMail's Galery";
$GalleryMailSent["en"]="The mail with the picture has been successfully sent. It's get added as soon as possible.";
$GalleryMailSent["de"]="Die e-Mail mit dem Foto wurde erfolgreich versandt. Es wird sobald wie möglich in die Datenbank aufgenommen.";
$LinksIntroText["en"]="Some useful links concerning SimpleMail";
$LinksIntroText["de"]="Einige nützliche Links für SimpleMail";
$LinksMCCText["en"]="Some needed MUI Custom Classes (MCC)";
$LinksMCCText["de"]="MUI Custom Classes (MCC), die von SimpleMail benötigt werden";
$LinksOptionalMCCText["en"]="Some optional used MUI Custom Classes (MCC)";
$LinksOptionalMCCText["de"]="MUI Custom Classes (MCC), die von SimpleMail verwendet werden, falls sie installiert sind.";
?>











