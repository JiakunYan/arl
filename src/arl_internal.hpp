#ifndef ARL_INTERNAL_HPP
#define ARL_INTERNAL_HPP

#include "arl.hpp"

#ifdef ARL_USE_GEX
#include "gasnetex.h"
#include "gasnet_coll.h"
#include "backend/gex/base.hpp"
#endif
#ifdef ARL_USE_LCI
#include "lci.hpp"
#include "mpi.h"
#include "backend/lci/base.hpp"
#endif

#endif//ARL_INTERNAL_HPP