#include "handler.h"
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <dirent.h>
#include <strings.h>
#include <time.h>

#define READ_TIMEOUT 60
#define CGI_PATTERN "/cgi/"

CHandler *CHandler::mHandler = NULL;

CHandler::CHandler(int vSocketFD)
{
	mSocketFD = vSocketFD;
	SetNonBlock();
}

CHandler::~CHandler()
{
	Close();
}

void CHandler::Handler(int vSocketFD)
{
	// 创建子进程
	pid_t pid = fork();
	if( pid < 0 )
	{
		printf("fork failed: %s\n", strerror(errno));
		return;
	}

	// 父进程继续监听，马上返回
	if( pid > 0 )
	{
		close(vSocketFD);
		return;
	}

	mHandler = new CHandler(vSocketFD);
	
	// 接收消息
	if( mHandler->RecvMsg() < 0 )
	{
		mHandler->SendError(400, "Bad Request", "Can't parse request.");
		delete mHandler;
		return;
	}

	// 解析消息头
	if( mHandler->mHttpHeader.Parse(mHandler->mRequest) < 0 )
	{
		mHandler->SendError(400, "Bad Request", "Can't parse request.");
		delete mHandler;
		return;
	}

	// 获取方法
	if( mHandler->mHttpHeader.ParseMethod() == METHOD_NONE )
	{
		mHandler->SendError(501, "Not Implemented", "That method is not implemented.");
		delete mHandler;
		return;
	}

	// 解析路径
	char *pPath = mHandler->mHttpHeader.GetPath();
	if( *pPath != '/' )
	{
		mHandler->SendError(400, "Bad Request", "Bad filename.");
		delete mHandler;
		return;
	}

	// 组织路径
	char buffer[10000] = {0};
	getcwd(buffer, sizeof(buffer));
	memcpy(buffer + strlen(buffer), pPath, strlen(pPath));
	
	struct stat sb;
	int ret = stat(buffer, &sb);
	if( ret < 0 && errno == EACCES )
	{
		mHandler->SendError(403, "Forbidden", "Directory or file is protected");
		delete mHandler;
		return;
	}
	else if( ret < 0 )
	{
		mHandler->SendError(404, "Not Found", "Directory or file not found");
		delete mHandler;
		return;
	}

	// 如果是目录
	if( S_ISDIR(sb.st_mode) )
	{
		const char *indexs[] = {
			"index.html",
			"index.htm",
			"index.xhtml"
		};

		// 有没有可能是索引页
		for( int i = 0 ; i < sizeof(indexs) / sizeof(char *) ; i++ )
		{
			char indexbuffer[10000];
			snprintf(indexbuffer, sizeof(indexbuffer), "%s%s", buffer, indexs[i]);
			struct stat s;
			if( stat(indexbuffer, &s) >= 0 )
			{
				mHandler->DoFile(indexbuffer);
				delete mHandler;
				return;
			}
		}

		// 如果真是目录
		mHandler->DoDir(buffer);
	}
	// 如果是文件
	else if( S_ISREG(sb.st_mode) )
	{
		if( strncmp(mHandler->mHttpHeader.GetPath(), CGI_PATTERN, strlen(CGI_PATTERN)) == 0 ) 
		{
			mHandler->DoCGI(buffer);
		}
		else
		{
			mHandler->DoFile(buffer);
		}
	}

	delete mHandler;
	return;
}

void CHandler::DoCGI(char *vFileName)
{
	int fd[2];
	if( pipe(fd) < 0 )
	{
		SendError(500, "Internal Error", "Something unexpected went wrong making a pipe.");
		return;
	}

	// 将http数据放入管道
	char *data = mHttpHeader.GetHttpData();
	int datalen = strlen(data);
	int contentlen = mHttpHeader.GetContentLength();
	if( contentlen == 0 )
	{
		// do nothing
	}
	else if( datalen < contentlen )
	{
		char buffer[10000];			
		if( RecvMsg(buffer, contentlen - datalen) < 0 )
		{
			SendError(500, "Internal Error", "Something unexpected went wrong receiving http data.");
			return;
		}

		write(fd[1], data, datalen);
		write(fd[1], buffer, contentlen - datalen);
	}
	else
	{
		write(fd[1], data, contentlen);
	}

	// 创建子进程
	int ret = fork();
	if( ret < 0 )
	{
		SendError(500, "Internal Error", "Something unexpected went wrong forking an interposer.");
		return;
	}
	else if( ret > 0 )
	{
		close(fd[0]);
		close(fd[1]);

		wait(NULL);
		exit(0);
	}
	
	// 子进程
	close(fd[1]);

	// 设置环境变量
	char buffer[100] = {0};			
	snprintf(buffer, sizeof(buffer), "CONTENT_LENGTH=%d", mHttpHeader.GetContentLength());
	putenv(buffer);
	setenv("QUERY_STRING", mHttpHeader.GetQuery(), 1);

	// 重定向文件描述符
	if( dup2(mSocketFD, STDOUT_FILENO) < 0 || dup2(fd[0], STDIN_FILENO) < 0 )
	{
		SendError(500, "Internal Error", "Something unexpected went wrong dup file descriptor.");
		exit(0);
	};

	char *file = rindex(vFileName, '/');
	file++;

	// 执行文件
	if( execlp(vFileName, file, (char *)0) < 0 )
	{
		SendError(500, "Internal Error", "Something unexpected went wrong execute file.");
		exit(0);
	}

	exit(0);
}

void CHandler::DoFile(char *vFileName)
{
	int fd = open(vFileName, O_RDONLY);
	if( fd < 0 )
	{
		SendError(403, "Forbidden", "File is protected");
		return;
	}

	struct stat s;
	stat(vFileName, &s);

	char buffer[256];
	CRequest request;
	int len = snprintf(buffer, sizeof(buffer), "HTTP/1.1 200 OK\r\n");
	request.AddRequest(buffer, len);
	len = snprintf(buffer, sizeof(buffer), "Server: lest\r\n");
	request.AddRequest(buffer, len);
	time_t now = time(NULL);
	char timebuffer[128];
	strftime(timebuffer, sizeof(timebuffer), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
	len = snprintf(buffer, sizeof(buffer), "Date: %s\r\n", timebuffer);
	request.AddRequest(buffer, len);
	char *type = ChooseType(vFileName);
	len = snprintf(buffer, sizeof(buffer), "Content-Type: %s\r\n", type);
	free(type);
	request.AddRequest(buffer, len);
	len = snprintf(buffer, sizeof(buffer), "Content-Length: %d\r\n", s.st_size);
	request.AddRequest(buffer, len);
	strftime(timebuffer, sizeof(timebuffer), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&s.st_mtime));
	len = snprintf(buffer, sizeof(buffer), "Last-Modified: %s\r\n", timebuffer);
	request.AddRequest(buffer, len);
	len = snprintf(buffer, sizeof(buffer), "Connection: keep-alive\r\nAccept-Ranges: none\r\n\r\n");
	request.AddRequest(buffer, len);
	SendMsg(request);

	char filebuffer[10000];
	while( (len = read(fd, filebuffer, sizeof(filebuffer))) > 0 )
	{
		SendMsg(filebuffer, len);
	}
	exit(0);
}

void CHandler::DoDir(char *vDirName)
{
	SendError(501, "Not Implemented", "Working with directory is not implemented.");
	exit(0);
}

void CHandler::SetNonBlock()
{
	if( mSocketFD < 0 )
	{
		return;
	}

	int flags = fcntl(mSocketFD, F_GETFL, 0);
	if( flags < 0 )
	{
		return;
	}
	fcntl(mSocketFD, F_SETFL, flags|O_NONBLOCK);
}

void CHandler::ReadTimeout(int vSigNum)
{
	if( mHandler != NULL )
	{
		mHandler->SendError(408, "Request Timeout", "No request appeared within a reasonable time period.");
	}
}

void CHandler::SendError(int vErrorNum, const char *vTitle, const char *vText)
{
	CRequest request;
	char buffer[256];	
	int len = snprintf(buffer, sizeof(buffer), "HTTP/1.1 %d %s\r\n", vErrorNum, vTitle);
	request.AddRequest(buffer, len);
	len = snprintf(buffer, sizeof(buffer), "Server: lest\r\n");
	request.AddRequest(buffer, len);
	time_t now = time(NULL);
	char timebuffer[128];
	strftime(timebuffer, sizeof(timebuffer), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
	len = snprintf(buffer, sizeof(buffer), "Date: %s\r\n", timebuffer);
	request.AddRequest(buffer, len);
	len = snprintf(buffer, sizeof(buffer), "Content-Type: text/html; charset=utf-8\r\n");
	request.AddRequest(buffer, len);
	char contentbuffer[10000];
	int contentlen = snprintf(contentbuffer, sizeof(contentbuffer), "<html>\r\n<head><title>%d %s</title></head>\r\n<body bgcolor=\"white\">\r\n<center><h1>%d %s</h1></center>\r\n<hr><center>%s</center>\r\n</body>\r\n</html>\r\n", vErrorNum, vTitle, vErrorNum, vTitle, vText);
	len = snprintf(buffer, sizeof(buffer), "Content-Length: %d\r\n", contentlen);
	request.AddRequest(buffer, len);
	len = snprintf(buffer, sizeof(buffer), "Connection: close\r\n\r\n");
	request.AddRequest(buffer, len);
	request.AddRequest(contentbuffer, contentlen);
	SendMsg(request);
	Close();
	exit(0);
}

void CHandler::SendMsg(char *vBuffPtr, int vLeftLen)
{
	if( mSocketFD < 0 )
	{
		return;
	}

	while(vLeftLen != 0)
	{
		int len = send(mSocketFD, vBuffPtr, vLeftLen, 0);
		if( len < 0 && errno == EINTR )
		{
			continue;
		}
		else if( len < 0 )
		{
			printf("send error: %s\n", strerror(errno));
			return;
		}
		else
		{
			vBuffPtr += len;
			vLeftLen -= len;
		}
	}
}

void CHandler::SendMsg(CRequest &vRequest)
{
	SendMsg(vRequest.GetData(), vRequest.GetLength());
}

int CHandler::RecvMsg(char *vBuffer, int vLen)
{
	if( mSocketFD < 0 )
	{
		return -1;
	}

	while(vLen > 0)
	{
		int len = recv(mSocketFD, vBuffer, vLen, 0);
		if( len < 0 && ( errno == EINTR || errno == EAGAIN ) )
		{
			continue;
		}
		else if( len < 0 )
		{
			return -2;
		}
		else
		{
			vLen -= len;
			vBuffer += len;
		}
	}
	return 0;
}

int CHandler::RecvMsg()
{
	if( mSocketFD < 0 )
	{
		return -1;
	}

	signal(SIGALRM, ReadTimeout);
	alarm(READ_TIMEOUT);

	while(1)
	{
		fd_set fds;
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 10000;

		FD_ZERO(&fds);
		FD_SET(mSocketFD, &fds);
		int ret = select(mSocketFD + 1, &fds, NULL, NULL, &tv);
		if( ret == 0 )
		{
			usleep(1);
			continue;
		}
		else if( ret < 0 )
		{
			continue;
		}
		
		if( FD_ISSET(mSocketFD, &fds) )
		{
			char buffer[10240];
			while(1)
			{
				int ret = recv(mSocketFD, &buffer, sizeof(buffer), 0);
				if( ret < 0 && (errno == EINTR || errno == EAGAIN) )
				{
					continue;
				}
				else if( ret <= 0 )
				{
					goto NOTFOUND;
				}

				mRequest.AddRequest((char *)&buffer, ret);
				if( strstr(mRequest.GetData(), "\r\n\r\n") != NULL )
				{
					goto GOTIT;
				}
			}
		}

NOTFOUND:
		return -2;

GOTIT:
		return 0;
	}
}

void CHandler::Close()
{
	if( mSocketFD > 0 )
	{
		close(mSocketFD);
		mSocketFD = -1;
	}
}

#define TYPECHOOSER(a, b) \
	if( strcasecmp(p, a) == 0 ) \
	{ \
		snprintf(type, 64, b); \
		return type; \
	}

char *CHandler::ChooseType(char *vFileName)
{
	char *type = (char *)malloc(64);
	char *p = rindex(vFileName, '/');
	p = rindex(p, '.');
	p++;

	TYPECHOOSER("a", "application/octet-stream")
	TYPECHOOSER("aab", "application/x-authorware-bin")
	TYPECHOOSER("aam", "application/x-authorware-map")
	TYPECHOOSER("aas", "application/x-authorware-seg")
	TYPECHOOSER("ai", "application/postscript")
	TYPECHOOSER("aif", "audio/x-aiff")
	TYPECHOOSER("aifc", "audio/x-aiff")
	TYPECHOOSER("aiff", "audio/x-aiff")
	TYPECHOOSER("asc", "text/plain")
	TYPECHOOSER("asf", "video/x-ms-asf")
	TYPECHOOSER("asx", "video/x-ms-asf")
	TYPECHOOSER("au", "audio/basic")
	TYPECHOOSER("avi", "video/x-msvideo")
	TYPECHOOSER("bcpio", "application/x-bcpio")
	TYPECHOOSER("bin", "application/octet-stream")
	TYPECHOOSER("bmp", "image/bmp")
	TYPECHOOSER("cdf", "application/x-netcdf")
	TYPECHOOSER("class", "application/x-java-vm")
	TYPECHOOSER("cpio", "application/x-cpio")
	TYPECHOOSER("cpt", "application/mac-compactpro")
	TYPECHOOSER("crl", "application/x-pkcs7-crl")
	TYPECHOOSER("crt", "application/x-x509-ca-cert")
	TYPECHOOSER("csh", "application/x-csh")
	TYPECHOOSER("css", "text/css")
	TYPECHOOSER("dcr", "application/x-director")
	TYPECHOOSER("dir", "application/x-director")
	TYPECHOOSER("djv", "image/vnd.djvu")
	TYPECHOOSER("djvu", "image/vnd.djvu")
	TYPECHOOSER("dll", "application/octet-stream")
	TYPECHOOSER("dms", "application/octet-stream")
	TYPECHOOSER("doc", "application/msword")
	TYPECHOOSER("dtd", "text/xml")
	TYPECHOOSER("dump", "application/octet-stream")
	TYPECHOOSER("dvi", "application/x-dvi")
	TYPECHOOSER("dxr", "application/x-director")
	TYPECHOOSER("eps", "application/postscript")
	TYPECHOOSER("etx", "text/x-setext")
	TYPECHOOSER("exe", "application/octet-stream")
	TYPECHOOSER("ez", "application/andrew-inset")
	TYPECHOOSER("fgd", "application/x-director")
	TYPECHOOSER("fh", "image/x-freehand")
	TYPECHOOSER("fh4", "image/x-freehand")
	TYPECHOOSER("fh5", "image/x-freehand")
	TYPECHOOSER("fh7", "image/x-freehand")
	TYPECHOOSER("fhc", "image/x-freehand")
	TYPECHOOSER("gif", "image/gif")
	TYPECHOOSER("gtar", "application/x-gtar")
	TYPECHOOSER("hdf", "application/x-hdf")
	TYPECHOOSER("hqx", "application/mac-binhex40")
	TYPECHOOSER("htm", "text/html; charset=utf-8")
	TYPECHOOSER("html", "text/html; charset=utf-8")
	TYPECHOOSER("ice", "x-conference/x-cooltalk")
	TYPECHOOSER("ief", "image/ief")
	TYPECHOOSER("iges", "model/iges")
	TYPECHOOSER("igs", "model/iges")
	TYPECHOOSER("iv", "application/x-inventor")
	TYPECHOOSER("jar", "application/x-java-archive")
	TYPECHOOSER("jfif", "image/jpeg")
	TYPECHOOSER("jpe", "image/jpeg")
	TYPECHOOSER("jpeg", "image/jpeg")
	TYPECHOOSER("jpg", "image/jpeg")
	TYPECHOOSER("js", "application/x-javascript")
	TYPECHOOSER("kar", "audio/midi")
	TYPECHOOSER("latex", "application/x-latex")
	TYPECHOOSER("lha", "application/octet-stream")
	TYPECHOOSER("lzh", "application/octet-stream")
	TYPECHOOSER("m3u", "audio/x-mpegurl")
	TYPECHOOSER("man", "application/x-troff-man")
	TYPECHOOSER("mathml", "application/mathml+xml")
	TYPECHOOSER("me", "application/x-troff-me")
	TYPECHOOSER("mesh", "model/mesh")
	TYPECHOOSER("mid", "audio/midi")
	TYPECHOOSER("midi", "audio/midi")
	TYPECHOOSER("mif", "application/vnd.mif")
	TYPECHOOSER("mime", "message/rfc822")
	TYPECHOOSER("mml", "application/mathml+xml")
	TYPECHOOSER("mov", "video/quicktime")
	TYPECHOOSER("movie", "video/x-sgi-movie")
	TYPECHOOSER("mp2", "audio/mpeg")
	TYPECHOOSER("mp3", "audio/mpeg")
	TYPECHOOSER("mp4", "video/mp4")
	TYPECHOOSER("mpe", "video/mpeg")
	TYPECHOOSER("mpeg", "video/mpeg")
	TYPECHOOSER("mpg", "video/mpeg")
	TYPECHOOSER("mpga", "audio/mpeg")
	TYPECHOOSER("ms", "application/x-troff-ms")
	TYPECHOOSER("msh", "model/mesh")
	TYPECHOOSER("mv", "video/x-sgi-movie")
	TYPECHOOSER("mxu", "video/vnd.mpegurl")
	TYPECHOOSER("nc", "application/x-netcdf")
	TYPECHOOSER("o", "application/octet-stream")
	TYPECHOOSER("oda", "application/oda")
	TYPECHOOSER("ogg", "application/x-ogg")
	TYPECHOOSER("pac", "application/x-ns-proxy-autoconfig")
	TYPECHOOSER("pbm", "image/x-portable-bitmap")
	TYPECHOOSER("pdb", "chemical/x-pdb")
	TYPECHOOSER("pdf", "application/pdf")
	TYPECHOOSER("pgm", "image/x-portable-graymap")
	TYPECHOOSER("pgn", "application/x-chess-pgn")
	TYPECHOOSER("png", "image/png")
	TYPECHOOSER("pnm", "image/x-portable-anymap")
	TYPECHOOSER("ppm", "image/x-portable-pixmap")
	TYPECHOOSER("ppt", "application/vnd.ms-powerpoint")
	TYPECHOOSER("ps", "application/postscript")
	TYPECHOOSER("qt", "video/quicktime")
	TYPECHOOSER("ra", "audio/x-realaudio")
	TYPECHOOSER("ram", "audio/x-pn-realaudio")
	TYPECHOOSER("ras", "image/x-cmu-raster")
	TYPECHOOSER("rdf", "application/rdf+xml")
	TYPECHOOSER("rgb", "image/x-rgb")
	TYPECHOOSER("rm", "audio/x-pn-realaudio")
	TYPECHOOSER("roff", "application/x-troff")
	TYPECHOOSER("rpm", "audio/x-pn-realaudio-plugin")
	TYPECHOOSER("rss", "application/rss+xml")
	TYPECHOOSER("rtf", "text/rtf")
	TYPECHOOSER("rtx", "text/richtext")
	TYPECHOOSER("sgm", "text/sgml")
	TYPECHOOSER("sgml", "text/sgml")
	TYPECHOOSER("sh", "application/x-sh")
	TYPECHOOSER("shar", "application/x-shar")
	TYPECHOOSER("silo", "model/mesh")
	TYPECHOOSER("sit", "application/x-stuffit")
	TYPECHOOSER("skd", "application/x-koan")
	TYPECHOOSER("skm", "application/x-koan")
	TYPECHOOSER("skp", "application/x-koan")
	TYPECHOOSER("skt", "application/x-koan")
	TYPECHOOSER("smi", "application/smil")
	TYPECHOOSER("smil", "application/smil")
	TYPECHOOSER("snd", "audio/basic")
	TYPECHOOSER("so", "application/octet-stream")
	TYPECHOOSER("spl", "application/x-futuresplash")
	TYPECHOOSER("src", "application/x-wais-source")
	TYPECHOOSER("stc", "application/vnd.sun.xml.calc.template")
	TYPECHOOSER("std", "application/vnd.sun.xml.draw.template")
	TYPECHOOSER("sti", "application/vnd.sun.xml.impress.template")
	TYPECHOOSER("stw", "application/vnd.sun.xml.writer.template")
	TYPECHOOSER("sv4cpio", "application/x-sv4cpio")
	TYPECHOOSER("sv4crc", "application/x-sv4crc")
	TYPECHOOSER("svg", "image/svg+xml")
	TYPECHOOSER("svgz", "image/svg+xml")
	TYPECHOOSER("swf", "application/x-shockwave-flash")
	TYPECHOOSER("sxc", "application/vnd.sun.xml.calc")
	TYPECHOOSER("sxd", "application/vnd.sun.xml.draw")
	TYPECHOOSER("sxg", "application/vnd.sun.xml.writer.global")
	TYPECHOOSER("sxi", "application/vnd.sun.xml.impress")
	TYPECHOOSER("sxm", "application/vnd.sun.xml.math")
	TYPECHOOSER("sxw", "application/vnd.sun.xml.writer")
	TYPECHOOSER("t", "application/x-troff")
	TYPECHOOSER("tar", "application/x-tar")
	TYPECHOOSER("tcl", "application/x-tcl")
	TYPECHOOSER("tex", "application/x-tex")
	TYPECHOOSER("texi", "application/x-texinfo")
	TYPECHOOSER("texinfo", "application/x-texinfo")
	TYPECHOOSER("tif", "image/tiff")
	TYPECHOOSER("tiff", "image/tiff")
	TYPECHOOSER("tr", "application/x-troff")
	TYPECHOOSER("tsp", "application/dsptype")
	TYPECHOOSER("tsv", "text/tab-separated-values")
	TYPECHOOSER("txt", "text/plain")
	TYPECHOOSER("ustar", "application/x-ustar")
	TYPECHOOSER("vcd", "application/x-cdlink")
	TYPECHOOSER("vrml", "model/vrml")
	TYPECHOOSER("vx", "video/x-rad-screenplay")
	TYPECHOOSER("wav", "audio/x-wav")
	TYPECHOOSER("wax", "audio/x-ms-wax")
	TYPECHOOSER("wbmp", "image/vnd.wap.wbmp")
	TYPECHOOSER("wbxml", "application/vnd.wap.wbxml")
	TYPECHOOSER("wm", "video/x-ms-wm")
	TYPECHOOSER("wma", "audio/x-ms-wma")
	TYPECHOOSER("wmd", "application/x-ms-wmd")
	TYPECHOOSER("wml", "text/vnd.wap.wml")
	TYPECHOOSER("wmlc", "application/vnd.wap.wmlc")
	TYPECHOOSER("wmls", "text/vnd.wap.wmlscript")
	TYPECHOOSER("wmlsc", "application/vnd.wap.wmlscriptc")
	TYPECHOOSER("wmv", "video/x-ms-wmv")
	TYPECHOOSER("wmx", "video/x-ms-wmx")
	TYPECHOOSER("wmz", "application/x-ms-wmz")
	TYPECHOOSER("wrl", "model/vrml")
	TYPECHOOSER("wsrc", "application/x-wais-source")
	TYPECHOOSER("wvx", "video/x-ms-wvx")
	TYPECHOOSER("xbm", "image/x-xbitmap")
	TYPECHOOSER("xht", "application/xhtml+xml; charset=utf-8")
	TYPECHOOSER("xhtml", "application/xhtml+xml; charset=utf-8")
	TYPECHOOSER("xls", "application/vnd.ms-excel")
	TYPECHOOSER("xml", "text/xml")
	TYPECHOOSER("xpm", "image/x-xpixmap")
	TYPECHOOSER("xsl", "text/xml")
	TYPECHOOSER("xwd", "image/x-xwindowdump")
	TYPECHOOSER("xyz", "chemical/x-xyz")
	TYPECHOOSER("zip", "application/zip")

	snprintf(type, 64, "text/plain; charset=utf-8");
	return type;
}
