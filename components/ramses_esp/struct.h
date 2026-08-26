#pragma once

#include <cstdint>
#include <tuple>
#include <optional>
#include <algorithm>
#include <array>
#include <span>
#include <type_traits>

namespace esphome {
namespace ramses_esp {

// 1. Fixed-string container usable as a C++20 NTTP
template <size_t N>
struct FormatString {
  char value[N];
  constexpr FormatString(const char (&str)[N]) {
    std::copy_n(str, N, value);
  }
  constexpr size_t size() const { return N - 1; }
};

// 2. Field Descriptors computed at compile time
enum class Endianness { BIG, LITTLE, NATIVE };
enum class TypeCode { U8, I8, BE_U16, LE_U16, BE_I16, LE_I16, BE_U32, LE_U32, BE_I32, LE_I32, PAD };

template <TypeCode Code>
struct FieldTypeMap;

template <> struct FieldTypeMap<TypeCode::U8>     { using type = uint8_t;  static constexpr size_t size = 1; };
template <> struct FieldTypeMap<TypeCode::I8>     { using type = int8_t;   static constexpr size_t size = 1; };
template <> struct FieldTypeMap<TypeCode::BE_U16> { using type = uint16_t; static constexpr size_t size = 2; };
template <> struct FieldTypeMap<TypeCode::LE_U16> { using type = uint16_t; static constexpr size_t size = 2; };
template <> struct FieldTypeMap<TypeCode::BE_I16> { using type = int16_t;  static constexpr size_t size = 2; };
template <> struct FieldTypeMap<TypeCode::LE_I16> { using type = int16_t;  static constexpr size_t size = 2; };
template <> struct FieldTypeMap<TypeCode::BE_U32> { using type = uint32_t; static constexpr size_t size = 4; };
template <> struct FieldTypeMap<TypeCode::LE_U32> { using type = uint32_t; static constexpr size_t size = 4; };
template <> struct FieldTypeMap<TypeCode::BE_I32> { using type = int32_t;  static constexpr size_t size = 4; };
template <> struct FieldTypeMap<TypeCode::LE_I32> { using type = int32_t;  static constexpr size_t size = 4; };
template <> struct FieldTypeMap<TypeCode::PAD>    { using type = void;     static constexpr size_t size = 1; };

// 3. Compile-time parser & code generator
template <FormatString Fmt>
class Struct {
 private:
  static consteval auto parse_schema() {
    std::array<TypeCode, Fmt.size()> codes{};
    size_t count = 0;
    Endianness endian = Endianness::BIG; // Default to Network/Big Endian

    for (size_t i = 0; i < Fmt.size(); ++i) {
      char c = Fmt.value[i];
      if (c == '>' || c == '!') { endian = Endianness::BIG; continue; }
      if (c == '<') { endian = Endianness::LITTLE; continue; }

      switch (c) {
        case 'B': codes[count++] = TypeCode::U8; break;
        case 'b': codes[count++] = TypeCode::I8; break;
        case 'H': codes[count++] = (endian == Endianness::BIG) ? TypeCode::BE_U16 : TypeCode::LE_U16; break;
        case 'h': codes[count++] = (endian == Endianness::BIG) ? TypeCode::BE_I16 : TypeCode::LE_I16; break;
        case 'I': codes[count++] = (endian == Endianness::BIG) ? TypeCode::BE_U32 : TypeCode::LE_U32; break;
        case 'i': codes[count++] = (endian == Endianness::BIG) ? TypeCode::BE_I32 : TypeCode::LE_I32; break;
        case 'x': codes[count++] = TypeCode::PAD; break;
        default:  throw "Unsupported format specifier character in format string";
      }
    }
    return std::pair{codes, count};
  }

  static constexpr auto schema = parse_schema();
  static constexpr auto codes = schema.first;
  static constexpr size_t field_count = schema.second;

  template <size_t Idx>
  static constexpr size_t field_size() {
    switch (codes[Idx]) {
      case TypeCode::U8: case TypeCode::I8: case TypeCode::PAD: return 1;
      case TypeCode::BE_U16: case TypeCode::BE_I16:
      case TypeCode::LE_U16: case TypeCode::LE_I16: return 2;
      case TypeCode::BE_U32: case TypeCode::BE_I32:
      case TypeCode::LE_U32: case TypeCode::LE_I32: return 4;
    }
    return 0;
  }

  template <size_t... Is>
  static consteval size_t calc_total_size(std::index_sequence<Is...>) {
    return (field_size<Is>() + ... + 0);
  }

 public:
  static constexpr size_t size = calc_total_size(std::make_index_sequence<field_count>{});

  // Deduction of the unpacked std::tuple return type (filtering out PAD 'x' bytes)
  template <size_t... Is>
  static auto tuple_type_builder(std::index_sequence<Is...>) {
    auto filtered_tuple = std::tuple_cat([]<size_t I>() {
      constexpr TypeCode code = codes[I];
      if constexpr (code == TypeCode::PAD) {
        return std::tuple<>{};
      } else {
        return std::tuple<typename FieldTypeMap<code>::type>{};
      }
    }.template operator()<Is>()...);

    return filtered_tuple;
  }

  using TupleType = decltype(tuple_type_builder(std::make_index_sequence<field_count>{}));

  // --- UNPACK ---
  static std::optional<TupleType> unpack(std::span<const uint8_t> buf) {
    if (buf.size() < size) return std::nullopt;
    const uint8_t *ptr = buf.data();
    TupleType result;

    auto read_all = [&]<size_t... Is>(std::index_sequence<Is...>) {
      (read_field<Is>(ptr, result), ...);
    };
    read_all(std::make_index_sequence<field_count>{});
    return result;
  }

  static std::optional<TupleType> unpack(const uint8_t *buf, size_t len) {
    if (!buf) return std::nullopt;
    return unpack(std::span<const uint8_t>(buf, len));
  }

  static std::optional<TupleType> unpack_from(std::span<const uint8_t> &buf, size_t advance_by = size) {
    if (buf.size() < size || buf.size() < advance_by) return std::nullopt;
    auto res = unpack(buf);
    buf = buf.subspan(advance_by);
    return res;
  }

  class Range {
   public:
    class Iterator {
     public:
      using value_type = TupleType;
      using difference_type = std::ptrdiff_t;
      using pointer = const TupleType*;
      using reference = const TupleType&;
      using iterator_category = std::forward_iterator_tag;

      Iterator() : buf_({}), stride_(size), current_(std::nullopt) {}
      Iterator(std::span<const uint8_t> buf, size_t stride) : buf_(buf), stride_(stride) {
        fetch();
      }

      const TupleType &operator*() const { return current_.value(); }
      const TupleType *operator->() const { return &current_.value(); }

      Iterator &operator++() {
        if (buf_.size() >= stride_) {
          buf_ = buf_.subspan(stride_);
        } else {
          buf_ = {};
        }
        fetch();
        return *this;
      }

      Iterator operator++(int) {
        Iterator tmp = *this;
        ++(*this);
        return tmp;
      }

      bool operator==(const Iterator &other) const {
        if (!current_.has_value() && !other.current_.has_value()) return true;
        if (current_.has_value() != other.current_.has_value()) return false;
        return buf_.data() == other.buf_.data() && buf_.size() == other.buf_.size();
      }

      bool operator!=(const Iterator &other) const {
        return !(*this == other);
      }

     private:
      void fetch() {
        if (buf_.size() >= size && buf_.size() >= stride_) {
          current_ = Struct::unpack(buf_);
        } else {
          current_ = std::nullopt;
        }
      }

      std::span<const uint8_t> buf_;
      size_t stride_{size};
      std::optional<TupleType> current_{std::nullopt};
    };

    Range(std::span<const uint8_t> buf, size_t stride) : buf_(buf), stride_(stride) {}

    Iterator begin() const { return Iterator(buf_, stride_); }
    Iterator end() const { return Iterator(); }

   private:
    std::span<const uint8_t> buf_;
    size_t stride_;
  };

  static Range unpack_all(std::span<const uint8_t> buf, size_t stride = size) {
    return Range(buf, stride);
  }

  static Range unpack_all(const uint8_t *buf, size_t len, size_t stride = size) {
    if (!buf) return Range({}, stride);
    return Range(std::span<const uint8_t>(buf, len), stride);
  }

  // --- PACK ---
  static size_t pack(uint8_t *buf, const TupleType &tup) {
    if (!buf) return 0;
    uint8_t *ptr = buf;
    auto write_all = [&]<size_t... Is>(std::index_sequence<Is...>) {
      (write_field<Is>(ptr, tup), ...);
    };
    write_all(std::make_index_sequence<field_count>{});
    return size;
  }

  static size_t pack(std::span<uint8_t> buf, const TupleType &tup) {
    if (buf.size() < size) return 0;
    return pack(buf.data(), tup);
  }

  template <typename... Args>
  requires (sizeof...(Args) != 1 || !std::is_same_v<std::decay_t<std::tuple_element_t<0, std::tuple<Args...>>>, TupleType>)
  static size_t pack(uint8_t *buf, const Args &... args) {
    return pack(buf, TupleType(args...));
  }

  template <typename... Args>
  requires (sizeof...(Args) != 1 || !std::is_same_v<std::decay_t<std::tuple_element_t<0, std::tuple<Args...>>>, TupleType>)
  static size_t pack(std::span<uint8_t> buf, const Args &... args) {
    if (buf.size() < size) return 0;
    return pack(buf.data(), args...);
  }

 private:
  template <size_t Idx, typename TupleT>
  static void read_field(const uint8_t *&ptr, TupleT &tup) {
    constexpr TypeCode code = codes[Idx];
    if constexpr (code == TypeCode::PAD) {
      ptr += 1;
    } else if constexpr (code == TypeCode::U8) {
      std::get<tuple_index<Idx>()>(tup) = *ptr++;
    } else if constexpr (code == TypeCode::I8) {
      std::get<tuple_index<Idx>()>(tup) = static_cast<int8_t>(*ptr++);
    } else if constexpr (code == TypeCode::BE_U16) {
      std::get<tuple_index<Idx>()>(tup) = (static_cast<uint16_t>(ptr[0]) << 8) | ptr[1];
      ptr += 2;
    } else if constexpr (code == TypeCode::LE_U16) {
      std::get<tuple_index<Idx>()>(tup) = (static_cast<uint16_t>(ptr[1]) << 8) | ptr[0];
      ptr += 2;
    } else if constexpr (code == TypeCode::BE_I16) {
      std::get<tuple_index<Idx>()>(tup) = static_cast<int16_t>((static_cast<uint16_t>(ptr[0]) << 8) | ptr[1]);
      ptr += 2;
    } else if constexpr (code == TypeCode::LE_I16) {
      std::get<tuple_index<Idx>()>(tup) = static_cast<int16_t>((static_cast<uint16_t>(ptr[1]) << 8) | ptr[0]);
      ptr += 2;
    } else if constexpr (code == TypeCode::BE_U32) {
      std::get<tuple_index<Idx>()>(tup) = (static_cast<uint32_t>(ptr[0]) << 24) |
                                          (static_cast<uint32_t>(ptr[1]) << 16) |
                                          (static_cast<uint32_t>(ptr[2]) << 8) |
                                          ptr[3];
      ptr += 4;
    } else if constexpr (code == TypeCode::LE_U32) {
      std::get<tuple_index<Idx>()>(tup) = (static_cast<uint32_t>(ptr[3]) << 24) |
                                          (static_cast<uint32_t>(ptr[2]) << 16) |
                                          (static_cast<uint32_t>(ptr[1]) << 8) |
                                          ptr[0];
      ptr += 4;
    } else if constexpr (code == TypeCode::BE_I32) {
      std::get<tuple_index<Idx>()>(tup) = static_cast<int32_t>(
                                          (static_cast<uint32_t>(ptr[0]) << 24) |
                                          (static_cast<uint32_t>(ptr[1]) << 16) |
                                          (static_cast<uint32_t>(ptr[2]) << 8) |
                                          ptr[3]);
      ptr += 4;
    } else if constexpr (code == TypeCode::LE_I32) {
      std::get<tuple_index<Idx>()>(tup) = static_cast<int32_t>(
                                          (static_cast<uint32_t>(ptr[3]) << 24) |
                                          (static_cast<uint32_t>(ptr[2]) << 16) |
                                          (static_cast<uint32_t>(ptr[1]) << 8) |
                                          ptr[0]);
      ptr += 4;
    }
  }

  template <size_t Idx, typename TupleT>
  static void write_field(uint8_t *&ptr, const TupleT &tup) {
    constexpr TypeCode code = codes[Idx];
    if constexpr (code == TypeCode::PAD) {
      *ptr++ = 0x00;
    } else {
      auto val = std::get<tuple_index<Idx>()>(tup);
      if constexpr (code == TypeCode::U8) {
        *ptr++ = static_cast<uint8_t>(val);
      } else if constexpr (code == TypeCode::I8) {
        *ptr++ = static_cast<uint8_t>(static_cast<int8_t>(val));
      } else if constexpr (code == TypeCode::BE_U16 || code == TypeCode::BE_I16) {
        uint16_t v = static_cast<uint16_t>(val);
        ptr[0] = static_cast<uint8_t>((v >> 8) & 0xFF);
        ptr[1] = static_cast<uint8_t>(v & 0xFF);
        ptr += 2;
      } else if constexpr (code == TypeCode::LE_U16 || code == TypeCode::LE_I16) {
        uint16_t v = static_cast<uint16_t>(val);
        ptr[0] = static_cast<uint8_t>(v & 0xFF);
        ptr[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        ptr += 2;
      } else if constexpr (code == TypeCode::BE_U32 || code == TypeCode::BE_I32) {
        uint32_t v = static_cast<uint32_t>(val);
        ptr[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
        ptr[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
        ptr[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
        ptr[3] = static_cast<uint8_t>(v & 0xFF);
        ptr += 4;
      } else if constexpr (code == TypeCode::LE_U32 || code == TypeCode::LE_I32) {
        uint32_t v = static_cast<uint32_t>(val);
        ptr[0] = static_cast<uint8_t>(v & 0xFF);
        ptr[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        ptr[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        ptr[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
        ptr += 4;
      }
    }
  }

  // Maps physical field index to tuple index (skipping padding bytes)
  template <size_t TargetIdx>
  static consteval size_t tuple_index() {
    size_t non_pad_idx = 0;
    for (size_t i = 0; i < TargetIdx; ++i) {
      if (codes[i] != TypeCode::PAD) non_pad_idx++;
    }
    return non_pad_idx;
  }
};

namespace binary {
  template <FormatString Fmt>
  using Struct = ::esphome::ramses_esp::Struct<Fmt>;
}

} // namespace ramses_esp
} // namespace esphome
