while true
do
    echo "Press any to continue. \"q\" for exit."
    read -n 1 action
    if [ "$action" == "q" ]
    then
        exit 0
    fi
    echo "Connecting..."
    nc -lp 7706 > data/labels.json & nc -lp 7707 > data/images.json
    echo "Connection closed"
done