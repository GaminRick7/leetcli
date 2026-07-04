#include "leetcode_api.h"
#include "utils.h"
#include "tui.h"
#include <iostream>

int main(int argc, char **argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (argc < 2) {
        leetcli::run_tui();
        return 0;
    }

    std::string command = argv[1];

    if (command == "--interactive") {
        leetcli::run_tui();
        return 0;
    }

    if (command == "init") {
        leetcli::init_problems_folder();
        return 0;
    }

    if (command == "fetch") {

        if (argc < 3) {
            std::cerr << "Usage: leetcli fetch <slug|number> [--lang=cpp|python3|java]\n";
            return 1;
        }

        std::string slug = argv[2];

        if (slug == "daily") {
            slug = leetcli::get_daily_question_slug();
        } else if (!slug.empty() &&
                   slug.find_first_not_of("0123456789") == std::string::npos) {
            // A purely numeric argument is a problem number (e.g. `fetch 1`).
            slug = leetcli::get_slug_by_number(std::stoi(slug));
            if (slug.empty()) return 1;
        }

        std::string lang_override;

        // Check for --lang=xxx
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.rfind("--lang=", 0) == 0) {
                lang_override = arg.substr(7); // everything after --lang=
            }
        }

        std::string problem = leetcli::fetch_problem(slug, lang_override); // pass override to function
        std::cout << problem << "\n";
        return 0;
    }

    if (command == "solve") {
        if (argc < 3) {
            std::cerr << "Usage: leetcli solve <slug> [--lang=cpp|python3|java]\n";
            return 1;
        }
        std::string lang_override;

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.rfind("--lang=", 0) == 0) {
                lang_override = arg.substr(7);
            }
        }
        std::string slug = argv[2];
        if (slug == "daily") {
            slug = leetcli::get_daily_question_slug();
        }
        leetcli::solve_problem(slug, lang_override);
        return 0;
    }
    if (command == "hints") {
        if (argc < 3) {
            std::cerr << "Usage: leetcli hints <slug>\n";
            return 1;
        }

        std::string slug = argv[2];
        if (slug == "daily") {
            slug = leetcli::get_daily_question_slug();
        }
        leetcli::fetch_problem_hints(slug);
        return 0;
    }
    if (command == "topics") {
        if (argc < 3) {
            std::cerr << "Usage: leetcli topics <slug>\n";
            return 1;
        }

        std::string slug = argv[2];
        if (slug == "daily") {
            slug = leetcli::get_daily_question_slug();
        }
        leetcli::fetch_problem_topics(slug);
        return 0;
    }
    if (command == "list") {
        leetcli::list_fetched_problems();
        return 0;
    }

    if (command == "login") {
        leetcli::set_session_cookie();
        return 0;
    }
    if (command == "submit") {
        if (argc < 3) {
            std::cerr << "Usage: leetcli submit <slug|number> [--lang=cpp|python3|java]\n";
            return 1;
        }
        std::string slug = argv[2];
        if (slug == "daily") {
            slug = leetcli::get_daily_question_slug();
        } else if (!slug.empty() &&
                   slug.find_first_not_of("0123456789") == std::string::npos) {
            slug = leetcli::get_slug_by_number(std::stoi(slug));
            if (slug.empty()) return 1;
        }
        std::string lang_override;

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.rfind("--lang=", 0) == 0) {
                lang_override = arg.substr(7);
            }
        }

        leetcli::submit_solution(slug, lang_override);
        return 0;
    }
    if (command == "run") {
        if (argc < 3) {
            std::cerr << "Usage: leetcli run <slug|number> [--lang=cpp|python3|java]\n";
            return 1;
        }
        std::string slug = argv[2];
        if (slug == "daily") {
            slug = leetcli::get_daily_question_slug();
        } else if (!slug.empty() &&
                   slug.find_first_not_of("0123456789") == std::string::npos) {
            slug = leetcli::get_slug_by_number(std::stoi(slug));
            if (slug.empty()) return 1;
        }
        std::string lang_override;

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.rfind("--lang=", 0) == 0) {
                lang_override = arg.substr(7);
            }
        }
        leetcli::run_tests(slug, lang_override);
        return 0;
    } if (command == "config") {
        leetcli::handle_config_command(args);
        return 0;
    }
    if (command == "runtime") {
        if (argc < 3) {
            std::cerr << "Usage: leetcli runtime <slug> [--lang=cpp|python3|java]\n";
            return 1;
        }
        std::string slug = argv[2];
        if (slug == "daily") {
            slug = leetcli::get_daily_question_slug();
        }
        std::string lang_override;

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.rfind("--lang=", 0) == 0) {
                lang_override = arg.substr(7);
            }
        }
        leetcli::analyze_runtime(slug, lang_override);
        return 0;
    }
    if (command == "hint") {
        if (argc < 3) {
            std::cerr << "Usage: leetcli hint <slug> [--lang=cpp|python3|java]\n";
            return 1;
        }
        std::string slug = argv[2];
        if (slug == "daily") {
            slug = leetcli::get_daily_question_slug();
        }
        std::string lang_override;

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.rfind("--lang=", 0) == 0) {
                lang_override = arg.substr(7);
            }
        }
        leetcli::give_hint(slug, lang_override);
        return 0;
    }
    if (command == "reset") {
        if (argc < 3) {
            std::cerr << "Usage: leetcli reset <slug>\n";
            return 1;
        }
        std::string slug = argv[2];
        if (slug == "daily") {
            slug = leetcli::get_daily_question_slug();
        }
        leetcli::reset_solution(slug);
        return 0;
    }
    if (command == "sync") {
        int limit = 0;  // 0 = all
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.rfind("--limit=", 0) == 0) limit = std::atoi(arg.c_str() + 8);
        }
        std::cout << "Fetching your solved & attempted problem list (this can take a while)...\n";
        leetcli::sync_problems(limit, [](const leetcli::SyncProgress& p) {
            if (!p.error.empty()) {
                std::cerr << p.error << "\n";
                return;
            }
            if (p.finished) {
                std::cout << "Sync complete: " << p.done << " problem(s) processed.\n";
                return;
            }
            if (p.total > 0 && !p.current.empty()) {
                std::cout << "[" << p.done << "/" << p.total << "] " << p.current
                          << " - " << p.last_result << "\n";
            }
        });
        return 0;
    }
    if (command == "browse" || command == "search") {
        int page = 1;
        std::string keyword;
        // `search <kw>` is a thin alias: the first non-flag arg is the keyword.
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.rfind("--page=", 0) == 0) page = std::atoi(arg.c_str() + 7);
            else if (arg.rfind("--search=", 0) == 0) keyword = arg.substr(9);
            else if (command == "search" && arg.rfind("--", 0) != 0 && keyword.empty()) keyword = arg;
        }
        leetcli::browse_problems(page, keyword);
        return 0;
    }
    if (command == "daily") {
        leetcli::show_daily();
        return 0;
    }
    if (command == "start") {
        if (argc < 3) {
            std::cerr << "Usage: leetcli start <slug> [--lang=cpp|python3|java|...]\n";
            return 1;
        }
        std::string slug = argv[2];
        if (slug == "daily") {
            slug = leetcli::get_daily_question_slug();
        }
        std::string lang_override;
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.rfind("--lang=", 0) == 0) lang_override = arg.substr(7);
        }
        leetcli::start_solution(slug, lang_override);
        return 0;
    }
    if (command == "help") {
        std::cout << "leetcli - LeetCode CLI Tool\n\n"
                  << "Main Usage:\n"
                  << "  leetcli                              Launch the interactive terminal UI (recommended)\n"
                  << "  leetcli --interactive                Same as running leetcli with no arguments\n\n"
                  << "Other commands:\n"
                  << "  leetcli init                        Initialize the problems directory in your current directory\n"
                  << "  leetcli fetch <slug|number> [--lang=...]  Fetch a problem by slug, number (e.g. 1), or 'daily' (langs: cpp, python3, java)\n"
                  << "  leetcli browse [--page=N] [--search=kw]  Browse the LeetCode catalog, 100 per page (alias: search <kw>)\n"
                  << "  leetcli daily                       Show today's daily challenge\n"
                  << "  leetcli start <slug> [--lang=...]   Write starter code for a fetched problem (18 languages supported)\n"
                  << "  leetcli solve <slug> [--lang=...]   Open the solution file in your default editor\n"
                  << "  leetcli list                        List all fetched problems with difficulty & solved/attempted status\n"
                  << "  leetcli login                       Set your LEETCODE_SESSION and CSRF token\n"
                  << "  leetcli sync [--limit=N]            Download all your solved & attempted problems (with your submitted code) locally\n"
                  << "  leetcli reset <slug>                Delete your local solution file for a problem\n"
                  << "  leetcli run <slug>  [--lang=...]    Run your solution against example testcases (per-testcase pass/fail)\n"
                  << "  leetcli submit <slug> [--lang=...]  Submit your solution (verdict + runtime/memory percentiles)\n"
                  << "  leetcli runtime <slug> [--lang=...] Analyze time/space complexity using Gemini\n"
                  << "  leetcli hint <slug> [--lang=...]    Ask Gemini for a helpful hint based on your solution progress\n"
                  << "  leetcli hints <slug>                Gets the hints for the given problem in leetcode\n"
                  << "  leetcli topics <slug>               Gets the topics for the given problem in leetcode\n"
                  << "  leetcli config <subcommand>         Configure leetcli (show | set-gemini-key | set-lang | set-editor | set-problems-dir | set-image-rendering)\n"
                  << "  leetcli help                        Show this help message\n";
        return 0;
    }

    std::cerr << "Unknown command: " << command << "\n";
    return 1;
}
