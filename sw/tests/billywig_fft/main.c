// To be included by specific main_*.c files.

extern uint32_t input_size;
extern double input[];
extern double input_twiddle[];
extern double output[];
// extern double output_checksum[4];

static void populate(double *ptr, uint32_t size, uint32_t seed) {
    for (uint32_t i = 0; i < size; i++) {
        *ptr = (double)seed * 3.141;
        ++ptr;
        ++seed;
    }
}

int main(uint32_t core_id, uint32_t core_num) {
    double *ptr = (void*)&l1_alloc_base;
    // double *input = ptr; ptr += input_size*2;
    // double *input_twiddle = ptr; ptr += input_size;
	double *buffer = ptr; ptr += input_size*2;
    // if (core_id == 0) populate(input, input_size*2, 1);
    // if (core_id == 1) populate(input_twiddle, input_size, 2);
    pulp_barrier();
	double *y = fft_inner(input_size, input, buffer, input_twiddle, 1);
	if (core_id == 0) {
		uint32_t diffs = 0;
		// for (uint32_t n = 0; n < 4; n++) {
		// 	double sum = 0.0;
		// 	for (uint32_t i = n; i < input_size*2; i += 4) {
		// 		sum += y[i];
		// 	}
		// 	double d = sum - output_checksum[n];
		// 	if (d < 0)
		// 		d = -d;
		// 	diffs += d > 0.001;
		// }
		for (uint32_t i = 0; i < input_size*2; i++) {
			double d = y[i] - output[i];
			if (d < 0)
				d = -d;
			diffs += d > 0.01;
		}
		return diffs;
	}
	return 0;
}
