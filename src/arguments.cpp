//Copyright 2017 Ryan Wick

//This file is part of LongQC

//LongQC is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//LongQC is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

//You should have received a copy of the GNU General Public License
//along with LongQC.  If not, see <http://www.gnu.org/licenses/>.


#include "arguments.h"

#include <iostream>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>

#include "args.h"


struct DoublesReader
{
    void operator()(const std::string &name, const std::string &value, double &destination) {
        try {
            if (value.find_first_not_of("0123456789.") != std::string::npos)
                throw std::invalid_argument("");
            destination = std::stod(value);
        }
        catch ( ... ) {
            std::ostringstream problem;
            problem << "Error: argument '" << name << "' received invalid value type '" << value << "'";
            throw args::ParseError(problem.str());
        }
    }
};


typedef args::ValueFlag<double, DoublesReader> d_arg;
typedef args::ValueFlag<long long> i_arg;
typedef args::ValueFlag<std::string> s_arg;


Arguments::Arguments(int argc, char **argv) {

    args::ArgumentParser parser("LongQC: a quality filtering tool for Nanopore and PacBio reads",
                                "For more information, go to: https://github.com/rrwick/LongQC");
    parser.LongSeparator(" ");

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int terminal_width = w.ws_col;

    int indent_size;
    if (terminal_width > 120)
        indent_size = 4;
    else if (terminal_width > 80)
        indent_size = 3;
    else if (terminal_width > 60)
        indent_size = 2;
    else
        indent_size = 1;

    parser.helpParams.showTerminator = false;
    parser.helpParams.progindent = 0;
    parser.helpParams.descriptionindent = 0;
    parser.helpParams.width = terminal_width;
    parser.helpParams.flagindent = indent_size;
    parser.helpParams.eachgroupindent = indent_size;

    args::Positional<std::string> input_reads_arg(parser, "input_reads",
                                      "Input long reads to be filtered");

    args::Group thresholds_group(parser, "output thresholds:");
    d_arg min_score_arg(thresholds_group, "min score",
                        "reads with a final score lower than this will be discarded",
                        {"min_score"});
    i_arg target_bases_arg(thresholds_group, "target bases",
                           "keep only the best reads up to this many total bases",
                           {"target_bases"});
    d_arg keep_percent_arg(thresholds_group, "keep percent",
                           "keep only this fraction of the best reads",
                           {"keep_percent"});

    args::Group references_group(parser, "NLexternal references "   // The NL at the start results in a newline
            "(if provided, read quality will be determined using these instead of from the Phred scores):");
    s_arg assembly_arg(references_group, "assembly",
                       "reference assembly in FASTA format",
                        {"assembly"});
    s_arg illumina_reads_1_arg(references_group, "illumina reads 1",
                               "reference Illumina reads in FASTQ format",
                               {"illumina_reads_1"});
    s_arg illumina_reads_2_arg(references_group, "illumina reads 2",
                               "reference Illumina reads in FASTQ format",
                               {"illumina_reads_2"});

    args::Group hard_cutoffs_group(parser, "NLhard cut-offs "    // The NL at the start results in a newline
                                           "(reads that fall below these thresholds are discarded):");
    i_arg min_length_arg(hard_cutoffs_group, "min length",
                         "minimum length threshold",
                         {"min_length"});
    d_arg min_mean_q_arg(hard_cutoffs_group, "min mean q",
                         "minimum mean quality threshold",
                         {"min_mean_q"});
    d_arg min_window_q_arg(hard_cutoffs_group, "min window q",
                           "minimum window quality threshold",
                           {"min_window_q"});

    args::Group score_weights_group(parser, "NLscore weights "    // The NL at the start results in a newline
                                            "(control the relative contribution of each score to the final read score):");
    d_arg length_weight_arg(score_weights_group, "length weight",
                            "weight given to the length score",
                            {"length_weight"});
    d_arg mean_q_weight_arg(score_weights_group, "mean q weight",
                            "weight given to the mean quality score",
                            {"mean_q_weight"});
    d_arg window_q_weight_arg(score_weights_group, "window q weight",
                              "weight given to the window quality score",
                              {"window_q_weight"});

    // Might need a scoring options group here as well. This could have parameters to adjust how lengths and qualities
    // are translated into scores.
    // E.g. half_length_score: reads of this length get a 50% score

    args::Group other_group(parser, "NLother:");    // The NL at the start results in a newline
    i_arg window_size_arg(other_group, "window size",
                          "size of sliding window used when measuring window quality",
                          {"window_size"});
    args::Flag verbose_arg(other_group, "verbose",
                           "Print a table with info for each read",
                           {"verbose"});
    args::Flag version_arg(other_group, "version",
                           "Display the program version and quit",
                           {"version"});

    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});


    parsing_result = GOOD;
    try {
        parser.ParseCLI(argc, argv);
    }
    catch (args::Help) {
        std::cerr << parser;
        parsing_result = HELP;
        return;
    }
    catch (args::ParseError e) {
        std::cerr << e.what() << "\n";
        parsing_result = BAD;
        return;
    }
    catch (args::ValidationError e) {
        std::cerr << e.what() << "\n";
        parsing_result = BAD;
        return;
    }
    if (argc == 1) {
        std::cerr << parser;
        parsing_result = HELP;
        return;
    }
    if (args::get(version_arg)) {
        parsing_result = VERSION;
        return;
    }

    input_reads = args::get(input_reads_arg);
    if (input_reads.empty()) {
        std::cerr << "Error: input reads are required" << "\n";
        parsing_result = BAD;
        return;
    }

    min_score_set = bool(min_score_arg);
    min_score = args::get(min_score_arg);

    target_bases_set = bool(target_bases_arg);
    target_bases = args::get(target_bases_arg);

    keep_percent_set = bool(keep_percent_arg);
    keep_percent = args::get(keep_percent_arg);

    assembly_set = bool(assembly_arg);
    assembly = args::get(assembly_arg);

    if (bool(illumina_reads_1_arg))
        illumina_reads.push_back(args::get(illumina_reads_1_arg));
    if (bool(illumina_reads_2_arg))
        illumina_reads.push_back(args::get(illumina_reads_2_arg));

    min_length_set = bool(min_length_arg);
    min_length = args::get(min_length_arg);

    min_mean_q_set = bool(min_mean_q_arg);
    min_mean_q = args::get(min_mean_q_arg);

    min_window_q_set = bool(min_window_q_arg);
    min_window_q = args::get(min_window_q_arg);

    length_weight_set = bool(length_weight_arg);
    length_weight = args::get(length_weight_arg);

    mean_q_weight_set = bool(mean_q_weight_arg);
    mean_q_weight = args::get(mean_q_weight_arg);

    window_q_weight_set = bool(window_q_weight_arg);
    window_q_weight = args::get(window_q_weight_arg);

    window_size_set = bool(window_size_arg);
    window_size = args::get(window_size_arg);

    verbose = args::get(verbose_arg);
}
