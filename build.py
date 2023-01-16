import sys, os, logging, git
from build_tools import Builder, futil, GitProgress
from build_tools import Green, Red, Blue, Grey, Yellow, White
from build_tools import log_init, debug, warning, info, error, status, status_ok, status_ko

# specific builder properties
class CustomBuilder(Builder):
    def __init__(self, dst_dir):
        super().__init__(dst_dir)

if __name__ == '__main__':
    try:
        # create builder with specific output directory
        builder = CustomBuilder('x64-windows')

        # execute build script with command-line args
        builder.execute(sys.argv)
    except Exception as err:
        error(Red('error: ') + repr(err))
        sys.exit(-1)