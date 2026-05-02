#include "aurex/base/source.hpp"

#include <cassert>
#include <utility>

namespace aurex::base {

usize SourceRange::length() const noexcept {
    return end >= begin ? end - begin : 0;
}

bool SourceRange::empty() const noexcept {
    return begin == end;
}

SourceFile::SourceFile(const SourceId id, std::string path, std::string text)
    : id_(id), path_(std::move(path)), text_(std::move(text)) {}

SourceId SourceFile::id() const noexcept {
    return id_;
}

std::string_view SourceFile::path() const noexcept {
    return path_;
}

std::string_view SourceFile::text() const noexcept {
    return text_;
}

SourceId SourceManager::add_source(std::string path, std::string text) {
    const SourceId id {static_cast<u32>(files_.size())};
    files_.emplace_back(id, std::move(path), std::move(text));
    return id;
}

const SourceFile& SourceManager::get(const SourceId id) const noexcept {
    assert(id.value < files_.size());
    return files_[id.value];
}

std::string_view SourceManager::text(const SourceId id) const noexcept {
    return get(id).text();
}

} // namespace aurex::base
