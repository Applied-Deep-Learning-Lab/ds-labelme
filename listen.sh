while true
do
    echo "Press any to continue. \"q\" for exit."
    read -n 1 action
    if [ "$action" == "q" ]
    then
        exit 0
    fi
    echo "Connecting..."
    nc -lp 776 > data/labels.json & nc -lp 777 > data/images.json
    echo "Connection closed"
done