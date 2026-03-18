import time
import os
from pathlib import Path
from unittest.mock import patch
from backend.video_cache import VideoCache

def test_resolve_local_file(tmp_path):
    cache = VideoCache(cache_dir=tmp_path)
    local = tmp_path / "test.webm"
    local.write_bytes(b"video data")
    assert cache.resolve(str(local)) == str(local)

def test_resolve_remote_downloads(tmp_path):
    cache = VideoCache(cache_dir=tmp_path)
    with patch("backend.video_cache.urlretrieve") as mock_dl:
        mock_dl.side_effect = lambda url, path: Path(path).write_bytes(b"downloaded")
        result = cache.resolve("https://assets.masko.ai/video.webm")
        assert Path(result).exists()
        assert Path(result).read_bytes() == b"downloaded"

def test_resolve_remote_cached(tmp_path):
    cache = VideoCache(cache_dir=tmp_path)
    with patch("backend.video_cache.urlretrieve") as mock_dl:
        mock_dl.side_effect = lambda url, path: Path(path).write_bytes(b"downloaded")
        r1 = cache.resolve("https://assets.masko.ai/video.webm")
        r2 = cache.resolve("https://assets.masko.ai/video.webm")
        assert r1 == r2
        assert mock_dl.call_count == 1

def test_evict_old(tmp_path):
    cache = VideoCache(cache_dir=tmp_path, max_age_days=0)
    old = tmp_path / "old.webm"
    old.write_bytes(b"old")
    os_time = time.time() - 86400 * 31
    os.utime(old, (os_time, os_time))
    cache.evict_old()
    assert not old.exists()
