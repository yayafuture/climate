file(GLOB climate_source "*.cpp" "*.h")

#create excutable
add_executable(climate ${climate_source})

#this project needs fftw3 package
find_package (FFTW3)

if (FFTW3_FOUND)
    MESSAGE("-- Found FFTW3: " ${FFTW3_LIBRARY})
else ()
    MESSAGE(FATAL_ERROR "FFTW3 cannot be found. Be sure to set FFTW3_DIR.")
endif()

find_package (HDF5)
#enable hdf5 library for big data
if (HDF5_FOUND)
    MESSAGE("-- Found HDF5: ${HDF5_LIBRARIES}")
    list(APPEND external_include_dir ${HDF5_INCLUDE_DIRS})
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__HDF5__")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__HDF5__")
else ()
    MESSAGE(WARNING "Could NOT find HDF5")
endif()


#end find hdf5 library

if (FFTW3_FOUND)
        list( APPEND external_include_dir ${FFTW3_INCLUDE_DIR})
        list( APPEND external_lib_dir ${FFTW3_LIBRARY})
endif()

#include directories
target_include_directories(climate PUBLIC
				   ${solver_include_dir}
				   ${frontier_include_dir}	
				   ${ifluid_include_dir}
				   ${external_include_dir})

#link libraries
target_link_libraries(climate frontier solver ifluid ${FFTW3_LIBRARY} ${MPI_C_LIBRARIES})

