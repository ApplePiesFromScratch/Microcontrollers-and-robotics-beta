# Chat kit: build with James Pugmire's method in a plain Claude or Grok chat

The kit needs no repo, no terminal and no emulator: a chat window, the Arduino IDE and the board.

| file | what it is | what you do with it |
|---|---|---|
| `CHAT_START.md` | the method, the board, the build loop, the presumptions to refuse, the pre-send check, the report card | paste or attach it as the first message (or put it in the chat project's instructions) |
| `starter.ino` | a template that compiles for the ESP32-C3 and runs: panel, wire, letters, frame loop, books line | attach it with CHAT_START.md; every build starts from it |
| `CALIBRATION.md` | 12 questions, 6 of them presumption traps, with the answers in Part B | send only Part A; score the reply yourself with Part B |

## How to use it

1. New chat. Attach `CHAT_START.md` and `starter.ino`. Say: "Read both whole. Then answer the questions I send next."
2. Send Part A of `CALIBRATION.md`. Score the replies with Part B. Twelve of twelve right means the chat is ready to
   build.
3. Ask for what you want built in one or two sentences. Expect relations first, then the whole file, then a report card
   that ends with PLEASE RUN and PLEASE REPORT.
4. Paste the file into the Arduino IDE (board "ESP32C3 Dev Module", USB CDC On Boot: Enabled) and upload. Report back
   exactly what the card asks: the books line, what moves, the serial line every 100 latches.
5. If the replies start drifting (a `float` appears, "this works" is claimed, a library is added), paste section 5 of
   `CHAT_START.md` again, or start a new chat.

Long chats drift: the model's attention to the first message fades as the chat grows. Start a new chat for each new
sketch, and carry the current file over by attaching it.

## What was checked (26 Nov 2026)

| check | result |
|---|---|
| `starter.ino` builds for the C3 (arduino-cli, esp32 core, CDC on boot) | flash 321,319 bytes, globals 40,504 bytes |
| `starter.ino` on the desk twin, 1,000 latches | the books line HELD: 3 × 37333 + 1 = 112000 = 7 × 16 × 1000 |
| every number in CALIBRATION Part B | recomputed by script |
| refused-word lint over the kit's docs | 0 flags |

**Not checked:** `starter.ino` on a board (its panel, wire and text code are BIGRACE's, which James's board runs).
Whether a given chat model follows the kit: CALIBRATION is how you find out, every time.
