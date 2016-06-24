# baidudl
This is a multi-thread download tool for linux, it main purpose is used to download from pan.baidu.com. (for Chinese: 这是一个Linux下的多线程下载工具，其初衷是为了用于baidu网盘的多线程下载。初期没有实现对批量下载的地址解析，可能会在以后版本添加更成熟的批量下载方式，但现在您仍需要手动添加地址到一个文件中用于批量下载。)

![image](https://github.com/yzfedora/baidudl/raw/master/demo.png)
# Dependencies
	Your need download and install liberr first.

	$ git clone https://github.com/yzfedora/liberr.git
	$ cd liberr
	$ make
	$ sudo make install

# Install
	$ git clone https://github.com/yzfedora/baidudl.git
	$ cd baidudl
	$ ./configure
	$ make
	$ sudo make install

# Usage
	download from a single url:
	$ bdpandl -n 8 'http://lx.cdn.baidupcs.com/file/...'

	I sugges you using single quotes to surround the url, prevent bash
	parsing extra symbols.

	if you want to download many files, you can store these urls to a file,
	then using -l option:
	$ bdpandl -n 10 -l listfile

# Notice
	The download address is copy from your browser, when you click the
	download button in the page of pan.baidu.com/..., then it pop a new
	link or windows to download. this link just we need.


	现在百度抽风了, 有些服务器改用HTTPS下载链接了, 你可以直接把链接中的
	"https"改为"http", 这样也能下载. 现存的代码不依赖于libcurl, 所以不支持
	https下载, 可能以后我会添加相关代码支持https. :-)

	Any suggestions or bugs you can report to <yzfedora@gmail.com>
						Thanks in advance!
