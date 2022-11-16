set -x

SOURCE_DIR=`pwd`
BUILD_DIR=${BUILD_DIR:-../build}
BUILD_TYPE=$1
eval BUILD_TYPE=${BUILD_TYPE:-Debug}

mkdir -p $BUILD_DIR/$BUILD_TYPE \
    && cd $BUILD_DIR/$BUILD_TYPE \
    && cmake $SOURCE_DIR -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    && make VERBOSE=1