# baidudl
This is a multi-thread download tool for linux, mainly used to download large
file from pan.baidu.com, since pan.baidu.com has changed their server to use
https instead of http, and also they are using https 302 redirect, this caused
our program can't resolve the correct download url. and I just fix it today :-)

百度网盘多线程下载工具, 支持协议http, https, ftp, 支持平台Linux, Unix-Like,
OSX, Windows, 支持断点续传.

Notice:
    pan.baidu.com has no longer supports the HTTP HEAD request for batch
    download url, that's meaning we can't use multithreadding for this
    kind of url, I guess this is because the batch download has some bugs
    in their servers, because I have found some bug indeed. so they return
    a HTTP 405 Not Allowed error to prevent some tools use multithreadding
    to download it, I think this is reasonable, but the best way is to fix
    this bug.
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
