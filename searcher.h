#pragma once

#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/iterator_range.hpp>

//!
//! namespace reducer
//!

namespace bfs = boost::filesystem;

//!
//! \brief The Checksum enum
//!

enum class Checksum
{
    crc32 = 0,
    md5
};

//!
//! \brief The FileInfo class
//!

class FileInfo
{
public:
    static const uint32_t BlockSizeDef;
    static const size_t MinFileSizeDef;
    static const int RecursiveLevelDef;
    static const std::string CheckSumDef;

    struct Hash
    {
        size_t operator()(const bfs::path &path) const
        {
            std::hash<std::string> hs;
            return hs(path.string());
        }
    };

    FileInfo() = default;
    FileInfo(const bfs::path &path);

    std::string nextHash();
    bool isFinished() const;

    static void setBlockSize(const size_t value);
    static void setChecksum(const Checksum value);

private:
    std::string crc32(const std::string &str) const;
    std::string md5(const std::string &str) const;

    static Checksum checkSum;
    static size_t blockSize;

    std::ifstream m_ifs;
    size_t m_pos {0};
    std::string m_hash;
    bfs::path m_path;
    size_t m_size {0};

};

//!
//! \brief The Searcher class
//!

class Searcher
{
public:
    class FilesGroup
    {
    public:
        FileInfo &operator[](const bfs::path &path)
        {
            return m_group[path];
        }

        std::unordered_map<bfs::path, FileInfo, FileInfo::Hash> &filesGroup()
        {
            return m_group;
        }

        bool isFinished() const
        {
            return m_group.begin()->second.isFinished();
        }

    private:
        std::unordered_map<bfs::path, FileInfo, FileInfo::Hash> m_group;

    };

    Searcher(std::vector<bfs::path> &&includeDirs, std::vector<bfs::path> &&excludeDirs, const int recursiveLevel,
             const std::string &checkSum, const uint32_t blockSize, const std::string &filter, const size_t minSize);

    void find();
    void print();

protected:
    bool allFinished() const;
    void nextStep();

private:
    void findGroups();

    template <class Container, class Func>
    void erase(Container &cont, Func func)
    {
        auto beg = cont.begin();
        const auto end = cont.end();
        while (beg != end) {
            if (func(*beg)) {
                beg = cont.erase(beg);
            } else {
                ++beg;
            }
        }
    }

    std::unordered_map<size_t, std::vector<bfs::path>> groupBySize(const int level)
    {
        std::unordered_map<size_t, std::vector<bfs::path>> sizeGroups;
        for (const auto &directory: m_includeDirs) {
            for (bfs::recursive_directory_iterator it(directory), end;it != end;++it) {
                if ((m_excludeDirs.find(it->path()) != m_excludeDirs.end()) || (it.depth() > level)) {
                    it.disable_recursion_pending();
                    continue;
                }

                boost::smatch what;
                if (bfs::is_regular_file(it->path()) &&
                    (bfs::file_size(it->path()) >= m_minSize) &&
                    (m_filter.str().empty() ? true : boost::regex_match(it->path().filename().string(), what, m_filter))) {
                    sizeGroups[bfs::file_size(it->path())].push_back(it->path());
                }
            }

            erase(sizeGroups, [](const auto &item)
            {
                return (item.second.size() == 1);
            });
        }
        return sizeGroups;
    }

    static size_t groupId;

    std::vector<bfs::path> m_includeDirs;
    std::unordered_set<bfs::path, FileInfo::Hash> m_excludeDirs;
    int m_recursiveLevel {FileInfo::RecursiveLevelDef};
    size_t m_minSize {FileInfo::MinFileSizeDef};
    boost::regex m_filter;
    std::unordered_map<size_t, FilesGroup> m_filesGroups;

};
