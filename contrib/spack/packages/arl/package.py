from spack.package import *


class Arl(CMakePackage):
    """ARL: the Asynchronous RPC Library"""

    homepage = "https://github.com/JiakunYan/arl"
    url = "https://github.com/JiakunYan/arl.git"
    git = "https://github.com/JiakunYan/arl.git"

    maintainers("JiakunYan")

    version("master", branch="master")

    variant(
        "backend",
        default="lci",
        values=("lci", "gex"),
        multi=False,
        description="Communication backend",
    )
    variant("upcxx", default=False, description="Enable UPC++ Executable")
    variant(
        "gex-module",
        default="gasnet-ibv-par",
        description="MPI module to use with GASNet-EX",
        when="backend=gex",
    )
    variant("gex-with-mpi", default=False, description="Enable MPI support in GASNet-EX")
    variant("debug", default=False, description="Enable the debug mode")
    variant("info", default=False, description="Enable the info mode")

    generator("ninja", "make", default="ninja")

    depends_on("cmake@3.13:", type="build")
    depends_on("lci")
    depends_on("gasnet", when="backend=gex")
    depends_on("mpi", when="+gex-with-mpi")
    depends_on("upcxx", when="+upcxx")

    def cmake_args(self):
        args = [
            self.define_from_variant("ARL_BACKEND", "backend"),
            self.define_from_variant("ARL_DEBUG", "debug"),
            self.define_from_variant("ARL_INFO", "info"),
            self.define_from_variant("ARL_USE_GASNET_MODULE", "gex-module"),
        ]

        if self.spec.satisfies("backend=gex"):
            args += [
                self.define_from_variant("ARL_USE_GASNET_MODULE", "gex-module"),
                self.define_from_variant("ARL_USE_GASNET_NEED_MPI", "gex-with-mpi"),
            ]

        return args
