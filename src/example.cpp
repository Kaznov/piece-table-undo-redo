#include <iostream>
#include <list>

#include "../include/piece_table.h"

int main() {
    PieceTable<char, std::string, std::string, std::list<PieceTableBlock>> pt {
        "Original text buffer"
    };

    pt.delete_range(9, 5);
    std::cout << pt.to_string() << std::endl;

    pt.append_range(" is cool");
    std::cout << pt.to_string() << std::endl;

    pt.insert_range_at(pt.size() - 5, "pretty ");
    std::cout << pt.to_string() << std::endl;
}
