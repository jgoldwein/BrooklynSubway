#pragma once
struct FakeSerial {void println(const char *) {}};extern FakeSerial Serial;
