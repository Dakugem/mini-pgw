#cmake -S . -B build -DBUILD_TESTING=OFF #Отключение тестов
cmake -S . -B build -DENABLE_COVERAGE=ON
cmake --build build

#cd build/pgw_server
#xterm -hold -e ./pgw_server &
#sleep 1
#cd ..
#cd pgw_client
#xterm -hold -e ./pgw_client -M 012345678901234 -N 1000 &
#cd ../..

cd build
ctest -VV


