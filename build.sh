#cmake -S . -B build -DBUILD_TESTING=OFF #Отключение тестов
cmake -S . -B build
cmake --build build

cd build
ctest -VV