mtpfs
=====
for mtp camera

1. decompress
tar xvf libusb_0.1.12.orig.tar.gz
tar xvf libmtp-1.1.2.tar.gz 
2. install libusb
cd libusb-0.1.12
./configure && make && sudo make install
cd ..

3.有的机子需要添加引用库
vi /etc/ld.so.conf.d/
把/usr/local/lib/加入
sudo ldconfig

4. install libmtp 
patch -p0 < libmtp.makefile.in.patch
patch -p0 < libmtp.patch
cd libmtp-1.1.2
./configure && make && sudo make install
cd ..

5. install mtpfs
cd mtpfs-0.1
./genconf.sh
./configure && make 

