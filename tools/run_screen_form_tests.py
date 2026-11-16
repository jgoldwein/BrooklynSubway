#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Exercise the production screen-settings field loop, including optional sound alerts."""
import subprocess
import tempfile
from pathlib import Path
import phase4e_host as h


def main():
    source = (h.ROOT / "src/lan_web_ui.cpp").read_text()
    handler = source.split("void LanWebUi::handleSaveScreenOptions() {", 1)[1]
    loop = handler[handler.index("  uint"):handler.index("  const String start=")]
    loop = loop.replace("return;", "return 0xffff;")
    program = r'''
#include <stdint.h>
#include <string.h>
struct String {
  const char *value;
  bool operator==(const char *other) const {return strcmp(value,other)==0;}
};
struct Server {
  const char **names;int count;
  int args() const {return count;}
  String argName(int i) const {return {names[i]};}
  void send(int,const char *,const char *) {}
} server_;
uint16_t parse(const char **names,int count) {
  server_.names=names;server_.count=count;
''' + loop + r'''
  return seen;
}
int main() {
  const char *fields[]={"csrf","start","end","contrast","quiet","startup","mapMode","mapBrightness","waitWifi","serviceSounds"};
  if(parse(fields,10)!=1023) return 6;
  if(parse(fields,9)!=511) return 1;
  if(parse(fields,8)!=255) return 2;
  const char *checked[]={"waitWifi"};
  if(parse(checked,1)!=256) return 3;
  const char *duplicate[]={"waitWifi","waitWifi"};
  if(parse(duplicate,2)!=0xffff) return 4;
  const char *unknown[]={"unknown"};
  if(parse(unknown,1)!=0xffff) return 5;
  const char *service[]={"serviceSounds"};if(parse(service,1)!=512)return 7;
  const char *twice[]={"serviceSounds","serviceSounds"};if(parse(twice,2)!=0xffff)return 8;
  return 0;
}
'''
    with tempfile.TemporaryDirectory() as tmp:
        cpp = Path(tmp) / "screen_form.cpp"
        exe = Path(tmp) / "screen_form.exe"
        cpp.write_text(program)
        subprocess.run(h.compiler() + ["-std=c++17", "-Wall", "-Wextra", "-Werror",
                                      str(cpp), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    print("Production screen form: ninth flag retained, unchecked omitted, duplicate/unknown rejected PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
