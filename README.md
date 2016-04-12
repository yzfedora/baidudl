# baidudl
This is a multi-thread download tool for linux, it main purpose is used to download from pan.baidu.com. (for Chinese: 这是一个Linux下的多线程下载工具，其初衷是为了用于baidu网盘的多线程下载。初期没有实现对批量下载的地址解析，可能会在以后版本添加更成熟的批量下载方式，但现在您仍需要手动添加地址到一个文件中用于批量下载。)

![image](https://github.com/yzfedora/baidudl/raw/master/demo.png)
# Usage
	Your need download liberr also.
	git clone https://github.com/yzfedora/liberr.git
	cd liberr
	make
	sudo make install

	cd ..
	git clone https://github.com/yzfedora/baidudl.git
	cd baidudl
	make
	sudo make install
	
	for single download:
	bdpandl -n 8 'http://lx.cdn.baidupcs.com/file/...'
	or your can use batch download, by specify a 'list file'. which is consists by download URLs, and line by line
	bdpandl -n 10 -l listfile
	

# Notice
	The download address is copy from your browser, when you click the
	download button in the page of pan.baidu.com/..., then it pop a new
	link or windows to download. this link just we need.
	
	And I suggested using two single quotes to surround the address, like
	above examples we demostrated.


	Any suggestions or bugs you can report to <yzfedora@gmail.com>
						Thanks in advance!
