#!/usr/bin/perl

print "HTTP/1.1 200 OK\r\n";
print "Content-Type: text/html; charset=utf-8\r\n";
my $string = "<html>\r\n<head>\r\n<title>你好</title>\r\n<body><h1><center>嘿嘿</center></h1></body>\r\n</html>\r\n";
my $length = length($string);
print "Content-Length: $length\r\n\r\n";
print $string;
