<?php
require_once("php_common.inc");

switch ($theme)
{
 case "carsten":
 case "modern":
  include($this_file . "_" . $theme . ".php");
  break;
 default:
   redirect("index.php#" . $this_file);
   break;
}

?>
