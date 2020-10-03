cd build
cmake .. && make -j 12
cmake $@
#cmake $@ -G Ninja ..
make -j $(nproc)