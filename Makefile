solve_matrix_market: solve_matrix_market.cpp
	g++ solve_matrix_market.cpp -o solve_matrix_market \
	-I/opt/intel/oneapi/mkl/latest/include \
	-L/opt/intel/oneapi/mkl/latest/lib/intel64 \
	-L/opt/intel/oneapi/compiler/latest/linux/compiler/lib/intel64_lin \
	-lmkl_intel_lp64 -lmkl_intel_thread -lmkl_core -liomp5 -lpthread -lm -ldl

.PHONY: clean

clean:
	rm -f solve_matrix_market
