
#ifndef BUFFER_H
#define BUFFER_H

#include <assert.h>
#include <malloc.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>

/*   +-------------+------------------+------------------+
 *   | prependable |     readable     |     writable     |
 *   +-------------+------------------+------------------+
 *             read_index       writable_index          cap
 */

class Buffer {
 public:
  Buffer(size_t cap)
      : cap_(cap),
        write_index_(0),
        read_index_(0),
        data_(static_cast<char*>(malloc(cap))) {}

  ~Buffer() { free(data_); }

  int WriteFD(int fd) {
    int readable = Readable();
    int n = write(fd, data_ + read_index_, readable);
    if (n <= 0) return n;
    read_index_ += n;
    return n;
  }

  int ReadFD(int fd, char** data) {
    int writable = Writable();
    if (writable <= 0) {
      assert(writable == 0);
      Expand();
      writable = Writable();
    }

    int n = read(fd, data_ + write_index_, writable);
    if (n <= 0) return n;
    *data = data_ + write_index_;
    write_index_ += n;
    return n;
  }

  /*
   * 1. If writable >= size then append to buff
   * 2. If prepandable + writable >= size then move readable content to 0
   * 3. Otherwise, expand buff
   *
   * repeat until 1 or 2 satisfied
   */
  void Write(const char* src, size_t n) {
    for (;;) {
      size_t writable = Writable();
      if (writable >= n) break;

      size_t prependable = Prependable();
      if (prependable + writable >= n) {  // move readable content to 0
        int readable = Readable();
        memmove(data_, data_ + read_index_, readable);
        read_index_ = 0;
        write_index_ -= prependable;
        break;
      }

      Expand();
    }

    memcpy(data_ + write_index_, src, n);
    write_index_ += n;
  }

  void Read(char* dst, size_t n) {
    size_t readable = Readable();
    n = std::min(readable, n);
    memcpy(dst, data_ + read_index_, n);
    read_index_ += n;
  }

  bool Empty() const { return Readable() <= 0; }

  void Clear() {
    write_index_ = 0;
    read_index_ = 0;
  }

 private:
  size_t Readable() const { return write_index_ - read_index_; }
  size_t Writable() const { return cap_ - write_index_; }
  size_t Prependable() const { return read_index_; }

  void Expand() {
    int new_cap = cap_ * 2;
    char* new_data = static_cast<char*>(realloc(data_, new_cap));
    if (nullptr == new_data) std::__throw_bad_alloc();
    data_ = new_data;
    cap_ = new_cap;
  }

  size_t cap_;
  size_t write_index_;
  size_t read_index_;
  char* data_;
};
#endif  // BUFFER_H