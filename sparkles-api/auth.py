"""
auth.py — JWT cookie auth for Sparkles API.

Config: auth_config.yaml (same directory)
Cookie: sparkles_token (HTTP-only, SameSite=Lax)
"""

import os
import logging
import yaml
from datetime import datetime, timedelta, timezone
from pathlib import Path

from jose import jwt, JWTError
from passlib.context import CryptContext

logger = logging.getLogger("auth")

_CONFIG_PATH = Path(__file__).parent / "auth_config.yaml"
SECRET_KEY    = os.environ.get("SPARKLES_SECRET", "sparkles-dev-secret-change-me")
ALGORITHM     = "HS256"
TOKEN_TTL_H   = 24 * 7   # 1 week
COOKIE_NAME   = "sparkles_token"

pwd_ctx = CryptContext(schemes=["bcrypt"], deprecated="auto")


# ---------------------------------------------------------------------------
# Config loading + first-run password hashing
# ---------------------------------------------------------------------------

def _load_raw() -> dict:
    with open(_CONFIG_PATH) as f:
        return yaml.safe_load(f)


def _save_raw(cfg: dict):
    with open(_CONFIG_PATH, "w") as f:
        yaml.dump(cfg, f, allow_unicode=True, sort_keys=False)


def bootstrap():
    """Hash any password_plain entries and rewrite config on startup."""
    cfg = _load_raw()
    changed = False
    for username, user in cfg.get("users", {}).items():
        if "password_plain" in user:
            plain = str(user.pop("password_plain"))[:72]  # bcrypt hard limit
            user["password_hash"] = pwd_ctx.hash(plain)
            changed = True
            logger.info("Hashed password for user '%s'", username)
    if changed:
        _save_raw(cfg)


def load_config() -> dict:
    return _load_raw()


# ---------------------------------------------------------------------------
# Auth helpers
# ---------------------------------------------------------------------------

def authenticate(username: str, password: str) -> "str | None":
    """Return role string on success, None on failure."""
    cfg = load_config()
    user = cfg.get("users", {}).get(username)
    if not user or "password_hash" not in user:
        return None
    if not pwd_ctx.verify(password, user["password_hash"]):
        return None
    return user["role"]


def create_token(username: str, role: str) -> str:
    payload = {
        "sub": username,
        "role": role,
        "exp": datetime.now(timezone.utc) + timedelta(hours=TOKEN_TTL_H),
    }
    return jwt.encode(payload, SECRET_KEY, algorithm=ALGORITHM)


def decode_token(token: str) -> "dict | None":
    try:
        return jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
    except JWTError:
        return None


def get_current_user(request) -> "dict | None":
    token = request.cookies.get(COOKIE_NAME)
    if not token:
        return None
    return decode_token(token)


# ---------------------------------------------------------------------------
# Access checking
# ---------------------------------------------------------------------------

def _role_allowed(role: str, allowed: list) -> bool:
    return role in allowed


def check_api_access(path: str, method: str, role: str) -> bool:
    cfg = load_config()
    rules: dict = cfg.get("access", {}).get("api", {})
    key = f"{method} {path}"

    # exact match
    if key in rules:
        return _role_allowed(role, rules[key])

    # prefix match (longest first)
    matches = [
        (k, v) for k, v in rules.items()
        if k != "*" and path.startswith(k.split(" ", 1)[-1]) and k.startswith(method)
    ]
    if matches:
        best = max(matches, key=lambda x: len(x[0]))
        return _role_allowed(role, best[1])

    # default
    return _role_allowed(role, rules.get("*", ["admin", "user"]))


def check_page_access(path: str, role: str) -> bool:
    cfg = load_config()
    rules: dict = cfg.get("access", {}).get("pages", {})

    # prefix match (longest first)
    matches = [(k, v) for k, v in rules.items() if k != "*" and path.startswith(k)]
    if matches:
        best = max(matches, key=lambda x: len(x[0]))
        return _role_allowed(role, best[1])

    return _role_allowed(role, rules.get("*", ["admin", "user"]))
