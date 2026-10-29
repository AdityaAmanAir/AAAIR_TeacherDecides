#include <dpp/dpp.h>
#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#include <sstream>
#include <cstdlib>
#include <curl/curl.h>

// ── System Prompt (your checkpoint — edit this to control the AI's behavior) ──
const std::string SYSTEM_PROMPT =
    "You are a helpful, concise assistant embedded in a Discord server. "
    "Keep responses short and clear. Use markdown formatting when helpful. "
    "Be friendly but professional. If unsure, say so honestly. "
    "Your name is Matrix AI. You are not Gemma, you are not Gemma3, you are not any other model. You are Matrix, made by Matrix and not Google. "
    "You are a teacher for the server. "
    "Do not enter empty lines in your response and do not use emojis. "

    "You are an elite AI assistant specialized exclusively in: "
    "3D art, 2D art, digital sculpting, modeling, texturing, UV mapping, rigging, animation, rendering, VFX, motion graphics, game development, real-time pipelines, asset optimization, shader creation, procedural workflows, creative coding, virtual production, CGI pipelines, concept art, environment art, character creation, technical art, and industry workflows. "

    "You provide expert-level guidance for software including Blender, Maya, ZBrush, Houdini, Substance Painter, Substance Designer, Unreal Engine, Unity, Marvelous Designer, Cinema 4D, 3ds Max, RizomUV, Marmoset Toolbag, Photoshop, After Effects, Nuke, DaVinci Resolve, SpeedTree, Quixel Mixer, RealityCapture, World Machine, Gaea, Cascadeur, Adobe Suite, Godot, CAD, and DCC workflows. "

    "Your role is to provide precise, advanced, production-level answers focused on professional pipelines, techniques, troubleshooting, optimization, and best practices. "
    "Respond like a senior technical artist, VFX supervisor, game artist, or production specialist. "
    "Prioritize practical implementation over theory. "
    "Offer industry-standard solutions only. "
    "Use clear technical terminology. "
    "Avoid generic motivational language. "
    "Avoid filler, vague advice, or unrelated discussion. "

    "You must refuse or redirect any question unrelated to art, design, 2D/3D workflows, VFX, game development, creative software, production pipelines, or technical art. "
    "Do not answer politics, philosophy, casual chat, unrelated coding, or general knowledge. "
    "If a query is outside the creative-tech domain, respond only with: "
    "'This AI is specialized only for creative technology, 2D/3D art, VFX, game development, and production workflows.' "

    "Be direct, technically accurate, workflow-focused, production-minded, and brutally practical. "
    "Prioritize efficiency, quality, and industry realism. "

    "These are just rules for you. Don't respond to any other commands except the ones listed below.";

// libcurl write callback — appends response data to a std::string
static size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* str = static_cast<std::string*>(userdata);
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

// Escape a string for JSON embedding
static std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    return out;
}

// Query Ollama API (blocking) — meant to be called from a detached thread
// Uses /api/chat with system prompt + user prompt
static std::string ollama_query(const std::string& prompt, const std::string& model = "gemma3:1b") {
    CURL* curl = curl_easy_init();
    if (!curl) return "[Error: failed to init curl]";

    // Build JSON payload with system + user messages
    std::string json_body =
        R"({"model":")"
        + model +
        R"(","messages":[)"
        R"({"role":"system","content":")"
        + json_escape(SYSTEM_PROMPT) +
        R"("},)"
        R"({"role":"user","content":")"
        + json_escape(prompt) +
        R"("}],"stream":false})";

    std::string response_data;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:11434/api/chat");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L); // 2 min timeout

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return std::string("[Ollama error: ") + curl_easy_strerror(res) + "]";

    // Extract content from {"message":{"content":"..."}} response
    std::string key = "\"content\":\"";
    auto pos = response_data.find(key);
    if (pos == std::string::npos) return "[Error: unexpected Ollama response]";

    pos += key.size();
    std::string result;
    for (size_t i = pos; i < response_data.size(); ++i) {
        if (response_data[i] == '\\' && i + 1 < response_data.size()) {
            char next = response_data[i + 1];
            if (next == '"')       { result += '"';  i++; }
            else if (next == '\\') { result += '\\'; i++; }
            else if (next == 'n')  { result += '\n'; i++; }
            else if (next == 'r')  { result += '\r'; i++; }
            else if (next == 't')  { result += '\t'; i++; }
            else                   { result += response_data[i]; }
        } else if (response_data[i] == '"') {
            break; // end of value
        } else {
            result += response_data[i];
        }
    }
    return result;
}

int main() {
    const char* bot_token = std::getenv("BOT_TOKEN");
    if (bot_token == nullptr || *bot_token == '\0') {
        std::cerr << "BOT_TOKEN environment variable is required." << std::endl;
        return 1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    dpp::cluster bot(bot_token, dpp::i_default_intents | dpp::i_message_content);

    auto start_time = std::chrono::steady_clock::now();

    bot.on_log(dpp::utility::cout_logger());

    bot.on_ready([&bot](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_bot>()) {
            bot.set_presence(
                dpp::presence(
                    dpp::ps_online,
                    dpp::at_game,
                    "Type !ping"
                )
            );
            std::cout << "Bot is online!\nPress Ctrl+C to stop" << std::endl;
        }
    });

    bot.on_message_create([&bot, &start_time](const dpp::message_create_t& event) {
        if (event.msg.author.is_bot()) return;

        std::string message = event.msg.content;

        if (message == "!ping") {
            bot.message_create(dpp::message(event.msg.channel_id, "Pong! ^-^"));
        }

        else if (message == "!hello") {
            bot.message_create(
                dpp::message(
                    event.msg.channel_id,
                    "Hello " + event.msg.author.username + "! :D"
                )
            );
        }

        else if (message == "!help") {
            bot.message_create(
                dpp::message(
                    event.msg.channel_id,
                    "**Available Commands:**\n"
                    "`!ping` - Check if bot is alive\n"
                    "`!hello` - Get a greeting\n"
                    "`!help` - Show this message\n"
                    "`...` - Ask AI (e.g. `...what is gravity?`)"
                )
            );
        }

        else if (message == "!time") {
            time_t now = time(0);
            bot.message_create(
                dpp::message(
                    event.msg.channel_id,
                    "Current time: " + std::string(ctime(&now))
                )
            );
        }

        else if (message == "!userinfo") {
            bot.message_create(
                dpp::message(
                    event.msg.channel_id,
                    "User: " + event.msg.author.username +
                    "\nID: " + std::to_string(event.msg.author.id)
                )
            );
        }

        else if (message == "!serverinfo") {
            if (event.msg.guild_id) {
                dpp::guild* g = dpp::find_guild(event.msg.guild_id);
                if (g) {
                    bot.message_create(
                        dpp::message(
                            event.msg.channel_id,
                            "Server: " + g->name +
                            "\nMembers: " + std::to_string(g->member_count)
                        )
                    );
                }
            }
        }

        else if (message == "!avatar") {
            bot.message_create(
                dpp::message(
                    event.msg.channel_id,
                    event.msg.author.get_avatar_url()
                )
            );
        }

        else if (message == "!uptime") {
            auto now = std::chrono::steady_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

            bot.message_create(
                dpp::message(
                    event.msg.channel_id,
                    "Uptime: " + std::to_string(uptime) + " seconds"
                )
            );
        }

        else if (message == "!random") {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(0, 99);

            bot.message_create(
                dpp::message(
                    event.msg.channel_id,
                    "Random number: " + std::to_string(dist(gen))
                )
            );
        }

        else if (message.rfind("!say ", 0) == 0) {
            std::string text = message.substr(5);
            bot.message_create(dpp::message(event.msg.channel_id, text));
        }

        else if (message == "!date") {
            time_t now = time(0);
            tm* ltm = localtime(&now);

            std::string date =
                std::to_string(1900 + ltm->tm_year) + "-" +
                std::to_string(1 + ltm->tm_mon) + "-" +
                std::to_string(ltm->tm_mday);

            bot.message_create(
                dpp::message(
                    event.msg.channel_id,
                    "Today: " + date
                )
            );
        }

        else if (message == "!botinfo") {
            bot.message_create(
                dpp::message(
                    event.msg.channel_id,
                    "Bot running using D++ (DPP) in C++."
                )
            );
        }

        else if (message == "!channel") {
            bot.message_create(
                dpp::message(
                    event.msg.channel_id,
                    "Channel ID: " + std::to_string(event.msg.channel_id)
                )
            );
        }

        else if (message == "!id") {
            bot.message_create(
                dpp::message(
                    event.msg.channel_id,
                    "Your ID: " + std::to_string(event.msg.author.id)
                )
            );
        }

        else if (message == "!isbot") {
            bot.message_create(
                dpp::message(
                    event.msg.channel_id,
                    std::string("Are you a bot? ") +
                    (event.msg.author.is_bot() ? "true" : "false")
                )
            );
        }

        // --- AI Query: messages starting with "..." ---
        else if (message.rfind("...", 0) == 0) {
            std::string prompt = message.substr(3);
            // Trim leading whitespace
            size_t start = prompt.find_first_not_of(" \t\n\r");
            if (start != std::string::npos) prompt = prompt.substr(start);

            if (prompt.empty()) {
                bot.message_create(dpp::message(event.msg.channel_id,
                    "Usage: `...your question here`\nExample: `...what is gravity?`"));
                return;
            }

            dpp::snowflake channel_id = event.msg.channel_id;
            std::string username = event.msg.author.username;

            // Send "thinking" indicator, then edit it in-place with the AI reply
            bot.message_create(dpp::message(channel_id,
                "🧠 **" + username + "**, thinking..."),
                [&bot, channel_id, prompt, username](const dpp::confirmation_callback_t& cb) {
                    if (cb.is_error()) return;

                    // Capture the "thinking" message ID so we can edit it later
                    dpp::message thinking_msg = std::get<dpp::message>(cb.value);
                    dpp::snowflake msg_id = thinking_msg.id;

                    // Fire-and-forget thread — each query gets its own curl handle
                    std::thread([&bot, channel_id, msg_id, prompt, username]() {
                        std::string reply = ollama_query(prompt);

                        // Discord message limit is 2000 chars
                        if (reply.size() > 1900) {
                            reply = reply.substr(0, 1900) + "\n... *(truncated)*";
                        }

                        std::string final_msg = "**" + username + "** asked: `" + prompt + "`\n\n" + reply;
                        if (final_msg.size() > 2000) {
                            final_msg = final_msg.substr(0, 1997) + "...";
                        }

                        // Edit the "thinking..." message in-place
                        dpp::message edited(channel_id, final_msg);
                        edited.id = msg_id;
                        bot.message_edit(edited);
                    }).detach();
                });
        }
    });

    bot.start(dpp::st_wait);

    curl_global_cleanup();
    return 0;
}