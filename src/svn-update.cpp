#include <string>
#include <sstream>
#include <vector>
#include <filesystem>
#include <console/init.hpp>
#include <console/parser.hpp>

/*============================================
| Declaration
==============================================*/
// program version
const std::string PROGRAM_NAME = "svn-update";
const std::string PROGRAM_VERSION = "1.0";

/*============================================
| Function definitions
==============================================*/
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

  return 0;
}