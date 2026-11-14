# Brooklyn Map 0.2.42

- After sign-in, return to the page that required it: firmware updates,
  diagnostics, Wi-Fi setup, or the relevant settings tab.
- Carry the destination through the sign-in form, with a fixed local allowlist.
  Keep CSRF/session checks and compatibility with existing sign-in forms.
- Return to the page without replaying a denied setting change, reboot or upload.
  Retry the action after signing in.

Validation: 526 LAN-page/policy checks and 157 administrator-auth checks pass.
Browser fixtures verify the rendered destination field, return navigation,
settings-tab selection and absence of action replay. All four firmware targets
build successfully; responsive UI checks pass at 320/390/768/1100 pixels.
