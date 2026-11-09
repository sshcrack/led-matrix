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
#include <vector>

#ifdef _WIN32
#include <windows.h>
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

        std::expected<void, std::string> point_current_at(const fs::path &root, const std::string &version)
        {
            const fs::path current = root / "current";
            std::error_code ec;
#ifdef _WIN32
            // Junctions need no admin rights or developer mode (directory symlinks do).
            const fs::path next = root / "current.new";
            const fs::path old = root / "current.old";
            fs::remove(next, ec);
            fs::remove(old, ec);
            const int rc = run_process({(system32() / L"cmd.exe").wstring(), L"/d", L"/c", L"mklink", L"/J",
                                        next.wstring(), (versions_dir(root) / version).wstring()});
            if (rc != 0 || !exists_nofollow(next))
                return std::unexpected("Could not create the 'current' junction (mklink exit code " + std::to_string(rc) + ")");
            if (exists_nofollow(current))
            {
                fs::rename(current, old, ec);
                if (ec)
                    return std::unexpected("Could not replace 'current': " + ec.message());
            }
            fs::rename(next, current, ec);
            if (ec)
                return std::unexpected("Could not activate the new version: " + ec.message());
            fs::remove(old, ec); // removes the junction only, never its target
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

        /// Cross-process lock on <root>/.install.lock: one install, update or prune at a time,
        /// so a second app instance or a concurrent `--install` cannot delete files an update
        /// is still writing. Not reentrant: take it only in the public entry points.
        class InstallLock
        {
        public:
            explicit InstallLock(bool wait)
            {
                const fs::path root = install_root();
                std::error_code ec;
                fs::create_directories(root, ec);
                const fs::path file = root / ".install.lock";
#ifdef _WIN32
                handle_ = CreateFileW(file.wstring().c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (handle_ == INVALID_HANDLE_VALUE)
                    return;
                OVERLAPPED ov{};
                const DWORD flags = LOCKFILE_EXCLUSIVE_LOCK | (wait ? 0 : LOCKFILE_FAIL_IMMEDIATELY);
                held_ = LockFileEx(handle_, flags, 0, 1, 0, &ov) != 0;
#else
                fd_ = open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
                if (fd_ < 0)
                    return;
                int rc;
                do
                    rc = flock(fd_, LOCK_EX | (wait ? 0 : LOCK_NB));
                while (rc != 0 && errno == EINTR);
                held_ = rc == 0;
#endif
            }
            ~InstallLock()
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
            InstallLock(const InstallLock &) = delete;
            InstallLock &operator=(const InstallLock &) = delete;
            bool held() const { return held_; }

        private:
#ifdef _WIN32
            HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
            int fd_ = -1;
#endif
            bool held_ = false;
        };

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
        const bool destIsRunning = running && fs::equivalent(*running, dest, ec);
        if (!destIsRunning)
        {
            fs::remove_all(dest, ec);
            bool placed = false;
            if (move)
            {
                // Antivirus scanners (Defender) briefly hold freshly extracted files open,
                // which makes renaming their folder fail with a sharing violation.
                for (int attempt = 0; attempt < 20 && !placed; attempt++)
                {
                    fs::rename(prefix, dest, ec);
                    placed = !ec;
                    if (!placed)
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                }
            }
            if (!placed)
                if (auto r = copy_tree(prefix, dest); !r)
                {
                    fs::remove_all(dest, ec);
                    return r;
                }
        }

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
            return std::unexpected("Could not lock " + (install_root() / ".install.lock").string());
        return install_prefix_locked(prefix, version, move, first_install);
    }

    std::expected<void, std::string> download_and_install(const std::string &url, const std::string &sha256,
                                                          const std::string &version,
                                                          const std::function<void(size_t, size_t)> &progress)
    {
        InstallLock lock(true); // held until the new version is in place
        if (!lock.held())
            return std::unexpected("Could not lock " + (install_root() / ".install.lock").string());
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

        if (!sha256.empty())
        {
            std::string expected = sha256;
            std::ranges::transform(expected, expected.begin(), [](unsigned char c) { return std::tolower(c); });
            if (const std::string actual = sha256_file(file); actual != expected)
            {
                fs::remove(file, ec);
                return std::unexpected("Checksum mismatch (expected " + expected + ", got " + actual + ")");
            }
        }
        else
        {
            spdlog::warn("Release asset has no sha256 digest; skipping verification");
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
        for (const auto &entry: fs::directory_iterator(versions_dir(root), ec))
        {
            std::error_code e2;
            if (fs::equivalent(entry.path(), current, e2) || (running && fs::equivalent(entry.path(), *running, e2)))
                continue;
            fs::remove_all(entry.path(), e2); // a still-running older version keeps its files busy on Windows; retried next time
            if (!e2)
                spdlog::info("Removed old desktop version {}", entry.path().filename().string());
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
        remove_launchers();
        if (Autostart::isEnabled(APP_NAME))
            (void)Autostart::disable(APP_NAME);
        const fs::path root = install_root();
#ifdef _WIN32
        // Our own files are in use until we exit; remove them right after.
        // cmd.exe has its own quoting (no \" escapes), so build this line by hand.
        run_cmdline(quote_arg((system32() / L"cmd.exe").wstring()) + L" /d /c ping -n 3 127.0.0.1 >nul & rmdir /s /q \"" +
                        root.wstring() + L"\"",
                    false);
#else
        std::error_code ec;
        fs::remove_all(root, ec);
#endif
        report(std::string(DISPLAY_NAME) + " was uninstalled. Your settings are kept in " + get_data_dir().string(), false);
        return 0;
    }

    bool restart_into_current(bool start_minimized)
    {
        const fs::path exe = current_exec_path();
        spdlog::info("Restarting into {}", exe.string());
#ifdef _WIN32
        // --start-minimized first: releases before 1.21 only look at argv[1].
        std::vector<std::wstring> args{exe.wstring()};
        if (start_minimized)
            args.emplace_back(L"--start-minimized");
        args.emplace_back(L"--after-update");
        return run_process(args, false) == 0;
#else
        spdlog::default_logger()->flush();
        const std::string exeStr = exe.string();
        std::string minimized = "--start-minimized", afterUpdate = "--after-update";
        // --start-minimized first: releases before 1.21 only look at argv[1].
        std::vector<char *> argv{const_cast<char *>(exeStr.c_str())};
        if (start_minimized)
            argv.push_back(minimized.data());
        argv.push_back(afterUpdate.data());
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
        if (!fs::exists(path))
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
