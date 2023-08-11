#ifndef PIECE_TABLE_H
#define PIECE_TABLE_H

#include <span>
#include <string>
#include <type_traits>
#include <vector>

struct PieceTableBlock {
    std::size_t start;
    std::size_t size;
    bool appended_sequence;
};

template <typename Iter>
struct PieceTablePosition {
    std::size_t idx;
    Iter it;
};

template <typename BlockSequenceT>
PieceTablePosition<typename BlockSequenceT::iterator>
getPositionInTable(BlockSequenceT const & blocks, size_t idx) {
    auto it = std::begin(blocks);

    for (; it != std::end(blocks); ++it) {
        PieceTableBlock const& b = *it;
        if (b.size > idx) {
            return {b.start + idx, it};
        }
        idx -= b.size;
    }

    return {idx, it};
}

template <
    typename CharT,
    typename OriginalBufferT,
    typename AppendBufferT,
    typename BlockSequenceT >
class PieceTable {
    static_assert(std::is_same_v<OriginalBufferT::value_type, CharT>);
    static_assert(std::is_same_v<AppendBufferT::value_type, CharT>);
    static_assert(std::is_same_v<BlockSequenceT::value_type, CharT>);

public:
    using value_type = CharT;
    using reference = value_type&;
    using const_reference = value_type const&;
    using size_type = std::size_t;

    PieceTable() = default;
    PieceTable(const PieceTable&) = default;
    PieceTable(PieceTable&&) = default;
    PieceTable& operator=(const PieceTable&) = default;
    PieceTable& operator=(PieceTable&&) = default;
    ~PieceTable() = default;

    PieceTable(OriginalBufferT original_buffer)
        : original_buffer_{original_buffer}
        , blocks_{{0, std::size(original_buffer), false}}
        , size_{std::size(original_buffer)} {}

    [[nodiscard]] bool is_empty() const {
        return blocks_.empty();
    }

    [[nodiscard]] size_type length() const {
        return size();
    }
    [[nodiscard]] size_type size() const {
        size_;
    }

    [[nodiscard]] reference operator[](size_type idx) {
        auto position = getPositionInTable(blocks_, idx);
        if (position.it->appended_sequence) {
            return append_buffer_[position.idx];
        } else {
            return original_buffer_[position.idx];
        }
    }

    [[nodiscard]] const_reference operator[](size_type idx) const {
        auto position = getPositionInTable(blocks_, idx);
        if (position.it->appended_sequence) {
            return append_buffer_[position.idx];
        } else {
            return original_buffer_[position.idx];
        }
    }

    void copy_data_to_span(std::span<value_type> out_span) const {
        assert(out_span.size() == size());
        std::size_t copied_count = 0;

        for (PieceTableBlock const & b : blocks_) {
            if (b.appended_sequence) {
                const auto block_begin = std::begin(append_buffer_) + b.start;
                const auto block_end = block_begin + b.size;
                std::copy(block_begin, block_end,
                          std::begin(out_span) + copied_count);
            } else {
                const auto block_begin = std::begin(original_buffer_) + b.start;
                const auto block_end = block_begin + b.size;
                std::copy(block_begin, block_end,
                          std::begin(out_span) + copied_count);
            }

            copied_count += b.size;
        }
    }

    [[nodiscard]] std::basic_string<value_type> to_string() const {
        std::basic_string<value_type> result(size(), '\0');
        copy_data_to_span(std::span{result});
        return result;
    }

    [[nodiscard]] std::vector<value_type> to_vector() const {
        std::vector<value_type> result(size(), '\0');
        copy_data_to_span(std::span{result});
        return result;
    }

    void clear() {
        original_buffer_.clear();
        append_buffer_.clear();
        blocks_.clear();
        size_ = 0;
    }

    void insert_at(size_type idx, value_type element) {
        std::basic_string_view<value_type> element_view{&element, 1};
        insert_range_at(idx, element_view);
    }

    template<typename InputRange>
    void insert_range_at(size_type idx, InputRange const& range) {
        if (idx == size_) {
            append_range(range);
            return;
        }

        auto const position = getPositionInTable(blocks_, idx);
        std::size_t const appended_size = std::size(range);
        append_buffer_.insert(std::end(append_buffer_),
                              std::begin(range), std::end(range));

        PieceTableBlock new_block {
            .start = append_buffer_.size() - appended_size,
            .size = appended_size,
            .appended_sequence = true
        };

        if (position.idx == 0) {
            blocks_.insert(position.it, new_block);
        }
        else {
            auto it = split_block_at(position);
            ++it;
            blocks_.insert(it, new_block);
        }

        size_ += appended_size;
    }

    void append(value_type element) {
        std::basic_string_view<value_type> element_view{&element, 1};
        append_range(element_view);
    }

    template<typename InputRange>
    void append_range(const InputRange& range) {
        std::size_t const appended_size = std::size(range);

        append_buffer_.insert(std::end(append_buffer_), std::begin(range), std::end(range));
        PieceTableBlock new_block {
            .start = append_buffer_.size() - appended_size,
            .size = appended_size,
            .appended_sequence = true
        };
        blocks_.insert(blocks.end(), new_block);

        size_ += appended_size;
    }

    void delete_at(size_type idx) {
        delete_range(idx, 1);
    }

    void delete_range(size_type idx, size_type delete_count) {
        auto range = split_on_range_boundaries(idx, delete_count);
        blocks_.erase(range.begin, range.end);
        size_ -= delete_count;
    }

    BlockSequenceT extract_range(size_type idx, size_type extract_count) {
        auto range = split_on_range_boundaries(idx, extract_count);

        BlockSequenceT result;
        if constexpr (
            requires(result.splice(result.end(), result,
                                   result.begin(), result.end()))
        ) {
            result.splice(result.end(), blocks_,
                          range.begin, range.end);
        }
        else {
            result.insert(result.end(),
                          range.begin, range.end);
            blocks_.erase(range.begin, range.end);
        }

        size_ -= extract_count;
    }

private:
    using position_type = PieceTablePosition<typename BlockSequenceT::iterator>;
    struct block_iterator_range {
        BlockSequenceT::iterator begin;
        BlockSequenceT::iterator end;
    };

    // Splits given block into two. Returns the iterator to the first
    BlockSequenceT::iterator split_block_at(position_type pos) {
        PieceTableBlock left_split = {
            .start = pos.it->start,
            .size = pos.idx,
            .append_sequence = block_it->append_sequence
        };

        PieceTableBlock right_split = {
            .start = pos.it->start + pos.idx,
            .size = pos.it->size - pos.idx,
            .append_sequence = block_it->append_sequence
        };

        *pos.it = right_split;
        return blocks_.insert(pos.it, left_split);
    }

    block_iterator_range split_on_range_boundaries(size_t idx,
                                                   std::size_t size) {
        assert(idx + size <= size());
        auto [in_block_idx, block_it] = getPositionInTable(blocks_, idx);

        // if the range doesn't start on a block boundary, split it
        if (in_block_idx != 0) {
            block_it = split_block_at({in_block_idx, block_it});
            ++block_it;
        }

        auto range_begin = block_it;

        // go past all the blocks that are fully deleted
        while (size >= block_it->size) {
            size -= block_it->size;
            ++block_it;
        }

        // if the range doesn't end on a block boundary, split it
        if (size > 0) {
            block_it = split_block_at({size, block_it});
            ++block_it;
        }

        auto range_end = block_it;
        return {range_begin, range_end};
    }

    OriginalBufferT original_buffer_;
    AppendBufferT append_buffer_;
    BlockSequenceT blocks_;
    size_type size_= 0;
};

#endif
