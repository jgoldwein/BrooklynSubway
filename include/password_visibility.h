// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace brooklyn::web {
// No name attribute: the checkbox is never submitted with credentials.
constexpr char kPasswordVisibilityControl[] =
    "<label><input type=\"checkbox\" onchange=\""
    "this.form.querySelectorAll('input').forEach(function(e){"
    "if(e.name==='pass'||e.name==='password'||e.name==='confirmation')"
    "e.type=this.checked?'text':'password'},this)\">Show passwords</label>";
}
