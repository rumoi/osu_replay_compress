
#include "rrf_write.h"
#include "rrf_read.h"

int main(const int param_count, char** param) {

	if (param_count != 3) {
	
		puts("rrf.exe {input_path} {output_path}\n");
	
		printf("\n Final Compression Ratio: %f\n", float(DIAG::OUTPUT_SIZE) / float(DIAG::INPUT_SIZE));

		return 0;
	}

	if(std::string_view(param[1]).find(".osr") != std::string::npos) {

		osr_to_rrf(param[1], param[2], RRF_FLAG::has_osr_header);
		printf("\n Final Compression Ratio: %f\n", float(DIAG::OUTPUT_SIZE) / float(DIAG::INPUT_SIZE));

	}

	if(std::string_view(param[1]).find(".rrf") != std::string::npos) {

		rrf_to_osr(param[1], param[2]);

	}

	return 1;
}