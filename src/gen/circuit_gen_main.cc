#include "circuit_gen_main.h"

#include "../arg_parse.h"
#include "circuit_gen_params.h"
#include "gen_color_code.h"
#include "gen_rep_code.h"
#include "gen_surface_code.h"

using namespace stim_internal;

std::map<std::string, GeneratedCircuit (*)(const CircuitGenParameters &)> code_name_to_func_map{
    {"color_code", &generate_color_code_circuit},
    {"repetition_code", &generate_rep_code_circuit},
    {"surface_code", &generate_surface_code_circuit}};
std::vector<const char *> known_commands{
    "--after_clifford_depolarization",
    "--after_reset_flip_probability",
    "--task",
    "--before_measure_flip_probability",
    "--before_round_data_depolarization",
    "--distance",
    "--gen",
    "--out",
    "--in",
    "--rounds",
};

int stim_internal::main_generate_circuit(int argc, const char **argv) {
    check_for_unknown_arguments(known_commands, "--gen", argc, argv);
    auto func = find_enum_argument("--gen", nullptr, code_name_to_func_map, argc, argv);
    CircuitGenParameters params(
        (uint64_t)find_int64_argument("--rounds", -1, 1, INT64_MAX, argc, argv),
        (uint32_t)find_int64_argument("--distance", -1, 2, 2047, argc, argv),
        require_find_argument("--task", argc, argv));
    params.before_round_data_depolarization =
        find_float_argument("--before_round_data_depolarization", 0, 0, 1, argc, argv);
    params.before_measure_flip_probability =
        find_float_argument("--before_measure_flip_probability", 0, 0, 1, argc, argv);
    params.after_reset_flip_probability = find_float_argument("--after_reset_flip_probability", 0, 0, 1, argc, argv);
    params.after_clifford_depolarization = find_float_argument("--after_clifford_depolarization", 0, 0, 1, argc, argv);
    auto out_stream = find_output_stream_argument("--out", true, argc, argv);
    std::ostream &out = out_stream.stream();
    out << "# Generated " << find_argument("--gen", argc, argv) << " circuit.\n";
    out << "# task: " << params.task << "\n";
    out << "# rounds: " << params.rounds << "\n";
    out << "# distance: " << params.distance << "\n";
    out << "# before_round_data_depolarization: " << params.before_round_data_depolarization << "\n";
    out << "# before_measure_flip_probability: " << params.before_measure_flip_probability << "\n";
    out << "# after_reset_flip_probability: " << params.after_reset_flip_probability << "\n";
    out << "# after_clifford_depolarization: " << params.after_clifford_depolarization << "\n";
    out << "# layout:\n";
    auto generated = func(params);
    out << generated.layout_str();
    out << generated.hint_str;
    out << generated.circuit;
    out << "\n";
    return 0;
}
