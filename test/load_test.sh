cd build/pgw_server
xterm -e ./pgw_server &
sleep 1
cd ..
cd pgw_client
xterm -e ./pgw_client -M 012345678901234 -N 1000 &
cd ../..
