// Include.h - portable common includes (rebuild).
// The original pulled in Linux-only headers (unistd.h, pthread.h, sys/epoll.h,
// sys/socket.h, arpa/inet.h, iconv.h, mysql.h). Per the cross-platform mandate
// those are gone from general code: threading uses <thread>/<mutex>, sockets/
// timers use Asio, charset conversion goes through a portable wrapper. Only the
// MySQL C client header remains here (it builds on Windows + Linux via vcpkg).
#ifndef _INCLUDE_H_
#define _INCLUDE_H_

// C standard library
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <ctime>
#include <cerrno>
#include <cstdarg>
#include <cmath>
#include <cctype>
#include <cstdint>

// C++ standard library
#include <algorithm>
#include <deque>
#include <map>
#include <list>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

// MySQL C client API (vcpkg: libmysql).
#include <mysql.h>

// Shared portable helpers (StrCopyN, LocalTime, Tokener, ...).
#include "Util.h"

using namespace std;

#endif // _INCLUDE_H_
