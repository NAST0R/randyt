# RandYT

A relatively lightweight C program made for Windows, opens up a random YouTube video, given a channel username, from its recent RSS feed.
Made for educational purposes in order to try and code something with zero dynamic allocations at runtime.

## Features

- Uses WinHTTP to perform HTTP GET requests.
- Parses YouTube channel homepage HTML to extract the channel ID.
- Fetches the channel's RSS feed and extracts video IDs.
- Picks a random video from the feed and opens it in the default web browser.
- Minimal dependencies and easy to build on Windows.

## Requirements

- Windows OS (uses WinHTTP and ShellExecute).
- GCC or compatible C compiler supporting Win32 API.
- No external libraries needed.

## Build Instructions

Source code was compiled and tested using MinGW64 with the following line:
```bash
gcc main.c http_client.c utils.c -Os -s -Wl,--gc-sections -Wl,--strip-all -lwinhttp -o randyt.exe
```

# Usage
```cmd
randyt.exe vsauce
```
