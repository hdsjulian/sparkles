# 15. Offline deploy with git bundles

- Status: Accepted (replaces `deploy.py` and `setup_pi.py`)
- Date: 2026-06-09 (`6094ef2`), bundles 2026-07-21 (`dffaf4c`)

## Context

In the field the Pi usually has no internet. Code is changed on a Mac and has to reach the Pi over a local link.

## Decision

- `push_to_pi.sh` on the Mac bundles the current branch, copies the bundle to the Pi, pulls it there, then runs `update.sh`. It pulls first so an old `update.sh` on the Pi can't get in the way (`3f181b1`).
- `update.sh` on the Pi probes the git remote with a timeout. Online it pulls from GitHub. Offline it pulls from `/tmp/sparkles-update.bundle` and skips `npm install`. Then it builds the UI, restarts `sparkles`, `aubio` and `keyboard_midi`, and kills Chromium so the kiosk comes back with the new build.
- A fresh Pi is set up with `setup.sh` and `setup/01..07`: system packages, Python venv, folders and udev, systemd services, kiosk, Wi-Fi, display.
- Services are systemd units with `Restart=always`. The music daemons `Require` the main service because they need its socket.

## Consequences

- Deploys work in a forest.
- `git pull <bundle> <branch>` merges into whatever branch the Pi has checked out. Put the Pi on the right branch first, or a push lands in a branch it was not meant for.
- Some setup lives only on the Pi and is not in the scripts: `ConnectionSupervisionTimeout` in `/etc/bluetooth/main.conf`, and bleak and numpy for the Muse (ADR 26).
- `deploy.py`, `setup_pi.py`, `upload_ota.py` and `update_master.sh` belong to earlier setups and are not used.
