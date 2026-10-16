// Model-free transport fixture; never compiled into the production worker.
#include "decision/request.h"
#include "decision/transport.h"
#include <csignal>
#ifdef SAMOSA_DECISION_PUBLIC_FIXTURE
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <limits.h>
extern "C" const void *OrtGetApiBase();
#endif
#include <stdexcept>
#include <unistd.h>
int main(int argc, char **argv) {
#ifdef SAMOSA_DECISION_PUBLIC_FIXTURE
  if ((argc != 5 && argc != 11) || std::string(argv[1]) != "--model" ||
      std::string(argv[3]) != "--tokenizer" || !OrtGetApiBase())
    return 2;
  if (argc == 11 && (std::string(argv[5]) != "--image-rgba" ||
      std::string(argv[7]) != "--width" || std::string(argv[9]) != "--height" ||
      access(argv[6], R_OK))) return 2;
  std::ifstream config(argv[2]);
  std::string mode;
  config >> mode;
  if (mode.empty())
    return 2;
#else
  (void)argc;
  (void)argv;
#endif
  std::signal(SIGPIPE, SIG_IGN);
  samosa::decision::PairInference infer = [](const auto &pairs, auto,
                                             auto *logits, auto *) {
    logits->assign(pairs.size(), {0, 0});
    return true;
  };
  return static_cast<int>(samosa::decision::serve_requests(
      STDIN_FILENO, STDOUT_FILENO,
      [&](const auto &request, auto *reply, auto *error) {
#ifdef SAMOSA_DECISION_PUBLIC_FIXTURE
        if (mode == "wait") {
          const char *marker = std::getenv("SAMOSA_DECISION_QA_MARKER");
          if (!marker)
            return false;
          char path[PATH_MAX];
#ifdef __APPLE__
          if (fcntl(STDOUT_FILENO, F_GETPATH, path) < 0)
            return false;
#else
          const ssize_t n = readlink("/proc/self/fd/1", path, sizeof(path) - 1);
          if (n < 0)
            return false;
          path[n] = 0;
#endif
          const std::string pending = std::string(marker) + ".pending";
          {
            std::ofstream ready(pending);
            ready << getpid() << "\n" << path << "\n";
            if (!ready)
              return false;
          }
          if (rename(pending.c_str(), marker))
            return false;
          const std::string release = std::string(marker) + ".release";
          while (access(release.c_str(), F_OK))
            usleep(1000);
        }
        if (mode == "die")
          std::_Exit(17);
        if (mode == "input-limit") {
          *error = "decision_input_limit";
          return false;
        }
        if (mode == "invalid") {
          *reply = R"({"type":"noul","noul":2})";
          return true;
        }
        if (mode == "wrong-type") {
          *reply = R"({"type":"choice","choice":"private"})";
          return true;
        }
        if (mode == "malformed") {
          *reply = "{";
          return true;
        }
#endif
        if (request == "throw")
          throw std::runtime_error("/private/model/location");
        if (request == "input-limit") {
          *error = "decision_input_limit";
          return false;
        }
        if (request == "oversize") {
          reply->assign(1048577, 'x');
          return true;
        }
        if (request == "empty") {
          reply->clear();
          return true;
        }
        return samosa::decision::execute_request(infer, request, reply, error);
      }));
}
