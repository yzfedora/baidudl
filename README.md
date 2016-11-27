# baidudl
This is a multi-thread download tool for linux, mainly used to download large
file from pan.baidu.com, since pan.baidu.com has changed their server to use
https instead of http, and also they are using https 302 redirect, this caused
our program can't resolve the correct download url. and I just fix it today :-)

百度网盘多线程下载工具, 支持协议http, https, ftp, 支持平台Linux, Unix-Like,
OSX, 支持断点续传.

![image](https://github.com/yzfedora/baidudl/raw/master/demo.png)

# Tips
if you find the download speed become very slow, even smaller than 100
KiB/s, you can type Ctrl-C first to stop it. and restore again. and don't
forget to use "-n" option to specify use how many number of threads to
download, normally, I use set it to 100 or 200 according the situation.
but, please be care, for every threads, the program will use at least 1 MiB
to cache. so default memory usage will be 100 threads x 1 MiB per thread = 100
MiB.

# Bugs
1. there is a bug in pan.baidu.com, especially when try specify a batch
   download url, pan.baidu.com may send broken file data to you. in this
   situation, you will see some files was broken after you unzip. you can
   choose to download broken files only, or just remove the download file,
   and try again.
2. there still exists an "Illegal Hardware Instruction" bug on Mac OSX, I
   have tried to find why it failed, but with no luck.

# Next Steps
1. add CRC32 or MD5 checksum supports, for integrality check.
2. compile rpm, deb package for different Linux distributions if need.
3. try fix "Illegal Hardware Instruction" bug on Mac OSX.

# Recently Updates
1. use libcurl to supports http, https, and ftp multithreading download.
2. use valentine background for status bar.
3. use -f option to download from file. -l option is deprecated.
4. use internal cache mechanism to optimize the IO speed.
5. change the default threads number to 100.

# Features
1. multithreading download, number of threads depending on server.
2. continues download, supports restore download from a previous file.
3. http, https, ftp protocol supports. (axel is great, but no https supports)
4. graceful status bar.
5. Linux, Unix-like, OSX supports, maybe Windows also.


# Compile
	$ git clone https://github.com/yzfedora/baidudl.git
	$ cd baidudl
	$ ./autogen.sh
	$ make
	$ sudo make install

# Install
	if you don't want to compile manually, I have built a rpm package for
	you. please go to subdirectory "packages/rpms", then you will find it.

	$ git clone https://github.com/yzfedora/baidudl.git
	$ cd baidudl/packages/rpms
	$ sudo rpm -ivh baidudl-1.0-1.x86_64.rpm

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
