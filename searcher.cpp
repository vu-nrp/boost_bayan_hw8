#include <iostream>
#include <boost/crc.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/uuid/detail/md5.hpp>
#include "searcher.h"

//!
//! \brief ChecksumNames
//!

static const std::map<Checksum, std::string> ChecksumNames
{
    {Checksum::crc32, "crc32"},
    {Checksum::md5, "md5"}
};

//!
//! \brief FileInfo constants
//!

size_t FileInfo::blockSize {FileInfo::BlockSizeDef};
Checksum FileInfo::checkSum {Checksum::md5};

const uint32_t FileInfo::BlockSizeDef {10};
const size_t FileInfo::MinFileSizeDef {1};
const int FileInfo::RecursiveLevelDef {0};
const std::string FileInfo::CheckSumDef {ChecksumNames.at(Checksum::md5)};

//!
//! \brief FileInfo::FileInfo
//! \param path
//!

FileInfo::FileInfo(const bfs::path &path) :
    m_path(path)
{
    m_size = bfs::file_size(m_path);
}

void FileInfo::setBlockSize(const size_t value)
{
    if (value > 0) {
        FileInfo::blockSize = value;
    } else {
        throw std::runtime_error("Block size must be greater or equal to 1 byte");
    }
}

void FileInfo::setChecksum(const Checksum value)
{
    FileInfo::checkSum = value;
}

bool FileInfo::isFinished() const
{
    return (m_pos >= m_size);
}

std::string FileInfo::nextHash()
{
    if (m_pos > m_size) {
        return m_hash;
    }

    if (!m_ifs.is_open()) {
        m_ifs.open(m_path.c_str(), std::ios::in | std::ios::binary);
    }

    std::string chunk(FileInfo::blockSize, 0);
    m_ifs.read(chunk.data(), FileInfo::blockSize);
    m_pos += FileInfo::blockSize;
    m_hash = (FileInfo::checkSum == Checksum::md5) ? md5(chunk) : crc32(chunk);

    return m_hash;
}

std::string FileInfo::crc32(const std::string &str) const
{
    boost::crc_32_type crc32;
    crc32.process_bytes(str.data(), str.length());
    return std::to_string(crc32.checksum());
}

std::string FileInfo::md5(const std::string &str) const
{
    auto toString = [](const boost::uuids::detail::md5::digest_type &digest)
    {
        std::string result;
        const auto intDigest = reinterpret_cast<const int *>(&digest);
        boost::algorithm::hex(intDigest, intDigest + (sizeof(boost::uuids::detail::md5::digest_type) / sizeof(int)), std::back_inserter(result));
        return result;
    };

    boost::uuids::detail::md5 md5;
    boost::uuids::detail::md5::digest_type digest;

    md5.process_bytes(str.data(), str.size());
    md5.get_digest(digest);

    return toString(digest);
}

//!
//! \brief Searcher::Searcher
//! \param include
//! \param exclude
//! \param recursive
//! \param checksum
//! \param block_size
//! \param filter
//! \param min_size
//!

size_t Searcher::groupId {0};

Searcher::Searcher(std::vector<bfs::path> &&includeDirs, std::vector<bfs::path> &&excludeDirs,
                   const int recursiveLevel, const std::string &checkSum, const uint32_t blockSize,
                   const std::string &filter, const size_t minSize) :
    m_includeDirs(std::move(includeDirs)),
    m_excludeDirs(excludeDirs.begin(), excludeDirs.end()),
    m_recursiveLevel(recursiveLevel),
    m_minSize(minSize),
    m_filter(filter, boost::regex::icase)
{
    if (checkSum == ChecksumNames.at(Checksum::crc32)) {
        FileInfo::setChecksum(Checksum::crc32);
    } else if (checkSum == ChecksumNames.at(Checksum::md5)) {
        FileInfo::setChecksum(Checksum::md5);
    } else {
        std::cout << "checksum unknown option, md5 will used" << std::endl;
    }
    FileInfo::setBlockSize(blockSize);
}

void Searcher::findGroups()
{
    const auto sizeGroups = groupBySize(m_recursiveLevel);

    for (auto &sizeGroup : sizeGroups) {
        FilesGroup group;
        for (auto &file: sizeGroup.second) {
            group[file] = FileInfo(file);
        }
        m_filesGroups[Searcher::groupId++] = std::move(group);
    }
}

bool Searcher::allFinished() const
{
    for (const auto &group : m_filesGroups) {
        if (!group.second.isFinished()) {
            return false;
        }
    }
    return true;
}

void Searcher::find()
{
    findGroups();
    while (!allFinished())
        nextStep();
}

void Searcher::print()
{
    for (auto it = m_filesGroups.begin(); it != m_filesGroups.end(); ++it) {
        for (const auto &duplicate : it->second.filesGroup())
            std::cout << duplicate.first.string() << std::endl;
        std::cout << std::endl;
    }
}

void Searcher::nextStep()
{
    std::unordered_set<size_t> removed;
    for (auto &group: m_filesGroups) {
        if (group.second.isFinished()) {
            continue;
        }

        std::unordered_map<std::string, std::vector<bfs::path>> hashGroups;
        for (auto &file : group.second.filesGroup()) {
            hashGroups[file.second.nextHash()].push_back(file.first);
        }

        if (hashGroups.size() > 1) {
            removed.insert(group.first);
            for (auto &hashGroup : hashGroups) {
                if (hashGroup.second.size() > 1) {
                    FilesGroup newGroup;
                    for (const auto &file : hashGroup.second) {
                        newGroup[file] = std::move(group.second[file]);
                    }
                    m_filesGroups[Searcher::groupId++] = std::move(newGroup);
                }
            }
        }
    }

    erase(m_filesGroups, [&removed](const auto &item)
    {
        return (removed.find(item.first) != removed.end());
    });
}
