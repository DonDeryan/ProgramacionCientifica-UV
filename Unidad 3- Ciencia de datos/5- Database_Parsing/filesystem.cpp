#include "filesystem.h"
#include <fstream>

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <ftw.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>

#elif defined(_WIN32)
#define NOMINMAX
#include <direct.h>
#include <Shlobj.h>
#include <windows.h>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#endif

using namespace std;

/****************************
*                           *
*    Llamadas al sistema    *
*                           *
****************************/

#if defined(__linux__) || defined(__APPLE__)
int s_unlink_cb(const char* fpath, const struct stat* sb, int typeflag, struct FTW* ftwbuf)
{
    int rv = remove(fpath);
    if (rv)
        perror(fpath);
    return rv;
}

/**
* @brief Borra ruta recursivamente.
* @param path Ruta a borrar.
*/
int s_rmrf(string path)
{
    return nftw(path.c_str(), s_unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

int s_mkpath(std::string s, mode_t mode)
{
    size_t pre = 0, pos;
    std::string dir;
    int mdret = 1;

    if (s[s.size() - 1] != '/')
        s += '/';

    while ((pos = s.find_first_of('/', pre)) != std::string::npos)
    {
        dir = s.substr(0, pos++);
        pre = pos;
        if (dir.size() == 0)
            continue;

        if ((mdret = mkdir(dir.c_str(), mode)) && errno != EEXIST)
            return mdret;
    }
    return mdret;
}
#endif

/**
* @brief Reemplaza ocurrencias en cadena de texto.
*/
string replace_all(const string& in, const string& search, const string& replace)
{
    std::string s = in;
    for (size_t pos = 0;; pos += replace.length())
    {
        pos = s.find(search, pos);
        if (pos == string::npos) break;
        s.erase(pos, search.length());
        s.insert(pos, replace);
    }
    return s;
}

string s_format_path(const string in, bool support_long_path = false)
{
#if defined(__linux__) || defined(__APPLE__)
    return string(in);
#elif defined(_WIN32)
    string out(in);
    replace_all(out, "/", "\\");
    if (support_long_path)
        out = string("\\\\?\\") + out;
    return out;
#endif
}

/**
* @brief Crea directorio.
* @param arg Ruta al directorio a crear.
*/
void s_mkdir(const std::string arg)
{
#if defined(__linux__) || defined(__APPLE__)
    s_mkpath(arg, 0755);
#elif defined(_WIN32)
    string in = s_format_path(arg);
    wstring w_arg = wstring(in.begin(), in.end());
    CreateDirectory(w_arg.c_str(), NULL);
#endif
}

/**
* @brief Borra archivo.
* @param arg Ruta al archivo a borrar.
*/
void s_rm(const string arg)
{
#if defined(__linux__) || defined(__APPLE__)
    ::remove(arg.c_str());
#elif defined(_WIN32)
    std::ifstream infile(arg);
    if (infile.is_open())
    {
        infile.close();
        std::string in = s_format_path(arg);
        std::wstring w_arg = std::wstring(in.begin(), in.end());
        DeleteFile(w_arg.c_str());
    }
#endif
}


/***********************************************
*                                              *
*   Administración de directorios y archivos   *
*                                              *
***********************************************/


#if defined(__linux__) || defined(__APPLE__)
const std::string filesystemSeparator = "/";
#elif defined(_WIN32)
const std::string filesystemSeparator = "\\";
#endif

/**
* @brief Verifica si existe el directorio.
*/
bool directory_exist(string path)
{
    const char* pzPath = path.c_str();
#if defined(__linux__) || defined(__APPLE__)
    if (pzPath == NULL) return false;

    DIR* pDir;
    bool bExists = false;

    pDir = opendir(pzPath);

    if (pDir != NULL)
    {
        bExists = true;
        (void)closedir(pDir);
    }

    return bExists;
#elif defined(_WIN32)
    DWORD ftyp = GetFileAttributesA(pzPath);
    if (ftyp == INVALID_FILE_ATTRIBUTES)
        return false;

    if (ftyp & FILE_ATTRIBUTE_DIRECTORY)
        return true;
    return false;
#endif
}

/**
* @brief Verifica si existe el archivo.
*/
bool file_exist(string fileName)
{
    ifstream infile(fileName.c_str());
    bool res = infile.is_open();
    if (res)
        infile.close();
    return res;
}

/**
* @brief Devuelve la ruta del directorio que contiene el programa ejecutable.
*/
std::string get_app_folder()
{
#if defined(__linux__)
    string path = "";
    pid_t pid = getpid();
    char buf[20] = { 0 };
    sprintf(buf, "%d", pid);
    string _link = "/proc/";
    _link.append(buf);
    _link.append("/exe");
    char proc[512];
    int ch = readlink(_link.c_str(), proc, 512);
    if (ch != -1)
    {
        proc[ch] = 0;
        path = proc;
        std::string::size_type t = path.find_last_of("/");
        path = path.substr(0, t);
    }
    return path;
#elif defined(_WIN32)
    wchar_t ownPth[MAX_PATH];
    char mbOwnPth[MAX_PATH];

    HMODULE hModule = GetModuleHandle(NULL);
    if (hModule != NULL)
    {
        GetModuleFileNameW(hModule, ownPth, MAX_PATH);
        PathRemoveFileSpec(ownPth);
        wcstombs(mbOwnPth, ownPth, MAX_PATH);
        return std::string(mbOwnPth);
    }
    else
        return std::string("");
#endif
}

/**
* @brief Crea directorio si no existe.
*/
void create_directory(string directory_name)
{
    string path = get_app_folder() + filesystemSeparator + directory_name;
#if defined(__linux__) || defined(__APPLE__)
    if (df_directory_exist(path))
        s_rmrf(path);
#endif
    s_mkdir(path);
}

/**
* @brief Unión de ruta de archivo multiplataforma.
*/
string filename_join(initializer_list<string> path)
{
    string fullPath = get_app_folder();
    for (auto elem : path)
    {
        fullPath += filesystemSeparator;
        fullPath += elem;
    }
    return fullPath;
}

string file_basename(const string path)
{
    string base_filename = path.substr(path.find_last_of("/\\") + 1);
    string::size_type const p(base_filename.find_last_of('.'));
    string file_without_extension = base_filename.substr(0, p);
    return file_without_extension;
}

string file_extension(const string filepath)
{
    return filepath.substr(filepath.find_last_of('.') + 1);
}