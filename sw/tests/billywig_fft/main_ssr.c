#include "billywig_runtime.h"

static double *fft_inner(uint32_t N, double *x, double *y, double *twiddle, int par) {
	uint32_t core_id = pulp_get_core_id();
	uint32_t core_num = pulp_get_core_count();

	// Start of SSR region.
	register volatile double ft0 asm ("ft0");
	register volatile double ft1 asm ("ft1");
	asm volatile ("" : "=f"(ft0), "=f"(ft1));

	for (uint32_t n = N, s = 1; n > 1; n /= 2, s *= 2) {
		uint32_t j0 = 0, js = 1, j1 = s;
		uint32_t i0 = 0, is = 1, i1 = n/2;
		if (par) {
			if (s < core_num) {
				i0 = core_id;
				is = core_num;
				i1 = n/2 / core_num;
			} else {
				j0 = core_id;
				js = core_num;
				j1 = s / core_num;
			}
		}

		pulp_ssr_loop_4d(SSR_DM0, 2, 2, i1, j1, 8,    -8*N, 8*is*s*2, 8*js*2);
		ssr_config_reg[SSR_DM0].repeat.value = 1;
		pulp_ssr_loop_4d(SSR_DM1, 2, 2, i1, j1, 8*s*2, 8,   8*is*s*4, 8*js*2);
		pulp_ssr_enable();

		double *x_prime = &x[i0*s*2+j0*2+N];
		double *y_prime = &y[i0*s*4+j0*2];

		ssr_config_reg[SSR_DM0].rptr[SSR_4D].value = (uint32_t)x_prime;
		ssr_config_reg[SSR_DM1].wptr[SSR_4D].value = (uint32_t)y_prime;

		for (uint32_t j = 0; j < j1; ++j) {
			// asm volatile ("loop_j_start:");
			double tw_re = twiddle[(j*js+j0)*n+0];
			double tw_im = twiddle[(j*js+j0)*n+1];
			for (uint32_t i = 0; i < i1; ++i) {
				// asm volatile ("loop_i_start:");
				asm volatile (
					"fmul.d    ft2, %[tw_re], ft0 \n"
					"fmul.d    ft3, %[tw_im], ft0 \n"
					"fnmsub.d  ft2, %[tw_im], ft0, ft2 \n"
					"fmadd.d   ft3, %[tw_re], ft0, ft3 \n"
					"fadd.d    ft1, ft0, ft2 \n"
					"fsub.d    ft1, ft0, ft2 \n"
					"fadd.d    ft1, ft0, ft3 \n"
					"fsub.d    ft1, ft0, ft3 \n"
					:: [tw_re]"f"(tw_re), [tw_im]"f"(tw_im) : "ft0", "ft1", "ft2", "ft3"
				);
				// asm volatile ("loop_i_end:");
			}
			// asm volatile ("loop_j_end:");
		}

		// Synchronize and swap buffers.
		double *tmp = x;
		x = y;
		y = tmp;
		fpu_fence();
		if (par) pulp_barrier();
	}

	// End of SSR region.
	asm volatile ("" :: "f"(ft0), "f"(ft1));
	pulp_ssr_disable();

	return x;
}

#include "main.c"
