import base64
import hashlib
import socket
import threading

import paramiko
from PySide6.QtCore import QThread, Signal

from . import debug
from .config import get_known_hosts_path


KEY_CLASSES = [paramiko.Ed25519Key, paramiko.RSAKey, paramiko.ECDSAKey]
# DSSKey (DSA) was removed in newer paramiko versions; include it only if present.
if hasattr(paramiko, "DSSKey"):
    KEY_CLASSES.append(paramiko.DSSKey)


def key_fingerprint(key) -> str:
    digest = hashlib.sha256(key.asbytes()).digest()
    return "SHA256:" + base64.b64encode(digest).decode().rstrip("=")


class PromptHostKeyPolicy(paramiko.MissingHostKeyPolicy):
    """Host key policy that asks the GUI (via a signal) to confirm an
    unknown host key, blocking the SSH thread until the user responds.
    Accepted keys are persisted to the app's known_hosts file."""

    def __init__(self, session):
        self.session = session

    def missing_host_key(self, client, hostname, key):
        event = threading.Event()
        result = {"accepted": False}
        self.session.host_key_unknown.emit(
            hostname, key.get_name(), key_fingerprint(key), event, result
        )
        event.wait()

        if not result["accepted"]:
            raise paramiko.SSHException(
                f"Host key for {hostname} was not accepted"
            )

        client.get_host_keys().add(hostname, key.get_name(), key)
        try:
            client.get_host_keys().save(str(get_known_hosts_path()))
        except OSError:
            pass


def load_private_key(path, passphrase=None):
    """Load a private key file, trying each supported key type.

    Raises paramiko.PasswordRequiredException if the key is encrypted and
    no (or the wrong) passphrase was given.
    """
    last_error = None
    for key_cls in KEY_CLASSES:
        try:
            return key_cls.from_private_key_file(path, password=passphrase)
        except paramiko.PasswordRequiredException as exc:
            last_error = exc
        except paramiko.SSHException:
            continue
    if last_error:
        raise last_error
    raise ValueError(f"Unsupported or invalid private key file: {path}")


class SSHSession(QThread):
    data_received = Signal(bytes)
    connection_error = Signal(str)
    connection_closed = Signal()
    connected = Signal()
    # hostname, key_type, fingerprint, threading.Event, result dict({"accepted": bool})
    host_key_unknown = Signal(str, str, str, object, object)

    def __init__(self, host, port, username, password=None, key_path=None,
                 key_passphrase=None, term="vt100", cols=80, rows=24):
        super().__init__()
        self.host = host
        self.port = port
        self.username = username
        self.password = password
        self.key_path = key_path
        self.key_passphrase = key_passphrase
        self.term = term
        self.cols = cols
        self.rows = rows
        self.client = None
        self.channel = None
        self._running = False

    def run(self):
        debug.log(f"SSH connect: {self.username}@{self.host}:{self.port}")
        try:
            pkey = None
            if self.key_path:
                pkey = load_private_key(self.key_path, self.key_passphrase)

            self.client = paramiko.SSHClient()
            known_hosts_path = get_known_hosts_path()
            if known_hosts_path.exists():
                self.client.load_host_keys(str(known_hosts_path))
            self.client.set_missing_host_key_policy(PromptHostKeyPolicy(self))
            self.client.connect(
                hostname=self.host,
                port=self.port,
                username=self.username,
                password=None if pkey else self.password,
                pkey=pkey,
                allow_agent=False,
                look_for_keys=False,
            )

            self.channel = self.client.invoke_shell(
                term=self.term, width=self.cols, height=self.rows
            )
            self.channel.settimeout(0.5)
            debug.log(f"SSH connected: {self.username}@{self.host}:{self.port}")
            self.connected.emit()

            self._running = True
            while self._running:
                try:
                    data = self.channel.recv(4096)
                except socket.timeout:
                    continue
                if not data:
                    break
                self.data_received.emit(data)
        except Exception as exc:
            debug.log(f"SSH connection error for {self.username}@{self.host}:{self.port}: {exc!r}")
            self.connection_error.emit(str(exc))
        finally:
            if self.client:
                self.client.close()
            debug.log(f"SSH connection closed: {self.username}@{self.host}:{self.port}")
            self.connection_closed.emit()

    def send(self, data: bytes):
        if self.channel:
            self.channel.send(data)

    def resize(self, cols, rows):
        if self.channel:
            self.channel.resize_pty(width=cols, height=rows)

    def stop(self):
        self._running = False
        if self.channel:
            self.channel.close()


# Sample /proc/stat twice (0.3s apart) to compute CPU%, plus a one-shot
# load average and free -b for RAM/swap.
STATS_COMMAND = (
    "cat /proc/stat | head -1 && sleep 0.3 && "
    "cat /proc/stat | head -1 && cat /proc/loadavg && free -b"
)


def _parse_remote_stats(output: str):
    lines = [line for line in output.strip().splitlines() if line.strip()]
    if len(lines) < 6:
        return None
    try:
        cpu1 = [int(x) for x in lines[0].split()[1:]]
        cpu2 = [int(x) for x in lines[1].split()[1:]]
        total_delta = sum(cpu2) - sum(cpu1)
        idle_delta = cpu2[3] - cpu1[3]
        cpu_pct = (total_delta - idle_delta) * 100 / total_delta if total_delta > 0 else 0.0

        load1 = float(lines[2].split()[0])

        mem_fields = lines[-2].split()
        swap_fields = lines[-1].split()
        mem_total, mem_used = int(mem_fields[1]), int(mem_fields[2])
        swap_total, swap_used = int(swap_fields[1]), int(swap_fields[2])

        ram_pct = mem_used * 100 / mem_total if mem_total > 0 else 0.0
        swap_pct = swap_used * 100 / swap_total if swap_total > 0 else 0.0
    except (ValueError, IndexError, ZeroDivisionError):
        return None

    return {"cpu": cpu_pct, "load": load1, "ram": ram_pct, "swap": swap_pct}


class RemoteStatsWorker(QThread):
    """Periodically polls the remote host (over a separate exec channel) for
    CPU/load/RAM/swap stats and emits stats_ready(dict)."""

    stats_ready = Signal(dict)

    POLL_INTERVAL_MS = 2000
    SLEEP_STEP_MS = 100

    def __init__(self, client, parent=None):
        super().__init__(parent)
        self.client = client
        self._running = True

    def run(self):
        while self._running:
            try:
                _, stdout, _ = self.client.exec_command(STATS_COMMAND, timeout=5)
                stats = _parse_remote_stats(stdout.read().decode(errors="replace"))
            except Exception:
                stats = None

            if stats is not None:
                self.stats_ready.emit(stats)

            for _ in range(self.POLL_INTERVAL_MS // self.SLEEP_STEP_MS):
                if not self._running:
                    break
                self.msleep(self.SLEEP_STEP_MS)

    def stop(self):
        self._running = False
