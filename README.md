# network
windows/linux tcp/udp C++ code with iocp and epoll 


这是windows和linux封装了具有相同接口的网络库，windows下使用iocp（完成端口）实现，linux下使用epoll实现。同时还提供了定时器接口。你可以用它只作为网络模块的代码，也可以使用它作为你的程序框架。因为使用了C++11的语法，所以你的开发工具版本不宜过低，我自己使用的是windows下VS2015和linux下的gcc6.2.0。windows下的udp实现，自我感觉不是很满意，或许完成端口不太适合udp，也希望能得到大家的建议和意见，有问题可以联系我交流，邮箱61077307@qq.com。另外值得注意的是：出于性能的考虑，在网络工作繁忙时，定时器的触发会不是很精确
