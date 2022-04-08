
platform=$1
if [ -z "$platform" ]
then
    echo Доступные платформы:
    ls configs/

    echo Укажите пожалуйста платформу, если хотите создать новую, то укажите new:
    read platform   
fi

if [ "$platform" = "new" ]
then
    platform=$2

    if [ -z "$platform" ]
    then
        echo Укажите пожалуйста имя новой платформы:
        read platform
    fi

    cp -r configs/new configs/$platform
    cp platform-paths/new.sh platform-paths/$platform.sh
fi

rm -rf platform
mkdir platform
cd platform

ln -s ../configs/$platform config
ln -s ../platform-paths/$platform.sh paths.sh