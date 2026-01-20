## 2025-05-15 - [Windows Command Injection in shell_escape]
**Vulnerability:** The `shell_escape` function for Windows in `src/main.cpp` used `\"` to escape double quotes inside command arguments. However, `cmd.exe` does not treat `\"` as an escaped quote; it treats `"` as a toggle. This allowed attackers to break out of the quoted argument string using a double quote, leading to command injection via operators like `&`.
**Learning:** Windows `cmd.exe` argument parsing rules are inconsistent with C runtime `CommandLineToArgvW` rules. Safely escaping double quotes inside a double-quoted argument for `cmd.exe` is notoriously difficult and context-dependent.
**Prevention:** For file paths and simple strings on Windows, strictly forbid (throw exception) or strip double quotes rather than attempting to escape them. This is the only fail-safe way to prevent `cmd.exe` interpretation issues when using `std::system` or `cmd /c`.

## 2025-05-24 - [Flask-SocketIO Default CORS Permissiveness]
**Vulnerability:** `scripts/flask_app.py` intended to prevent cross-origin websocket hijacking (CSWSH) but used `SocketIO(app)` without arguments. By default, `python-socketio` (and thus `flask-socketio`) sets `cors_allowed_origins` to `None`, which allows ALL origins. This contradicted the code comments and security intent.
**Learning:** Security-critical libraries often prioritize ease of use over security by default. Always verify the default behavior of security-related parameters in documentation or by testing, rather than assuming a "secure default" exists.
**Prevention:** Explicitly configure `cors_allowed_origins` to a restricted list (e.g. `['http://127.0.0.1:port', 'http://localhost:port']`) when initializing `SocketIO` for internal/dashboard tools.
