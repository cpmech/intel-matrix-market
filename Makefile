solve_matrix_market: solve_matrix_market.cpp
	g++ -g solve_matrix_market.cpp -o solve_matrix_market -fPIC \
	-I/opt/intel/oneapi/mkl/latest/include \
	-L/opt/intel/oneapi/mkl/latest/lib/intel64 \
	-L/opt/intel/oneapi/compiler/latest/linux/compiler/lib/intel64_lin \
	-Wl,-rpath,/opt/intel/oneapi/mkl/2023.2.0/lib/intel64:/opt/intel/oneapi/compiler/latest/linux/compiler/lib/intel64_lin \
	-Wl,-rpath=/opt/intel/oneapi/mkl/2023.2.0/lib/intel64 \
	/opt/intel/oneapi/mkl/2023.2.0/lib/intel64/libmkl_intel_ilp64.so \
	/opt/intel/oneapi/mkl/2023.2.0/lib/intel64/libmkl_intel_thread.so \
	/opt/intel/oneapi/mkl/2023.2.0/lib/intel64/libmkl_core.so \
	/opt/intel/oneapi/compiler/latest/linux/compiler/lib/intel64_lin/libiomp5.so \
	-lm -ldl -lpthread 

memcheck: solve_matrix_market
	valgrind --leak-check=full --show-leak-kinds=all --suppressions=valgrind.supp ./solve_matrix_market

.PHONY: clean

clean:
	rm -f solve_matrix_market
