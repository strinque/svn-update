#include <console/init.hpp>
#include <console/parser.hpp>

/*============================================
| Declaration
==============================================*/
// program version
const std::string PROGRAM_NAME = "xxx";
const std::string PROGRAM_VERSION = "1.0";

/*============================================
| Function definitions
==============================================*/
int main(int argc, char** argv)
{
  // initialize Windows console
  console::init();

  // parse command-line arguments
  console::parser parser(PROGRAM_NAME, PROGRAM_VERSION);
  if (!parser.parse(argc, argv))
  {
    parser.print_usage();
    return -1;
  }
  return 0;
}