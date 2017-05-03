# baidudl
This is a multi-thread download tool for linux, mainly used to download large
file from pan.baidu.com.
百度网盘多线程下载工具, 支持协议http, https, ftp, 支持平台Linux, Unix-Like,
OSX, 支持断点续传.

![image](https://github.com/yzfedora/baidudl/raw/master/demo.png)

# Tips
if you find the download speed become very slow, even smaller than 100
KiB/s, you can type Ctrl-C first to stop it. and restore again. and don't
forget to use "-n" option to specify use how many number of threads to
download, normally, I set it to 100 or 200 according the situation.


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
6. add underline as indicator to indicates are we recvory the download
   from previous file or not.
7. adjust buffer size dynamically supports. all buffers memory will be
   write to disk for every minute(60 second default).
   
# Features
1. multithreading download, number of threads depending on server.
2. continues download, supports restore download from a previous file.
3. http, https, ftp protocol supports. (axel is great, but no https supports)
4. graceful status bar.
5. Linux, Unix-like, OSX supports, (still no time to port it to windows,
   mainly, use windows native threads supports to replace Linux's pthread).


# Compile
	if you want to use the latest version program, maybe you should
	try compile it. the pre-compiled packages are provided also. but,
	I can not make sure it's latest version. because the latest code
	on repository may not stable.
	
	$ git clone https://github.com/yzfedora/baidudl.git
	$ cd baidudl
	$ ./autogen.sh
	$ make
	$ sudo make install

# Install
	if you don't want to compile manually, the pre-compiled packages were
	provided, please go to subdirectory "packages", then you will find it.

	$ git clone https://github.com/yzfedora/baidudl.git

	For RPM systems:

	$ cd baidudl/packages/rpms
	$ sudo [yum | dnf] install baidudl-1.0-1.x86_64.rpm

	For Debian/Ubuntu systems:

	$ cd baidudl/packages/debs
	$ sudo dpkg -i baidudl_1.0-1_amd64.deb

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
