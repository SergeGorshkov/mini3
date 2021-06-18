Mini-3 triple store

See product info at https://mini-3.com

Build guide for CentOS

1) Install gcc and standard libraries:
yum install gcc  
yum install libstdc++  
yum install gcc-c++
  
2) Install librabbitmq  
yum install cmake  
Download and unpack https://github.com/alanxz/rabbitmq-c/archive/master.zip. Run from the unpacked folder:
mkdir build && cd build  
cmake -DCMAKE_C_COMPILER="/usr/bin/gcc" -DCMAKE_MAKE_PROGRAM="/usr/bin/make" -DENABLE_SSL_SUPPORT=OFF ..  
cmake --build . --target install  
Check that the library is built into the right location (for example, /lib64)
  
3) Install librdkafka  
Download and unpack https://github.com/edenhill/librdkafka/archive/master.zip. Run from the unpacked folder:
./configure --install-deps  
make  
sudo make install  
  
4) Build mini-3
/usr/bin/gcc -m64 -lcrypto -lpthread -lstdc++ -lrabbitmq -lrdkafka -g0 mini3.cpp -o /path/to/mini-3  
(Change /path/to to the folder with the sources)  

5) Run server, for example:
./mini3 8082  

Feel free to contact us by e-mail: mail@serge-gorshkov.ru
