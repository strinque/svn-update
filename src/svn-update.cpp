#include <string>
#include <vector>
#include <queue>
#include <deque>
#include <map>
#include <regex>
#include <filesystem>
#include <memory>
#include <thread>
#include <mutex>
#include <stdbool.h>
#include <fmt/core.h>
#include <fmt/color.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <winpp/console.hpp>
#include <winpp/parser.hpp>
#include <winpp/utf8.hpp>
#include <winpp/progress-bar.hpp>
#include <winpp/files.hpp>
#include <winpp/win.hpp>
#include <fort.hpp>

/*============================================
| Declaration
==============================================*/
// program version
const std::string PROGRAM_NAME = "svn-update";
const std::string PROGRAM_VERSION = "1.1";

/*============================================
| Function definitions
==============================================*/
// lambda function to show colored tags
auto add_tag = [](const fmt::color color, const std::string& text) {
  spdlog::debug(fmt::format(fmt::fg(color) | fmt::emphasis::bold, "[{}]\n", text));
};

// initialize logger - spdlog
bool init_logger(const std::string& file)
{
  try
  {
    // generate log filename based on date
    std::filesystem::path log_name(file);

    // initialize console output
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);
    console_sink->set_pattern("%v");

    std::shared_ptr<spdlog::logger> logger;
    if (!file.empty())
    {
      // initialize file output
      auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_name.string(), true);
      file_sink->set_level(spdlog::level::info);
      file_sink->set_pattern("%v");
      logger = std::make_shared<spdlog::logger>(spdlog::logger("multi_sink", { console_sink, file_sink }));
    }
    else
      logger = std::make_shared<spdlog::logger>(spdlog::logger("multi_sink", { console_sink }));

    // set default logger
    logger->set_level(spdlog::level::trace);
    spdlog::set_default_logger(logger);

    // disable end-of-line
    auto custom_formatter = std::make_unique<spdlog::pattern_formatter>("%v", spdlog::pattern_time_type::local, std::string(""));
    logger->set_formatter(std::move(custom_formatter));

    // activate flush on level: info
    logger->flush_on(spdlog::level::info);

    return true;
  }
  catch (...)
  {
    return false;
  }
}

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
    if (std::filesystem::directory_entry(dir).exists())
      dirs.push_back(dir);
  }
  return dirs;
}

class SvnRepos final
{
public:
  // constructor
  SvnRepos() :
    m_repos(),
    m_results(),
    m_progress_bar(),
    m_nb_repos(),
    m_running(false),
    m_threads(),
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
  void update(const std::vector<std::filesystem::path>& repos)
  {
    // initialize update variables
    m_repos = std::queue(std::deque(repos.begin(), repos.end()));
    m_nb_repos = m_repos.size();
    m_results.clear();

    {
      // create progress-bar
      m_progress_bar = std::make_unique<console::progress_bar>("update svn repositories: ", m_nb_repos);
      
      // start threads
      spdlog::debug(fmt::format(fmt::emphasis::bold, "launch the svn update commands on repositories:\n"));
      m_running = true;
      const std::size_t max_cpu = static_cast<std::size_t>(std::thread::hardware_concurrency());
      m_threads = std::vector<std::thread>(std::min(m_nb_repos, max_cpu));
      for (auto& t : m_threads)
        t = std::thread(&SvnRepos::run, this);

      // wait for threads completion
      for (auto& t : m_threads)
        if (t.joinable())
          t.join();
      
      // delete progress-bar
      m_progress_bar.reset();
    }

    // log updated repositories as table
    log();
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
        repo = m_repos.front().parent_path();
        m_repos.pop();
      }

      // execute the update process
      std::string logs;
      std::regex update_ok(R"(^A |^D |^U |^C |^G |E^ |R^ )");
      int exit_code = win::execute("svn.exe update --accept theirs-full", logs, repo);

      // store result and update progress-bar - protected by mutex
      {
        std::lock_guard<std::mutex> lck(m_mutex);
        if (exit_code != 0)
          m_results[repo] = false;
        else if (std::regex_search(logs, update_ok))
          m_results[repo] = true;
        m_progress_bar->tick();
      }
    }
  }

  void log()
  {
    if (!m_results.empty())
    {
       // create table stylesheet
      fort::utf8_table table;
      table.set_border_style(FT_NICE_STYLE);
      table.column(0).set_cell_text_align(fort::text_align::left);
      table.column(0).set_cell_content_text_style(fort::text_style::bold);
      table.column(1).set_cell_text_align(fort::text_align::center);
      table.column(1).set_cell_content_text_style(fort::text_style::bold);

      // create header
      table << fort::header << "PROJECTS" << "UPDATED" << fort::endr;

      // add rows
      std::size_t idx = 1;
      for (const auto& r : m_results)
      {
        table << utf8::to_utf8(r.first.string()) << (r.second ? "OK" : "KO") << fort::endr;
        table[idx++][1].set_cell_content_fg_color(r.second ? fort::color::green : fort::color::red);
      }
      spdlog::info("{}\n\n", table.to_string());
    }
    spdlog::info("total repositories updated: [{}/{}]\n", m_results.size(), m_nb_repos);
  }

private:
  // svn repos informations
  std::queue<std::filesystem::path> m_repos;
  std::map<std::filesystem::path, bool> m_results;

  // progress-bar
  std::unique_ptr<console::progress_bar> m_progress_bar;
  std::size_t m_nb_repos;

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
    // initialize logger
    if (!init_logger(log_file))
    {
      fmt::print("{} {}\n",
        fmt::format(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "error: "),
        fmt::format("can't create the log file: \"{}\"", log_file));
      return -1;
    }

    // construct the list of directories to skip
    spdlog::debug(fmt::format(fmt::emphasis::bold, "{:<45}", "get the list of directories to skip:"));
    const std::vector<std::filesystem::path>& svn_skip = extract_dirs(svn_path, svn_skip_str);
    add_tag(fmt::color::green, "OK");

    // list all svn repositories
    spdlog::debug(fmt::format(fmt::emphasis::bold, "{:<45}", "get all svn repositories:"));
    const std::vector<std::filesystem::path>& svn_repos = files::get_dirs(svn_path, std::regex(R"((\.svn$))"), svn_skip);
    add_tag(fmt::color::green, "OK");

    // update all the svn repositories
    SvnRepos svn;
    svn.update(svn_repos);
  }
  catch (const std::exception& ex)
  {
    add_tag(fmt::color::red, "KO");
    spdlog::debug("{} {}\n",
      fmt::format(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "error: "),
      ex.what());
    return -1;
  }

  return 0;
}