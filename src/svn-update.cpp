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
const std::string PROGRAM_VERSION = "1.2";

// declare as global g_svn for access to exit_program signal
static SvnRepos g_svn;

/*============================================
| Function definitions
==============================================*/
// define the function to be called when ctrl-c is sent to process
void exit_program(int signum)
{
  fmt::print("event: ctrl-c called => stopping program\n");
  g_svn.stop();
}

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

int main(int argc, char** argv)
{
  // initialize Windows console
  console::init();

  // register signal handler
  signal(SIGINT, exit_program);

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
    g_svn.update(svn_repos);
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