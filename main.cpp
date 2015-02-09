#include <string>
#include <vector>
#include <limits>
#include <cstdio>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <rapidjson/rapidjson.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/document.h>
#include <rapidjson/filestream.h>

using document = rapidjson::Document;
using value = rapidjson::Value;

double convert_bytes_to_kb(const double &bytes) {
    return static_cast<double>(bytes) / (2 << 10);
}

void set_value_with_attributes
(document &doc,
 value &root,
 const boost::filesystem::path path,
 const boost::filesystem::file_type type,
 const size_t level,
 const size_t total_size,
 const size_t recursive_total_size,
 const double average_size,
 const double recursive_average_size,
 const size_t file_count,
 const size_t recursive_file_count,
 const size_t directory_count,
 const size_t recursive_directory_count,
 value &files,
 size_t maximum_size,
 boost::filesystem::path maximum_path,
 size_t minimum_size,
 boost::filesystem::path minimum_path) {
    using namespace boost::filesystem;
    auto &alloc = doc.GetAllocator();
    value v(path.string().c_str(), alloc);
    root.AddMember("Path", v, alloc);
    root.AddMember("Type", type == file_type::regular_file ? "Regular" : "Directory", alloc);
    root.AddMember("Level", static_cast<uint64_t>(level), alloc);
    root.AddMember("Total size", convert_bytes_to_kb(total_size), alloc);
    root.AddMember("Recursive total size", convert_bytes_to_kb(recursive_total_size), alloc);
    root.AddMember("Average size", convert_bytes_to_kb(average_size), alloc);
    root.AddMember("Recursive average size", convert_bytes_to_kb(recursive_average_size), alloc);
    root.AddMember("File count", static_cast<uint64_t>(file_count), alloc);
    root.AddMember("Recursive file count", static_cast<uint64_t>(recursive_file_count), alloc);
    root.AddMember("Directory count", static_cast<uint64_t>(directory_count), alloc);
    root.AddMember("Recursive directory count", static_cast<uint64_t>(recursive_directory_count), alloc);
    if (type == file_type::directory_file) {
        if (files.Size() > 0) {
            root.AddMember("Files", files, alloc);
            value max(rapidjson::kObjectType);
            max.AddMember("Size", convert_bytes_to_kb(maximum_size), alloc);
            v.SetString(maximum_path.string().c_str(), alloc);
            max.AddMember("Path", v, alloc);
            value min(rapidjson::kObjectType);
            min.AddMember("Size", convert_bytes_to_kb(minimum_size), alloc);
            v.SetString(minimum_path.string().c_str(), alloc);
            min.AddMember("Path", v, alloc);
            root.AddMember("Maximum", max, alloc);
            root.AddMember("Minimum", min, alloc);
        }
    }
}

const boost::filesystem::path unknown_path = "[unknown]";

struct result {
    size_t total_size = 0;
    size_t recursive_total_size = 0;
    size_t file_count = 0;
    size_t recursive_file_count = 0;
    size_t directory_count = 0;
    size_t recursive_directory_count = 0;
    boost::filesystem::path path = unknown_path;
    boost::filesystem::file_type type = boost::filesystem::file_type::status_error;
};

result traversal
(document &doc,
 const boost::filesystem::path path,
 const size_t level,
 value &val) {
    using namespace std;
    using namespace boost;
    using namespace boost::filesystem;
    using namespace rapidjson;
    auto &alloc = doc.GetAllocator();
    directory_entry root(path);
    result r;
    value files(kArrayType);
    auto maximum_size = numeric_limits<size_t>::min();
    boost::filesystem::path maximum_path = unknown_path;
    auto minimum_size = numeric_limits<size_t>::max();
    boost::filesystem::path minimum_path = unknown_path;
    r.type = root.status().type();
    if (r.type == file_type::regular_file) {
        r.total_size = file_size(path);
        r.recursive_total_size = file_size(path);
        r.file_count = 1;
        r.recursive_file_count = 1;
        r.directory_count = 0;
        r.recursive_directory_count = 0;
        r.path = path;
    } else {
        using namespace std;
        directory_iterator root_directory(root);
        for (auto &current: root_directory) {
            value file(kObjectType);
            auto sub_r = traversal(doc, current.path(), level + 1, file);
            if (sub_r.type == file_type::directory_file) {
                r.recursive_total_size += sub_r.recursive_total_size;
                r.recursive_directory_count += sub_r.recursive_directory_count;
                r.recursive_file_count += sub_r.recursive_file_count;
                ++r.recursive_directory_count;
                ++r.directory_count;
            } else {
                r.recursive_total_size += sub_r.recursive_total_size;
                r.total_size += sub_r.total_size;
                ++r.recursive_file_count;
                ++r.file_count;
                if (sub_r.total_size > maximum_size) {
                    maximum_size = sub_r.total_size;
                    maximum_path = sub_r.path;
                }
                if (sub_r.total_size < minimum_size) {
                    minimum_size = sub_r.total_size;
                    minimum_path = sub_r.path;
                }
            }
            files.PushBack(file, alloc);
        }
    }
    set_value_with_attributes
    (doc, val, path, r.type, level,
     r.total_size, r.recursive_total_size,
     (r.file_count == 0 ? 0 : r.total_size / r.file_count),
     (r.recursive_file_count == 0 ? 0 : r.recursive_total_size / r.recursive_file_count),
     r.file_count, r.recursive_file_count,
     r.directory_count, r.recursive_directory_count,
     files, maximum_size, maximum_path, minimum_size, minimum_path);
    return r;
}

int main(int argc, const char *argv[]) {
    using namespace boost::filesystem;
    using namespace std;
    using namespace rapidjson;
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [Path]\n", path(argv[0]).filename().c_str());
        return EXIT_FAILURE;
    }
    document doc;
    doc.SetObject();
    traversal(doc, argv[1], 0, doc);
    FileStream fout(stdout);
    PrettyWriter<FileStream> writer(fout);
    doc.Accept(writer);
    printf("\n");
    return EXIT_SUCCESS;
}
