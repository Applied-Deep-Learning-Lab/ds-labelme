# Change paths and rename to platform-paths

NVDS_VERSION=6.0
CUDA_VER=11.4

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
-I ../lib/apps-common/includes \
-I ../lib/includes \
-I ds_base/ -DDS_VERSION_MINOR=0 -DDS_VERSION_MAJOR=5 \
-I /usr/local/cuda-$CUDA_VER/include \
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
