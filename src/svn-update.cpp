#include <string>
#include <filesystem>
#include <memory>
#include <vector>
#include <sstream>
#include <regex>
#include <signal.h>
#include <stdbool.h>
#include <fmt/core.h>
#include <fmt/color.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <winpp/console.hpp>
#include <winpp/parser.hpp>
#include <winpp/files.hpp>
#include "SvnRepos.h"

/*============================================
| Declaration
==============================================*/
// program version
const std::string PROGRAM_NAME = "svn-update";
const std::string PROGRAM_VERSION = "1.6";

// default length in characters to align status 
constexpr std::size_t g_status_len = 50;

// declare as global g_svn for access to exit_program signal
static SvnRepos g_svn;
static bool g_cancelled = false;

/*============================================
| Function definitions
==============================================*/
// define the function to be called when ctrl-c is sent to process
void exit_program(int signum)
{
  fmt::print("event: ctrl-c called => stopping program\n");
  g_cancelled = true;
  g_svn.stop();
}

// lambda function to show colored tags
auto add_tag = [](const fmt::color color, const std::string& text) {
  spdlog::debug(fmt::format(fmt::fg(color) | fmt::emphasis::bold, "[{}]\n", text));
};

// execute a sequence of actions with tags
void exec(const std::string& str, std::function<void()> fct)
{
  spdlog::debug(fmt::format(fmt::emphasis::bold, "{:<" + std::to_string(g_status_len) + "}", str + ": "));
  try
  {
    fct();
    add_tag(fmt::color::green, "OK");
  }
  catch (const std::exception& ex)
  {
    add_tag(fmt::color::red, "KO");
    throw ex;
  }
}

// initialize logger - spdlog
bool init_logger(const std::filesystem::path& file)
{
  try
  {
    // initialize console output
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);
    console_sink->set_pattern("%v");

    std::shared_ptr<spdlog::logger> logger;
    if (!file.empty())
    {
      // initialize file output
      auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file.string(), true);
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

int main(int argc, char** argv)
{
  // initialize Windows console
  console::init();

  // register signal handler
  signal(SIGINT, exit_program);

  // parse command-line arguments
  std::filesystem::path svn_path;
  std::vector<std::filesystem::path> svn_skip;
  std::filesystem::path log_file;
  bool interactive = false;
  console::parser parser(PROGRAM_NAME, PROGRAM_VERSION);
  parser.add("p", "path", "set the path to update the svn repositories", svn_path, true)
        .add("s", "skip", "skip the update of those directories (separated by ';')", svn_skip)
        .add("l", "log", "save the updated list of directories to a log file", log_file)
        .add("i", "interactive", "prompt the user to terminate the program", interactive);
  if (!parser.parse(argc, argv))
  {
    parser.print_usage();
    return -1;
  }

  // initialize logger
  if (!init_logger(log_file))
  {
    fmt::print("{} {}\n",
      fmt::format(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "error:"),
      fmt::format("can't create the log file: \"{}\"", log_file.u8string()));
    return -1;
  }

  int ret;
  try
  {
    // check arguments validity
    if (!std::filesystem::directory_entry(svn_path).exists())
      throw std::runtime_error(fmt::format("the directory: \"{}\" doesn't exists", svn_path.u8string()));

    // list all svn repositories
    std::vector<std::filesystem::path> svn_repos;
    exec("get all svn repositories", [&]() {
      svn_repos = files::get_dirs(svn_path,
        files::infinite_depth,
        std::regex(R"(\\.svn$)"),
        svn_skip);
      });

    // update all the svn repositories
    g_svn.update(svn_repos);
    if (g_cancelled)
      throw std::runtime_error("process has been cancelled");
    ret = 0;
  }
  catch (const std::exception& ex)
  {
    spdlog::debug("{} {}\n",
      fmt::format(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "error:"),
      ex.what());
    ret = -1;
  }

  // ask user to terminate the program
  if (interactive)
    system("pause");

  return ret;
}