#pragma once

#include <string>
#include <filesystem>
#include <optional>
#include <vector>

#include <ftxui/dom/elements.hpp>

namespace leetcli {
    void set_gemini_key(const std::string& key);
    std::string get_gemini_key();
    static std::filesystem::path get_home();
    std::string get_file_extension(const std::string& filename);
    void init_problems_folder();
    std::string html_to_text(const std::string& html);
    // Parses LeetCode problem-description HTML into richly-styled ftxui
    // elements (bold/italic/code spans, word-wrapped paragraphs, lists,
    // preformatted code blocks, ...). Used by the TUI to render descriptions
    // directly from HTML instead of a flattened markdown/plaintext dump.
    ftxui::Elements html_to_ftxui(const std::string& html);
    void write_markdown_file(const std::string& path, const std::string& title, const std::string& markdown);
    // Writes the raw problem-description HTML to disk (description.html),
    // alongside the README.md markdown rendering, so the TUI can render it
    // richly without needing to re-fetch from the network.
    void write_html_file(const std::string& path, const std::string& html);
    // Writes one string per line (used for topics.txt, hints.txt, .slug).
    void write_lines_file(const std::string& path, const std::vector<std::string>& lines);
    void write_solution_file(const std::string& path, const std::string& code);
    void launch_in_editor(const std::string& path);
    // "editor" from config.json, else $EDITOR, else "" (no preference — the
    // caller applies its own platform-specific fallback).
    std::string get_preferred_editor();
    // Persists the chosen editor command to config.json's "editor" field —
    // used by the TUI's onboarding wizard after its editor-picker UI.
    void set_editor_preference(const std::string& editor);
    std::string get_problems_dir();
    std::string get_preferred_language();
    // True once ~/.leetcli/config.json exists. Used by the TUI to decide
    // whether to run its first-launch onboarding wizard.
    bool is_leetcli_initialized();
    // Writes the initial config.json (problems_dir + lang) and creates
    // problems_dir, without touching stdin — the non-interactive counterpart
    // to init_problems_folder(), used by the TUI's onboarding wizard after
    // its own folder-picker UI has chosen a destination.
    void write_initial_config(const std::string& problems_dir, const std::string& lang);
    // Merges leetcode_session/csrf_token into the existing config.json
    // without touching stdin — the non-interactive counterpart to
    // set_session_cookie(), used by the TUI's onboarding wizard after its
    // own login-form UI has collected the values.
    void save_session_tokens(const std::string& session, const std::string& csrf);
    void set_session_cookie();
    std::string get_session_cookie();
    std::string get_csrf_token();
    int get_solution_folder(const std::string &slug, std::string &folder_path);
    std::string get_question_id(const std::string& slug, const std::string& session, const std::string& csrf);
    void fetch_testcases(const std::string& slug, const std::string& folder_path);
    std::vector<std::string> load_testcases(const std::string& filepath);
    int get_solution_filepath(const std::string& slug, std::string& solution_file,  const std::optional<std::string> &language = std::nullopt);
    void handle_config_command(const std::vector<std::string> &args);
}
