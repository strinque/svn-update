#include <string>
#include <sstream>
#include <vector>
#include <queue>
#include <deque>
#include <map>
#include <regex>
#include <filesystem>
#include <thread>
#include <mutex>
#include <stdbool.h>
#include <fmt/core.h>
#include <fmt/color.h>
#include <console/init.hpp>
#include <console/parser.hpp>
#include <windows.h>

/*============================================
| Declaration
==============================================*/
// program version
const std::string PROGRAM_NAME = "svn-update";
const std::string PROGRAM_VERSION = "1.0";

/*============================================
| Function definitions
==============================================*/
// lambda function to show colored tags
auto add_tag = [](const fmt::color color, const std::string& text) {
  fmt::print(fmt::format(fmt::fg(color) | fmt::emphasis::bold, "[{}]\n", text));
};

// split a string into a vector using a char delimiter
std::vector<std::string> split(const std::string& s, char delim)
{
  std::stringstream ss(s);
  std::string item;
  std::vector<std::string> elems;
  while (std::getline(ss, item, delim))
    elems.push_back(std::move(item));
  return elems;
}

// extract a list of directories from a string
std::vector<std::filesystem::path> extract_dirs(const std::string& root,
                                                const std::string& paths)
{
  std::vector<std::filesystem::path> dirs;
  for (const auto& s : split(paths, ';'))
  {
    std::filesystem::path dir(root);
    dir /= s;
    dir = std::filesystem::absolute(dir);
    if (!std::filesystem::directory_entry(dir).exists())
      throw std::runtime_error(fmt::format("the directory: \"{}\" doesn't exists", dir.string()).c_str());
    dirs.push_back(dir);
  }
  return dirs;
}

// list all sub-directories with filters
std::vector<std::filesystem::path> list_dirs(const std::string& root,
                                             const std::regex& pattern = std::regex(R"(.*)"),
                                             const std::vector<std::filesystem::path>& skip_dirs = {})
{
  std::vector<std::filesystem::path> dirs;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
  {
    if (entry.is_directory() &&
        std::regex_search(entry.path().filename().string(), pattern) &&
        std::find(skip_dirs.begin(), skip_dirs.end(), entry.path().parent_path()) == skip_dirs.end())
    {
      dirs.push_back(entry.path().parent_path());
    }
  }
  return dirs;
}

class SvnRepos
{
public:
  // constructor
  SvnRepos(const std::vector<std::filesystem::path>& repos) :
    m_repos(std::deque(repos.begin(), repos.end())),
    m_results(),
    m_running(false),
    m_threads(std::min(repos.size(), static_cast<std::size_t>(std::thread::hardware_concurrency()))),
    m_mutex()
  {
  }

  // destructor
  ~SvnRepos()
  {
    // stop threads
    m_running = false;
    for (auto& t : m_threads)
      if (t.joinable())
        t.join();
  }

  // start all the threads to update svn repositories
  void update()
  {
    // start all threads
    m_running = true;
    for (auto& t : m_threads)
      t = std::thread(&SvnRepos::run, this);

    // wait for threads completion
    for (auto& t : m_threads)
      if (t.joinable())
        t.join();

    // log updated repositories as table
  }

private:
  // run the update process - thread
  void run()
  {
    while (m_running)
    {
      // retrieve one repos - protected by mutex
      std::filesystem::path repo;
      {
        std::lock_guard<std::mutex> lck(m_mutex);
        if (m_repos.empty())
          return;
        repo = m_repos.front();
        m_repos.pop();
      }

      // execute the update process
      std::string logs;
      std::regex update_ok(R"(^A |^D |^U |^C |^G |E^ |R^ )");
      bool executed = exec("svn.exe update --accept theirs-full", repo, logs);

      // store result - protected by mutex
      if (!executed)
      {
        std::lock_guard<std::mutex> lck(m_mutex);
        m_results[repo] = false;
      }
      else if (std::regex_search(logs, update_ok))
      {
        std::lock_guard<std::mutex> lck(m_mutex);
        m_results[repo] = true;
      }
    }
  }

  // execute a windows process - blocking
  bool exec(const std::string& cmd,
            const std::filesystem::path& path = std::filesystem::current_path(),
            std::string& logs=std::string())
  {
    HANDLE stdout_rd = nullptr;
    HANDLE stdout_wr = nullptr;
    bool result;
    logs.clear();
    
    try
    {
      // set the security attributes to be inherited by child process
      SECURITY_ATTRIBUTES sec_attr{};
      sec_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
      sec_attr.bInheritHandle = TRUE;
      sec_attr.lpSecurityDescriptor = NULL;

      // create a pipe for the child process's STDOUT 
      if (!CreatePipe(&stdout_rd, &stdout_wr, &sec_attr, 0))
        throw std::runtime_error(fmt::format("CreatePipe failed with error: 0x{:x}", GetLastError()));

      // setup members of STARTUPINFO
      STARTUPINFO startup_info{};
      startup_info.cb = sizeof(startup_info);
      startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
      startup_info.hStdOutput = stdout_wr;
      startup_info.hStdError = stdout_wr;
      startup_info.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
      startup_info.wShowWindow = SW_HIDE;

      // setup members of PROCESS_INFORMATION
      PROCESS_INFORMATION process_info{};

      // create the child process
      std::string args = cmd;
      if (!CreateProcess(NULL,                  // no module name
                         &args[0],              // command line 
                         NULL,                  // process security attributes 
                         NULL,                  // primary thread security attributes 
                         TRUE,                  // handles are inherited 
                         CREATE_NO_WINDOW,      // creation flags 
                         NULL,                  // use parent's environment 
                         path.string().c_str(), // set working current directory 
                         &startup_info,         // pointer to STARTUPINFO structure 
                         &process_info))        // pointer to PROCESS_INFORMATION structure
        throw std::runtime_error(fmt::format("CreateProcess failed with error: 0x{:x}", GetLastError()));

      // close pipes we don't need anymore
      CloseHandle(process_info.hProcess);
      CloseHandle(process_info.hThread);
      CloseHandle(stdout_wr);
      stdout_wr = nullptr;

      // read buffer from pipe - until the other end has been broken
      DWORD len = 0;
      char buf[1024];
      while (true)
      {
        // check if there are still data to retrieve from stdout
        if (!PeekNamedPipe(stdout_rd, 0, 0, 0, &len, 0) || !len)
          break;

        // read output from the child process's pipe for STDOUT - blocking
        if (ReadFile(stdout_rd, buf, sizeof(buf), &len, NULL) && len)
        {
          buf[len >= sizeof(buf) ? sizeof(buf) - 1 : len] = 0;
          logs += buf;
        }
      }
      if (GetLastError() != ERROR_SUCCESS && 
          GetLastError() != ERROR_BROKEN_PIPE)
        throw std::runtime_error(fmt::format("ReadFile failed with error: 0x{:x}", GetLastError()).c_str());
      result = true;
    }
    catch (...)
    {
      result = false;
    }

    // close pipes
    if (stdout_rd)
      CloseHandle(stdout_rd);
    if (stdout_wr)
      CloseHandle(stdout_wr);

    return result;
  }

private:
  // svn repos informations
  std::queue<std::filesystem::path> m_repos;
  std::map<std::filesystem::path, bool> m_results;

  // handling threads
  std::atomic_bool m_running;
  std::vector<std::thread> m_threads;
  std::mutex m_mutex;
};

int main(int argc, char** argv)
{
  // initialize Windows console
  console::init();

  // parse command-line arguments
  std::string svn_path;
  std::string svn_skip_str;
  std::string log_file;
  console::parser parser(PROGRAM_NAME, PROGRAM_VERSION);
  parser.add("p", "path", "set the path to update the svn repositories", svn_path, true)
        .add("s", "skip", "skip the update of those directories (relative, separated by ';')", svn_skip_str)
        .add("l", "log", "save the updated list of directories to a log file", log_file);
  if (!parser.parse(argc, argv))
  {
    parser.print_usage();
    return -1;
  }
  if (!std::filesystem::directory_entry(std::filesystem::path(svn_path)).exists())
  {
    fmt::print("{} {}\n",
      fmt::format(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "error: "),
      fmt::format("the directory: \"{}\" doesn't exists", svn_path));
    return -1;
  }

  try
  {
    // construct the list of directories to skip
    fmt::print(fmt::format(fmt::emphasis::bold, "{:<45}", "get the list of directories to skip:"));
    std::vector<std::filesystem::path> svn_skip = extract_dirs(svn_path, svn_skip_str);
    add_tag(fmt::color::green, "OK");

    // list all svn repositories
    fmt::print(fmt::format(fmt::emphasis::bold, "{:<45}", "list all svn repositories:"));
    std::vector<std::filesystem::path> svn_repos = list_dirs(svn_path, std::regex(R"(\.svn$)"), svn_skip);
    add_tag(fmt::color::green, "OK");

    // update all the svn repositories
    SvnRepos svn(svn_repos);
    svn.update();
  }
  catch (const std::exception& ex)
  {
    add_tag(fmt::color::red, "KO");
    fmt::print("{} {}\n",
      fmt::format(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "error: "),
      ex.what());
      return -1;
  }

  return 0;
}