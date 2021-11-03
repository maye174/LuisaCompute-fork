#pragma once
#include <vstl/vector.h>
#include <fstream>
#include <vstl/vstring.h>
#include <vstl/Memory.h>
#include <vstl/string_view.h>
#include <span>
namespace vstd {

struct VENGINE_DLL_COMMON CharSplitIterator {
    char const *curPtr;
    char const *endPtr;
    char sign;
    std::string_view result;
    std::string_view operator*() const;
    void operator++();
    bool operator==(IteEndTag) const;
};
struct VENGINE_DLL_COMMON StrVSplitIterator {
    char const *curPtr;
    char const *endPtr;
    std::string_view sign;
    std::string_view result;
    std::string_view operator*() const;
    void operator++();
    bool operator==(IteEndTag) const;
};
template<typename SignT, typename IteratorType>
struct StrvIEnumerator {
    char const *curPtr;
    char const *endPtr;
    SignT sign;
    IteratorType begin() const {
        IteratorType c{curPtr, endPtr, sign};
        ++c;
        return c;
    }
    IteEndTag end() const {
        return {};
    }
};
class VENGINE_DLL_COMMON StringUtil {
private:
    StringUtil() = delete;
    KILL_COPY_CONSTRUCT(StringUtil)
public:
    static StrvIEnumerator<char, CharSplitIterator> Split(std::string_view str, char sign) {
        return StrvIEnumerator<char, CharSplitIterator>{str.data(), str.data() + str.size(), sign};
    }
    static StrvIEnumerator<std::string_view, StrVSplitIterator> Split(std::string_view str, std::string_view sign) {
        return StrvIEnumerator<std::string_view, StrVSplitIterator>{str.data(), str.data() + str.size(), sign};
    }

    static variant<int64, double> StringToNumber(std::string_view numStr);
    static void ToLower(std::string &str);
    static void ToUpper(std::string &str);

    static std::string ToLower(std::string_view str);
    static std::string ToUpper(std::string_view str);
    static void EncodeToBase64(std::span<uint8_t const> binary, std::string &result);
    static void EncodeToBase64(std::span<uint8_t const> binary, char *result);
    static void DecodeFromBase64(std::string_view str, vector<uint8_t> &result);
    static void DecodeFromBase64(std::string_view str, uint8_t *size);
    static void TransformWCharToChar(
        wchar_t const *src,
        char *dst,
        size_t sz);
};
}// namespace vstd