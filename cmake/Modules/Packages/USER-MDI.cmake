set(LAMMPS_LIB_MDI_BIN_DIR ${LAMMPS_LIB_BINARY_DIR}/mdi)

include(ExternalProject)
message(STATUS "Building mdi.")
ExternalProject_Add(mdi_external
    URL https://github.com/MolSSI-MDI/MDI_Library/archive/v1.1.7.tar.gz
    UPDATE_COMMAND ""
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LAMMPS_LIB_MDI_BIN_DIR}
               -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
               -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
               -DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}
               -DCMAKE_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}
               -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
               -DENABLE_OPENMP=${ENABLE_OPENMP}
               -DENABLE_XHOST=${ENABLE_XHOST}
               -DBUILD_FPIC=${BUILD_FPIC}
               -DENABLE_GENERIC=${ENABLE_GENERIC}
               -DLIBC_INTERJECT=${LIBC_INTERJECT}
               -Dlanguage=C
    CMAKE_CACHE_ARGS -DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS}
                     -DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
                     -DTargetOpenMP_FIND_COMPONENTS:STRING=C;CXX)

target_include_directories(lammps PRIVATE ${LAMMPS_LIB_MDI_BIN_DIR}/include/mdi)
target_link_directories(lammps PUBLIC ${LAMMPS_LIB_MDI_BIN_DIR}/lib/mdi)
target_link_libraries(lammps PRIVATE mdi)

add_definitions(-DLMP_USER_MDI=1)
