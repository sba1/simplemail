<?php

   function counterWrite($file, $increase, $lockfile = null) {
       // ceate a lock file - if not possible someone else is active so wait (a while)
       $lock = ($lockfile)? $lockfile : $file.'.lock';
       if (file_exists($lock))
	   {
   		  if (time()-filemtime($lock)>10)
          unlink($lock);
       } 

       $lf = @fopen ($lock, 'x');
       while ($lf === FALSE && $i++ < 20) {
           clearstatcache();
           usleep(rand(5,85));
           $lf = @fopen ($lock, 'x');
       }
  
       $hits = 1;

       // if lockfile (finally) has been created, file is ours, else give up ...
       if ($lf !== False)
       {
           $fp = @fopen($file, 'r');
           if ($fp == null)
	         $fp = @fopen($file.".tmp", 'r');
           $hits = intval(fgets($fp));
		   fclose ($fp);

		   if ($increase)
		   {
			   $hits++;
			   $fp = fopen($file.".tmp", 'w');
    	       fwrite($fp, $hits);
        	   fclose($fp);
        	   unlink($file);
        	   rename($file.".tmp",$file);
		   }

           // and unlock
           fclose($lf);
           unlink($lock);
       }
       return $hits;
   }

	if (!isset($_COOKIE["visited"]))
	{
        setcookie("visited","true");
        $visited = 0;
    } else $visited = 1;

	$hits = counterWrite("hits/hits.txt", $visited == 0);
?>
