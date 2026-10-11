#include "../include/path.h"

#include <system_error>

dbasic::Path::Path(const std::string &path) {
    SetPath(path);
}

dbasic::Path::Path(const char *path) {
    SetPath(std::string(path));
}

dbasic::Path::Path(const Path &path) : m_path(path.m_path) {
    /* void */
}

dbasic::Path::Path() {
    /* void */
}

dbasic::Path::Path(const std::filesystem::path &path) : m_path(path) {
    /* void */
}

dbasic::Path::~Path() {
    /* void */
}

std::string dbasic::Path::ToString() const {
    return m_path.string();
}

void dbasic::Path::SetPath(const std::string &path) {
    m_path = std::filesystem::path(path);
}

bool dbasic::Path::operator==(const Path &path) const {
    std::error_code ec;
    const bool equivalent = std::filesystem::equivalent(m_path, path.m_path, ec);
    return ec ? (m_path.lexically_normal() == path.m_path.lexically_normal()) : equivalent;
}

dbasic::Path dbasic::Path::Append(const Path &path) const {
    return Path(m_path / path.m_path);
}

void dbasic::Path::GetParentPath(Path *path) const {
    path->m_path = m_path.parent_path();
}

const dbasic::Path &dbasic::Path::operator=(const Path &b) {
    m_path = b.m_path;
    return *this;
}

std::string dbasic::Path::GetExtension() const {
    return m_path.extension().string();
}

std::string dbasic::Path::GetStem() const {
    return m_path.stem().string();
}

dbasic::Path dbasic::Path::GetAbsolute() const {
    std::error_code ec;
    const std::filesystem::path absolute = std::filesystem::absolute(m_path, ec);
    return ec ? Path(m_path) : Path(absolute);
}

bool dbasic::Path::IsAbsolute() const {
    return m_path.is_absolute();
}

bool dbasic::Path::Exists() const {
    std::error_code ec;
    return std::filesystem::exists(m_path, ec);
}

bool dbasic::Path::CreateDir() const {
    std::error_code ec;
    std::filesystem::create_directories(m_path, ec);
    return !ec;
}
