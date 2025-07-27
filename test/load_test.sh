#!/bin/bash
cd build/pgw_server
./pgw_server &
sleep 1
cd ../pgw_client
./pgw_client -M 012345678901234 -N 1000 &
cd ../..
wait
