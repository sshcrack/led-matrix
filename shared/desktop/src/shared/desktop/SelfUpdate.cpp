#include "shared/desktop/SelfUpdate.h"

#include "shared/common/utils/utils.h"
#include "shared/desktop/autostart.h"
#include "shared/desktop/config.h"
#include "shared/desktop/utils.h"
#include "led_matrix_version.h" // generated

#include <cpr/cpr.h>
#include <picosha2.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#include <shellapi.h>
#include <shlobj.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace fs = std::filesystem;

namespace SelfUpdate
{
    namespace
    {
        constexpr const char *DISPLAY_NAME = "LED Matrix Controller";
#ifdef _WIN32
        constexpr const char *EXE_NAME = "led-matrix-desktop.exe";
        constexpr const wchar_t *UNINSTALL_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\led-matrix-desktop";
#else
        constexpr const char *EXE_NAME = "led-matrix-desktop";
#endif

        std::string user_agent() { return std::string("led-matrix-desktop/") + PROJECT_VERSION_STRING; }

        // ---- processes ----------------------------------------------------

#ifdef _WIN32
        std::wstring widen(const std::string &s)
        {
            if (s.empty())
                return {};
            const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
            std::wstring w(n, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
            return w;
        }

        std::wstring quote_arg(const std::wstring &arg)
        {
            if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos)
                return arg;
            std::wstring out = L"\"";
            size_t backslashes = 0;
            for (wchar_t c: arg)
            {
                if (c == L'\\')
                {
                    backslashes++;
                    continue;
                }
                if (c == L'"')
                    out.append(backslashes * 2 + 1, L'\\');
                else
                    out.append(backslashes, L'\\');
                backslashes = 0;
                out += c;
            }
            out.append(backslashes * 2, L'\\');
            return out + L"\"";
        }

        /// Runs a command line without a console window. wait=false detaches it.
        int run_cmdline(std::wstring cmd, bool wait)
        {
            STARTUPINFOW si{};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi{};
            const DWORD flags = CREATE_NO_WINDOW | (wait ? 0 : DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP);
            if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, flags, nullptr, nullptr, &si, &pi))
                return -1;
            DWORD code = 0;
            if (wait)
            {
                WaitForSingleObject(pi.hProcess, INFINITE);
                GetExitCodeProcess(pi.hProcess, &code);
            }
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return static_cast<int>(code);
        }

        int run_process(const std::vector<std::wstring> &args, bool wait = true)
        {
            std::wstring cmd;
            for (const auto &a: args)
                cmd += (cmd.empty() ? L"" : L" ") + quote_arg(a);
            return run_cmdline(std::move(cmd), wait);
        }

        fs::path system32() {
            wchar_t buf[MAX_PATH];
            const UINT n = GetSystemDirectoryW(buf, MAX_PATH);
            return n ? fs::path(std::wstring(buf, n)) : fs::path(L"C:\\Windows\\System32");
        }
#else
        int run_process(const std::vector<std::string> &args, bool wait = true)
        {
            std::vector<char *> argv;
            for (const auto &a: args)
                argv.push_back(const_cast<char *>(a.c_str()));
            argv.push_back(nullptr);
            pid_t pid = 0;
            if (posix_spawnp(&pid, argv[0], nullptr, nullptr, argv.data(), environ) != 0)
                return -1;
            if (!wait)
                return 0;
            int status = 0;
            if (waitpid(pid, &status, 0) < 0)
                return -1;
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        }
#endif

        // ---- filesystem ---------------------------------------------------

        bool exists_nofollow(const fs::path &p)
        {
            std::error_code ec;
            return fs::symlink_status(p, ec).type() != fs::file_type::not_found && !ec;
        }

        fs::path versions_dir(const fs::path &root) { return root / "versions"; }

        /// versions/<v> of the running binary, when it runs from an install root.
        std::optional<fs::path> running_version_dir()
        {
            std::error_code ec;
            const fs::path exe = fs::weakly_canonical(get_exec_file(), ec);
            if (ec)
                return std::nullopt;
            const fs::path versionDir = exe.parent_path().parent_path();
            if (exe.parent_path().filename() != "bin" || versionDir.parent_path().filename() != "versions")
                return std::nullopt;
            return versionDir;
        }

        std::expected<void, std::string> copy_tree(const fs::path &from, const fs::path &to)
        {
            std::error_code ec;
            fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
            if (ec)
                return std::unexpected("Could not copy " + from.string() + " to " + to.string() + ": " + ec.message());
            return {};
        }

#ifdef _WIN32
        /// Makes the directory `link` (empty, or already a junction) a junction to `target`.
        std::expected<void, std::string> set_junction_target(const fs::path &link, const fs::path &target)
        {
            std::error_code ec;
            const std::wstring print = fs::absolute(target, ec).wstring();
            const std::wstring substitute = L"\\??\\" + print;
            const size_t substituteBytes = substitute.size() * sizeof(wchar_t);
            const size_t printBytes = print.size() * sizeof(wchar_t);

            // REPARSE_DATA_BUFFER (MountPointReparseBuffer) is only in the DDK headers.
            struct Header
            {
                DWORD ReparseTag;
                WORD ReparseDataLength;
                WORD Reserved;
                WORD SubstituteNameOffset;
                WORD SubstituteNameLength;
                WORD PrintNameOffset;
                WORD PrintNameLength;
            };
            std::vector<char> buffer(sizeof(Header) + substituteBytes + sizeof(wchar_t) + printBytes + sizeof(wchar_t), 0);
            auto *header = reinterpret_cast<Header *>(buffer.data());
            header->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
            header->ReparseDataLength = static_cast<WORD>(buffer.size() - 8); // everything after the first 8 bytes
            header->SubstituteNameOffset = 0;
            header->SubstituteNameLength = static_cast<WORD>(substituteBytes);
            header->PrintNameOffset = static_cast<WORD>(substituteBytes + sizeof(wchar_t));
            header->PrintNameLength = static_cast<WORD>(printBytes);
            char *names = buffer.data() + sizeof(Header);
            std::memcpy(names, substitute.data(), substituteBytes);
            std::memcpy(names + substituteBytes + sizeof(wchar_t), print.data(), printBytes);

            HANDLE h = CreateFileW(link.wstring().c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                   FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
            if (h == INVALID_HANDLE_VALUE)
                return std::unexpected("Could not open " + link.string() + " (error " + std::to_string(GetLastError()) + ")");
            DWORD returned = 0;
            const BOOL ok = DeviceIoControl(h, FSCTL_SET_REPARSE_POINT, buffer.data(), static_cast<DWORD>(buffer.size()),
                                            nullptr, 0, &returned, nullptr);
            const DWORD err = GetLastError();
            CloseHandle(h);
            if (!ok)
                return std::unexpected("Could not point " + link.string() + " at " + target.string() + " (error " +
                                       std::to_string(err) + ")");
            return {};
        }
#endif

        std::expected<void, std::string> point_current_at(const fs::path &root, const std::string &version)
        {
            const fs::path current = root / "current";
            std::error_code ec;
#ifdef _WIN32
            // A junction needs no admin rights or developer mode (directory symlinks do).
            // Re-pointing an existing junction rewrites its reparse data in one call, so
            // `current` never goes missing (renaming junctions around would leave a gap).
            if (!exists_nofollow(current) && !CreateDirectoryW(current.wstring().c_str(), nullptr))
                return std::unexpected("Could not create " + current.string() + " (error " + std::to_string(GetLastError()) + ")");
            if (auto r = set_junction_target(current, versions_dir(root) / version); !r)
                return r;
#else
            const fs::path tmp = root / "current.tmp";
            fs::remove(tmp, ec);
            fs::create_directory_symlink(fs::path("versions") / version, tmp, ec);
            if (ec)
                return std::unexpected("Could not create the 'current' link: " + ec.message());
            fs::rename(tmp, current, ec); // atomic replace
            if (ec)
                return std::unexpected("Could not activate the new version: " + ec.message());
#endif
            return {};
        }

        std::string sha256_file(const fs::path &file)
        {
            std::ifstream in(file, std::ios::binary);
            std::vector<unsigned char> hash(picosha2::k_digest_size);
            picosha2::hash256(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>(), hash.begin(), hash.end());
            return picosha2::bytes_to_hex_string(hash.begin(), hash.end());
        }

        /// Directory holding bin/<exe> inside an extracted package (searched up to 3 levels deep).
        std::optional<fs::path> find_prefix(const fs::path &dir, int depth = 0)
        {
            if (fs::exists(dir / "bin" / EXE_NAME))
                return dir;
            if (depth >= 3)
                return std::nullopt;
            std::error_code ec;
            for (const auto &entry: fs::directory_iterator(dir, ec))
                if (entry.is_directory())
                    if (auto found = find_prefix(entry.path(), depth + 1))
                        return found;
            return std::nullopt;
        }

        fs::path legacy_marker() { return get_data_dir() / "legacy_install.txt"; }

        /// The install this process runs from, if it is one we can later remove for the user.
        std::optional<fs::path> removable_legacy_location()
        {
            if (running_version_dir())
                return std::nullopt;
            const fs::path exe = get_exec_file();
#ifdef _WIN32
            const fs::path dir = exe.parent_path().parent_path();
            if (fs::exists(dir / "Uninstall.exe"))
                return dir; // the old admin NSIS install
#else
            if (exe.string().rfind("/usr/", 0) == 0)
                return exe; // the .deb
#endif
            return std::nullopt;
        }

        // ---- launchers ----------------------------------------------------

#ifdef _WIN32
        fs::path known_folder(REFKNOWNFOLDERID id)
        {
            PWSTR p = nullptr;
            fs::path out;
            if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p)))
                out = p;
            CoTaskMemFree(p);
            return out;
        }

        void set_reg_string(HKEY key, const wchar_t *name, const std::wstring &value)
        {
            RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(value.c_str()),
                           static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
        }

        void write_launchers(const fs::path &root, const std::string &version, bool first_install)
        {
            const fs::path exe = root / "current" / "bin" / EXE_NAME;
            const std::string lnk = std::string(DISPLAY_NAME) + ".lnk";

            const fs::path programs = known_folder(FOLDERID_Programs);
            if (!programs.empty())
                if (auto r = Autostart::create_shortcut(programs / lnk, exe, "", DISPLAY_NAME); !r)
                    spdlog::warn("Start Menu shortcut: {}", r.error());
            const fs::path desktop = known_folder(FOLDERID_Desktop);
            if (first_install && !desktop.empty())
                if (auto r = Autostart::create_shortcut(desktop / lnk, exe, "", DISPLAY_NAME); !r)
                    spdlog::warn("Desktop shortcut: {}", r.error());

            // "Installed apps" entry, per user (HKCU needs no admin rights).
            HKEY key = nullptr;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, UNINSTALL_KEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS)
            {
                set_reg_string(key, L"DisplayName", widen(DISPLAY_NAME));
                set_reg_string(key, L"DisplayVersion", widen(version));
                set_reg_string(key, L"Publisher", L"sshcrack");
                set_reg_string(key, L"DisplayIcon", exe.wstring());
                set_reg_string(key, L"InstallLocation", root.wstring());
                set_reg_string(key, L"URLInfoAbout", L"https://github.com/sshcrack/led-matrix");
                set_reg_string(key, L"UninstallString", quote_arg(exe.wstring()) + L" --uninstall");
                DWORD one = 1;
                RegSetValueExW(key, L"NoModify", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&one), sizeof(one));
                RegSetValueExW(key, L"NoRepair", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&one), sizeof(one));
                RegCloseKey(key);
            }
        }

        void remove_launchers()
        {
            const std::string lnk = std::string(DISPLAY_NAME) + ".lnk";
            std::error_code ec;
            fs::remove(known_folder(FOLDERID_Programs) / lnk, ec);
            fs::remove(known_folder(FOLDERID_Desktop) / lnk, ec);
            RegDeleteKeyW(HKEY_CURRENT_USER, UNINSTALL_KEY);
        }
#else
        fs::path home()
        {
            const char *h = std::getenv("HOME");
            return h ? fs::path(h) : fs::path("/tmp");
        }

        fs::path xdg_data_home()
        {
            const char *x = std::getenv("XDG_DATA_HOME");
            return (x && *x) ? fs::path(x) : home() / ".local" / "share";
        }

        std::string desktop_exec_quote(const std::string &s)
        {
            if (s.find_first_of(" \t\"'\\$`") == std::string::npos)
                return s;
            std::string out = "\"";
            for (char c: s)
            {
                if (c == '"' || c == '`' || c == '$' || c == '\\')
                    out += '\\';
                out += c;
            }
            return out + "\"";
        }

        void write_launchers(const fs::path &root, const std::string &, bool)
        {
            const fs::path exe = root / "current" / "bin" / EXE_NAME;
            const fs::path icon = root / "current" / "share" / "icons" / "hicolor" / "scalable" / "apps" / "led-matrix-desktop.svg";
            const fs::path apps = xdg_data_home() / "applications";
            std::error_code ec;
            fs::create_directories(apps, ec);
            // Same file name as the .deb's entry, so this one wins in launchers.
            std::ofstream entry(apps / "led-matrix-desktop.desktop", std::ios::trunc);
            entry << "[Desktop Entry]\n"
                  << "Type=Application\n"
                  << "Name=" << DISPLAY_NAME << "\n"
                  << "Comment=Control your RGB LED matrix panels\n"
                  << "Exec=" << desktop_exec_quote(exe.string()) << "\n"
                  << "Icon=" << icon.string() << "\n"
                  << "Categories=Utility;\n"
                  << "Terminal=false\n";
            entry.close();
            run_process({"update-desktop-database", "-q", apps.string()});

            const fs::path binDir = home() / ".local" / "bin";
            fs::create_directories(binDir, ec);
            const fs::path link = binDir / EXE_NAME;
            if (!exists_nofollow(link) || fs::is_symlink(link, ec))
            {
                fs::remove(link, ec);
                fs::create_symlink(exe, link, ec);
            }
        }

        void remove_launchers()
        {
            std::error_code ec;
            fs::remove(xdg_data_home() / "applications" / "led-matrix-desktop.desktop", ec);
            const fs::path link = home() / ".local" / "bin" / EXE_NAME;
            if (fs::is_symlink(link, ec))
                fs::remove(link, ec);
        }
#endif

        void refresh_autostart(const fs::path &root)
        {
            if (!Autostart::isEnabled(APP_NAME))
                return;
            if (auto r = Autostart::enable((root / "current" / "bin" / EXE_NAME).string(), APP_NAME); !r)
                spdlog::warn("Could not point autostart at the new install: {}", r.error());
        }

        /// Advisory file lock (flock / LockFileEx), released when the object dies.
        class FileLock
        {
        public:
            FileLock(const fs::path &file, bool exclusive, bool wait)
            {
#ifdef _WIN32
                handle_ = CreateFileW(file.wstring().c_str(), GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
                if (handle_ == INVALID_HANDLE_VALUE)
                    return;
                OVERLAPPED ov{};
                const DWORD flags = (exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0) | (wait ? 0 : LOCKFILE_FAIL_IMMEDIATELY);
                held_ = LockFileEx(handle_, flags, 0, 1, 0, &ov) != 0;
#else
                fd_ = open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
                if (fd_ < 0)
                    return;
                int rc;
                do
                    rc = flock(fd_, (exclusive ? LOCK_EX : LOCK_SH) | (wait ? 0 : LOCK_NB));
                while (rc != 0 && errno == EINTR);
                held_ = rc == 0;
#endif
            }
            ~FileLock()
            {
#ifdef _WIN32
                if (handle_ != INVALID_HANDLE_VALUE)
                {
                    if (held_)
                    {
                        OVERLAPPED ov{};
                        UnlockFileEx(handle_, 0, 1, 0, &ov);
                    }
                    CloseHandle(handle_);
                }
#else
                if (fd_ >= 0)
                    close(fd_); // releases the flock
#endif
            }
            FileLock(const FileLock &) = delete;
            FileLock &operator=(const FileLock &) = delete;
            bool held() const { return held_; }

        private:
#ifdef _WIN32
            HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
            int fd_ = -1;
#endif
            bool held_ = false;
        };

        /// Lock files live next to the install root, not in it, and are never deleted: a lock
        /// on a file that `--uninstall` removed would exclude nobody (Linux), or keep it
        /// delete-pending so the next open fails (Windows).
        fs::path lock_file(const char *name)
        {
            const fs::path dir = install_root().parent_path() / "led-matrix-desktop.locks";
            std::error_code ec;
            fs::create_directories(dir, ec);
            return dir / name;
        }

        /// Cross-process lock: one install, update, prune or uninstall at a time, so a second
        /// app instance or a concurrent `--install` cannot delete files an update is still
        /// writing. Not reentrant: take it only in the public entry points.
        struct InstallLock : FileLock
        {
            explicit InstallLock(bool wait) : FileLock(lock_file("install.lock"), true, wait) {}
        };

        /// Every running app holds a shared lock on versions/<v>/.in-use for its lifetime;
        /// pruning removes a version only when it can take that lock exclusively.
        constexpr const char *IN_USE_FILE = ".in-use";

        /// versions.lock orders "is this version in use?" against "start using it":
        /// removers hold it exclusively from the .in-use probe until the version is gone, a
        /// starting app holds it shared while it takes its .in-use lock. Short-lived on both
        /// sides (unlike InstallLock, which spans a whole download).
        struct VersionsLock : FileLock
        {
            VersionsLock(bool exclusive, bool wait)
                : FileLock(lock_file("versions.lock"), exclusive, wait) {}
        };

        /// Antivirus scanners (Defender) briefly hold freshly extracted files open,
        /// which makes renaming their folder fail with a sharing violation.
        bool rename_retrying(const fs::path &from, const fs::path &to)
        {
            std::error_code ec;
            for (int attempt = 0; attempt < 20; attempt++)
            {
                fs::rename(from, to, ec);
                if (!ec)
                    return true;
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
            spdlog::warn("Could not rename {} to {}: {}", from.string(), to.string(), ec.message());
            return false;
        }

        /// Removes everything under root. Files the OS refuses to
        /// delete (Windows: this process's own exe and DLLs, which can still be renamed)
        /// are moved into `busy`, outside root, to be deleted after this process exits.
        std::expected<void, std::string> clear_install_root(const fs::path &root, const fs::path &busy)
        {
            std::error_code ec;
#ifdef _WIN32
            RemoveDirectoryW((root / "current").wstring().c_str()); // the junction, not its target
#else
            fs::remove(root / "current", ec);
#endif
            std::vector<fs::path> paths;
            for (auto it = fs::recursive_directory_iterator(root, ec); !ec && it != fs::recursive_directory_iterator();
                 it.increment(ec))
                paths.push_back(it->path());
            if (ec)
                return std::unexpected("Could not list " + root.string() + ": " + ec.message());
            int moved = 0;
            for (auto p = paths.rbegin(); p != paths.rend(); ++p) // children before their folder
            {
                std::error_code e2;
                const bool dir = fs::is_directory(fs::symlink_status(*p, e2));
                if (fs::remove(*p, e2) || dir)
                    continue;
                fs::create_directories(busy, e2);
                fs::rename(*p, busy / (std::to_string(moved++) + "-" + p->filename().string()), e2);
                if (e2)
                    return std::unexpected("Could not remove " + p->string() + ": " + e2.message());
            }
            fs::remove(root, ec);
            if (fs::exists(root))
                return std::unexpected("Could not remove " + root.string());
            return {};
        }

        /// True when no app process holds versions/<v>. Call with VersionsLock held exclusively.
        bool version_unused(const fs::path &dir)
        {
            std::error_code ec;
            if (!fs::exists(dir / IN_USE_FILE, ec) && !ec)
                return true;
            FileLock probe(dir / IN_USE_FILE, true, false);
            return probe.held();
        }

        void report(const std::string &message, bool error)
        {
            (error ? std::cerr : std::cout) << message << std::endl;
#ifdef _WIN32
            // The GUI build has no console; the setup .exe waits on our exit code instead.
            if (error)
                MessageBoxW(nullptr, widen(message).c_str(), widen(DISPLAY_NAME).c_str(), MB_OK | MB_ICONERROR);
#endif
        }
    } // namespace

    std::string package_asset_name(const std::string &version)
    {
#ifdef _WIN32
        return "led-matrix-desktop-" + version + "-win64.zip";
#else
        return "led-matrix-desktop-" + version + "-Linux.tar.gz";
#endif
    }

    fs::path install_root()
    {
#ifdef _WIN32
        return known_folder(FOLDERID_LocalAppData) / "led-matrix-desktop";
#else
        return xdg_data_home() / "led-matrix-desktop";
#endif
    }

    bool is_managed_install() { return running_version_dir().has_value(); }

    bool is_recognized_system_install() { return removable_legacy_location().has_value(); }

    bool is_dev_build()
    {
        const fs::path dir = get_exec_dir();
        return fs::exists(dir / "CMakeCache.txt") || fs::exists(dir.parent_path() / "CMakeCache.txt") ||
               fs::exists(dir.parent_path().parent_path() / "CMakeCache.txt");
    }

    HoldResult hold_running_version()
    {
        const auto dir = running_version_dir();
        if (!dir)
            return HoldResult::Held;
        static std::unique_ptr<FileLock> held; // process lifetime
        if (held)
            return HoldResult::Held;
        VersionsLock versions(false, true);
        if (!versions.held())
        {
            spdlog::error("Could not lock {}", lock_file("versions.lock").string());
            return HoldResult::Failed;
        }
        // An update or prune may have removed this version while we were starting.
        if (!fs::exists(*dir / "bin" / EXE_NAME))
            return HoldResult::Removed;
        auto lock = std::make_unique<FileLock>(*dir / IN_USE_FILE, false, true);
        if (!lock->held())
        {
            spdlog::error("Could not lock {}", (*dir / IN_USE_FILE).string());
            return HoldResult::Failed;
        }
        held = std::move(lock);
        return HoldResult::Held;
    }

    fs::path current_exec_path() { return install_root() / "current" / "bin" / EXE_NAME; }

    namespace
    {
        void prune_locked();

        std::expected<void, std::string> install_prefix_locked(const fs::path &prefix, const std::string &version, bool move,
                                                               bool first_install)
    {
        if (!fs::exists(prefix / "bin" / EXE_NAME))
            return std::unexpected("Not a desktop package: " + (prefix / "bin" / EXE_NAME).string() + " is missing");

        const fs::path root = install_root();
        const fs::path dest = versions_dir(root) / version;
        std::error_code ec;
        fs::create_directories(versions_dir(root), ec);
        if (ec)
            return std::unexpected("Could not create " + versions_dir(root).string() + ": " + ec.message());

        const auto running = running_version_dir();
        // `current/bin/led-matrix-desktop --install` installs a version onto itself.
        const bool sameAsSource = fs::exists(dest) && fs::equivalent(prefix, dest, ec);
        bool destInUse = sameAsSource || (running && fs::equivalent(*running, dest, ec));
        {
            // Held until dest is replaced, so no app starts using the old copy in between.
            VersionsLock versions(true, true);
            if (!versions.held())
                return std::unexpected("Could not lock " + lock_file("versions.lock").string());
            // Same version already there and another app process runs it: keep it as is.
            if (!destInUse && fs::exists(dest / "bin" / EXE_NAME))
                destInUse = !version_unused(dest);
            if (!destInUse)
            {
                // Build the new copy next to dest first: `current` may point at dest, and a
                // failed copy (disk full) must not leave it pointing at nothing. Leftover
                // .new-/.old- folders are not in use, so prune removes them.
                const fs::path fresh = versions_dir(root) / (".new-" + version);
                const fs::path old = versions_dir(root) / (".old-" + version);
                fs::remove_all(fresh, ec);
                // An earlier attempt failed between moving dest away and putting it back: restore it.
                if (fs::exists(old, ec) && !fs::exists(dest, ec))
                    rename_retrying(old, dest);
                fs::remove_all(old, ec);
                bool placed = false;
                if (move)
                    placed = rename_retrying(prefix, fresh);
                if (!placed)
                    if (auto r = copy_tree(prefix, fresh); !r)
                    {
                        fs::remove_all(fresh, ec);
                        return r;
                    }
                const bool hadOld = fs::exists(fs::symlink_status(dest, ec));
                if (hadOld && !rename_retrying(dest, old))
                {
                    fs::remove_all(fresh, ec);
                    return std::unexpected("Could not replace " + dest.string() + ": it is in use");
                }
                if (!rename_retrying(fresh, dest))
                {
                    fs::remove_all(fresh, ec);
                    if (hadOld && !rename_retrying(old, dest))
                        return std::unexpected("Could not move the new version into " + dest.string() +
                                               ", and the previous copy is still in " + old.string() +
                                               " (it is restored on the next install attempt)");
                    return std::unexpected("Could not move the new version into " + dest.string());
                }
                fs::remove_all(old, ec);
            }
        } // before prune_locked(), which takes VersionsLock itself

        // Migrating away from a .deb / admin install: remember it so the app can offer to remove it.
        if (const auto legacy = removable_legacy_location())
            if (std::ofstream marker(legacy_marker(), std::ios::trunc); marker)
                marker << legacy->string();

        if (auto r = point_current_at(root, version); !r)
            return r;
        write_launchers(root, version, first_install);
        refresh_autostart(root);
        prune_locked();
        spdlog::info("Installed LED Matrix Controller {} to {}", version, dest.string());
        return {};
    }
    } // namespace

    std::expected<void, std::string> install_prefix(const fs::path &prefix, const std::string &version, bool move,
                                                    bool first_install)
    {
        InstallLock lock(true);
        if (!lock.held())
            return std::unexpected("Could not lock " + lock_file("install.lock").string());
        return install_prefix_locked(prefix, version, move, first_install);
    }

    std::expected<void, std::string> download_and_install(const std::string &url, const std::string &sha256,
                                                          const std::string &version,
                                                          const std::function<void(size_t, size_t)> &progress)
    {
        InstallLock lock(true); // held until the new version is in place
        if (!lock.held())
            return std::unexpected("Could not lock " + lock_file("install.lock").string());
        const fs::path root = install_root();
        const fs::path downloads = root / "downloads";
        std::error_code ec;
        fs::create_directories(downloads, ec);
        const fs::path file = downloads / package_asset_name(version);

        spdlog::info("Downloading desktop update {} from {}", version, url);
        {
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            if (!out)
                return std::unexpected("Could not write " + file.string());
            cpr::Session session;
            session.SetUrl(cpr::Url{url});
            session.SetHeader({{"User-Agent", user_agent()}});
            session.SetProgressCallback(cpr::ProgressCallback{
                [&progress](cpr::cpr_pf_arg_t total, cpr::cpr_pf_arg_t now, cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, intptr_t) {
                    if (progress)
                        progress(static_cast<size_t>(now), static_cast<size_t>(total));
                    return true;
                }});
            const cpr::Response res = session.Download(cpr::WriteCallback{[&out](std::string_view data, intptr_t) {
                out.write(data.data(), static_cast<std::streamsize>(data.size()));
                return static_cast<bool>(out);
            }});
            out.close();
            if (res.error)
                return std::unexpected("Download failed: " + res.error.message);
            if (res.status_code != 200)
                return std::unexpected("Download failed: HTTP " + std::to_string(res.status_code));
            if (!out)
                return std::unexpected("Could not write " + file.string());
        }

        // Never install an unverified package. GitHub records a sha256 for every release asset.
        std::string expected = sha256;
        std::ranges::transform(expected, expected.begin(), [](unsigned char c) { return std::tolower(c); });
        if (expected.size() != 64 || !std::ranges::all_of(expected, [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); }))
        {
            fs::remove(file, ec);
            return std::unexpected("The release has no valid sha256 digest for " + package_asset_name(version) +
                                   ", so it cannot be verified. Install it manually from the releases page.");
        }
        if (const std::string actual = sha256_file(file); actual != expected)
        {
            fs::remove(file, ec);
            return std::unexpected("Checksum mismatch (expected " + expected + ", got " + actual + ")");
        }

        const fs::path staging = root / ("staging-" + version);
        fs::remove_all(staging, ec);
        fs::create_directories(staging, ec);
#ifdef _WIN32
        // bsdtar ships with Windows 10 1803+ and reads zip.
        const int rc = run_process({(system32() / L"tar.exe").wstring(), L"-xf", file.wstring(), L"-C", staging.wstring()});
#else
        const int rc = run_process({"tar", "-xzf", file.string(), "-C", staging.string()});
#endif
        fs::remove(file, ec);
        if (rc != 0)
        {
            fs::remove_all(staging, ec);
            return std::unexpected("Could not extract the update (tar exit code " + std::to_string(rc) + ")");
        }
        const auto prefix = find_prefix(staging);
        if (!prefix)
        {
            fs::remove_all(staging, ec);
            return std::unexpected("The update package does not contain bin/" + std::string(EXE_NAME));
        }
        auto result = install_prefix_locked(*prefix, version, true, false);
        fs::remove_all(staging, ec);
        return result;
    }

    void prune()
    {
        InstallLock lock(false);
        if (!lock.held())
        {
            spdlog::info("Another instance is installing an update; not pruning now");
            return;
        }
        prune_locked();
    }

    namespace
    {
    void prune_locked()
    {
        const fs::path root = install_root();
        std::error_code ec;
        const fs::path current = fs::weakly_canonical(root / "current", ec);
        const auto running = running_version_dir();
        VersionsLock versions(true, true); // no app starts an old version while it is removed
        if (!versions.held())
        {
            spdlog::warn("Could not lock {}; not pruning", lock_file("versions.lock").string());
            return;
        }
        std::vector<fs::path> entries;
        for (const auto &entry: fs::directory_iterator(versions_dir(root), ec))
            entries.push_back(entry.path());
        for (const auto &path: entries)
        {
            // The only copy of a version whose replacement failed half-way: put it back, don't prune it.
            if (const std::string name = path.filename().string(); name.rfind(".old-", 0) == 0)
            {
                std::error_code e2;
                if (const fs::path dest = versions_dir(root) / name.substr(5); !fs::exists(dest, e2))
                {
                    rename_retrying(path, dest);
                    continue;
                }
            }
            std::error_code e2;
            if (fs::equivalent(path, current, e2) || (running && fs::equivalent(path, *running, e2)))
                continue;
            // Another app process (e.g. still open while `--install` ran) uses this version.
            if (!version_unused(path))
            {
                spdlog::info("Keeping desktop version {}: still running", path.filename().string());
                continue;
            }
            fs::remove_all(path, e2); // Windows can still hold files briefly; retried next time
            if (!e2)
                spdlog::info("Removed old desktop version {}", path.filename().string());
        }
        for (const auto &entry: fs::directory_iterator(root, ec))
        {
            const std::string name = entry.path().filename().string();
            if (name.rfind("staging-", 0) == 0 || name == "downloads")
            {
                std::error_code e2;
                fs::remove_all(entry.path(), e2);
            }
        }
    }
    } // namespace

    std::optional<int> handle_command_line(int argc, char *argv[])
    {
        bool install = false, uninstall = false, launch = false;
        for (int i = 1; i < argc; i++)
        {
            const std::string a = argv[i];
            install |= a == "--install";
            uninstall |= a == "--uninstall";
            launch |= a == "--launch";
        }
        if (!install && !uninstall)
            return std::nullopt;

        if (install)
        {
            const fs::path prefix = get_exec_dir().parent_path();
            if (auto r = install_prefix(prefix, PROJECT_VERSION_STRING, false, true); !r)
            {
                report("Installation failed: " + r.error(), true);
                return 1;
            }
            report(std::string(DISPLAY_NAME) + " " + PROJECT_VERSION_STRING + " installed to " + install_root().string() +
                       "\nIt keeps itself up to date.",
                   false);
            if (launch)
            {
#ifdef _WIN32
                run_process({current_exec_path().wstring()}, false);
#else
                run_process({current_exec_path().string()}, false);
#endif
            }
            return 0;
        }

        // --uninstall: settings in the data dir stay.
        const fs::path root = install_root();
        if (!fs::exists(root))
        {
            report(std::string(DISPLAY_NAME) + " is not installed for this user.", false);
            return 0;
        }
        const fs::path busy = root.parent_path() / ("led-matrix-desktop.uninstalled-" + std::to_string(
#ifdef _WIN32
                                                                                         GetCurrentProcessId()
#else
                                                                                         getpid()
#endif
                                                                                             ));
        {
            // No update may run while the files go, and no app may still run from them.
            InstallLock installLock(true);
            VersionsLock versionsLock(true, true);
            if (!installLock.held() || !versionsLock.held())
            {
                report("Uninstall failed: could not lock " + root.string(), true);
                return 1;
            }
            std::error_code ec;
            for (const auto &entry: fs::directory_iterator(versions_dir(root), ec))
                if (!version_unused(entry.path()))
                {
                    report("Close " + std::string(DISPLAY_NAME) + " (it is still running) and uninstall again.", true);
                    return 1;
                }

            remove_launchers();
            if (Autostart::isEnabled(APP_NAME))
                (void)Autostart::disable(APP_NAME);
            if (auto r = clear_install_root(root, busy); !r)
            {
                report("Uninstall failed: " + r.error(), true);
                return 1;
            }
        }
#ifdef _WIN32
        if (fs::exists(busy))
        {
            // Our own executable and DLLs stay locked until this process exits; cmd.exe deletes
            // them right after, retrying while they are released. They are outside root, so
            // nothing a later install writes is touched. cmd.exe has its own quoting (no \"
            // escapes), so build this line by hand.
            const std::wstring b = L"\"" + busy.wstring() + L"\"";
            run_cmdline(quote_arg((system32() / L"cmd.exe").wstring()) + L" /d /c for /l %i in (1,1,30) do @if exist " + b +
                            L" (ping -n 2 127.0.0.1 >nul & rmdir /s /q " + b + L" 2>nul)",
                        false);
        }
#endif
        report(std::string(DISPLAY_NAME) + " was uninstalled. Your settings are kept in " + get_data_dir().string(), false);
        return 0;
    }

    bool restart_into_current(bool start_minimized, bool after_update, bool version_removed)
    {
        const fs::path exe = current_exec_path();
        spdlog::info("Restarting into {}", exe.string());
#ifdef _WIN32
        // --start-minimized first: releases before 1.21 only look at argv[1].
        std::vector<std::wstring> args{exe.wstring()};
        if (start_minimized)
            args.emplace_back(L"--start-minimized");
        if (after_update)
            args.emplace_back(L"--after-update");
        if (version_removed)
            args.emplace_back(L"--version-was-removed");
        return run_process(args, false) == 0;
#else
        spdlog::default_logger()->flush();
        const std::string exeStr = exe.string();
        std::string minimized = "--start-minimized", afterUpdate = "--after-update", removed = "--version-was-removed";
        // --start-minimized first: releases before 1.21 only look at argv[1].
        std::vector<char *> argv{const_cast<char *>(exeStr.c_str())};
        if (start_minimized)
            argv.push_back(minimized.data());
        if (after_update)
            argv.push_back(afterUpdate.data());
        if (version_removed)
            argv.push_back(removed.data());
        argv.push_back(nullptr);
        execv(exeStr.c_str(), argv.data());
        spdlog::error("Could not restart into {}: {}", exeStr, std::strerror(errno));
        return false;
#endif
    }

    std::optional<fs::path> legacy_install()
    {
        std::ifstream in(legacy_marker());
        std::string path;
        if (!in || !std::getline(in, path) || path.empty())
            return std::nullopt;
        std::error_code ec;
        if (!fs::exists(path, ec))
        {
            forget_legacy_install();
            return std::nullopt;
        }
        return fs::path(path);
    }

    bool remove_legacy_install()
    {
        const auto legacy = legacy_install();
        if (!legacy)
            return true;
#ifdef _WIN32
        const std::wstring uninstaller = (*legacy / "Uninstall.exe").wstring();
        SHELLEXECUTEINFOW sei{};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_FLAG_NO_UI;
        sei.lpVerb = L"runas";
        sei.lpFile = uninstaller.c_str();
        sei.lpParameters = L"/S";
        sei.nShow = SW_HIDE;
        if (!ShellExecuteExW(&sei))
            return false;
#else
        if (run_process({"pkexec", "dpkg", "--remove", "led-matrix-desktop"}) != 0)
            return false;
#endif
        forget_legacy_install();
        return true;
    }

    void forget_legacy_install()
    {
        std::error_code ec;
        fs::remove(legacy_marker(), ec);
    }
} // namespace SelfUpdate
