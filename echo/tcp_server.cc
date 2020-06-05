#include "tcp_connection.h"

class TcpServer {
 public:
  TcpServer(ev::loop_ref* loop) : loop_(loop) {}
  ~TcpServer() = default;

  void SetMessageCallback(const MessageCallback& cb) { msg_cb_ = cb; }
  void SetCloseCallback(const CloseCallback& cb) { close_cb_ = cb; }
  void Start(const char* ip, unsigned short port);

 private:
  void OnConnection() {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int newfd = accept(listen_w_.fd, reinterpret_cast<sockaddr*>(&addr), &len);
    if (newfd == -1) Exit();

    char* ip = inet_ntoa(addr.sin_addr);
    unsigned short port = addr.sin_port;
    LOG("new connection,ip=%s,port=%d", ip, port);

    TcpConnection* new_conn = new TcpConnection(loop_, newfd);
    new_conn->SetMessageCallback(msg_cb_);
    new_conn->SetCloseCallback(close_cb_);
  }

  MessageCallback msg_cb_;
  CloseCallback close_cb_;
  ev::loop_ref* loop_;
  ev::io listen_w_;
};

void TcpServer::Start(const char* ip, unsigned short port) {
  int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listenfd == -1) Exit();

  int reuse = 1;
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, static_cast<void*>(&reuse),
                 sizeof(reuse)) == -1)
    Exit();

  sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(ip);
  servaddr.sin_port = htons(port);
  bind(listenfd, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr));
  listen(listenfd, 16);

  listen_w_.set<TcpServer, &TcpServer::OnConnection>(this);
  listen_w_.set(loop_->raw_loop);
  listen_w_.start(listenfd, ev::READ);

  LOG("start,ip=%s,port=%d", ip, port);
}

int main(int argc, char const* argv[]) {
  ev::default_loop loop;
  TcpServer server(&loop);
  server.SetMessageCallback(
      [](TcpConnection* conn, const char* data, size_t n) {
        assert(n > 0);
        std::string s(data, n);
        LOG("[c->s]:%s", s.data());
        conn->Write(data, n);
      });
  server.SetCloseCallback(
      [&loop](TcpConnection* conn) { LOG("client closed"); });
  server.Start(argv[1], atoi(argv[2]));
  loop.run();
  return 0;
}
