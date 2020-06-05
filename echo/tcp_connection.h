#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <functional>

#include "buffer.h"
#include "libev/ev++.h"
#define LOG(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__);

void Exit() {
  perror(nullptr);
  exit(errno);
}

int SetNonBlocking(int fd) {
  int flag;
  if ((flag = fcntl(fd, F_GETFL, 0)) == -1) return -1;
  if (fcntl(fd, F_SETFL, flag | O_NONBLOCK) == -1) return -1;
  return flag;
}

int SetNoDelay(int fd) {
  int flag = 1;
  return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
}

const int kInitBufferSize = 1024;
class TcpConnection;

typedef std::function<void(TcpConnection*, const char*, size_t)>
    MessageCallback;

typedef std::function<void(TcpConnection*)> CloseCallback;

class TcpConnection {
 public:
  TcpConnection(ev::loop_ref* loop, int fd)
      : wbuffer_(kInitBufferSize), rbuffer_(kInitBufferSize), shutdown_(false) {
    SetNonBlocking(fd);
    SetNoDelay(fd);

    w_.set<TcpConnection, &TcpConnection::OnEventTrigger>(this);
    w_.set(loop->raw_loop);
    w_.start(fd, ev::READ);
  }

  ~TcpConnection() { w_.stop(); }

  void SetMessageCallback(const MessageCallback& cb) { msg_cb_ = cb; }
  void SetCloseCallback(const CloseCallback& cb) { close_cb_ = cb; }

  void Shutdown() {
    shutdown_ = true;
    HandleWrite();
  }

  void Write(const char* data, size_t n) {
    wbuffer_.Write(data, n);
    HandleWrite();
  }

 private:
  void OnEventTrigger(ev::io& w, int revents) {
    if (revents & ev::WRITE) HandleWrite();
    if (revents & ev::READ) HandleRead();
  }

  void HandleWrite();
  void HandleRead();
  void HandleClose();

 private:
  MessageCallback msg_cb_;
  CloseCallback close_cb_;
  ev::io w_;
  Buffer wbuffer_;
  Buffer rbuffer_;
  bool shutdown_;
};

void TcpConnection::HandleWrite() {
  wbuffer_.WriteFD(w_.fd);
  if (wbuffer_.Empty()) {
    if (shutdown_) {
      HandleClose();
      return;
    }
    w_.set(ev::READ);
  } else {
    w_.set(ev::READ | ev::WRITE);
  }
}

void TcpConnection::HandleRead() {
  char* data;
  int n = rbuffer_.ReadFD(w_.fd, &data);
  if (n > 0) {
    msg_cb_(this, data, n);
    return;
  }

  if (n == 0 ||
      (n < 0 && errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN)) {
    Shutdown();
  }
}

void TcpConnection::HandleClose() {
  close(w_.fd);
  close_cb_(this);
  delete this;
}