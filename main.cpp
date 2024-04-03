#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool PreprocessRecursive(const path& in_file, ofstream& out, const vector<path>& include_directories) {
    ifstream in(in_file);

    if(!in.is_open()) {
        return false;
    }

    static regex include_relative(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static regex include_vector(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");

    int line = 1;
    string line_text;

    while(getline(in, line_text)) {
        smatch m;

        if(regex_match(line_text, m, include_relative)) {
            path include_file = string(m[1]);
            path current_dir = in_file.parent_path();
            path search_path = current_dir / include_file;

            if(!filesystem::exists(search_path)) {
                bool is_found = false;

                for(const auto& dir : include_directories) {
                    search_path = dir / include_file;

                    if(filesystem::exists(search_path)) {
                        is_found = true;
                        break;
                    }
                }

                if(!is_found) {
                    cout << "unknown include file " << include_file.string() << " at file " << in_file.string() << " at line " << line << endl;
                    return false;
                }
            }

            if(!PreprocessRecursive(search_path, out, include_directories)) {
                cout << "unknown include file " << include_file.string() << " at file " << in_file.string() << " at line " << line << endl;
                return false;
            }
        } else if(regex_match(line_text, m, include_vector)) {
            path include_file = string(m[1]);
            bool is_found = false;
            for(const auto& dir : include_directories) {
                path search_path = dir / include_file;

                if(filesystem::exists(search_path)) {
                    is_found = true;

                    if(!PreprocessRecursive(search_path, out, include_directories)) {
                        cout << "unknown include file " << include_file.string() << " at file " << in_file.string() << " at line " << line << endl;
                        return false;
                    }
                    break;
                }
            }

            if(!is_found) {
                cout << "unknown include file " << include_file.string() << " at file " << in_file.string() << " at line " << line << endl;
                return false;
            }    
        } else {
            out << line_text << endl;
        }
        ++line;
    }

    return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ofstream out(out_file);

    if(!out.is_open()) {
        return false;
    }

    return PreprocessRecursive(in_file, out, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}