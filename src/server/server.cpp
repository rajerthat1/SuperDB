#include "server/server.h"
#include "server/connection.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

Server::Server(uint16_t port, IStorageEngine &storage)
    : port_(port), storage_(storage) {}

void Server::run() {
  int listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

  int opt = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = INADDR_ANY;
  bind(listen_fd, (sockaddr *)&addr, sizeof(addr));
  listen(listen_fd, 128);

  // Graceful shutdown via signalfd:
  // Block signals so the default handlers don't run, then create a signalfd
  // that becomes readable when a signal arrives. The signalfd goes into epoll,
  // so the event loop wakes up naturally and we can clean up.
  sigset_t sig_mask;
  sigemptyset(&sig_mask);
  sigaddset(&sig_mask, SIGINT);
  sigaddset(&sig_mask, SIGTERM);
  sigprocmask(SIG_BLOCK, &sig_mask, nullptr);

  int shutdown_fd = signalfd(-1, &sig_mask, SFD_NONBLOCK);

  int epoll_fd = epoll_create1(0);

  epoll_event listen_ev{EPOLLIN, {.fd = listen_fd}};
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &listen_ev);

  epoll_event sig_ev{EPOLLIN, {.fd = shutdown_fd}};
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, shutdown_fd, &sig_ev);

  std::unordered_map<int, Connection> connections;
  std::vector<epoll_event> events(64);

  bool running = true;

  while (running) {
    int n = epoll_wait(epoll_fd, events.data(), events.size(), -1);

    for (int i = 0; i < n; i++) {
      int fd = events[i].data.fd;

      if (fd == shutdown_fd) {
        struct signalfd_siginfo fdsi;
        [[maybe_unused]] auto _ = read(shutdown_fd, &fdsi, sizeof(fdsi));
        running = false;
        break;
      }

      if (fd == listen_fd) {
        while (true) {
          int conn_fd = accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK);
          if (conn_fd == -1)
            break;
          epoll_event conn_ev{EPOLLIN | EPOLLOUT, {.fd = conn_fd}};
          epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &conn_ev);
          connections.emplace(conn_fd, Connection{});
        }
        continue;
      }

      auto it = connections.find(fd);
      if (it == connections.end())
        continue;

      bool closed = false;

      if (events[i].events & EPOLLIN) {
        char buf[65536];
        ssize_t nread = read(fd, buf, sizeof(buf));
        if (nread <= 0) {
          close(fd);
          connections.erase(it);
          closed = true;
        } else {
          it->second.on_readable(buf, nread, storage_);
        }
      }

      if (!closed && (events[i].events & EPOLLOUT)) {
        it->second.on_writable(fd);
        if (it->second.should_close()) {
          close(fd);
          connections.erase(it);
          closed = true;
        }
      }
    }
  }

  // Graceful shutdown: close all client connections, then server fds
  for (auto &[fd, conn] : connections)
    close(fd);
  connections.clear();
  close(listen_fd);
  close(shutdown_fd);
  close(epoll_fd);
}
