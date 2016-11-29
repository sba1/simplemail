[![Build Status](https://travis-ci.org/sba1/simplemail.svg?branch=master)](https://travis-ci.org/sba1/simplemail)
[![PayPal Donate](https://img.shields.io/badge/paypal-donate-yellow.svg?style=flat)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=SBN2L9L3NYLMU)

SimpleMail
==========

SimpleMail is a simple email client targeting AmigaOS (both the classic OS 3.1 and OS 4.x)
and compatible platforms such as AROS and MorphOS.

Download
--------

You can download binaries from http://simplemail.sourceforge.net/index.php?body=downloads.
MorphOS users may also check SimpleMail-MOS at https://code.google.com/p/simplemail-mos/.
This was a fork of SimpleMail focusing on MorphOS, which, however, is discontinued.

Continuous Integration
----------------------

For latest builds for AmigaOS 4.x please check https://sonumina.de/jenkins/job/simplemail/.
This site's SHA1 fingerprint is ```5C 31 5A A3 AD C9 2A 79 CD 81 EC 0C 38 BA 4D 9A
09 19 E5 13```


Building for AmigaOS 4.x
------------------------

The build process for AmigaOS 4.x uses gcc. For building the AmigaOS 4.x variant, use:

```
 $ make -f makefile.aos4
```

If you want the variant with statically linked OpenSSL use

```
 $ make -f makefile.aos4 USE_OPENSSL=1
```


Building for AmigaOS 3.x
------------------------

The build process for AmigaOS 3.x currently uses SAS C. Building is as easy as typing

```
 1.> smake
```

in the shell.
