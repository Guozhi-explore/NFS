for i in {0..50}
do
    ./start.sh
    ./test-lab-3-b ./yfs1 ./yfs2
    ./stop.sh
done