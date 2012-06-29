#!/usr/bin/perl

print "HTTP/1.1 200 OK\r\n";
print "Content-Type: text/html; charset=utf-8\r\n";
my $string = "<html>\r\n<head>\r\n<title>测试网页</title>\r\n<body><h1><center>一只小鸟<br></center></h1><center><img src=\"/icons/bird.png\"><br><br>";
$string = $string."接收到如下参数: ".$ENV{QUERY_STRING};
$string = $string."</center></body>\r\n</html>\r\n";
my $length = length($string);
print "Content-Length: $length\r\n\r\n";
print $string;
