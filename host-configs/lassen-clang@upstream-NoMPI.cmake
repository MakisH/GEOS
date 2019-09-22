include(${CMAKE_CURRENT_LIST_DIR}/../host-configs/lassen-clang@upstream.cmake)

set(ENABLE_MPI OFF CACHE BOOL "" FORCE)
unset(MPI_ROOT CACHE )
unset(MPI_C_COMPILER     CACHE )
unset(MPI_CXX_COMPILER    CACHE )
unset(MPI_Fortran_COMPILER  CACHE )
unset(MPIEXEC                 CACHE )
unset(MPIEXEC_NUMPROC_FLAG   CACHE )
set(ENABLE_WRAP_ALL_TESTS_WITH_MPIEXEC OFF CACHE BOOL "")

set(ENABLE_PAMELA OFF CACHE BOOL "" FORCE)
set(ENABLE_PVTPackage ON CACHE BOOL "" FORCE)
set(ENABLE_GEOSX_PTP OFF CACHE BOOL "" FORCE)
set(ENABLE_PARMETIS OFF CACHE BOOL "" FORCE)
set(ENABLE_SUPERLU_DIST OFF CACHE BOOL "" FORCE)
set(CMAKE_CUDA_HOST_COMPILER ${CMAKE_CXX_COMPILER} CACHE STRING "" FORCE)
