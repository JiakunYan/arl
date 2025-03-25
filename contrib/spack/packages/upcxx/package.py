from spack.package import *


class Upcxx(CMakePackage):
    """UPCXX Fake Package for external library purpose"""

    homepage = "https://github.com/fake/upcxx"
    url = "https://github.com/fake/upcxx.git"
    git = "https://github.com/fake/upcxx.git"

    maintainers("JiakunYan")

    version("master", branch="master")
    version("2023.9.0", branch="fake")

    depends_on("gasnet")

    def cmake_args(self):
        return args
