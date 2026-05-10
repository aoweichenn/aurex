#include <aurex/base/source.hpp>

#include <cassert>
#include <utility>

namespace aurex::base {

usize SourceRange::length() const noexcept {
    return this->end >= this->begin ? this->end - this->begin : 0;
}

bool SourceRange::empty() const noexcept {
    return this->begin == this->end;
}

SourceFile::SourceFile(const SourceId id, std::string path, std::string text)
    : id_(id), path_(std::move(path)), text_(std::move(text)) {}

SourceId SourceFile::id() const noexcept {
    return this->id_;
}

std::string_view SourceFile::path() const noexcept {
    return this->path_;
}

std::string_view SourceFile::text() const noexcept {
    return this->text_;
}

SourceId SourceManager::add_source(std::string path, std::string text) {
    const SourceId id {static_cast<u32>(this->files_.size())};
    this->files_.emplace_back(id, std::move(path), std::move(text));
    return id;
}

const SourceFile& SourceManager::get(const SourceId id) const noexcept {
    assert(id.value < this->files_.size());
    return this->files_[id.value];
}

std::string_view SourceManager::text(const SourceId id) const noexcept {
    return this->get(id).text();
}

} // namespace aurex::base
