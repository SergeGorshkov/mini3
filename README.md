Описание для CentOS

Установка gcc и стандартных библиотек  
yum install gcc  
yum install libstdc++  
yum install gcc-c++
  
Установка librabbitmq  
yum install cmake  
Скачать https://github.com/alanxz/rabbitmq-c/archive/master.zip и разархивировать. Из этой папки  
mkdir build && cd build  
cmake -DCMAKE_C_COMPILER="/usr/bin/gcc" -DCMAKE_MAKE_PROGRAM="/usr/bin/make" -DENABLE_SSL_SUPPORT=OFF ..  
cmake --build . --target install  
По какой-то причине на моей машине эта команда собрала библиотеку в /usr/local/lib64, а нужно было в /lib64. Пришлось скопировать туда библиотеку вручную.  
  
Установка librdkafka  
https://github.com/edenhill/librdkafka/archive/master.zip и разархивировать. Из этой папки  
./configure --install-deps  
make  
sudo make install  
  
Сборка mini3 - выполняем из папки mini3  
/usr/bin/gcc -m64 -lcrypto -lpthread -lstdc++ -lrabbitmq -lrdkafka -g0 mini3.cpp -o /path/to/mini3  
(/path/to заменить на путь к папке с исходниками)  

После этого можно запускать сервер, например:  
./mini3 8081  


