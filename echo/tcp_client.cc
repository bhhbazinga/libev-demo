#include "libev/ev++.h"
#include "tcp_connection.h"

class TcpClient {
 public:
  TcpClient(ev::loop_ref* loop) : loop_(loop) {
    stdin_w_.set<TcpClient, &TcpClient::OnStdin>(this);
    stdin_w_.set(loop_->raw_loop);
    stdin_w_.start(STDIN_FILENO, ev::READ);
  }
  ~TcpClient() { stdin_w_.stop(); }

  void SetMessageCallback(const MessageCallback& cb) { msg_cb_ = cb; }
  void SetCloseCallback(const CloseCallback& cb) { close_cb_ = cb; }

  void Connect(const char* ip, unsigned short port);

 private:
  void OnStdin() {
    char buff[1024];
    int n = read(stdin_w_.fd, buff, sizeof(buff));
    conn_->Write(buff, n);
  }

  MessageCallback msg_cb_;
  CloseCallback close_cb_;
  ev::loop_ref* loop_;
  ev::io stdin_w_;
  TcpConnection* conn_;
};

void TcpClient::Connect(const char* ip, unsigned short port) {
  int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd == -1) Exit();

  sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(ip);
  servaddr.sin_port = htons(port);

  if (connect(fd, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) !=
          0 &&
      errno != EINPROGRESS) {
    close(fd);
    Exit();
  }

  conn_ = new TcpConnection(loop_, fd);
  conn_->SetMessageCallback(msg_cb_);
  conn_->SetCloseCallback(close_cb_);
  LOG("connect,ip=%s,port=%d", ip, port);
}

int main(int argc, char const* argv[]) {
  ev::default_loop loop;
  TcpClient client(&loop);
  client.SetMessageCallback(
      [](TcpConnection* conn, const char* data, size_t n) {
        assert(n > 0);
        std::string s(data, n);
        LOG("[s->c]:%s", s.data());
      });
  client.SetCloseCallback([&loop](TcpConnection* conn) {
    LOG("server closed");
    loop.break_loop();
  });
  client.Connect(argv[1], atoi(argv[2]));
  loop.run();
  return 0;
}
