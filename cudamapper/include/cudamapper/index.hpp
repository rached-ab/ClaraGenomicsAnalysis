#pragma  once

#include <cstdint>
#include <memory>

namespace genomeworks {
    /// Index - generates and manages (k,w)-minimizer index for one or more sequences
    class Index {
    public:
        /// \brief generates an in-memory minimizer index
        ///
        /// Given one or more sequences generates an-in memory (k,w)-minimizer index
        /// \param query_filename filepath to reads in FASTA or FASTQ format
        virtual void generate_index(const std::string &query_filename) = 0;

        /// create_index - return an Index object
        ///
        /// \param minimizer_size
        /// \param window_size
        ///
        /// \return Index implementation, generates minimizers indices
        static std::unique_ptr<Index> create_index(std::uint64_t minimizer_size, std::uint64_t window_size);
    };
}
