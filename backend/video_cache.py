import hashlib
import os
import time
from pathlib import Path
from urllib.request import urlretrieve


class VideoCache:
    def __init__(self, cache_dir=None, max_age_days=30):
        self._cache_dir = Path(cache_dir) if cache_dir else Path.home() / ".cache" / "masko-desktop" / "videos"
        self._cache_dir.mkdir(parents=True, exist_ok=True)
        self._max_age_seconds = max_age_days * 86400

    def resolve(self, url: str) -> str:
        path = Path(url)
        if path.exists():
            return str(path)
        cache_name = hashlib.sha256(url.encode()).hexdigest()[:16] + self._ext(url)
        cached = self._cache_dir / cache_name
        if cached.exists():
            return str(cached)
        urlretrieve(url, str(cached))
        return str(cached)

    def evict_old(self):
        now = time.time()
        for f in self._cache_dir.iterdir():
            if f.is_file() and (now - f.stat().st_mtime) > self._max_age_seconds:
                f.unlink()

    def _ext(self, url: str) -> str:
        for ext in (".webm", ".mov", ".mp4"):
            if ext in url:
                return ext
        return ".webm"
