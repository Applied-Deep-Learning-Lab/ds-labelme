
platform=$1
while [ -z "$platform" ]
do
    echo Укажите пожалуйста платформу:
    read platform   
done

rm -rf platform
mkdir platform

cd platform
cp -r ../configs/base/ config
cp ../platform-paths/$platform.sh paths.sh
cd ../

mkdir lib/
cp -r /opt/nvidia/deepstream/deepstream/sources/includes lib/
cp -r /opt/nvidia/deepstream/deepstream/sources/apps/apps-common lib/