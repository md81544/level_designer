#!/bin/bash

mkdir -p build
BUILD_TYPE=debug

while [[ "$1" != "" ]]; do
    case "$1" in
        "debug" )
            BUILD_TYPE=debug
            ;;
        "clean" )
            rm -rf build/*
            exit 0
            ;;
        "release" )
            BUILD_TYPE=release
            ;;
        *)
            echo "Unrecognized option $1"
            exit 2
            ;;
    esac
    shift
done

if [[ "$1" != "" ]]; then
    BUILD_TYPE=${1}
fi

if [[ "${BUILD_TYPE}" == "release" ]]; then
    if [[ -f "build/DEBUG" ]]; then
        rm -rf build/*
    fi
    touch build/RELEASE
fi
if [[ "${BUILD_TYPE}" == "debug" ]]; then
    if [[ -f "build/RELEASE" ]]; then
        rm -rf build/*
    fi
    touch build/DEBUG
fi

cd build
cmake ..
make ${BUILD_TYPE}
if [[ $? -ne 0 ]]; then
    exit 1
fi
cd ..
rm -f level_designer
ln -s build/level_designer
