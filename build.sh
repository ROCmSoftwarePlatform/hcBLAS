#!/bin/bash -e
#This script is invoked to build the hcblas library and test sources

# getconf _NPROCESSORS_ONLN
working_threads=8

# CHECK FOR COMPILER PATH
if [ ! -z $HCC_HOME ];
then
  if [ -x "$HCC_HOME/bin/clang++" ];
  then
    platform="hcc"
    cmake_c_compiler="$HCC_HOME/bin/clang"
    cmake_cxx_compiler="$HCC_HOME/bin/clang++"
  fi
elif [ -x "/opt/rocm/hcc/bin/clang++" ];
then
  platform="hcc"
  cmake_c_compiler="/opt/rocm/hcc/bin/clang"
  cmake_cxx_compiler="/opt/rocm/hcc/bin/clang++"
elif [ -x "/usr/local/cuda/bin/nvcc" ];
then
  platform="nvcc"
  cmake_c_compiler="/usr/bin/gcc"
  cmake_cxx_compiler="/usr/bin/g++"
else
  echo "Neither clang  or NVCC compiler found"
  echo "Not an AMD or NVCC compatible stack"
  exit 1
fi

if ( [ ! -z $HIP_PATH ] || [ -x "/opt/rocm/hip/bin/hipcc" ] ); then 
  export HIP_SUPPORT=on
elif ( [ "$platform" = "nvcc" ]); then
  echo "HIP not found. Install latest HIP to continue."
  exit 1
fi

#CURRENT_WORK_DIRECTORY
current_work_dir=$PWD

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$current_work_dir/build/lib/src
export PROFILER_PATH=/opt/rocm/profiler/bin

red=`tput setaf 1`
green=`tput setaf 2`
reset=`tput sgr0`
copt="-O3"
verbose=""
install=0

# Help menu
print_help() {
cat <<-HELP
=============================================================================================================================
This script is invoked to build hcBLAS library and test sources. Please provide the following arguments:

  ${green}--test${reset}     Test to enable the library testing (on/basic/off). Upon basic option minimal tests get evaluated
  ${green}--profile${reset}  Profile to enable profiling of five blas kernels namely SGEMM, CGEMM, SGEMV, SGER and SAXPY (CodeXL)
  ${green}--bench${reset}    Profile benchmark using chrono timer
  ${green}--debug${reset}    Compile with debug info (-g)
  ${green}--verbose${reset}  Run make with VERBOSE=1
  ${green}--install${reset}  Install the shared library and include the header files under /opt/rocm/hcblas  Requires sudo perms.
  ${green}--examples${reset} To build and run the example files in examples folder (on/off) (ONLY SUPPORTED ON AMD PLATFORM)

NOTE: export PROFILER_PATH=/path/to/profiler before enabling profile variable.
=============================================================================================================================
HELP
exit 0
}

while [ $# -gt 0 ]; do
  case "$1" in
    --test=*)
      testing="${1#*=}"
      ;;
    --profile=*)
      profiling="${1#*=}"
      ;;
    --debug)
      copt="-g"
      ;;
    --verbose)
      verbose="VERBOSE=1"
      ;;
    --install)
      install="1"
      ;;
    --bench=*)
      benchmark="${1#*=}"
      ;;
    --examples=*)
      examples="${1#*=}"
      ;;
    --help) print_help;;
    *)
      printf "************************************************************\n"
      printf "* Error: Invalid arguments, run --help for valid arguments.*\n"
      printf "************************************************************\n"
      exit 1
  esac
  shift
done

if [ "$install" = "1" ]; then
  export INSTALL_OPT=on
fi

set +e
# MAKE BUILD DIR
mkdir -p $current_work_dir/build
mkdir -p $current_work_dir/build/packaging 
set -e

# SET BUILD DIR
build_dir=$current_work_dir/build
# change to library build
cd $build_dir

if [ "$platform" = "hcc" ]; then
  cmake -DCMAKE_C_COMPILER=$cmake_c_compiler   -DCMAKE_CXX_COMPILER=$cmake_cxx_compiler -DCMAKE_CXX_FLAGS="$copt -fPIC" -DCMAKE_INSTALL_PREFIX=/opt/rocm/hcblas $current_work_dir

  make -j$working_threads package $verbose
  make -j$working_threads $verbose

  if [ "$install" = "1" ]; then
    sudo make -j$working_threads install
  fi
  cd $build_dir/packaging/ && cmake -DCMAKE_C_COMPILER=$cmake_c_compiler -DCMAKE_CXX_COMPILER=$cmake_cxx_compiler -DCMAKE_CXX_FLAGS=-fPIC -DCMAKE_INSTALL_PREFIX=/opt/rocm/hcblas $current_work_dir/packaging/
 
  echo "${green}hcBLAS Build Completed!${reset}"
     
# test=on 
  if ( [ "$testing" = "on" ] ); then
# Build Tests
    mkdir -p $current_work_dir/build/test
    cd $build_dir/test/ && cmake -DCMAKE_C_COMPILER=$cmake_c_compiler -DCMAKE_CXX_COMPILER=$cmake_cxx_compiler -DCMAKE_CXX_FLAGS=-fPIC $current_work_dir/test/
    set +e
    make -j$working_threads
# Invoke test script 
    printf "* UNIT TESTS *\n"
    printf "**************\n"
    ${current_work_dir}/build/test/unit/bin/unittest
    printf "* UNIT-API TESTS *\n"
    printf "******************\n"
    ${current_work_dir}/build/test/unit-api/bin/unit-api-test
    if [ $HIP_SUPPORT = "on" ]; then
      printf "* UNIT-HIP TESTS *\n"
      printf "******************\n"
      ${current_work_dir}/build/test/unit-hip/bin/unit-hip-test  
    fi
  fi

#test_basic=on
  if ( [ "$testing" = "basic" ] ); then
# Build Tests
    mkdir -p $current_work_dir/build/test
    cd $build_dir/test/ && cmake -DCMAKE_C_COMPILER=$cmake_c_compiler -DCMAKE_CXX_COMPILER=$cmake_cxx_compiler -DCMAKE_CXX_FLAGS=-fPIC -DTEST_BASIC=ON $current_work_dir/test/
    set +e
    make -j$working_threads
# Invoke test script 
    printf "* UNIT TESTS *\n"
    printf "**************\n"
    ${current_work_dir}/build/test/unit/bin/unittest
    printf "* UNIT-API TESTS *\n"
    printf "******************\n"
    ${current_work_dir}/build/test/unit-api/bin/unit-api-test
    if [ $HIP_SUPPORT = "on" ]; then
      printf "* UNIT-HIP TESTS *\n"
      printf "******************\n"
      ${current_work_dir}/build/test/unit-hip/bin/unit-hip-test  
    fi
  fi



# profile=on 
  if ( [ "$profiling" = "on" ] ); then 
    printf "* PROFILING *\n"
    printf "*************\n"
    cd $current_work_dir/profile/
# Invoke profiling script
    ./runme.sh
  fi

# bench=on
  if [ "$benchmark" = "on" ]; then
    printf "* BENCHMARKING *\n"
    printf "****************\n"
    cd $current_work_dir/benchmark/BLAS_benchmark_Convolution_Networks/
    ./runme.sh
  fi

#EXAMPLES
#Invoke examples script if --examples=on
  if [ "$examples" = "on" ]; then
    printf "* EXAMPLES *\n"
    printf "************\n"
    chmod +x $current_work_dir/examples/build.sh
    cd $current_work_dir/examples/
    ./build.sh
  fi
elif [ "$platform" = "nvcc" ]; then
  cmake -DCMAKE_C_COMPILER=$cmake_c_compiler -DCMAKE_CXX_COMPILER=$cmake_cxx_compiler -DCMAKE_CXX_FLAGS="$copt -fPIC" -DCMAKE_INSTALL_PREFIX=/opt/rocm/hcblas $current_work_dir
    
  make -j$working_threads package $verbose
  make -j$working_threads $verbose
  
  if [ "$install" = "1" ]; then
    sudo make -j$working_threads install
  fi
  cd $build_dir/packaging/ && cmake -DCMAKE_C_COMPILER=$cmake_c_compiler -DCMAKE_CXX_COMPILER=$cmake_cxx_compiler -DCMAKE_CXX_FLAGS=-fPIC -DCMAKE_INSTALL_PREFIX=/opt/rocm/hcblas $current_work_dir/packaging/    
  echo "${green}hipBLAS Build Completed!${reset}"

  if  [ "$testing" = "on" ]; then
    mkdir -p $current_work_dir/build/test
    cd $build_dir/test/ && cmake -DCMAKE_C_COMPILER=$cmake_c_compiler  -DCMAKE_CXX_COMPILER=$cmake_cxx_compiler -DCMAKE_CXX_FLAGS=-fPIC $current_work_dir/test/
    set +e
    make -j$working_threads
    printf "* UNIT HIP TESTS *\n"
    printf "******************\n"
    ${current_work_dir}/build/test/unit-hip/bin/unit-hip-test
  fi 
fi
