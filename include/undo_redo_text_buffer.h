#ifndef UNDO_REDO_TEXT_BUFFER_H
#define UNDO_REDO_TEXT_BUFFER_H

#include <list>
#include <vector>

#include "piece_table.h"

template <
    typename AppendBufferT,
    typename OriginalBufferT = std::basic_string<typename AppendBufferT::value_type>,
    typename PieceSequenceT = std::list<Piece>>
class UndoRedoTextBuffer {
  public:
    using value_type = CharT;
    using piece_table_t = PieceTable<OriginalBufferT,
                                     AppendBufferT,
                                     PieceSequenceT>;

    [[nodiscard]] bool is_empty() const {
        return piece_table_.empty();
    }

    [[nodiscard]] size_type length() const {
        return piece_table_.length();
    }
    [[nodiscard]] size_type size() const {
        return piece_table_.size();
    }

    [[nodiscard]] std::basic_string<value_type> to_string() const {
        return piece_table_.to_string()
    }

    void clear() {
        new_operation(piece_table_.clear());
    }

    template<typename InputRange>
    void insert_range_at(InputRange&& range) {
        new_operation(
            piece_table_.insert_range_at(std::forward<InputRange>(range)));
    }

    template<typename InputRange>
    void append_range(InputRange&& range) {
        new_operation(
            piece_table_.append_range(std::forward<InputRange>(range)));
    }

    void delete_range_at(size_type idx, size_type count) {
        new_operation(
            piece_table_.delete_range_at(idx, count));
    }

    void undo() {
        auto undo_pack = undo_stack_.back();
        undo_stack_.pop_back();
        redo_stack_.push_back(piece_table_.undo(std::move(undo_pack)));

    }
    void redo() {
        auto redo_pack = redo_stack_.back();
        redo_stack_.pop_back();
        undo_stack_.push_back(piece_table_.undo(std::move(redo_pack)));
    }

  private:
    void new_operation(UndoPack p) {
        redo_stack_.clear();
        undo_stack_.push_back(std::move(p));
    }

    std::vector<typename piece_table_t::UndoPack> undo_stack_;
    std::vector<typename piece_table_t::UndoPack> redo_stack_;
    piece_table_t piece_table_;
};

#endif  // UNDO_REDO_TEXT_BUFFER_H
