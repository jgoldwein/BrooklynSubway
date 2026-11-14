# Brooklyn Map 0.2.39

- Open the administrator sign-in page automatically when a setting or protected
  page needs a new sign-in, rather than displaying a plain-text error.
- Handle expired sign-in during manual firmware uploads by opening sign-in too.
- Keep authorization and CSRF protections. Failed actions are not automatically
  replayed; sign in, then retry the action. Existing settings-area navigation is retained.

Preserve public firmware-health confirmation when the update-progress session expires.

Validation: all 29 host suites and four firmware targets pass. Isolated browser
checks cover sign-in navigation, remembered settings, no action replay, and
update-progress session-expiry fallback. No hardware changes.
