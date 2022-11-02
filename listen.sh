
l1=$1
l2=$2

if [ "$l1" = "" ]
then
    l1="7706"
fi

if [ "$l2" = "" ]
then
    l2="7707"
fi

echo "listen $l1 and $l2"

while true
do
    echo "Press any to continue. \"q\" for exit."
    read -n 1 action
    if [ "$action" == "q" ]
    then
        exit 0
    fi
    echo "Connecting..."
    nc -lp $l1 > data/labels.json & nc -lp $l2 > data/images.json
    echo "Connection closed"
done