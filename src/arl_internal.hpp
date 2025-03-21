#ifndef ARL_INTERNAL_HPP
#define ARL_INTERNAL_HPP

#include "arl.hpp"

#ifdef ARL_USE_GEX
#include "backend/GASNet-EX/base.hpp"
#include "backend/GASNet-EX/collective.hpp"
#include "backend/GASNet-EX/reduce.hpp"
#include <gasnetex.h>
#endif
#ifdef ARL_USE_LCI
#include "backend/LCI/base.hpp"
#include "backend/LCI/collective.hpp"
#include "backend/LCI/reduce.hpp"
#include "lci.hpp"
#include "mpi.h"
#endif

#endif//ARL_INTERNAL_HPP