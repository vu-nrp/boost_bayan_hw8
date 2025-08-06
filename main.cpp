#include <iostream>
#include <boost/program_options/option.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/options_description.hpp>
#include "searcher.h"

//!
//! bpo boost program option
//!

namespace bpo = boost::program_options;

//!
//!
//!

static const auto HelpOp = "help";
static const auto IncludeDirOp = "includeDir";
static const auto ExcludeDirOp = "excludeDir";
static const auto ScanLevelOp = "scanLevel";
static const auto MinFileSizeOp = "minFileSize";
static const auto FileMaskOp = "fileMask";
static const auto BlockSizeOp = "blockSize";
static const auto CheckSumOp = "checkSum";

//!
//!
//!

static const auto DirsSeparator = ';';

//!
//! \brief splitPaths - split separated by semicolons paths string to vector of path
//! \param paths - separated by semicolons paths string
//! \return
//!

std::vector<bfs::path> splitPaths(const std::string &paths)
{
    std::string part;
    std::vector<bfs::path> result;
    std::istringstream stream(paths);

    while (std::getline(stream, part, DirsSeparator))
        result.push_back(bfs::path(part));

    return result;
}

//!
//! \brief main - main app function
//!

int main(int argc, char **argv)
{
    bpo::variables_map options;
    bpo::options_description allOptions("util options list");

    allOptions.add_options()
        (HelpOp, "print help")
        (IncludeDirOp, bpo::value<std::string>(), "required, search directories, separated by semicolons")
        (ExcludeDirOp, bpo::value<std::string>()->default_value(""), "directories excluded from the search, separated by semicolons")
        (ScanLevelOp, bpo::value<int>()->default_value(FileInfo::RecursiveLevelDef), "0 - search in the specified directories, > 0 - search nesting level")
        (MinFileSizeOp, bpo::value<size_t>()->default_value(FileInfo::MinFileSizeDef), "minimum file size")
        (FileMaskOp, bpo::value<std::string>()->default_value(""), "files filter")
        (BlockSizeOp, bpo::value<uint32_t>()->default_value(FileInfo::BlockSizeDef), "block size in bytes")
        (CheckSumOp, bpo::value<std::string>()->default_value(FileInfo::CheckSumDef), "checksum algorithm: md5 or crc32");

    bpo::store(bpo::parse_command_line(argc, argv, allOptions), options);
    bpo::notify(options);

    if ((options.count(HelpOp) > 0) ||
        (options.count(IncludeDirOp) == 0)) {
        std::cout << allOptions << std::endl;
    } else {
        try {
            Searcher searcher(splitPaths(options[IncludeDirOp].as<std::string>()), splitPaths(options[ExcludeDirOp].as<std::string>()),
                              options[ScanLevelOp].as<int>(), options[CheckSumOp].as<std::string>(), options[BlockSizeOp].as<uint32_t>(),
                              options[FileMaskOp].as<std::string>(), options[MinFileSizeOp].as<size_t>());

            searcher.find();
            searcher.print();
        } catch (std::exception &e) {
            std::cout << e.what() << std::endl;
        }
    }

    return 0;
}
