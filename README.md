# baidudl
This is a multi-thread download tool for linux, mainly used to download large
file from pan.baidu.com, since pan.baidu.com has changed their server to use
https instead of http, and also they are using https 302 redirect, this caused
our program can't resolve the correct download url. and I just fix it today :-)

百度网盘多线程下载工具, 支持协议http, https, ftp, 支持平台Linux, Unix-Like,
OSX, Windows, 支持断点续传.

最近baidu在更新内部服务器吧,不是到是不是他们知道了打包（批量）下载里面的bug,之前你是可以
用HEAD+Multithreading下载的,但现在对于打包下载的url是不支持HTTP HEAD了，会返回HTTP
405 Not Allowed错误. 也就是说打包下载不支持多线程下载了.要是能用多线程打包下载就好了,
就不用一个一个点下载按钮然后再Copy-Paste了.


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
