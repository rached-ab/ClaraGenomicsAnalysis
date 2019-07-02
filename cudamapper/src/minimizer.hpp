/*
* Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include <cstdint>
#include "cudamapper/sketch_element.hpp"

namespace claragenomics {

    /// Minimizer - represents one occurrance of a minimizer
    class Minimizer : public SketchElement {
    public:
        /// \brief constructor
        ///
        /// \param representation 4-bit packed representation of a kmer
        /// \param position position of the minimizer in the sequence
        /// \param direction in which the sequence was read (forward or reverse complimet)
        /// \param sequence_id sequence's id
        Minimizer(std::uint64_t representation, std::size_t position, DirectionOfRepresentation direction, std::uint64_t sequence_id);

        /// \brief representation and its direction
        struct RepresentationAndDirection {
            std::uint64_t representation_;
            DirectionOfRepresentation direction_;
        };

        /// \brief returns minimizers representation
        /// \return minimizer representation
        std::uint64_t representation() const override;

        /// \brief returns position of the minimizer in the sequence
        /// \return position of the minimizer in the sequence
        std::size_t position() const override;

        /// \brief returns representation's direction
        /// \return representation's direction
        DirectionOfRepresentation direction() const override;

        /// \brief returns sequence's ID
        /// \return sequence's ID
        std::uint64_t sequence_id() const override;

        /// \brief converts a kmer of length length into 4-bit packed numeric representation
        ///
        /// Representation uses lexicographical ordering. It returns the smaller of forward and reverse complement representation
        ///
        /// \param baseparis
        /// \param start_element where in basepairs the kmer actually starts
        /// \param length length of the kmer
        ///
        /// \return representation and direction of the sequence
        static RepresentationAndDirection kmer_to_integer_representation(const std::string& basepairs, std::size_t start_element, std::size_t length);

    private:
        std::uint64_t representation_; // supports up to 2*64 basepairs in a minimzer. Normaly minimizers of around 20 elements are used
        std::size_t position_;
        DirectionOfRepresentation direction_;
        std::uint64_t sequence_id_;
    };

}
