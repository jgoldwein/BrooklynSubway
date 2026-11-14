# Embedded web interface

The main page should be understandable to a 13-year-old. Use short labels and
put everyday tasks first. Navigation names are Overview, Train lights, Screen,
Wi-Fi and Advanced. Keep technical reports in closed native details panels.

The firmware renders forms and status values in src/lan_web_policy.cpp. Local
CSS and navigation JavaScript are in include/web_ui_assets.h and are served
read-only from flash by src/lan_web_ui.cpp. They require no new dependency and
must never contain device credentials or session tokens. Increment their URL
query version in the home and sign-in heads whenever the assets change.

Use ordinary POST forms with the existing server authorization and CSRF policy.
Remember only the current area in browser sessionStorage; never store passwords
or tokens there. Background refresh updates text values only and must not clear
unsaved inputs, rebuild forms or rotate sign-in tokens. Navigation is progressive
enhancement: ordinary anchors/forms/details must work without JavaScript.

Keep web::kStatusPageCapacity shared by firmware and page-capacity tests. The
full signed-in production page must fit with all route rows, station examples,
controls and escaped fields. Static assets are separate to avoid growing this
RAM buffer when improving presentation. Keep the public status API compatible.

Validate host rendering/authentication tests, all firmware targets and browser
layouts at desktop, tablet and narrow-phone widths. Check locked/setup/no-data
states as well as the signed-in happy path. Confirm every form still owns its
required fields and token, save returns to the current area, live refresh keeps
unsaved values, and expanded details do not create horizontal page overflow.
