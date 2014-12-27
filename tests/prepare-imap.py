#!/usr/bin/python
#
# Simple script to prepare the imap4 folder
# structure

import imaplib;

M = imaplib.IMAP4('localhost','10143')
M.login('test','test')
M.create('Archive')
M.create('Archive.2014')
M.create('Archive.2013')
M.create('Archive.2012')
M.create('Archive.2012.Important')
M.create('Archive.2012.LessImportant')
