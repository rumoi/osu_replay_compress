#include "rrf_write.h"

int main(const int param_count, char** param) {

	if (param_count != 3) {
	
		puts("rrf.exe {input_path} {output_path}\n");
	
		return 0;
	}

	osr_to_rrf(param[1], param[2], RRF_FLAG::has_osr_header);

	printf("\n Final Compression Ratio: %f\n", float(DIAG::OUTPUT_SIZE) / float(DIAG::INPUT_SIZE));

}