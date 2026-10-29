#!/bin/bash

# ── TeacherDecides Build & Run Script ──

# Stop on first error
set -e

if [[ -z "${BOT_TOKEN:-}" ]]; then
    echo "BOT_TOKEN environment variable is required. Set it before running this script." >&2
    exit 1
fi

echo "🚀 Starting TeacherDecides Build Process..."

# 1. Compile the Discord Bot
echo "📦 Compiling bot.cpp..."
g++ -std=c++17 bot.cpp -o bot -ldpp -lcurl -lpthread
echo "✅ Bot compiled."

# 2. Compile the Web Server
echo "🌐 Compiling server.cpp..."
g++ -std=c++17 -O2 server.cpp -o server -lpthread
echo "✅ Server compiled."

# 3. Setup cleanup trap
cleanup() {
    echo -e "\n🛑 Shutting down TeacherDecides..."
    kill $BOT_PID $SERVER_PID 2>/dev/null || true
    echo "👋 Cleanup complete. Goodbye!"
    exit
}
trap cleanup SIGINT SIGTERM

# 4. Launch both services
echo "⚡ Launching services..."

# Start Web Server in background (Port 5921)
./server &
SERVER_PID=$!
echo "📡 Web Server running at http://localhost:5921 (PID: $SERVER_PID)"

# Start Discord Bot in background
./bot &
BOT_PID=$!
echo "🤖 Discord Bot running (PID: $BOT_PID)"

echo "═══════════════════════════════════════════════════════"
echo "  TeacherDecides is now LIVE!"
echo "  Press Ctrl+C to stop both processes safely."
echo "═══════════════════════════════════════════════════════"

# Wait for both to finish
wait $BOT_PID $SERVER_PID
