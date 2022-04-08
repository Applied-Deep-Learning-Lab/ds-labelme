# Change paths and rename to platform-paths

NVDS_VERSION=6.0
CUDA_VER=10.2

if [ "$1" == "libs" ]
then
# lib paths
echo "\
\
-L/usr/local/cuda-$CUDA_VER/lib64/ \
"
fi

if [ "$1" == "includes" ]
then
# include paths
echo "\
\
-I /opt/nvidia/deepstream/deepstream-$NVDS_VERSION/sources/apps/apps-common/includes \
-I /opt/nvidia/deepstream/deepstream-$NVDS_VERSION/sources/includes \
-I ds_base/ -DDS_VERSION_MINOR=0 -DDS_VERSION_MAJOR=5 \
-I /usr/local/cuda-$CUDA_VER/include \
-I /usr/include/opencv4/ \
"
fi

if [ "$1" == "apps-common" ]
then
# apps-common dir
echo "../lib/apps-common/src/"
fi

if [ "$1" == "ds-so" ]
then
# ds-so dir
echo "/opt/nvidia/deepstream/deepstream-$NVDS_VERSION/lib/"
fi
