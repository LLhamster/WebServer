1、在使用write发送数据到浏览器的时候，写完直接close，通过公网IP浏览，发现是空白的
原因：在chunk模式下，由于公网IP有个中间设备，而直接close，服务器的接受缓存区里面还有数据的话，就会发送rst而不是fin，如果rst比最后一个终止块快到中间设备，那么这时候中间设备就会将缓冲区里的内容全部丢弃，所以导致了空白。


2、遇到问题：hamster@ubuntu:~/coding/WebServer1/build$ ./server 0.0.0.0 1234 bind error : Address already in use server: /home/hamster/coding/WebServer1/main.cpp:59: int main(int, char**): Assertion sockfd != -1' failed. 已放弃 (核心已转储)
原因：TIME_WAIT 状态:TCP 连接关闭后，端口可能会进入 TIME_WAIT，短时间内不能重新绑定（尤其是非 SO_REUSEADDR 的情况下）。

3、在将html发送给客户端的时候，使用创建EventSource来使得浏览器发送events请求给服务器，然而C++会先将其中的\n提前转义，导致JS的语法错误，脚本没有执行，因此就不会发起GET /events