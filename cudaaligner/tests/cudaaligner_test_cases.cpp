/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "cudaaligner_test_cases.hpp"

#include <random>
#include <claragenomics/utils/genomeutils.hpp>

namespace
{

constexpr int32_t n_random_testcases  = 10;
constexpr int32_t max_sequence_length = 5000;
constexpr uint32_t random_seed        = 5827349;

claraparabricks::genomeworks::TestCaseData generate_random_test_case(std::minstd_rand& rng)
{
    using claraparabricks::genomeworks::get_size;
    claraparabricks::genomeworks::TestCaseData t;
    std::uniform_int_distribution<int> random_length(0, max_sequence_length);
    t.target = claraparabricks::genomeworks::genomeutils::generate_random_genome(random_length(rng), rng);
    t.query  = claraparabricks::genomeworks::genomeutils::generate_random_sequence(t.target, rng, get_size(t.target), get_size(t.target), get_size(t.target));
    return t;
}
} // namespace

namespace claraparabricks
{

namespace genomeworks
{
std::vector<TestCaseData> create_cudaaligner_test_cases()
{
    std::vector<TestCaseData> tests;

    TestCaseData t;

    t.target = "AAAAAAAAAA";
    t.query  = "CGTCGTCGTC";
    tests.push_back(t);

    t.target = "AATAATAATA";
    t.query  = "CGTCGTCGTC";
    tests.push_back(t);

    t.target = "AATAATAATA";
    t.query  = "";
    tests.push_back(t);

    t.target = "";
    t.query  = "CGTCGTCGTC";
    tests.push_back(t);

    t.target = "AATAATAATA";
    t.query  = "C";
    tests.push_back(t);

    t.target = "CGTCGTCGTC";
    t.query  = "CGTCGTCGTC";
    tests.push_back(t);

    t.target = "CGTCGTCGTCCGTCGTCGTCCGTCGTCGTCGT";
    t.query  = "AGTCGTCGTCCGTAATCGTCCGTCGTCGTCGA";
    tests.push_back(t);

    t.target = "CGTCGTCGTCCGTCGTCGTCCGTCGTCGTCGTC";
    t.query  = "AGTCGTCGTCCGTAATCGTCCGTCGTCGTCGTA";
    tests.push_back(t);

    t.target = "GTCGTCGTCCGTCGTCGTCCGTCGTCGTCGTCGTCGTCGTCCGTCGTCGTCCGTCGTCGTCGTCGTCGTCGTCCGTCGTCGTCCGTCGTCGTCGTC";
    t.query  = "GTCGTCGTCCGTCGTCGTCCGTCGTCGTCGAAAACGTCGTCCGTCGTCGTCCGTCGTCGAAAACGTCGTCGTCCGTAGTCGTCCGACGTCGTCGTC";
    tests.push_back(t);

    t.target = "GTCGTCGTCCGTCGTCGTCCGTCGTCGTCGTCGTCGTCGTCCGTCGTCGTCCGTCGTCGTCGTCGTCGTCGTCCGTCGTCGTCCGTCGTCGTCGTC";
    t.query  = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    tests.push_back(t);

    std::minstd_rand rng(random_seed);
    for (int32_t i = 0; i < n_random_testcases; ++i)
        tests.push_back(generate_random_test_case(rng));
    return tests;
}
} // namespace genomeworks

} // namespace claraparabricks
