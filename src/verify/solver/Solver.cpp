#include "Solver.h"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <poll.h>
#include <sys/wait.h>
#include <thread>
#include <atomic>
#include <unistd.h>

namespace verutop {

namespace {

// ---- S-expressions: all of Z3's output that is read -----------------------

struct SX {
  std::string atom;
  std::vector<SX> kids;
  bool list = false;
};
SX parseSX(const std::string &s, size_t &i) {
  while (i < s.size() && isspace((unsigned char)s[i])) ++i;
  SX x;
  if (i >= s.size()) return x;
  if (s[i] == '(') {
    x.list = true;
    ++i;
    while (true) {
      while (i < s.size() && isspace((unsigned char)s[i])) ++i;
      if (i >= s.size() || s[i] == ')') break;
      x.kids.push_back(parseSX(s, i));
    }
    ++i;
    return x;
  }
  if (s[i] == '|') {
    size_t j = s.find('|', i + 1);
    x.atom = s.substr(i, j - i + 1);
    i = j + 1;
    return x;
  }
  size_t j = i;
  while (j < s.size() && !isspace((unsigned char)s[j]) && s[j] != '(' && s[j] != ')') ++j;
  x.atom = s.substr(i, j - i);
  i = j;
  return x;
}
// A model value as text: (- 1) is -1, (|Node| 1 |Leaf| |Leaf|) is
// Node(1, Leaf, Leaf).
std::string valueOf(const SX &x) {
  if (!x.list) {
    auto a = x.atom;
    if (a.size() > 1 && a.front() == '|') a = a.substr(1, a.size() - 2);
    return a;
  }
  if (x.kids.size() == 2 && x.kids[0].atom == "-") return "-" + valueOf(x.kids[1]);
  if (x.kids.empty()) return "()";
  std::string s = valueOf(x.kids[0]) + "(";
  for (size_t i = 1; i < x.kids.size(); ++i) s += (i > 1 ? ", " : "") + valueOf(x.kids[i]);
  return s + ")";
}

// ---- running a program with text on its standard input ---------------------

// Runs argv with `input` on stdin; returns what it wrote to stdout and
// stderr.  Writing and reading are interleaved, so neither side can block
// on a full pipe.
std::string runProcess(const std::vector<std::string> &argv, const std::string &input) {
  std::vector<char *> args;
  for (auto &a : argv) args.push_back(const_cast<char *>(a.c_str()));
  args.push_back(nullptr);

  // Queries run in parallel.  A child must not inherit the pipes of
  // another query's child, or that one would never see the end of its
  // input; so the pipes are close-on-exec, and creating them and forking
  // happen under one lock.
  static std::mutex launching;
  int in[2] = {-1, -1}, out[2] = {-1, -1};
  pid_t pid = -1;
  {
    std::lock_guard<std::mutex> lock(launching);
    if (pipe(in) != 0) return "";
    if (pipe(out) != 0) {
      close(in[0]);
      close(in[1]);
      return "";
    }
    for (int fd : {in[0], in[1], out[0], out[1]}) fcntl(fd, F_SETFD, FD_CLOEXEC);
    pid = fork();
    if (pid == 0) {
      // Only async-signal-safe calls from here on.
      dup2(in[0], 0);
      dup2(out[1], 1);
      dup2(out[1], 2);
      execvp(args[0], args.data());
      const char msg[] = "the solver could not be run (is z3 on the PATH?)\n";
      (void)!write(1, msg, sizeof msg - 1);
      _exit(127);
    }
  }
  if (pid < 0) {
    for (int fd : {in[0], in[1], out[0], out[1]}) close(fd);
    return "";
  }
  close(in[0]);
  close(out[1]);
  std::string output;
  size_t sent = 0;
  int wfd = in[1];
  if (input.empty()) {
    close(wfd);
    wfd = -1;
  }
  char buf[65536];
  while (true) {
    struct pollfd fds[2] = {{out[0], POLLIN, 0}, {wfd, POLLOUT, 0}};
    if (poll(fds, wfd >= 0 ? 2 : 1, -1) < 0) {
      if (errno == EINTR) continue;
      break;
    }
    if (wfd >= 0 && (fds[1].revents & (POLLOUT | POLLERR | POLLHUP))) {
      ssize_t n = write(wfd, input.data() + sent, std::min<size_t>(input.size() - sent, sizeof buf));
      if (n > 0) sent += (size_t)n;
      if (n < 0 || sent == input.size()) {
        close(wfd);
        wfd = -1;
      }
    }
    if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
      ssize_t n = read(out[0], buf, sizeof buf);
      if (n <= 0) break;
      output.append(buf, (size_t)n);
    }
  }
  if (wfd >= 0) close(wfd);
  close(out[0]);
  int status = 0;
  waitpid(pid, &status, 0);
  return output;
}

// A solver that exits before reading all its input must not kill the
// compiler with SIGPIPE.
void ignoreSigpipe() {
  static std::once_flag once;
  std::call_once(once, [] { signal(SIGPIPE, SIG_IGN); });
}

const char *CHECK = "(check-sat)\n";
const char *WHY = "(get-info :reason-unknown)\n";

} // namespace

std::string z3Input(const Background &bg, const Query &q) {
  std::string text = render(bg, q);
  if (auto at = text.find(CHECK); at != std::string::npos) text.insert(at + std::strlen(CHECK), WHY);
  return text;
}

Answer parseAnswer(const std::string &output, const Query &q) {
  std::vector<std::string> lines;
  for (size_t i = 0; i < output.size();) {
    size_t j = output.find('\n', i);
    if (j == std::string::npos) j = output.size();
    lines.push_back(output.substr(i, j - i));
    i = j + 1;
  }
  Answer a;
  a.raw = lines.empty() ? "no answer" : lines[0];
  a.verdict = a.raw == "unsat"     ? Verdict::Unsat
              : a.raw == "sat"     ? Verdict::Sat
              : a.raw == "unknown" ? Verdict::Unknown
                                   : Verdict::Error;
  // The reply to the question about `unknown` is not part of the answer.
  if (lines.size() > 1 && lines[1].rfind("(:reason-unknown", 0) == 0) {
    a.limited = a.verdict == Verdict::Unknown && (lines[1].find("canceled") != std::string::npos ||
                                                  lines[1].find("resource limit") != std::string::npos);
    lines.erase(lines.begin() + 1);
  }
  for (size_t i = 1; i < lines.size(); ++i) a.rest += lines[i] + "\n";

  size_t i = 0;
  SX sx = parseSX(a.rest, i);
  if (a.verdict == Verdict::Unsat && q.core) {
    for (auto &k : sx.kids) a.core.push_back(k.atom);
  } else if (a.verdict != Verdict::Unsat && !q.values.empty()) {
    // ((term value) ...), one pair for each term asked.
    bool pairs = sx.list && sx.kids.size() == q.values.size();
    for (auto &k : sx.kids) pairs = pairs && k.kids.size() == 2;
    if (pairs)
      for (auto &k : sx.kids) a.values.push_back(valueOf(k.kids[1]));
  }
  return a;
}

Answer Z3Process::check(const Background &bg, const Query &q) {
  ignoreSigpipe();
  return parseAnswer(runProcess({"z3", "-in"}, z3Input(bg, q)), q);
}

std::string Z3Process::version() {
  std::string v = runProcess({"z3", "--version"}, "");
  v = v.substr(0, v.find(" - "));
  while (!v.empty() && isspace((unsigned char)v.back())) v.pop_back();
  return v.empty() || v.find("could not be run") != std::string::npos ? "Z3" : v;
}

Answer CannedSolver::unsat(std::vector<std::string> core) {
  Answer a;
  a.verdict = Verdict::Unsat;
  a.raw = "unsat";
  a.core = std::move(core);
  return a;
}
Answer CannedSolver::sat(std::vector<std::string> values) {
  Answer a;
  a.verdict = Verdict::Sat;
  a.raw = "sat";
  a.values = std::move(values);
  return a;
}
Answer CannedSolver::unknown(bool limited) {
  Answer a;
  a.verdict = Verdict::Unknown;
  a.raw = "unknown";
  a.limited = limited;
  return a;
}

int defaultJobs() {
  unsigned n = std::thread::hardware_concurrency();
  return n == 0 ? 1 : (int)std::min(n, 4u);
}

std::vector<Answer> checkAll(Solver &solver, const Background &bg, const std::vector<const Query *> &queries,
                             int jobs) {
  std::vector<Answer> answers(queries.size());
  if (jobs <= 0) jobs = defaultJobs();
  jobs = (int)std::min<size_t>((size_t)jobs, queries.size());
  // Each worker takes the next query not yet taken.  Which worker answers
  // which query varies from run to run; where the answer is stored does not.
  std::atomic<size_t> next{0};
  auto work = [&] {
    for (size_t i = next++; i < queries.size(); i = next++) answers[i] = solver.check(bg, *queries[i]);
  };
  if (jobs <= 1) {
    work();
    return answers;
  }
  std::vector<std::thread> workers;
  for (int j = 0; j < jobs; ++j) workers.emplace_back(work);
  for (auto &w : workers) w.join();
  return answers;
}

} // namespace verutop
