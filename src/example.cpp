#include <iostream>
#include <list>

#include "../include/piece_table.h"

int main() {
    PieceTable<std::string, std::string, std::list<Piece>> pt {
        "Original text buffer"
    };

    pt.delete_range_at(9, 5);
    std::cout << pt.to_string() << std::endl;

    pt.append_range(" is cool");
    std::cout << pt.to_string() << std::endl;

    pt.insert_range_at(pt.size() - 4, "pretty ");
    std::cout << pt.to_string() << std::endl;

    pt.insert_at(pt.size() - 1, '-');
    std::cout << pt.to_string() << std::endl;

    pt.delete_range_at(0, 15);
    std::cout << pt.to_string() << std::endl;
    pt.insert_range_at(0, "Piece table");
    std::cout << pt.to_string() << std::endl;

    pt.delete_at(pt.size() - 2);
    pt.append('!');
    std::cout << pt.to_string() << std::endl;

    auto undo_pack1 = pt.clear();
    auto undo_pack2 = pt.append_range("Hello there!");
    std::cout << pt.to_string() << std::endl;

    pt.undo(std::move(undo_pack2));
    pt.undo(std::move(undo_pack1));
    std::cout << pt.to_string() << std::endl;
}
