# TeacherDecides

TeacherDecides is a small C++ project with two parts: a Discord bot for a creative-tech community and a static web server for the site in `public/`.

The bot handles a handful of chat commands. It can also send questions to a local Ollama instance and post the response back to Discord.

## Website

![TeacherDecides website screenshot](public/images/website-screenshot.png)

## What you need

- Linux, a C++17 compiler (`g++`), and pthreads
- D++ (DPP) installed on your system, including its headers and library
- libcurl development files
- A Discord bot token
- Ollama running locally if you want to use the AI replies

DPP is a dependency of the bot; its source is not included in this project. The build script expects the DPP headers and library to be available to the system compiler and linker.

On Ubuntu or Debian, the compiler and libcurl development package can be installed with:

```sh
sudo apt update
sudo apt install build-essential libcurl4-openssl-dev
```

Install DPP separately using its official installation instructions.

## Configure the Discord bot

Create a bot in the Discord Developer Portal, invite it to your server, and enable the **Message Content Intent** in the bot settings. The bot requests this intent so it can read text commands.

Set the bot token in your shell; don't paste it into `bot.cpp` or commit it to the project:

```sh
export BOT_TOKEN="your-discord-bot-token"
```

If a token was previously shared or committed, revoke it in the Developer Portal and use a replacement.

## Run

From the project directory:

```sh
./build.sh
```

The script compiles `bot.cpp` and `server.cpp`, then starts both programs. The website is served on port `5921`; the bot connects to Discord using `BOT_TOKEN`. Press Ctrl+C to stop the processes.

The server reads files from `./public`, so run the script from the project directory. It listens on all network interfaces (`0.0.0.0`); don't expose it to an untrusted network without adding appropriate access controls and HTTPS.

## Local AI replies

AI requests use Ollama's local chat API at `http://localhost:11434`, with the `gemma3:1b` model. Install and start Ollama, then download the model:

```sh
ollama pull gemma3:1b
```

In Discord, begin a message with `...` followed by a question:

```text
...How should I optimize a Blender scene with lots of geometry?
```

The bot sends the question to Ollama and edits its "thinking" message with the result. If Ollama isn't running or the model isn't available, the bot can't provide an AI response.

## Bot commands

| Command | What it does |
| --- | --- |
| `!ping` | Check whether the bot responds |
| `!hello` | Get a greeting |
| `!help` | Show the command list |
| `!time` | Show the current time |
| `!date` | Show today's date |
| `!userinfo` | Show your username and user ID |
| `!serverinfo` | Show the server name and member count |
| `!avatar` | Show your avatar URL |
| `!uptime` | Show how long the bot has been running |
| `!random` | Get a random number from 0 to 99 |
| `!say <text>` | Have the bot repeat text |
| `!botinfo` | Show the bot library |
| `!channel` | Show the current channel ID |
| `!id` | Show your user ID |
| `!isbot` | Check whether your account is a bot |

## Project files

- `bot.cpp` — Discord commands and Ollama requests
- `server.cpp` — HTTP server for the static site
- `public/` — website files served by the HTTP server
- `build.sh` — builds and starts the bot and server
