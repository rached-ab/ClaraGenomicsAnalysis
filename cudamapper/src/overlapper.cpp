/*
* Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/
#include <cstdlib>

#include <claragenomics/cudamapper/overlapper.hpp>
#include <claragenomics/utils/cudautils.hpp>
#include <claragenomics/utils/signed_integer_utils.hpp>

#include "cudamapper_utils.hpp"

namespace
{

/// Determines whether two overlaps can be fused into a single larger overlap based on aspects of
/// their proximity to each other.
/// To be merged, overlaps must be on the same query and target and the same strand.
/// Condition one (gap_ratio_okay): The gap between the two queries and the two targets is of similar size (i.e., the two "gaps" between
// the two queries / targets are at least 80% the same size)
/// Condition two (short_gap): The two queries and two targets are within 500bp of each other.
/// Condition three (short_gap_relative_to_length): Both the query and target gaps are less than 20% the size of the total query overlap / total
/// target overlap.
bool overlaps_mergable(const claraparabricks::genomeworks::cudamapper::Overlap o1, const claraparabricks::genomeworks::cudamapper::Overlap o2)
{
    const bool relative_strands_forward = (o2.relative_strand == claraparabricks::genomeworks::cudamapper::RelativeStrand::Forward) && (o1.relative_strand == claraparabricks::genomeworks::cudamapper::RelativeStrand::Forward);
    const bool relative_strands_reverse = (o2.relative_strand == claraparabricks::genomeworks::cudamapper::RelativeStrand::Reverse) && (o1.relative_strand == claraparabricks::genomeworks::cudamapper::RelativeStrand::Reverse);

    if (!(relative_strands_forward || relative_strands_reverse))
    {
        return false;
    }

    const bool ids_match = (o1.query_read_id_ == o2.query_read_id_) && (o1.target_read_id_ == o2.target_read_id_);

    if (!ids_match)
    {
        return false;
    }

    std::int32_t query_gap = abs(o2.query_start_position_in_read_ - o1.query_end_position_in_read_);
    std::int32_t target_gap;

    // If the strands are reverse strands, the coordinates of the target strand overlaps will be decreasing
    // as those of the query increase. We therefore need to know wether this is a forward or reverse match
    // before calculating the gap between overlaps.
    if (relative_strands_reverse)
    {
        target_gap = abs(o1.target_start_position_in_read_ - o2.target_end_position_in_read_);
    }
    else
    {
        target_gap = abs(o2.target_start_position_in_read_ - o1.target_end_position_in_read_);
    }

    /// The gaps between the queries / targets are less than 500bp.
    const bool short_gap = (query_gap < 500 && target_gap < 500);
    if (short_gap)
    {
        return true;
    }

    /// The ratio of the number of basepairs in the smaller gap (i.e., distance between the two queries OR two targets)
    /// is at least 80%, indicating the gaps are of similar size.
    const float unadjusted_gap_ratio = static_cast<float>(std::min(query_gap, target_gap)) / static_cast<float>(std::max(query_gap, target_gap));
    const bool gap_ratio_ok          = (unadjusted_gap_ratio > 0.8); //TODO make these user-configurable?
    if (gap_ratio_ok)
    {
        return true;
    }

    const std::uint32_t o1_query_length  = o1.query_end_position_in_read_ - o1.query_start_position_in_read_;
    const std::uint32_t o2_query_length  = o2.query_end_position_in_read_ - o2.query_start_position_in_read_;
    const std::uint32_t o1_target_length = o1.target_end_position_in_read_ - o1.target_start_position_in_read_;
    const std::uint32_t o2_target_length = o2.target_end_position_in_read_ - o2.target_start_position_in_read_;

    const std::uint32_t total_query_length  = o1_query_length + o2_query_length;
    const std::uint32_t total_target_length = o1_target_length + o2_target_length;

    const float query_gap_length_proportion  = static_cast<float>(query_gap) / static_cast<float>(total_query_length);
    const float target_gap_length_proportion = static_cast<float>(target_gap) / static_cast<float>(total_target_length);

    /// The gaps between the queries / targets are both less than 20% of the total length of the query OR target overlaps.
    const bool short_gap_relative_to_length = (query_gap_length_proportion < 0.2 && target_gap_length_proportion < 0.2);
    return short_gap_relative_to_length;
}

// Reverse complement lookup table
static char complement_array[26] = {
    84, 66, 71, 68, 69,
    70, 67, 72, 73, 74,
    75, 76, 77, 78, 79,
    80, 81, 82, 83, 65,
    85, 86, 87, 88, 89, 90};

void reverse_complement(std::string& s, const std::size_t len)
{
    for (std::size_t i = 0; i < len / 2; ++i)
    {
        char tmp       = s[i];
        s[i]           = static_cast<char>(complement_array[static_cast<int>(s[len - 1 - i] - 65)]);
        s[len - 1 - i] = static_cast<char>(complement_array[static_cast<int>(tmp) - 65]);
    }
}

claraparabricks::genomeworks::cga_string_view_t string_view_slice(const claraparabricks::genomeworks::cga_string_view_t& s, const std::size_t start, const std::size_t end)
{
    return s.substr(start, end - start);
}

} // namespace

namespace claraparabricks
{

namespace genomeworks
{

namespace cudamapper
{

void Overlapper::post_process_overlaps(std::vector<Overlap>& overlaps, const bool drop_fused_overlaps)
{
    const auto num_overlaps = get_size(overlaps);
    bool in_fuse            = false;
    int fused_target_start;
    int fused_query_start;
    int fused_target_end;
    int fused_query_end;
    int num_residues = 0;
    Overlap prev_overlap;
    std::vector<bool> drop_overlap_mask;
    if (drop_fused_overlaps)
    {
        drop_overlap_mask.resize(overlaps.size());
    }

    for (int i = 1; i < num_overlaps; i++)
    {
        prev_overlap                  = overlaps[i - 1];
        const Overlap current_overlap = overlaps[i];
        //Check if previous overlap can be merged into the current one
        if (overlaps_mergable(prev_overlap, current_overlap))
        {
            if (drop_fused_overlaps)
            {
                drop_overlap_mask[i]     = true;
                drop_overlap_mask[i - 1] = true;
            }

            if (!in_fuse)
            { // Entering a new fuse
                num_residues      = prev_overlap.num_residues_ + current_overlap.num_residues_;
                in_fuse           = true;
                fused_query_start = prev_overlap.query_start_position_in_read_;
                fused_query_end   = current_overlap.query_end_position_in_read_;

                // If the relative strands are forward, then the target positions are increasing.
                // However, if the relative strands are in the reverse direction, the target
                // positions along the read are decreasing. When fusing, this needs to be accounted for
                // by the following checks.
                if (current_overlap.relative_strand == RelativeStrand::Forward)
                {
                    fused_target_start = prev_overlap.target_start_position_in_read_;
                    fused_target_end   = current_overlap.target_end_position_in_read_;
                }
                else
                {
                    fused_target_start = current_overlap.target_start_position_in_read_;
                    fused_target_end   = prev_overlap.target_end_position_in_read_;
                }
            }
            else
            {
                // Continuing a fuse, query end is always incremented, however whether we increment the target start or
                // end depends on whether the overlap is a reverse or forward strand overlap.
                num_residues += current_overlap.num_residues_;
                fused_query_end = current_overlap.query_end_position_in_read_;
                // Query end has been incrememnted. Increment target end or start
                // depending on whether the overlaps are reverse or forward matching.
                if (current_overlap.relative_strand == RelativeStrand::Forward)
                {
                    fused_target_end = current_overlap.target_end_position_in_read_;
                }
                else
                {
                    fused_target_start = current_overlap.target_start_position_in_read_;
                }
            }
        }
        else
        {
            if (in_fuse)
            { //Terminate the previous overlap fusion
                in_fuse                                      = false;
                Overlap fused_overlap                        = prev_overlap;
                fused_overlap.query_start_position_in_read_  = fused_query_start;
                fused_overlap.target_start_position_in_read_ = fused_target_start;
                fused_overlap.query_end_position_in_read_    = fused_query_end;
                fused_overlap.target_end_position_in_read_   = fused_target_end;
                fused_overlap.num_residues_                  = num_residues;
                overlaps.push_back(fused_overlap);
                num_residues = 0;
            }
        }
    }
    //Loop terminates in the middle of an overlap fuse - fuse the overlaps.
    if (in_fuse)
    {
        Overlap fused_overlap                        = prev_overlap;
        fused_overlap.query_start_position_in_read_  = fused_query_start;
        fused_overlap.target_start_position_in_read_ = fused_target_start;
        fused_overlap.query_end_position_in_read_    = fused_query_end;
        fused_overlap.target_end_position_in_read_   = fused_target_end;
        fused_overlap.num_residues_                  = num_residues;
        overlaps.push_back(fused_overlap);
    }

    if (drop_fused_overlaps)
    {
        details::overlapper::drop_overlaps_by_mask(overlaps, drop_overlap_mask);
    }
}

namespace details
{
namespace overlapper
{
void drop_overlaps_by_mask(std::vector<claraparabricks::genomeworks::cudamapper::Overlap>& overlaps, const std::vector<bool>& mask)
{
    std::size_t i                                                               = 0;
    std::vector<claraparabricks::genomeworks::cudamapper::Overlap>::iterator it = overlaps.begin();
    while (it != end(overlaps) && i < mask.size())
    {
        if (mask[i])
        {
            it = overlaps.erase(it);
        }
        else
        {
            ++it;
        }
        ++i;
    }
}
} // namespace overlapper
} // namespace details

void details::overlapper::extend_overlap_by_sequence_similarity(Overlap& overlap,
                                                                cga_string_view_t& query_sequence,
                                                                cga_string_view_t& target_sequence,
                                                                const std::int32_t extension,
                                                                const float required_similarity)
{

    const position_in_read_t query_head_rescue_size  = std::min(overlap.query_start_position_in_read_, static_cast<position_in_read_t>(extension));
    const position_in_read_t target_head_rescue_size = std::min(overlap.target_start_position_in_read_, static_cast<position_in_read_t>(extension));
    // Calculate the shortest sequence length and use this as the window for comparison.
    const position_in_read_t head_rescue_size = std::min(query_head_rescue_size, target_head_rescue_size);

    const position_in_read_t query_head_start  = overlap.query_start_position_in_read_ - head_rescue_size;
    const position_in_read_t target_head_start = overlap.target_start_position_in_read_ - head_rescue_size;

    cga_string_view_t query_head_sequence  = string_view_slice(query_sequence, query_head_start, overlap.query_start_position_in_read_);
    cga_string_view_t target_head_sequence = string_view_slice(target_sequence, target_head_start, overlap.target_start_position_in_read_);

    float head_similarity = sequence_jaccard_similarity(query_head_sequence, target_head_sequence, 15, 1);
    if (head_similarity >= required_similarity)
    {
        overlap.query_start_position_in_read_  = overlap.query_start_position_in_read_ - head_rescue_size;
        overlap.target_start_position_in_read_ = overlap.target_start_position_in_read_ - head_rescue_size;
    }

    const position_in_read_t query_tail_rescue_size  = std::min(static_cast<position_in_read_t>(extension), static_cast<position_in_read_t>(query_sequence.length()) - overlap.query_end_position_in_read_);
    const position_in_read_t target_tail_rescue_size = std::min(static_cast<position_in_read_t>(extension), static_cast<position_in_read_t>(target_sequence.length()) - overlap.target_end_position_in_read_);
    // Calculate the shortest sequence length at the tail and use this as the window for comparison.
    const position_in_read_t tail_rescue_size = std::min(query_tail_rescue_size, target_tail_rescue_size);

    cga_string_view_t query_tail_sequence  = string_view_slice(query_sequence, overlap.query_end_position_in_read_, overlap.query_end_position_in_read_ + tail_rescue_size);
    cga_string_view_t target_tail_sequence = string_view_slice(target_sequence, overlap.target_end_position_in_read_, overlap.target_end_position_in_read_ + tail_rescue_size);

    const float tail_similarity = sequence_jaccard_similarity(query_tail_sequence, target_tail_sequence, 15, 1);
    if (tail_similarity >= required_similarity)
    {
        overlap.query_end_position_in_read_  = overlap.query_end_position_in_read_ + tail_rescue_size;
        overlap.target_end_position_in_read_ = overlap.target_end_position_in_read_ + tail_rescue_size;
    }
}

void Overlapper::rescue_overlap_ends(std::vector<Overlap>& overlaps,
                                     const io::FastaParser& query_parser,
                                     const io::FastaParser& target_parser,
                                     const std::int32_t extension,
                                     const float required_similarity)
{

    auto reverse_overlap = [](cudamapper::Overlap& overlap, std::uint32_t target_sequence_length) {
        overlap.relative_strand      = overlap.relative_strand == RelativeStrand::Forward ? RelativeStrand::Reverse : RelativeStrand::Forward;
        position_in_read_t start_tmp = overlap.target_start_position_in_read_;
        // Oddly, the target_length_ field appears to be zero up till this point, so use the sequence's length instead.
        overlap.target_start_position_in_read_ = target_sequence_length - overlap.target_end_position_in_read_;
        overlap.target_end_position_in_read_   = target_sequence_length - start_tmp;
    };

    // Loop over all overlaps
    // For each overlap, retrieve the read sequence and
    // check the similarity of the overlapping head and tail sections (matched for length)
    // If they are more than or equal to <required_similarity> similar, extend the overlap start/end fields by <extension> basepairs.

    for (auto& overlap : overlaps)
    {
        // Track whether the overlap needs to be reversed from its original orientation on the '-' strand.
        bool reversed = false;

        // Overlap rescue at "head" (i.e., "left-side") of overlap
        // Get the sequences of the query and target
        const std::string query_sequence = query_parser.get_sequence_by_id(overlap.query_read_id_).seq;
        cga_string_view_t query_view(query_sequence);
        // target_sequence is non-const as it may be modified when reversing an overlap.
        std::string target_sequence = target_parser.get_sequence_by_id(overlap.target_read_id_).seq;

        if (overlap.relative_strand == RelativeStrand::Reverse)
        {

            reverse_overlap(overlap, static_cast<uint32_t>(target_sequence.length()));
            reverse_complement(target_sequence, target_sequence.length());
            reversed = true;
        }
        cga_string_view_t target_view(target_sequence);

        const std::size_t max_rescue_rounds  = 3;
        std::size_t rescue_rounds            = 0;
        position_in_read_t prev_query_start  = overlap.query_start_position_in_read_;
        position_in_read_t prev_query_end    = overlap.query_end_position_in_read_;
        position_in_read_t prev_target_start = overlap.target_start_position_in_read_;
        position_in_read_t prev_target_end   = overlap.target_end_position_in_read_;

        while (rescue_rounds < max_rescue_rounds)
        {
            details::overlapper::extend_overlap_by_sequence_similarity(overlap, query_view, target_view, 100, 0.9);
            ++rescue_rounds;
            if (overlap.query_end_position_in_read_ == prev_query_start &&
                overlap.query_end_position_in_read_ == prev_query_end &&
                overlap.target_start_position_in_read_ == prev_target_start &&
                overlap.target_end_position_in_read_ == prev_target_end)
            {
                break;
            }
            prev_query_start  = overlap.query_start_position_in_read_;
            prev_query_end    = overlap.query_end_position_in_read_;
            prev_target_start = overlap.target_start_position_in_read_;
            prev_target_end   = overlap.target_end_position_in_read_;
        }

        if (reversed)
        {
            reverse_overlap(overlap, static_cast<uint32_t>(target_sequence.length()));
        }
    }
}

} // namespace cudamapper

} // namespace genomeworks

} // namespace claraparabricks
