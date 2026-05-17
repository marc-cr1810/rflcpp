#include <iostream>
#include <iomanip>
#include <rflcpp/msgpack.hpp>
#include <rflcpp/cbor.hpp>

using namespace rflcpp;

struct Point {
    double x, y;
    std::string label;
};

void print_hex(std::span<const uint8_t> data) {
    for (auto b : data) std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
    std::cout << std::dec << std::endl;
}

int main() {
    Point p{1.2, 3.4, "center"};

#ifdef RFLCPP_ENABLE_MSGPACK
    std::cout << "=== MessagePack ===" << std::endl;
    auto mp = to_msgpack(p);
    print_hex(mp);
    auto p2 = from_msgpack<Point>(mp);
    if (p2) std::cout << "Roundtrip OK: " << p2->label << " (" << p2->x << ", " << p2->y << ")" << std::endl;
    else std::cout << "Error: " << p2.error().message() << std::endl;
#endif

#ifdef RFLCPP_ENABLE_JSON
    std::cout << "\n=== CBOR ===" << std::endl;
    auto cb = to_cbor(p);
    print_hex(cb);
    auto p3 = from_cbor<Point>(cb);
    if (p3) std::cout << "Roundtrip OK: " << p3->label << " (" << p3->x << ", " << p3->y << ")" << std::endl;
    else std::cout << "Error: " << p3.error().message() << std::endl;
#endif

    return 0;
}
