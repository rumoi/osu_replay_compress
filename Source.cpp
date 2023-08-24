
#include "rrf_write.h"
#include "rrf_read.h"

#include <filesystem>

void run_test_set() {

	enum {
		group_std_game,
		group_std_screen,
		group_taiko,
		group_mania,
		group_fruits
	};

	struct _entry {

		std::string name;

		u32 new_size, old_size;

	};

	std::vector<_entry> ENTRY[5];

	for (const auto& file_entry : std::filesystem::directory_iterator("test/")) {

		const auto _p{ file_entry.path().native() };

		const auto file_name{ std::string(_p.begin(), _p.end()) };

		DIAG::OUTPUT_SIZE = 0; DIAG::INPUT_SIZE = 0;

		u32 flags{};

		flags |= (RRF_FLAG::gamemode_fruits) * (file_name.find("fruit") != std::string::npos);
		flags |= (RRF_FLAG::gamemode_mania) * (file_name.find("mania") != std::string::npos);
		flags |= (RRF_FLAG::gamemode_taiko) * (file_name.find("taiko") != std::string::npos);

		if (flags == 0 && file_name.find("osu") == std::string::npos)
			continue;

		std::string output_file{ file_name.substr(5)};
		DIAG::using_screen = 0;
		osr_to_rrf(file_name.c_str(), "", flags);

		if (DIAG::OUTPUT_SIZE == 0 || DIAG::INPUT_SIZE == 0) {
			continue;
		}

		auto index{ group_std_game };

		if (flags & RRF_FLAG::gamemode_mania)
			index = group_mania;

		if (flags & RRF_FLAG::gamemode_taiko)
			index = group_taiko;

		if (flags & RRF_FLAG::gamemode_fruits)
			index = group_fruits;

		if (DIAG::using_screen)
			index = group_std_screen;

		auto& e = ENTRY[index].emplace_back();

		e.old_size = DIAG::INPUT_SIZE;
		e.new_size = DIAG::OUTPUT_SIZE;

	}

	const std::string OUTFILE[5]{
		"res/osu-game.txt",
		"res/osu-screen.txt",
		"res/taiko.txt",
		"res/mania.txt",
		"res/fruits.txt",
	};

	for (size_t i{}; i < 5; ++i) {

		std::string o{};
		const auto& e = ENTRY[i];

		u32 T_OLD{}, T_NEW{};

		for (const auto& v : e) {
			T_OLD += v.old_size;
			T_NEW += v.new_size;
			o += std::to_string(float(v.new_size) / float(v.old_size)) + std::string(" ") + std::to_string(v.old_size) + " ";
		}
		
		write_file(OUTFILE[i].c_str(), o);
		printf("%i > %i\n", T_OLD, T_NEW);
	}


	//printf("%f percent %i bytes saved\n", 100.f * ((float)TOTAL_OUTPUT / float(TOTAL_INPUT)), TOTAL_INPUT - TOTAL_OUTPUT);

}


int main(const int param_count, char** param) {

	//run_test_set(); return 0;

	if (param_count != 3) {
	
		puts("rrf.exe {input_path} {output_path}\n");

		return 0;
	}

	if(std::string_view(param[1]).find(".osr") != std::string::npos) {

		osr_to_rrf(param[1], param[2], RRF_FLAG::has_osr_header);
		//printf("\n Final Compression Ratio: %f\n", float(DIAG::OUTPUT_SIZE) / float(DIAG::INPUT_SIZE));

	}

	if(std::string_view(param[1]).find(".rrf") != std::string::npos) {

		rrf_to_osr(param[1], param[2]);

	}

	return 1;
}