Name:           baidudl
Version:        1.0
Release:        1
Summary:        A multithreading download tool for pan.baidu.com

License:        GPLv3+
URL:            https://github.com/yzfedora/baidudl.git
Source0:	../SOURCES/baidudl/baidudl-1.0.tar.gz

Requires:       libcurl

%description
baidudl is a multithreading download tool, and original desined for
pan.baidu.com, http, https, ftp are supported also.

%prep
%autosetup


%build
%configure
%make_build


%files
/usr/bin/bdpandl
/usr/bin/bdpandl-decode


%install
rm -rf $RPM_BUILD_ROOT
%make_install


%changelog
* Sun Nov 27 2016 Yang Zhang <yzfedora@gmail.com>
- first rpm package built from github repository.
