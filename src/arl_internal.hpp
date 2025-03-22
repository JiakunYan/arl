#ifndef ARL_INTERNAL_HPP
#define ARL_INTERNAL_HPP

#include "arl.hpp"

#ifdef ARL_USE_GEX
#include <gasnetex.h>
#include "backend/GASNet-EX/base.hpp"
#include "backend/GASNet-EX/collective.hpp"
#include "backend/GASNet-EX/reduce.hpp"
#endif
#ifdef ARL_USE_LCI
#include "lci.hpp"
#include "mpi.h"
#include "backend/lci/base.hpp"
#endif

#endif//ARL_INTERNAL_HPP