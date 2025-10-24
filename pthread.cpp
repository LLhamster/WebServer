#include "pthread.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "util.h"
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <string>
#include "client_count.h"
#include <unordered_set>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>

#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <vector>

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>



pthread_mutex_t pthread::lock_ = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pthread::cond_ = PTHREAD_COND_INITIALIZER;
int pthread::front_ = 0;
int pthread::rear_ = 0;
pthread_t pthread::workers_[WORKER_NUM];
int pthread::queue_[1024];


static void drain_read(int fd) {
    char buf[4096];
    // //将设备数减去1
    // pthread_mutex_lock(&client_count_mutex);
    // client_count--;
    // pthread_mutex_unlock(&client_count_mutex);

    for (;;) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0) continue;
        if (n == 0) break; // 对端关闭
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        break;
    }
}


int pthread::pthread_init(int epollfd){
    for(int j = 0; j < WORKER_NUM; j++){
        pthread_create(&workers_[j], NULL, worker_, (void*)(intptr_t)epollfd);
    }
    return 0;
}

//处理线程
static std::unordered_set<int> clients;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool read_until(int fd, std::string& buf,  const char* mask){
    while(1){
        char tmp[4096];
        int num = recv(fd, tmp, sizeof(tmp), 0);
        // printf("read num=%d\n", num);
        if(num > 0){
            buf.append(tmp, tmp+num);
            if(buf.find(mask) != std::string::npos){
                return true;
            }
        }
        else if(num == 0) return true;
        else{
            if(errno ==EAGAIN || errno == EWOULDBLOCK) continue;
            if(errno == EINTR) continue;
            return false;
        }
    }
    
}

static ssize_t write_all(int fd, const char* buf, size_t len){
    size_t total = 0;
    while(total < len){
        ssize_t n = send(fd, buf+total, len-total, 0);
        if(n>0) total += n;
        else if(n==0) break;
        else{
            if(errno == EINTR) continue;
            if(errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return -1;
        }
    }
    return total;
}

static void respond_404(int fd){
    const char* header = 
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 13\r\n"
        "Connection: close\r\n"
        "\r\n"
        "404 Not Found";
    write_all(fd, header, strlen(header));
    shutdown(fd, SHUT_WR);
    drain_read(fd);
    close(fd);
}

static void respond_html(int fd){
    static const char* html =
        "<!DOCTYPE html>\n"
        "<html><head><meta charset=\"utf-8\"><title>聊天室</title>"
        "<style>body{font-family:sans-serif;max-width:680px;margin:20px auto;}#log{border:1px solid #ccc;height:300px;overflow:auto;padding:8px;white-space:pre-wrap;}form{display:flex;gap:8px;margin-top:8px;}input[type=text]{flex:1;padding:6px;}button{padding:6px 12px;}</style>"
        "</head><body><h2>聊天室</h2><div id=\"log\"></div>"
        "<form id=\"f\"><input id=\"msg\" type=\"text\" placeholder=\"输入消息后回车或点发送\"><button>发送</button></form>"
        "<script>const log=document.getElementById('log');function append(t){const b=log.scrollTop+log.clientHeight>=log.scrollHeight-5;if(t.startsWith('<button')){const div=document.createElement('div');div.innerHTML=t;log.appendChild(div);}else{const textNode=document.createTextNode(t+'\\n');log.appendChild(textNode);}if(b)log.scrollTop=log.scrollHeight;}const es=new EventSource('/events');es.onmessage=(e)=>append(e.data);es.onerror=()=>append('[系统] 连接断开，刷新页面重试');const f=document.getElementById('f');const m=document.getElementById('msg');f.addEventListener('submit',async(e)=>{e.preventDefault();const t=m.value.trim();if(!t)return;try{await fetch('/send',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'},body:'msg='+encodeURIComponent(t)});m.value='';}catch(err){append('[系统] 发送失败：'+err);}});</script>"
        "</body></html>";
    
    char header[256];
    int n = snprintf(header, sizeof(header),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Length: %zu\r\n"
                 "Content-Type: text/html;charset=utf-8\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 strlen(html));
    if(n < 0){
        perror("snprintf error");
        respond_404(fd);
        return;
    }
    if(write_all(fd, header, strlen(header)) < 0 || write_all(fd, html, strlen(html)) < 0){
        perror("write error");
        respond_404(fd);
        return;
    }
    shutdown(fd, SHUT_WR);
    drain_read(fd);
    close(fd);
}

static void broadcast(int fd, const char* msg, const char* url = nullptr){
    std::vector<int> to_close;
    // 按照 SSE 协议包装消息：每条消息使用 data: 开头，并以空行结束,不按照就没法显示。
    std::string payload;
    if (url) {
        // 如果提供了 URL，则构造一个按钮
        payload = "data: <button onclick=\"window.location.href='" + std::string(url) + "'\">" + std::string(msg) + "</button>\n\n";
    } else {
        // 普通消息
        payload = "data: " + std::string(msg) + "\n\n";
    }
    pthread_mutex_lock(&clients_mutex);
    for(int client_fd : clients){
        if(write_all(client_fd, payload.c_str(), payload.size()) < 0){
            to_close.push_back(client_fd);
            printf("shibai");
        }
    }
    for(int i :to_close){
        auto it = clients.find(i);
        if(it != clients.end()) clients.erase(it);
    }
    pthread_mutex_unlock(&clients_mutex);
    for(int i: to_close){
        //将设备数减去1
        pthread_mutex_lock(&client_count_mutex);
        client_count--;
        pthread_mutex_unlock(&client_count_mutex);
        shutdown(i, SHUT_WR);
        drain_read(i);
        close(i);
    }
}

static void keep_html(int fd){
    const char* header = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    if(write_all(fd, header, strlen(header)) < 0){
        perror("write error");
        respond_404(fd);
        return;
    }
    pthread_mutex_lock(&clients_mutex);
    clients.insert(fd);
    pthread_mutex_unlock(&clients_mutex);
    pthread_mutex_lock(&client_count_mutex);
    client_count++;
    pthread_mutex_unlock(&client_count_mutex);

    broadcast(fd, "[系统] 欢迎新用户进入聊天室！");
    // broadcast(fd, "点击访问 PDF 文档", "http://127.0.0.1/books/1/main.pdf");
}

static std::string decodeMsg(const std::string &msg){
    std::string tmp;
    int ii;
    for(int i=0;i < msg.length();i++){
        if(msg[i] == '%'){
            std::istringstream iss(msg.substr(i+1, 2));
            iss >> std::hex >> ii;
            char ch = static_cast<char>(ii);
            tmp += ch;
            i += 2;
        }
        else if(msg[i] == '+') tmp += ' ';
        else tmp += msg[i];
    }
    return tmp;
}

static std::string searchData(const std::string &title){
    // 请根据实际情况修改下面的连接信息
    const std::string dbUser = "root";
    const std::string dbPass = "123";
    const std::string dbName = "bookstore"; // 你的数据库名
    const std::string socketPath = "/var/run/mysqld/mysqld.sock"; // 根据系统调整

    sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();

    // 使用 ConnectOptionsMap 指定 Unix socket（localhost 的本地套接字）
    sql::ConnectOptionsMap connection_properties;
    connection_properties["hostName"] = sql::SQLString("localhost");
    connection_properties["userName"] = sql::SQLString(dbUser);
    connection_properties["password"] = sql::SQLString(dbPass);
    connection_properties["schema"] = sql::SQLString(dbName);
    connection_properties["socket"] = sql::SQLString(socketPath);
    // 可选：设置超时等
    // connection_properties["OPT_CONNECT_TIMEOUT"] = 10;

    std::unique_ptr<sql::Connection> conn(driver->connect(connection_properties));

    // 确保使用 utf8mb4，避免中文乱码
    {
        std::unique_ptr<sql::Statement> st(conn->createStatement());
        st->execute("SET NAMES utf8mb4");
    }

    std::unique_ptr<sql::PreparedStatement> ps(
        conn->prepareStatement("SELECT url FROM books_simple WHERE title = ? LIMIT 1")
    );
    ps->setString(1, title);
    std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());

    if (rs->next()) {
        std::string url = rs->getString("url");
        return url;
    } else {
        std::cout << "Not found: " << title << "\n";
        return "none";
    }
    
}

static void handleMsg(int fd, std::string &msg){
    const char* header = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Content-Type: text/html;charset=utf-8\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    if(write_all(fd, header, strlen(header)) < 0){
        perror("write error");
        respond_404(fd);
        return;
    }

    auto it = msg.find("msg=");
    if(it != std::string::npos){
        std::cout << "cut" << std::endl;
        msg = msg.substr(it+4);
    }
    std::string decoMsg = decodeMsg(msg);
    std::string path = searchData(decoMsg);
    if(path != "none"){
        std::string url = "http://127.0.0.1" + path;
        // std::cout << "Found: " << url << "\n";
        broadcast(fd, decoMsg.c_str(),url.c_str());
    }
    broadcast(fd, decoMsg.c_str());
    shutdown(fd, SHUT_WR);
    drain_read(fd);
    close(fd);
}

void* pthread::worker_(void* arg){
        while(1){
            int work_fd = dqueue();
            std::string buf;
            buf.reserve(4096);
            if(!read_until(work_fd, buf, "\r\n\r\n")) {
                perror("read error");
                close(work_fd);
                continue;
            }
            //打印请求命令
            std::cout << buf << std::endl;
            if(buf.find("/events") != std::string::npos){
                keep_html(work_fd);
                continue;
            }

            if(buf.find("GET /") != std::string::npos){
                respond_html(work_fd);
                continue;
            }

            if(buf.find("POST /send") != std::string::npos){
                // std::cout<<"post"<<std::endl;
                handleMsg(work_fd, buf);
                continue;
            }

            
        }
        return NULL;
}

int pthread::dqueue() {
    pthread_mutex_lock(&lock_);
    while(front_ == rear_){
        pthread_cond_wait(&cond_, &lock_);
    }
    int fd = queue_[front_];
    front_ = (front_ + 1) % 1024;
    pthread_mutex_unlock(&lock_);
    return fd;
}

void pthread::enque(int fd) {
    pthread_mutex_lock(&lock_);
    queue_[rear_] = fd;
    rear_ = (rear_ + 1)%1024;
    pthread_cond_signal(&cond_);
    pthread_mutex_unlock(&lock_);
}