site_name(HOST_NAME)

set(CMAKE_C_COMPILER "$ENV{CC}" CACHE PATH "" FORCE)
set(CMAKE_CXX_COMPILER "$ENV{CXX}" CACHE PATH "" FORCE)
set(ENABLE_FORTRAN OFF CACHE BOOL "" FORCE)

set(ENABLE_MPI ON CACHE PATH "" FORCE)
set(MPI_C_COMPILER "$ENV{MPICC}" CACHE PATH "" FORCE)
set(MPI_CXX_COMPILER "$ENV{MPICXX}" CACHE PATH "" FORCE)

set(MPIEXEC_EXECUTABLE "$ENV{MPIEXEC}" CACHE PATH "" FORCE)

set(ENABLE_GTEST_DEATH_TESTS ON CACHE BOOL "" FORCE)
set(ENABLE_CALIPER ON CACHE BOOL "")

# If not defined as argument, take from the environment...
if(NOT DEFINED ENABLE_HYPRE)
  set(ENABLE_HYPRE "$ENV{ENABLE_HYPRE}" CACHE BOOL "" FORCE)
endif()
# ... and then check the value.
if(ENABLE_HYPRE)
  set(GEOS_LA_INTERFACE "Hypre" CACHE STRING "" FORCE)
else()
  set(ENABLE_HYPRE OFF CACHE BOOL "" FORCE)
endif()

# Same pattern
if(NOT DEFINED ENABLE_TRILINOS)
  set(ENABLE_TRILINOS "$ENV{ENABLE_TRILINOS}" CACHE BOOL "" FORCE)
endif()
if(ENABLE_TRILINOS)
  set(GEOS_LA_INTERFACE "Trilinos" CACHE STRING "" FORCE)
else()
  set(ENABLE_TRILINOS FALSE CACHE BOOL "" FORCE)
endif()

if( (ENABLE_HYPRE AND ENABLE_TRILINOS) OR (NOT ENABLE_TRILINOS AND NOT ENABLE_HYPRE))
  MESSAGE(SEND_ERROR "Exactly one of ENABLE_HYPRE and ENABLE_TRILINOS must be defined.")
  MESSAGE(SEND_ERROR "ENABLE_HYPRE = ${ENABLE_HYPRE}.")
  MESSAGE(SEND_ERROR "ENABLE_TRILINOS = ${ENABLE_TRILINOS}.")
endif()

MESSAGE(STATUS "GEOS_LA_INTERFACE = ${GEOS_LA_INTERFACE}")

set(ENABLE_CUDA "$ENV{ENABLE_CUDA}" CACHE BOOL "" FORCE)
if(ENABLE_CUDA)

  set(CMAKE_CUDA_FLAGS "$ENV{CMAKE_CUDA_FLAGS}" CACHE STRING "" FORCE)
  if(NOT CMAKE_CUDA_FLAGS)
    set(CMAKE_CUDA_FLAGS "Unused" CACHE STRING "" FORCE)
  endif()

  set(CUDA_TOOLKIT_ROOT_DIR "$ENV{CUDA_TOOLKIT_ROOT_DIR}" CACHE PATH "" FORCE)
  if(NOT CUDA_TOOLKIT_ROOT_DIR)
    set(CUDA_TOOLKIT_ROOT_DIR "/usr/local/cuda" CACHE PATH "" FORCE)
  endif()

  set(CUDA_ARCH "$ENV{CUDA_ARCH}" CACHE STRING "" FORCE)
  if(NOT CUDA_ARCH)
    set(CUDA_ARCH "sm_70" CACHE STRING "" FORCE)
  endif()

  if(ENABLE_HYPRE)
    set(ENABLE_HYPRE_DEVICE CUDA CACHE STRING "" FORCE)
  endif()

  set(CMAKE_CUDA_FLAGS_RELEASE "-O3 -DNDEBUG -Xcompiler -DNDEBUG -Xcompiler -O3" CACHE STRING "")
  set(CMAKE_CUDA_FLAGS_RELWITHDEBINFO "-g -lineinfo ${CMAKE_CUDA_FLAGS_RELEASE}" CACHE STRING "")
  set(CMAKE_CUDA_FLAGS_DEBUG "-g -G -O0 -Xcompiler -O0" CACHE STRING "")

endif()

if(DEFINED ENV{BLAS_LIBRARIES})
  set(BLAS_LIBRARIES "$ENV{BLAS_LIBRARIES}" CACHE PATH "" FORCE)
endif()

if(DEFINED ENV{LAPACK_LIBRARIES})
  set(LAPACK_LIBRARIES "$ENV{LAPACK_LIBRARIES}" CACHE PATH "" FORCE)
endif()

set(GEOS_TPL_DIR "$ENV{GEOSX_TPL_DIR}" CACHE PATH "" FORCE)
include(${CMAKE_CURRENT_LIST_DIR}/tpls.cmake)
