#!/bin/bash

HERE="$(dirname "$(readlink -f "${0}")")"

mkdir -p ${HERE}/../build-Debug
cd ${HERE}/../build-Debug

cmake \
-DCMAKE_BUILD_TYPE=Debug \
-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
-DCMAKE_C_FLAGS="-fPIE -fPIC" \
-DCMAKE_CXX_FLAGS="-fPIE -fPIC" \
-DCMAKE_EXE_LINKER_FLAGS="-pie -Wl,--no-undefined" \
-DCMAKE_SHARED_LINKER_FLAGS="-Wl,--no-undefined" \
-DENABLE_EGL=OFF \
-DENABLE_GL=ON \
-DENABLE_GLES=OFF \
-DENABLE_VULKAN=ON \
-DENABLE_PYRENDERDOC=OFF \
-DENABLE_RENDERDOCCMD=OFF \
-DLIB_SUFFIX="64" \
-DVULKAN_LAYER_FOLDER="${HERE}/../Debug/share/vulkan/implicit_layer.d/" \
-DCMAKE_INSTALL_PREFIX="${HERE}/../Debug" \
-DINCLUDE_BIN_PATH="${HERE}/../bin/include-bin" \
${HERE}/../

#make -j12
#make install
cmake --build . --config Debug --target install -- -j12