#include "leetcode_api.h"
#include "utils.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <regex>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace leetcli {

    // LeetCode's bot protection rejects requests to the run/submit endpoints
    // that don't look like a real browser (cpr's default User-Agent gets a
    // 403), so run_against_testcases/submit_and_poll spoof one.
    static const char *kBrowserUserAgent =
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

    void give_hint(const std::string& slug, const std::string &lang_override) {
        std::string solution_path;
        std::string folder_path;
        if (!lang_override.empty()) {
            get_solution_filepath(slug, solution_path, lang_override);
        } else {
            get_solution_filepath(slug, solution_path);
        }
        get_solution_folder(slug, folder_path);
        std::string readme_path = folder_path + "/README.md";

        std::ifstream readme_file(readme_path), solution_file(solution_path);
        if (!readme_file || !solution_file) {
            std::cerr << "❌ Missing README or solution file for " << slug << "\n";
            return;
        }

        std::string description((std::istreambuf_iterator<char>(readme_file)), std::istreambuf_iterator<char>());
        std::string code((std::istreambuf_iterator<char>(solution_file)), std::istreambuf_iterator<char>());
        std::string api_key = get_gemini_key(); // or your Gemini key

        if (api_key.empty()) {
            std::cerr << "❌ No API key found. Use `leetcli config set-openai-key <your-key>`.\n";
            return;
        }

        std::string prompt =
            "You are a helpful coding assistant. Based on the following LeetCode problem description and the user's current partial solution, provide a helpful **hint** that nudges them toward the next step without giving away the full solution.\n\n"
            "**Problem Description:**\n" + description + "\n\n"
            "**Current Code:**\n" + code + "\n\n"
            "**Hint (as helpful and short as possible):**";

        std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + api_key;

        nlohmann::json payload = {
            {"contents", {{
                {"parts", {{
                    {"text", prompt}
                }}}
            }}}
        };

        cpr::Response r = cpr::Post(
            cpr::Url{url},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{payload.dump()}
        );

        if (r.status_code != 200) {
            std::cerr << "❌ Gemini API call failed: " << r.status_code << "\n" << r.text << "\n";
            std::cout << "Error: Gemini API call failed.";
        }

        try {
            auto response_json = nlohmann::json::parse(r.text);
            auto text = response_json["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
            std::cout <<"\n💡 Hint:\n"<< text;
        } catch (const std::exception& e) {
            std::cerr << "❌ Failed to parse Gemini response: " << e.what() << "\n";
            std::cout << "Error: Failed to parse Gemini response.";
        }
    }

    void analyze_runtime(const std::string& slug, const std::string &lang_override) {
        std::string path;
        if (!lang_override.empty()) {
            get_solution_filepath(slug, path, lang_override);
        } else {
            get_solution_filepath(slug, path);
        }

            std::ifstream file(path);
            if (!file) {
                std::cerr << "❌ Could not open solution file for " << slug << "\n";
                return;
            }

            std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            std::string api_key = get_gemini_key(); // Should be renamed for Gemini if needed

            if (api_key.empty()) {
                std::cerr << "❌ No Gemini API key found. Use `leetcli config set-gemini-key <your-key>` first.\n";
                return;
            }

            // Prompt for Gemini
            std::string prompt =
                "Analyze the time and space complexity of the following code and return a JSON object like:\n"
                "{ \"time\": \"O(n)\", \"space\": \"O(1)\" }\n"
                "If the code is invalid or empty, return:\n"
                "{ \"error\": \"Invalid or empty code\" }\n\n"
                "Code:\n" + code;

            // Construct Gemini request payload
            nlohmann::json payload = {
                {"contents", {{
                    {"parts", {{
                        {"text", prompt}
                    }}}
                }}},
                {"generationConfig", {
                    {"responseMimeType", "application/json"},
                    {"responseSchema", {
                        {"type", "OBJECT"},
                        {"properties", {
                            {"time", {{"type", "STRING"}}},
                            {"space", {{"type", "STRING"}}},
                            {"error", {{"type", "STRING"}}}
                        }},
                        {"required", {"time", "space"}}
                    }}
                }}
            };

            std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + api_key;

            cpr::Response r = cpr::Post(
                cpr::Url{url},
                cpr::Header{{"Content-Type", "application/json"}},
                cpr::Body{payload.dump()}
            );

            if (r.status_code != 200) {
                std::cerr << "❌ Gemini API call failed: " << r.status_code << "\n" << r.text << "\n";
                return;
            }

            nlohmann::json response_json = nlohmann::json::parse(r.text);
            try {
                nlohmann::json part = response_json["candidates"][0]["content"]["parts"][0];
                std::string raw_json = part["text"].get<std::string>();

                nlohmann::json inner = nlohmann::json::parse(raw_json);

                std::cout << "\n🧠 AI Runtime Analysis (Experimantal):\n";
                if (inner.contains("error")) {
                    std::cout << "  ⚠️  " << inner["error"] << "\n";
                } else {
                    std::cout << "  Time:  " << inner["time"] << "\n";
                    std::cout << "  Space: " << inner["space"] << "\n";
                }
            } catch (const std::exception &e) {
                std::cerr << "Failed to parse inner JSON: " << e.what() << "\n";
                std::cerr << "Raw text:\n" << r.text << "\n";
            }
    }

    ProblemPage fetch_problem_page(int skip, const std::string& keyword) {
        const std::string session = get_session_cookie();
        const std::string csrf = get_csrf_token();

        nlohmann::json filters = nlohmann::json::object();
        if (!keyword.empty()) filters["searchKeywords"] = keyword;

        nlohmann::json body = {
            {"operationName", "problemsetQuestionList"},
            {"query", R"(
                query problemsetQuestionList($categorySlug: String, $limit: Int, $skip: Int, $filters: QuestionListFilterInput) {
                    problemsetQuestionList: questionList(
                        categorySlug: $categorySlug
                        limit: $limit
                        skip: $skip
                        filters: $filters
                    ) {
                        total: totalNum
                        questions: data {
                            frontendQuestionId: questionFrontendId
                            title
                            titleSlug
                            difficulty
                        }
                    }
                }
            )"},
            {"variables", {
                {"categorySlug", ""},
                {"skip", skip},
                {"limit", 100},
                {"filters", filters}
            }}
        };

        cpr::Response r = cpr::Post(
            cpr::Url{"https://leetcode.com/graphql"},
            cpr::Header{
                {"Content-Type", "application/json"},
                {"x-csrftoken", csrf},
                {"Cookie", "LEETCODE_SESSION=" + session + "; csrftoken=" + csrf},
                {"Referer", "https://leetcode.com/problemset/all/"}
            },
            cpr::Body{body.dump()}
        );

        ProblemPage page;
        if (r.status_code != 200) {
            page.error = "HTTP " + std::to_string(r.status_code);
            return page;
        }

        try {
            auto json = nlohmann::json::parse(r.text);
            auto list = json["data"]["problemsetQuestionList"];
            page.total = list["total"].get<int>();
            for (const auto& q : list["questions"]) {
                ProblemSummary p;
                p.id         = q.value("frontendQuestionId", "");
                p.title      = q.value("title", "");
                p.slug       = q.value("titleSlug", "");
                p.difficulty = q.value("difficulty", "");
                page.problems.push_back(std::move(p));
            }
        } catch (const std::exception& e) {
            page.error = std::string("Parse error: ") + e.what();
            page.problems.clear();
        }

        return page;
    }

    std::string get_daily_question_slug() {
        const std::string& session = get_session_cookie();
        const std::string& csrf = get_csrf_token();
        const std::string graphql_url = "https://leetcode.com/graphql";

        std::string query = R"({
        "query": "query questionOfToday { activeDailyCodingChallengeQuestion { question { titleSlug } } }"
    })";

        auto response = cpr::Post(
            cpr::Url{graphql_url},
            cpr::Header{
                {"Content-Type", "application/json"},
                {"x-csrftoken", csrf},
                {"Cookie", "LEETCODE_SESSION=" + session + "; csrftoken=" + csrf},
                {"Referer", "https://leetcode.com/problemset/all/"}
            },
            cpr::Body{query}
        );

        if (response.status_code != 200) {
            std::cerr << "Failed to fetch daily question: " << response.status_code << "\n" << response.text << std::endl;
            return "";
        }

        auto json = nlohmann::json::parse(response.text);
        return json["data"]["activeDailyCodingChallengeQuestion"]["question"]["titleSlug"];
    }

    ProblemDescription fetch_problem_description(const std::string &slug) {
        ProblemDescription result;
        try {
            nlohmann::json query = {
                {
                    "query", R"(
                query getQuestionDetail($titleSlug: String!) {
                    question(titleSlug: $titleSlug) {
                        title
                        content
                    }
                }
            )"
                },
                {"variables", {{"titleSlug", slug}}}
            };

            cpr::Response r = cpr::Post(
                cpr::Url{"https://leetcode.com/graphql"},
                cpr::Header{{"Content-Type", "application/json"}},
                cpr::Body{query.dump()}
            );

            if (r.status_code != 200) {
                result.error = "Failed to fetch description: HTTP " + std::to_string(r.status_code);
                return result;
            }

            auto json = nlohmann::json::parse(r.text);
            if (!json.contains("data") || json["data"].is_null() || !json["data"].contains("question") ||
                json["data"]["question"].is_null()) {
                result.error = "Problem not found: \"" + slug + "\"";
                return result;
            }

            auto question = json["data"]["question"];
            result.title = question.value("title", "");
            result.content_html = question.value("content", "");
        } catch (const std::exception &e) {
            result.error = std::string("Unexpected error: ") + e.what();
        } catch (...) {
            result.error = "Unexpected error.";
        }
        return result;
    }

    // lang_override is currently unused: fetch no longer writes a solution
    // file (see write_starter_solution) so there's no starter code to pick a
    // language for at fetch time. Kept in the signature for CLI compatibility.
    std::string fetch_problem(const std::string &slug, const std::string &lang_override) {
        // GraphQL query: fetch title, content, questionId, starter code
        nlohmann::json query = {
            {
                "query", R"(
            query getQuestionDetail($titleSlug: String!) {
                question(titleSlug: $titleSlug) {
                    title
                    content
                    questionId
                    difficulty
                    topicTags {
                        name
                    }
                    hints
                }
            }
        )"
            },
            {"variables", {{"titleSlug", slug}}}
        };

        // Send POST request
        cpr::Response r = cpr::Post(
            cpr::Url{"https://leetcode.com/graphql"},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{query.dump()}
        );

        if (r.status_code != 200) {
            std::cerr << "Failed to fetch problem: HTTP " << r.status_code << "\n";
            return "Failed to fetch problem.";
        }

        // Parse response JSON
        auto json = nlohmann::json::parse(r.text);

        // Check for missing or null question field
        if (!json.contains("data") || json["data"].is_null() || !json["data"].contains("question") || json["data"][
                "question"].is_null()) {
            return "Problem not found. Check the title slug: \"" + slug + "\"";
        }

        auto question = json["data"]["question"];

        std::string title = question["title"];
        std::string id = question["questionId"];
        std::string difficulty = question.value("difficulty", "");
        std::string content_html = question.value("content", "");
        std::string markdown = html_to_text(content_html);

        // Make safe folder path: problems/{id}. {title}/
        std::string safe_title = std::regex_replace(title, std::regex("[\\\\/:*?\"<>|]"), "");
        std::string dir = get_problems_dir() + "/" + id + ". " + safe_title;
        std::filesystem::create_directories(dir);

        std::vector<std::string> topics;
        for (const auto &topic_json : question["topicTags"]) {
            topics.push_back(topic_json.value("name", ""));
        }
        std::vector<std::string> hints;
        for (const auto &hint_json : question["hints"]) {
            std::string hint = hint_json.get<std::string>();
            hints.push_back(std::regex_replace(hint, std::regex("<.*?>"), ""));
        }

        // Write files
        write_markdown_file(dir + "/README.md", title, markdown);
        write_html_file(dir + "/description.html", content_html);
        write_lines_file(dir + "/topics.txt", topics);
        write_lines_file(dir + "/hints.txt", hints);
        write_lines_file(dir + "/.slug", {slug});
        write_lines_file(dir + "/.difficulty", {difficulty});
        fetch_testcases(slug, dir);
        return title + "\n\n" + markdown;
    }

    StartSolutionResult write_starter_solution(const std::string &slug, const std::string &folder,
                                                const std::string &lang, const std::string &ext) {
        nlohmann::json query = {
            {
                "query", R"(
            query getQuestionDetail($titleSlug: String!) {
                question(titleSlug: $titleSlug) {
                    codeSnippets {
                        langSlug
                        code
                    }
                }
            }
        )"
            },
            {"variables", {{"titleSlug", slug}}}
        };

        cpr::Response r = cpr::Post(
            cpr::Url{"https://leetcode.com/graphql"},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{query.dump()}
        );

        if (r.status_code != 200) {
            return {"", "Failed to fetch starter code: HTTP " + std::to_string(r.status_code)};
        }

        auto json = nlohmann::json::parse(r.text);
        if (!json.contains("data") || json["data"].is_null() || !json["data"].contains("question") ||
            json["data"]["question"].is_null()) {
            return {"", "Problem not found. Check the title slug: \"" + slug + "\""};
        }

        auto question = json["data"]["question"];
        std::string code;
        bool found = false;
        for (const auto &snippet : question["codeSnippets"]) {
            if (snippet["langSlug"] == lang) {
                code = snippet["code"];
                found = true;
                break;
            }
        }
        if (!found) {
            return {"", "No starter code available for language '" + lang + "'."};
        }

        std::string path = folder + "/solution" + ext;
        write_solution_file(path, code);
        return {path, ""};
    }

    std::string read_question_id_from_readme(const std::string &path) {
        std::ifstream in(path);
        if (!in) return "";

        std::string line;
        std::getline(in, line); // expect "# Title"
        std::getline(in, line); // blank line
        std::getline(in, line); // content starts here
        return line.empty() ? "" : "unknown";
    }

    void solve_problem(const std::string &slug, const std::string &lang_override) {

        std::string solution_file;
        int status;
        if (!lang_override.empty()) {
            status = get_solution_filepath(slug, solution_file, lang_override);
        } else {
            status = get_solution_filepath(slug, solution_file);
        }
        if (!status) {
            launch_in_editor(solution_file);
        }
    }

    void list_fetched_problems() {
        std::string problems_dir = get_problems_dir();

        if (!std::filesystem::exists(problems_dir)) {
            std::cerr << "Problems directory not found: " << problems_dir << "\n";
            return;
        }

        std::cout << "Fetched problems:\n";

        for (const auto &entry: std::filesystem::directory_iterator(problems_dir)) {
            if (!entry.is_directory()) continue;

            std::string folder_name = entry.path().filename().string();
            std::string solution_path = entry.path().string() + "/solution.cpp";
            std::string status = std::filesystem::exists(solution_path) ? "[💾]" : "[ ]";

            std::cout << "  " << status << " " << folder_name << "\n";
        }
    }

    void submit_solution(const std::string &slug, const std::string &lang_override) {
        std::string session = get_session_cookie();
        std::string csrf = get_csrf_token();
        // Step 1: Read source code from file
        std::string solution_path;
        if (!lang_override.empty()) {
            get_solution_filepath(slug, solution_path, lang_override);
        } else {
            get_solution_filepath(slug, solution_path);
        }
        std::ifstream file(solution_path);
        if (!file) {
            // std::cerr << "Error: Could not open file " << solution_path << "\n";
            return;
        }

        std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        // Step 2: Query LeetCode to get questionId
        nlohmann::json query = {
            {
                "query", R"(
                query getQuestionDetail($titleSlug: String!) {
                    question(titleSlug: $titleSlug) {
                        questionId
                    }
                }
            )"
            },
            {"variables", {{"titleSlug", slug}}}
        };

        auto resp = cpr::Post(
            cpr::Url{"https://leetcode.com/graphql"},
            cpr::Header{{"Content-Type", "application/json"}, {"Cookie", "LEETCODE_SESSION=" + session}},
            cpr::Body{query.dump()}
        );

        if (resp.status_code != 200) {
            std::cerr << "Failed to fetch question ID\n";
            return;
        }

        auto json = nlohmann::json::parse(resp.text);
        std::string question_id = json["data"]["question"]["questionId"];

        // Step 3: Submit the solution
        std::string ext = get_file_extension(solution_path);
        std::string lang;
        if (ext == "cpp") {
            lang = "cpp";
        } else if (ext == "py") {
            lang = "python3";
        } else if (ext == "java") {
            lang = "java";
        } else {
            lang = get_preferred_language();
        }
        nlohmann::json payload = {
            {"lang", lang},
            {"question_id", question_id},
            {"typed_code", code}
        };

        auto submit_resp = cpr::Post(
            cpr::Url{"https://leetcode.com/problems/" + slug + "/submit/"},
            cpr::Header{
                {"Content-Type", "application/json"},
                {"Referer", "https://leetcode.com/problems/" + slug + "/"},
                {"x-csrftoken", csrf},
                {"Cookie", "LEETCODE_SESSION=" + session + "; csrftoken=" + csrf}
            },
            cpr::Body{payload.dump()}
        );

        if (submit_resp.status_code != 200) {
            std::cerr << "Submission failed\n";
            std::cerr << "Status: " << submit_resp.status_code << "\n";
            std::cerr << "Response body:\n" << submit_resp.text << "\n";
            return;
        }

        // Step 4: Extract submission_id
        std::string submission_id;
        try {
            auto j = nlohmann::json::parse(submit_resp.text);
            if (j.contains("submission_id")) {
                submission_id = std::to_string(j["submission_id"].get<int>());
            } else {
                std::cerr << "No submission_id in response.\n";
                std::cerr << "Raw JSON: " << j.dump(2) << "\n";
                return;
            }
        } catch (...) {
            std::cerr << "Invalid JSON. Raw response:\n" << submit_resp.text << "\n";
            return;
        }

        // Step 5: Poll submission result
        std::cout << "Waiting for result...\n";
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            auto result_resp = cpr::Get(
                cpr::Url{"https://leetcode.com/submissions/detail/" + submission_id + "/check/"},
                cpr::Header{{"Cookie", "LEETCODE_SESSION=" + session}}
            );

            if (result_resp.status_code != 200) {
                std::cerr << "Failed to poll submission\n";
                return;
            }

            auto result_json = nlohmann::json::parse(result_resp.text);
            // std::cout << result_json;
            std::string state = result_json["state"];
            if (state == "SUCCESS") {
                std::string status_msg = result_json["status_msg"];
                std::cout << "Result: " << status_msg << "\n";

                if (status_msg == "Accepted") {
                    std::cout << "✅ Accepted! Runtime: " << result_json["status_runtime"]
                            << ", Memory: " << result_json["status_memory"] << "\n";
                } else {
                    std::cout << "❌ " << status_msg << "\n";
                    if (result_json.contains("compile_error")) {
                        std::cout << "Compile Error:\n" << result_json["compile_error"] << "\n";
                    }
                    if (result_json.contains("input_formatted"))
                        std::cout << "Input:            " << result_json["input_formatted"] << "\n";

                    if (result_json.contains("expected_output"))
                        std::cout << "Expected Output:  " << result_json["expected_output"] << "\n";

                    if (result_json.contains("code_output"))
                        std::cout << "Your Output:      " << result_json["code_output"] << "\n";

                    if (result_json.contains("total_correct") && result_json.contains("total_testcases"))
                        std::cout << "Testcases Passed: " << result_json["total_correct"] << " / " << result_json[
                            "total_testcases"] << "\n";

                    std::cout << "\n🔍 View full details:\n";
                    std::cout << "   https://leetcode.com/submissions/detail/" << submission_id << "/\n";
                }

                break;
            }
        }
    }
    RunResult run_against_testcases(const std::string &slug, const std::string &folder,
                                     const std::string &lang, const std::string &code) {
        RunResult result;
        try {
            std::string session = get_session_cookie();
            std::string csrf = get_csrf_token();
            std::string question_id = get_question_id(slug, session, csrf);

            std::vector<std::string> cases = load_testcases(folder + "/testcases.txt");
            std::string joined;
            for (size_t i = 0; i < cases.size(); ++i) {
                if (i > 0) joined += "\n";
                joined += cases[i];
            }

            nlohmann::json body = {
                {"lang", lang},
                {"question_id", question_id},
                {"typed_code", code},
                {"data_input", joined}
            };

            cpr::Response r = cpr::Post(
                cpr::Url{"https://leetcode.com/problems/" + slug + "/interpret_solution/"},
                cpr::Header{
                    {"Content-Type", "application/json"},
                    {"x-csrftoken", csrf},
                    {"Cookie", "LEETCODE_SESSION=" + session + "; csrftoken=" + csrf},
                    {"Referer", "https://leetcode.com/problems/" + slug + "/"},
                    {"Origin", "https://leetcode.com"},
                    {"User-Agent", kBrowserUserAgent}
                },
                cpr::Body{body.dump()}
            );

            if (r.status_code != 200) {
                result.error = "Run request failed: HTTP " + std::to_string(r.status_code);
                return result;
            }

            std::string interpret_id = nlohmann::json::parse(r.text)["interpret_id"];
            std::string check_url = "https://leetcode.com/submissions/detail/" + interpret_id + "/check/";

            nlohmann::json check;
            bool done = false;
            for (int i = 0; i < 30; ++i) {
                cpr::Response cr = cpr::Get(
                    cpr::Url{check_url},
                    cpr::Header{
                        {"x-csrftoken", csrf},
                        {"Cookie", "LEETCODE_SESSION=" + session + "; csrftoken=" + csrf},
                        {"Referer", "https://leetcode.com/problems/" + slug + "/"},
                        {"User-Agent", kBrowserUserAgent}
                    }
                );
                if (cr.status_code != 200) {
                    result.error = "Polling failed: HTTP " + std::to_string(cr.status_code);
                    return result;
                }
                check = nlohmann::json::parse(cr.text);
                if (check.value("state", "") == "SUCCESS") { done = true; break; }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            if (!done) {
                result.error = "Timed out waiting for a result.";
                return result;
            }

            result.status_msg = check.value("status_msg", "Unknown");
            result.compile_error = check.value("compile_error", "");
            result.run_success = check.value("run_success", false);
            result.status_runtime = check.value("status_runtime", "");
            result.status_memory = check.value("status_memory", "");

            if (!result.compile_error.empty() || !result.run_success) {
                if (check.contains("full_runtime_error") && !check["full_runtime_error"].is_null())
                    result.runtime_error = check["full_runtime_error"].get<std::string>();
                else if (check.contains("runtime_error") && !check["runtime_error"].is_null())
                    result.runtime_error = check["runtime_error"].get<std::string>();
                else if (check.contains("std_output_list") && check["std_output_list"].is_array())
                    for (const auto &line : check["std_output_list"]) result.runtime_error += line.get<std::string>() + "\n";
                return result;
            }

            result.correct_answer = check.value("correct_answer", false);

            std::vector<std::string> code_answers, expected_answers;
            if (check.contains("code_answer") && check["code_answer"].is_array())
                for (const auto &v : check["code_answer"]) code_answers.push_back(v.get<std::string>());
            if (check.contains("expected_code_answer") && check["expected_code_answer"].is_array())
                for (const auto &v : check["expected_code_answer"]) expected_answers.push_back(v.get<std::string>());

            // compare_result is a "101..." bitstring, one char per real
            // testcase, from LeetCode's actual judge — it tolerates
            // order-independent / floating-point / multi-valid-answer cases
            // that a naive code_answer == expected_code_answer string
            // comparison gets wrong (e.g. Two Sum accepts either index
            // order). total_testcases is the authoritative count: the
            // code_answer/expected_code_answer arrays can contain a trailing
            // empty entry beyond the real testcases.
            std::string compare_result = check.value("compare_result", "");
            size_t n = check.value("total_testcases", 0);
            if (n == 0) n = std::max(cases.size(), std::max(code_answers.size(), expected_answers.size()));

            for (size_t i = 0; i < n; ++i) {
                TestcaseResult tc;
                tc.input = i < cases.size() ? cases[i] : "";
                tc.actual = i < code_answers.size() ? code_answers[i] : "";
                tc.expected = i < expected_answers.size() ? expected_answers[i] : "";
                tc.passed = i < compare_result.size() ? compare_result[i] == '1' : (tc.actual == tc.expected);
                result.testcases.push_back(std::move(tc));
            }
        } catch (const std::exception &e) {
            result.error = std::string("Unexpected error: ") + e.what();
        } catch (...) {
            result.error = "Unexpected error.";
        }
        return result;
    }

    SubmitResult submit_and_poll(const std::string &slug, const std::string &folder,
                                  const std::string &lang, const std::string &code) {
        SubmitResult result;
        try {
            std::string session = get_session_cookie();
            std::string csrf = get_csrf_token();
            std::string question_id = get_question_id(slug, session, csrf);

            nlohmann::json body = {
                {"lang", lang},
                {"question_id", question_id},
                {"typed_code", code}
            };

            cpr::Response r = cpr::Post(
                cpr::Url{"https://leetcode.com/problems/" + slug + "/submit/"},
                cpr::Header{
                    {"Content-Type", "application/json"},
                    {"Referer", "https://leetcode.com/problems/" + slug + "/"},
                    {"x-csrftoken", csrf},
                    {"Cookie", "LEETCODE_SESSION=" + session + "; csrftoken=" + csrf},
                    {"Origin", "https://leetcode.com"},
                    {"User-Agent", kBrowserUserAgent}
                },
                cpr::Body{body.dump()}
            );

            if (r.status_code != 200) {
                result.error = "Submit request failed: HTTP " + std::to_string(r.status_code);
                return result;
            }

            auto submit_json = nlohmann::json::parse(r.text);
            if (!submit_json.contains("submission_id")) {
                result.error = "No submission_id in response.";
                return result;
            }
            std::string submission_id = std::to_string(submit_json["submission_id"].get<long long>());
            std::string check_url = "https://leetcode.com/submissions/detail/" + submission_id + "/check/";

            nlohmann::json check;
            bool done = false;
            for (int i = 0; i < 30; ++i) {
                cpr::Response cr = cpr::Get(
                    cpr::Url{check_url},
                    cpr::Header{
                        {"Cookie", "LEETCODE_SESSION=" + session + "; csrftoken=" + csrf},
                        {"Referer", "https://leetcode.com/problems/" + slug + "/"},
                        {"User-Agent", kBrowserUserAgent}
                    }
                );
                if (cr.status_code != 200) {
                    result.error = "Polling failed: HTTP " + std::to_string(cr.status_code);
                    return result;
                }
                check = nlohmann::json::parse(cr.text);
                if (check.value("state", "") == "SUCCESS") { done = true; break; }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            if (!done) {
                result.error = "Timed out waiting for a result.";
                return result;
            }

            result.status_msg = check.value("status_msg", "Unknown");
            result.accepted = (result.status_msg == "Accepted");
            result.status_runtime = check.value("status_runtime", "");
            result.status_memory = check.value("status_memory", "");
            result.compile_error = check.value("compile_error", "");
            result.total_correct = check.value("total_correct", 0);
            result.total_testcases = check.value("total_testcases", 0);
            result.input_formatted = check.value("input_formatted", "");
            result.expected_output = check.value("expected_output", "");
            result.code_output = check.value("code_output", "");

            if (check.contains("runtime_percentile") && !check["runtime_percentile"].is_null())
                result.runtime_percentile = check["runtime_percentile"].get<double>();
            if (check.contains("memory_percentile") && !check["memory_percentile"].is_null())
                result.memory_percentile = check["memory_percentile"].get<double>();

            // The runtime/memory histogram data isn't in the check-poll
            // response (only the percentiles are) — it lives behind a
            // separate authenticated GraphQL query keyed by the submission's
            // numeric id, which the site's own submission-detail page hits.
            if (result.accepted) {
                nlohmann::json detail_query = {
                    {"query", R"(
                        query submissionDetails($submissionId: Int!) {
                            submissionDetails(submissionId: $submissionId) {
                                runtimeDistribution
                                memoryDistribution
                            }
                        }
                    )"},
                    {"variables", {{"submissionId", std::stoll(submission_id)}}}
                };
                cpr::Response dr = cpr::Post(
                    cpr::Url{"https://leetcode.com/graphql"},
                    cpr::Header{
                        {"Content-Type", "application/json"},
                        {"x-csrftoken", csrf},
                        {"Cookie", "LEETCODE_SESSION=" + session + "; csrftoken=" + csrf},
                        {"Referer", "https://leetcode.com/submissions/detail/" + submission_id + "/"},
                        {"User-Agent", kBrowserUserAgent}
                    },
                    cpr::Body{detail_query.dump()}
                );
                if (dr.status_code == 200) {
                    auto detail_json = nlohmann::json::parse(dr.text);
                    auto details = detail_json.value("data", nlohmann::json{}).value("submissionDetails", nlohmann::json{});

                    auto parse_distribution = [](const nlohmann::json &d, const char *key) {
                        std::vector<DistributionPoint> points;
                        if (!d.contains(key) || d[key].is_null()) return points;
                        auto dist_json = nlohmann::json::parse(d[key].get<std::string>());
                        if (dist_json.contains("distribution") && dist_json["distribution"].is_array()) {
                            for (const auto &pair : dist_json["distribution"]) {
                                if (pair.is_array() && pair.size() == 2) {
                                    DistributionPoint p;
                                    p.bucket = pair[0].is_string() ? pair[0].get<std::string>() : pair[0].dump();
                                    p.percentage = pair[1].get<double>();
                                    points.push_back(std::move(p));
                                }
                            }
                        }
                        return points;
                    };
                    result.runtime_distribution = parse_distribution(details, "runtimeDistribution");
                    result.memory_distribution = parse_distribution(details, "memoryDistribution");
                }
            }
        } catch (const std::exception &e) {
            result.error = std::string("Unexpected error: ") + e.what();
        } catch (...) {
            result.error = "Unexpected error.";
        }
        return result;
    }

    void run_problem(const std::string& slug, const std::string& lang, const std::string& question_id,
        const std::string& code, const std::string& test_input, const std::string& session, const std::string& csrf) {
        // Submit to LeetCode
        nlohmann::json body = {
            {"lang", lang},
            {"question_id", question_id},
            {"typed_code", code},
            {"data_input", test_input}
        };

        auto url = "https://leetcode.com/problems/" + slug + "/interpret_solution/";
        cpr::Response r = cpr::Post(
            cpr::Url{url},
            cpr::Header{
                {"Content-Type", "application/json"},
                {"x-csrftoken", csrf},
                {"Cookie", "LEETCODE_SESSION=" + session + "; csrftoken=" + csrf},
                {"Referer", "https://leetcode.com/problems/" + slug + "/"}
            },
            cpr::Body{body.dump()}
        );

        if (r.status_code != 200) {
            std::cerr << "Run failed: " << r.status_code << "\n" << r.text << std::endl;
            return;
        }

        std::string interpret_id = nlohmann::json::parse(r.text)["interpret_id"];
        std::string check_url = "https://leetcode.com/submissions/detail/" + interpret_id + "/check/";
        std::cout << "Waiting for result...\n";
        // Poll for result
        nlohmann::json result;
        for (int i = 0; i < 10; ++i) {
            cpr::Response check = cpr::Get(
                cpr::Url{check_url},
                cpr::Header{
                    {"x-csrftoken", csrf},
                    {"Cookie", "LEETCODE_SESSION=" + session + "; csrftoken=" + csrf},
                    {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36"},
                    {"Origin", "https://leetcode.com"},
                    {"Referer", "https://leetcode.com/problems/" + slug + "/"}
                }
            );

            if (check.status_code != 200) {
                std::cerr << "Polling failed: " << check.status_code << std::endl;
                return;
            }

            result = nlohmann::json::parse(check.text);
            if (result["state"] == "SUCCESS") break;

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Pretty-print final result
        std::cout << "\n≡ƒƒ⌐ Run Result\n";
        std::cout << "------------------------\n";

        // Always show status
        std::cout << "Status:        " << result.value("status_msg", "Unknown") << "\n";

        if (result.contains("compile_error") && !result["compile_error"].get<std::string>().empty()) {
            std::cout << "⛔ Compile Error:\n";
            std::cout << result["compile_error"].get<std::string>() << "\n";
            return;
        }

        if (!result.value("run_success", false)) {
            std::cout << "⛔ Runtime Error or Submission Failed\n";
            if (result.contains("std_output_list")) {
                std::cout << "Output: " << result["std_output_list"] << "\n";
            }
            return;
        }

        if (result.contains("correct_answer") && !result["correct_answer"].is_null()) {
            std::cout << "Correct:       " << (result["correct_answer"].get<bool>() ? "✅ Yes" : "❌ No") << "\n";
        } else {
            std::cout << "Correct:       Unknown\n";
        }

        if (result.contains("code_answer") && !result["code_answer"].empty()) {
            std::cout << "Your Output:   " << result["code_answer"][0] << "\n";
        }
        if (result.contains("expected_code_answer") && !result["expected_code_answer"].empty()) {
            std::cout << "Expected:      " << result["expected_code_answer"][0] << "\n";
        }

        std::cout << "Runtime:       " << result.value("status_runtime", "N/A") << "\n";
        std::cout << "Memory:        " << result.value("status_memory", "N/A") << "\n";
        std::cout << "Language:      " << result.value("pretty_lang", lang) << "\n";
    }
    void run_tests(const std::string& slug, const std::string &lang_override) {
        // Detect file
        std::string folder_path;
        get_solution_folder(slug, folder_path);
        std::string solution_path;
        if (!lang_override.empty()) {
            get_solution_filepath(slug, solution_path, lang_override);
        } else {
            get_solution_filepath(slug, solution_path);
        }
        std::string ext = get_file_extension(solution_path);
        std::string lang;
        if (ext == "cpp") {
            lang = "cpp";
        } else if (ext == "py") {
            lang = "python3";
        } else if (ext  ==  "java") {
            lang = "java";
        } else {
            std::cerr << "No solution file found.\n";
            return;
        }

        std::ifstream file(solution_path);
        if (!file) {
            std::cerr << "Error: Could not open file " << solution_path << "\n";
            return;
        }
        std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        // Load tokens
        std::string session = get_session_cookie();
        std::string csrf = get_csrf_token();
        std::string question_id = get_question_id(slug, session, csrf);

        std::vector<std::string> cases = load_testcases(folder_path + "/" + "testcases.txt");
        std::cout << "Running " << cases.size() << " testcases..." << std::endl;
        for (const std::string& test : cases) {
            std::cout << "Testcase:\n" << test << "\n---\n";
            run_problem(slug, lang, question_id, code, test, session, csrf);

            // Add delay between submissions to avoid rate limiting
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    TopicsHintsResult fetch_topics_and_hints(const std::string &slug) {
        TopicsHintsResult result;

        nlohmann::json query = {
            {
                "query", R"(
                query getTopicsAndHints($titleSlug: String!) {
                    question(titleSlug: $titleSlug) {
                        topicTags {
                            name
                        }
                        hints
                    }
                }
            )"
            },
            {"variables", {{"titleSlug", slug}}}
        };

        cpr::Response r = cpr::Post(
            cpr::Url{"https://leetcode.com/graphql"},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{query.dump()}
        );

        if (r.status_code != 200) {
            result.error = "HTTP " + std::to_string(r.status_code);
            return result;
        }

        try {
            auto json = nlohmann::json::parse(r.text);
            if (!json.contains("data") || json["data"].is_null() || !json["data"].contains("question")
                || json["data"]["question"].is_null()) {
                result.error = "Problem not found: \"" + slug + "\"";
                return result;
            }
            auto question = json["data"]["question"];
            for (const auto &topic_json : question["topicTags"]) {
                result.topics.push_back(topic_json.value("name", ""));
            }
            for (const auto &hint_json : question["hints"]) {
                std::string hint = hint_json.get<std::string>();
                result.hints.push_back(std::regex_replace(hint, std::regex("<.*?>"), ""));
            }
        } catch (const std::exception &e) {
            result.error = std::string("Parse error: ") + e.what();
        }

        return result;
    }

    void fetch_problem_topics(const std::string &slug) {
        TopicsHintsResult result = fetch_topics_and_hints(slug);
        if (!result.error.empty()) {
            std::cerr << "Failed to fetch topics: " << result.error << "\n";
            return;
        }
        std::cout << "Topics for \"" << slug << "\":\n";
        int count = 1;
        for (const auto &topic : result.topics) {
            std::cout << "  " << count++ << ". " << topic << "\n";
        }
    }

    void fetch_problem_hints(const std::string &slug) {
        TopicsHintsResult result = fetch_topics_and_hints(slug);
        if (!result.error.empty()) {
            std::cerr << "Failed to fetch hints: " << result.error << "\n";
            return;
        }
        std::cout << "Hints for \"" << slug << "\":\n";
        int count = 1;
        for (const auto &hint : result.hints) {
            std::cout << "  " << count++ << ". " << hint << "\n";
        }
    }
}
