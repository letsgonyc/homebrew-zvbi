<?php
require_once("php_common.inc");

switch ($theme)
{
 case "carsten":
  include($this_file . "_" . $theme . ".php");
  break;
 case "modern":
   redirect("links.php#resources");
   break;
 default:
   redirect("index.php#" . $this_file);
   break;
}

?>
