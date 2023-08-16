#ifndef PIECE_TABLE_H
#define PIECE_TABLE_H

#include <cassert>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

struct Piece {
    std::size_t start;
    std::size_t size;
    bool appended_sequence;
};

template <typename Iter>
struct PieceTablePosition {
    std::size_t in_piece_offset;
    Iter it;
};

template <typename Container>
concept Splicable = requires(Container c) {
    { c.splice(c.end(), c, c.begin(), c.end()) };
};

template <typename PieceSequenceT>
auto getPositionInTable(PieceSequenceT && pieces, std::size_t idx) {
    auto it = std::begin(pieces);

    for (; it != std::end(pieces); ++it) {
        if (it->size > idx) {
            break;
        }
        idx -= it->size;
    }

    return PieceTablePosition{idx, it};
}

template <
    typename OriginalBufferT,
    typename AppendBufferT,
    typename PieceSequenceT >
class PieceTable {
    static_assert(std::is_same_v<typename OriginalBufferT::value_type,
                                 typename AppendBufferT::value_type>);
    static_assert(std::is_same_v<typename PieceSequenceT::value_type,
                                 Piece>);

public:
    using value_type = OriginalBufferT::value_type;
    using size_type = std::size_t;

    using piece_sequence_index = std::conditional_t<
        Splicable<PieceSequenceT>,
        typename PieceSequenceT::iterator,
        typename PieceSequenceT::size_type>;

    struct UndoPack {
        piece_sequence_index begin;
        piece_sequence_index end;
        PieceSequenceT data;
    };

    PieceTable() = default;
    PieceTable(const PieceTable&) = default;
    PieceTable(PieceTable&&) = default;
    PieceTable& operator=(const PieceTable&) = default;
    PieceTable& operator=(PieceTable&&) = default;
    ~PieceTable() = default;

    PieceTable(OriginalBufferT original_buffer)
        : original_buffer_{original_buffer}
        , pieces_{{0, std::size(original_buffer), false}}
        , size_{std::size(original_buffer)} {}

    [[nodiscard]] bool is_empty() const {
        return pieces_.empty();
    }

    [[nodiscard]] size_type length() const {
        return size();
    }
    [[nodiscard]] size_type size() const {
        return size_;
    }

    void copy_data_to_span(std::span<value_type> out_span) const {
        assert(out_span.size() == size());
        std::size_t copied_count = 0;

        for (const Piece& b : pieces_) {
            if (b.appended_sequence) {
                const auto piece_begin = std::begin(append_buffer_) + b.start;
                const auto piece_end = piece_begin + b.size;
                std::copy(piece_begin, piece_end,
                          std::begin(out_span) + copied_count);
            } else {
                const auto piece_begin = std::begin(original_buffer_) + b.start;
                const auto piece_end = piece_begin + b.size;
                std::copy(piece_begin, piece_end,
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

    UndoPack clear() {
        size_ = 0;
        return replace_piece_range_with(std::begin(pieces_), std::end(pieces_),
                                        std::span<Piece>{});
    }

    UndoPack insert_at(size_type idx, value_type element) {
        std::basic_string_view<value_type> element_view{&element, 1};
        return insert_range_at(idx, element_view);
    }

    template<typename InputRange>
    UndoPack insert_range_at(size_type idx, InputRange&& range) {
        check_indices(idx);
        if (idx == size_) {
            return append_range(std::forward<InputRange>(range));
        }

        const auto position = getPositionInTable(pieces_, idx);
        const std::size_t appended_size = std::size(range);
        append_buffer_.insert(std::end(append_buffer_),
                              std::begin(range), std::end(range));

        Piece new_piece {
            .start = append_buffer_.size() - appended_size,
            .size = appended_size,
            .appended_sequence = true
        };

        size_ += appended_size;

        if (position.in_piece_offset == 0) {
            // insertion between pieces, no need to split
            auto it = pieces_.insert(position.it, new_piece);
            return UndoPack {
                .begin = it,
                .end = std::next(it),
                .data = {},
            };
        }
        else {
            // insertion in the middle of the piece
            // split the piece, extract the old one, add 3 new
            auto split_piece = split_piece_at(position);
            Piece inserted[3] {
                split_piece.parts[0],
                new_piece,
                split_piece.parts[1]
            };
            return replace_piece_range_with(position.it, std::next(position.it),
                                            std::span{inserted});
        }
    }

    UndoPack insert_range_at(size_type idx, const value_type* ptr) {
        return insert_range_at(idx, std::basic_string_view{ptr});
    }

    UndoPack insert_range_at(size_type idx,
                             const std::basic_string<value_type>& str) {
        return insert_range_at(idx, std::basic_string_view{str});
    }

    UndoPack append(value_type element) {
        std::basic_string_view<value_type> element_view{&element, 1};
        return append_range(element_view);
    }

    template<typename InputRange>
    UndoPack append_range(InputRange&& range) {
        const std::size_t appended_size = std::size(range);

        append_buffer_.insert(std::end(append_buffer_),
                              std::begin(range), std::end(range));
        Piece new_piece {
            .start = append_buffer_.size() - appended_size,
            .size = appended_size,
            .appended_sequence = true
        };
        auto inserted = pieces_.insert(std::end(pieces_), new_piece);

        size_ += appended_size;

        return UndoPack {
            .begin = inserted,
            .end = std::next(inserted),
            .data = {}
        };
    }

    UndoPack append_range(const value_type* ptr) {
        return append_range(std::basic_string_view{ptr});
    }

    UndoPack append_range(const std::basic_string<value_type>& str) {
        return append_range(std::basic_string_view{str});
    }

    UndoPack delete_at(size_type idx) {
        return delete_range_at(idx, 1);
    }

    UndoPack delete_range_at(size_type idx, size_type count) {
        check_indices(idx, count);
        size_ -= count;
        auto [in_piece_offset, piece_it] = getPositionInTable(pieces_, idx);

        // [ 0 ] - [ 1 ] - [ 2 ] - [ 3 ] - [ 4 ] - [ 5 ]

        Piece cut_border_pieces[2]; // intentionally uninitialized
        auto cut_begin = cut_border_pieces + 0;
        auto cut_end = cut_border_pieces + 2;

        const auto range_begin = piece_it;

        // if the range doesn't start on a piece boundary, split it
        // [ 0 ] - [ 1 ] - [ 2 ] - [ 3 ] - [ 4 ] - [ 5 ]
        //       [1a] [1b]
        //        ^ cut border piece
        if (in_piece_offset != 0) {
            cut_border_pieces[0]
                = split_piece_at({in_piece_offset, piece_it}).parts[0];

            const size_t to_piece_end = piece_it->size - in_piece_offset;

            // let's move on to the next piece *only* if the cut part goes past
            if (to_piece_end <= count) {
                count -= to_piece_end;
                in_piece_offset = 0;
                ++piece_it;
            }
        }
        else {
            // no piece to be cut from the left side
            ++cut_begin;
        }

        // go past all the pieces that are fully deleted
        while (count >= piece_it->size) {
            count -= piece_it->size;
            ++piece_it;
        }

        // if the range doesn't end on a piece boundary, split it
        // [ 0 ] - [ 1 ] - [ 2 ] - [ 3 ] - [ 4 ] - [ 5 ]
        //                               [4a] [4b]
        //                    cut border piece ^
        if (count > 0) {
            cut_border_pieces[1]
                = split_piece_at({in_piece_offset + count, piece_it}).parts[1];
            ++piece_it;
        }
        else {
            --cut_end;
        }

        const auto range_end = piece_it;

        // extract the range of modified pieces
        // [ 0 ] -snip- [ 1 ] - [ 2 ] - [ 3 ] - [ 4 ] -snip- [ 5 ]
        //    range_begin ^                          range_end ^
        // insert cut border pieces in its place
        // [ 0 ] - [ 1a ] - [ 4b ] - [ 5 ]
        // undo.begin^             ^undo.end

        return replace_piece_range_with(range_begin, range_end,
                                        std::span(cut_begin, cut_end));
    }

    UndoPack undo(UndoPack&& undo) {
        const size_type undo_length = get_part_size(std::begin(undo.data),
                                                    std::end(undo.data));
        UndoPack redo;
        if constexpr (Splicable<PieceSequenceT>) {
            // undo.begin, undo.end are persistent iterators to pieces_
            redo.data.splice(std::end(redo.data), pieces_, undo.begin, undo.end);
            // If the spliced-in part is empty, we need .begin = .end.
            // We can't just use std::begin(undo.data) - cause it would point
            // to std::end(undo.data), which as sentinel node is not sliced
            // into pieces_
            redo.begin = std::empty(undo.data) ? undo.end : std::begin(undo.data);
            pieces_.splice(undo.end, undo.data);
            redo.end = undo.end;
        }
        else {
            redo = replace_piece_range_with(redo.begin, redo.end, redo.data);
        }

        size_ = size_ + undo_length - get_part_size(std::begin(redo.data),
                                                    std::end(redo.data));
        return redo;
    }

private:
    using position_type = PieceTablePosition<typename PieceSequenceT::iterator>;

    struct SplitBlock {
        Piece parts[2];
    };

    void check_indices(size_type idx, size_type count = 0) const {
        assert(idx <= size_);
        assert(count <= size_);
        assert(idx + count <= size_);
    }

    template <typename PieceIt>
    size_type get_part_size(PieceIt begin, PieceIt end) const {
        size_type size = 0;
        for (auto it = begin; it != end; ++it) {
            size += it->size;
        }
        return size;
    }

    template <typename Range>
    UndoPack replace_piece_range_with(
            PieceSequenceT::iterator begin,
            PieceSequenceT::iterator end,
            Range&& elements) {
        UndoPack undo;

        if constexpr (Splicable<PieceSequenceT>) {
            // it's a splicable sequence (likely a list),
            // cut the piece nodes out
            undo.data.splice(undo.data.end(), pieces_, begin, end);
            // save iterators as undo/redo boundaries (they are not invalidated)
            undo.begin = pieces_.insert(end, std::begin(elements), std::end(elements));
            undo.end = end;
        }
        else {
            // it's a different kind of sequence, random access (vector/deque?)
            // let's just copy the removed piece range out
            undo.data.assign(begin, end);
            // save indices as undo/redo boundaries
            undo.begin = pieces_.erase(begin, end) - std::begin(pieces_);
            pieces_.insert(std::begin(pieces_) + undo.begin, end,
                           std::begin(elements), std::end(elements));
            undo.end = begin + std::size(elements);
        }

        return undo;
    }

    // Splits given piece into two.
    // Doesn't modify the piece chain.
    // Returns the split pair
    static SplitBlock split_piece_at(position_type pos) {
        assert(pos.in_piece_offset < pos.it->size);

        Piece left_split = {
            .start = pos.it->start,
            .size = pos.in_piece_offset,
            .appended_sequence = pos.it->appended_sequence
        };

        Piece right_split = {
            .start = pos.it->start + pos.in_piece_offset,
            .size = pos.it->size - pos.in_piece_offset,
            .appended_sequence = pos.it->appended_sequence
        };

        return {left_split, right_split};
    }

    OriginalBufferT original_buffer_;
    AppendBufferT append_buffer_;
    PieceSequenceT pieces_;
    size_type size_= 0;
};

#endif
