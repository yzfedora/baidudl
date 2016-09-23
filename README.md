# baidudl
This is a multi-thread download tool for linux, mainly used to download large
file from pan.baidu.com, since pan.baidu.com has changed their server to use
https instead of http, and also they are using https 302 redirect, this caused
our program can't resolve the correct download url. and I just fix it today :-)

百度网盘多线程下载工具, 支持协议http, https, ftp, 支持平台Linux, Unix-Like,
OSX, Windows, 支持断点续传.

我还是在想百度有没有把批量下载的BUG给修复了没? 因为批量下载的时候会把多个文件
或目录用ZIP格式压缩, 但是他们的服务器把内部的json格式的错误发送了过来(在数据
传输的过程中发送过来的), 难道不是应该他们内部服务器出错了的时候把tcp连接关闭?
然后客户端再重试下载? 这样才能保证接受到的数据一定是正确的. 你发个json格式的
错误过来算什么回事? 还是在数据流中 - -!


![image](https://github.com/yzfedora/baidudl/raw/master/demo.png)

# next steps
1. add CRC32 or MD5 checksum supports, for integrality check.
2. compile rpm, deb package for different Linux distributions if need.
3. I think maybe it's good to compile a OSX and Windows version, but I don't
   have MacBook, so it's hard. for Windows version, maybe I will try use
   MinGW to accomplish it.

# recently updates
1. use libcurl to supports http, https, and ftp multithreading download.
2. use valentine background for status bar.
3. use -f option to download from file. -l option is deprecated.

# features
1. multithreading download, number of threads depending on server.
2. continues download, supports restore download from a previous file.
3. http, https, ftp protocol supports. (axel is great, but no https supports)
4. graceful status bar.
5. Linux, Unix-like, OSX supports, maybe Windows also.


# Install
	$ git clone https://github.com/yzfedora/baidudl.git
	$ cd baidudl
	$ ./autogen.sh
	$ make
	$ sudo make install

# Usage
	download from a single url:
	$ bdpandl -n 8 'http://lx.cdn.baidupcs.com/file/...'

	or if you want to speed as fast as possible. try increase the -n
	option, maybe -n 100 or -n 200. 0.0, I think this would fast enough.

	I sugges you using single quotes to surround the url, prevent bash
	parsing extra symbols.

	if you want to download many files, you can store these urls to a file,
	then using -f option:
	$ bdpandl -n 10 -f listfile

# Notice
	The download address is copy from your browser, when you click the
	download button in the page of pan.baidu.com/..., then it pop a new
	link or windows to download. this link just we need.


	Any suggestions or bugs you can report to <yzfedora@gmail.com>
						Thanks in advance!
