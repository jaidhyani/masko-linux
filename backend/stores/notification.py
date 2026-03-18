import uuid
from dataclasses import dataclass, field
from datetime import datetime


@dataclass
class AppNotification:
    title: str
    message: str
    category: str
    priority: str = "normal"
    id: str = field(default_factory=lambda: str(uuid.uuid4()))
    read: bool = False
    created_at: datetime = field(default_factory=datetime.now)


class NotificationStore:
    def __init__(self):
        self._notifications: list[AppNotification] = []

    def add(self, notification: AppNotification):
        self._notifications.append(notification)

    def all(self) -> list[AppNotification]:
        return list(self._notifications)

    def unread_count(self) -> int:
        return sum(1 for n in self._notifications if not n.read)

    def mark_read(self, notification_id: str):
        for n in self._notifications:
            if n.id == notification_id:
                n.read = True
                break

    def dismiss(self, notification_id: str):
        self._notifications = [n for n in self._notifications if n.id != notification_id]

    def clear_all(self):
        self._notifications.clear()
